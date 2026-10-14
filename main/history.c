#include "history.h"
#include "app_config.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "history";

/* ── Flash event ring ────────────────────────────────────────────────── */

#define HIST_PARTITION_LABEL   "history"
#define HIST_SECTOR_SIZE       (4096)
#define HIST_SEQ_EMPTY         (0xFFFFFFFFU)

typedef struct __attribute__((packed)) {
    uint32_t seq;      /* monotonic; 0xFFFFFFFF in erased flash */
    int64_t  ts;       /* unix seconds, 0 when the clock was unsynced */
    uint8_t  type;
    uint8_t  value;
    uint16_t crc;      /* CRC-16/CCITT over the first 14 bytes */
} hist_record_t;

_Static_assert(sizeof(hist_record_t) == 16, "history record must stay 16 bytes");

#define RECORDS_PER_SECTOR (HIST_SECTOR_SIZE / sizeof(hist_record_t))

static const esp_partition_t *s_part = NULL;
static uint32_t s_total_records = 0;
static uint32_t s_write_idx     = 0;   /* next record slot to write        */
static uint32_t s_next_seq      = 1;
static uint32_t s_live_count    = 0;
static SemaphoreHandle_t s_flash_mutex = NULL;

/* ── RAM sample ring ─────────────────────────────────────────────────── */

typedef struct {
    int64_t ts;
    int16_t temp_c10;   /* °C × 10 */
    int8_t  rssi;
    uint8_t flags;      /* bit0: door closed */
} hist_sample_t;

#define SAMPLE_FLAG_CLOSED (1u << 0)

static hist_sample_t *s_samples = NULL;
static uint32_t s_sample_cap   = 0;
static uint32_t s_sample_head  = 0;   /* next slot to write */
static uint32_t s_sample_count = 0;
static bool     s_samples_psram = false;
static SemaphoreHandle_t s_sample_mutex = NULL;

/* ── Helpers ─────────────────────────────────────────────────────────── */

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static bool record_valid(const hist_record_t *r)
{
    if (r->seq == HIST_SEQ_EMPTY) {
        return false;
    }
    if (r->type >= HIST_EV_COUNT) {
        return false;
    }
    return crc16_ccitt((const uint8_t *)r, sizeof(*r) - sizeof(r->crc)) == r->crc;
}

static esp_err_t read_record(uint32_t idx, hist_record_t *out)
{
    return esp_partition_read(s_part, (size_t)idx * sizeof(hist_record_t), out, sizeof(*out));
}

static const char *k_event_names[HIST_EV_COUNT] = {
    [HIST_EV_BOOT]             = "boot",
    [HIST_EV_DOOR_OPENED]      = "door_opened",
    [HIST_EV_DOOR_CLOSED]      = "door_closed",
    [HIST_EV_RELAY]            = "relay_activated",
    [HIST_EV_ALARM_IMMEDIATE]  = "alarm_immediate",
    [HIST_EV_ALARM_LEFT_OPEN]  = "alarm_left_open",
    [HIST_EV_WIFI_LOST]        = "wifi_lost",
    [HIST_EV_WIFI_UP]          = "wifi_up",
    [HIST_EV_AUTO_CLOSE]       = "auto_close",
    [HIST_EV_SETTINGS]         = "settings",
    [HIST_EV_NOTIFY_FAIL]      = "notify_failed",
};

const char *history_event_name(hist_event_t type)
{
    if (type >= HIST_EV_COUNT || k_event_names[type] == NULL) {
        return "unknown";
    }
    return k_event_names[type];
}

/* ── Ring-head recovery ──────────────────────────────────────────────── */

/* Finds where the previous boot left off.
 *
 * Records are written with a strictly increasing sequence number, so the
 * sector whose *first* record has the highest sequence is the newest sector.
 * Reading one record per sector keeps recovery to ~256 small reads instead
 * of scanning the whole megabyte. */
static void recover_head(void)
{
    uint32_t sectors = s_total_records / RECORDS_PER_SECTOR;
    uint32_t best_sector = 0;
    uint32_t best_seq    = 0;
    bool     found       = false;

    for (uint32_t s = 0; s < sectors; s++) {
        hist_record_t r;
        if (read_record(s * RECORDS_PER_SECTOR, &r) != ESP_OK) {
            continue;
        }
        if (!record_valid(&r)) {
            continue;
        }
        if (!found || r.seq > best_seq) {
            found       = true;
            best_seq    = r.seq;
            best_sector = s;
        }
    }

    if (!found) {
        /* Blank or unreadable partition — start from scratch. */
        s_write_idx  = 0;
        s_next_seq   = 1;
        s_live_count = 0;
        ESP_LOGI(TAG, "Event ring is empty; starting a fresh log");
        return;
    }

    /* Walk forward inside the newest sector to the first empty slot. */
    uint32_t base = best_sector * RECORDS_PER_SECTOR;
    uint32_t last_valid_off = 0;
    uint32_t last_seq = best_seq;

    for (uint32_t i = 0; i < RECORDS_PER_SECTOR; i++) {
        hist_record_t r;
        if (read_record(base + i, &r) != ESP_OK || !record_valid(&r)) {
            break;
        }
        last_valid_off = i;
        last_seq       = r.seq;
    }

    s_write_idx = (base + last_valid_off + 1) % s_total_records;
    s_next_seq  = last_seq + 1;

    /* Count what is actually readable, so the UI can show occupancy. */
    s_live_count = 0;
    uint32_t idx = s_write_idx;
    for (uint32_t n = 0; n < s_total_records; n++) {
        idx = (idx == 0) ? (s_total_records - 1) : (idx - 1);
        hist_record_t r;
        if (read_record(idx, &r) != ESP_OK || !record_valid(&r)) {
            break;
        }
        s_live_count++;
    }

    ESP_LOGI(TAG, "Event ring recovered: %" PRIu32 " records, next seq %" PRIu32 ", head slot %" PRIu32,
        s_live_count, s_next_seq, s_write_idx);
}

/* ── Init ────────────────────────────────────────────────────────────── */

esp_err_t history_init(void)
{
    if (s_flash_mutex == NULL) {
        s_flash_mutex = xSemaphoreCreateMutex();
    }
    if (s_sample_mutex == NULL) {
        s_sample_mutex = xSemaphoreCreateMutex();
    }
    if (s_flash_mutex == NULL || s_sample_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* ── Flash tier ── */
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                      (esp_partition_subtype_t)0x40,
                                      HIST_PARTITION_LABEL);
    if (s_part == NULL) {
        ESP_LOGE(TAG, "No '%s' partition found — event logging disabled. "
                      "Flash the partition table from partitions.csv.", HIST_PARTITION_LABEL);
    } else {
        s_total_records = (uint32_t)(s_part->size / sizeof(hist_record_t));
        /* Round down to a whole number of sectors so erase-ahead is exact. */
        s_total_records -= (s_total_records % RECORDS_PER_SECTOR);
        recover_head();
        ESP_LOGI(TAG, "Event ring: %" PRIu32 " slots across %u KB",
            s_total_records, (unsigned)(s_part->size / 1024));
    }

    /* ── RAM tier ── */
    s_sample_cap = (uint32_t)APP_SAMPLE_RETENTION_DAYS * 24u * 3600u / APP_SAMPLE_INTERVAL_S;

    size_t bytes = (size_t)s_sample_cap * sizeof(hist_sample_t);
    s_samples = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_samples_psram = (s_samples != NULL);

    if (s_samples == NULL) {
        /* No PSRAM on this board — fall back to a 2-day window in internal
         * RAM, which is the retention the product promises as a minimum. */
        s_sample_cap = 2u * 24u * 3600u / APP_SAMPLE_INTERVAL_S;
        bytes = (size_t)s_sample_cap * sizeof(hist_sample_t);
        s_samples = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (s_samples == NULL) {
            ESP_LOGE(TAG, "Sample ring allocation failed (%u bytes)", (unsigned)bytes);
            s_sample_cap = 0;
            return ESP_ERR_NO_MEM;
        }
    }

    memset(s_samples, 0, bytes);
    s_sample_head  = 0;
    s_sample_count = 0;

    ESP_LOGI(TAG, "Sample ring: %" PRIu32 " slots (%u KB) in %s, %d-minute cadence",
        s_sample_cap, (unsigned)(bytes / 1024),
        s_samples_psram ? "PSRAM" : "internal RAM",
        APP_SAMPLE_INTERVAL_S / 60);

    return ESP_OK;
}

/* ── Event writing ───────────────────────────────────────────────────── */

void history_log(hist_event_t type, uint8_t value)
{
    if (s_part == NULL || s_total_records == 0 || type >= HIST_EV_COUNT) {
        return;
    }

    time_t now = 0;
    time(&now);

    hist_record_t r = {
        .seq   = s_next_seq,
        .ts    = (now > 1700000000) ? (int64_t)now : 0,  /* 0 = clock unsynced */
        .type  = (uint8_t)type,
        .value = value,
        .crc   = 0,
    };
    r.crc = crc16_ccitt((const uint8_t *)&r, sizeof(r) - sizeof(r.crc));

    xSemaphoreTake(s_flash_mutex, portMAX_DELAY);

    /* Erase the sector we are about to step into. Doing it lazily at the
     * boundary keeps the common path to a single 16-byte program. */
    if ((s_write_idx % RECORDS_PER_SECTOR) == 0) {
        size_t off = (size_t)s_write_idx * sizeof(hist_record_t);
        esp_err_t err = esp_partition_erase_range(s_part, off, HIST_SECTOR_SIZE);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Sector erase at 0x%x failed: %s", (unsigned)off, esp_err_to_name(err));
            xSemaphoreGive(s_flash_mutex);
            return;
        }
        if (s_live_count >= s_total_records - RECORDS_PER_SECTOR) {
            /* The erase just dropped the oldest sector's worth of records. */
            s_live_count = s_total_records - RECORDS_PER_SECTOR;
        }
    }

    esp_err_t err = esp_partition_write(s_part,
        (size_t)s_write_idx * sizeof(hist_record_t), &r, sizeof(r));

    if (err == ESP_OK) {
        s_next_seq++;
        s_write_idx = (s_write_idx + 1) % s_total_records;
        if (s_live_count < s_total_records) {
            s_live_count++;
        }
    } else {
        ESP_LOGW(TAG, "Event write failed: %s", esp_err_to_name(err));
    }

    xSemaphoreGive(s_flash_mutex);
}

/* ── Sample writing ──────────────────────────────────────────────────── */

void history_sample(bool door_closed, int rssi, float temp_c)
{
    if (s_samples == NULL || s_sample_cap == 0) {
        return;
    }

    time_t now = 0;
    time(&now);

    int temp10 = (int)(temp_c * 10.0f);
    if (temp10 > 32767) temp10 = 32767;
    if (temp10 < -32768) temp10 = -32768;

    if (rssi > 0)    rssi = 0;
    if (rssi < -128) rssi = -128;

    hist_sample_t s = {
        .ts       = (now > 1700000000) ? (int64_t)now : 0,
        .temp_c10 = (int16_t)temp10,
        .rssi     = (int8_t)rssi,
        .flags    = door_closed ? SAMPLE_FLAG_CLOSED : 0u,
    };

    xSemaphoreTake(s_sample_mutex, portMAX_DELAY);
    s_samples[s_sample_head] = s;
    s_sample_head = (s_sample_head + 1) % s_sample_cap;
    if (s_sample_count < s_sample_cap) {
        s_sample_count++;
    }
    xSemaphoreGive(s_sample_mutex);
}

/* ── Readers ─────────────────────────────────────────────────────────── */

int history_events_json(char *out, size_t out_size, int max_records, int64_t since_ts)
{
    if (out == NULL || out_size < 32) {
        return -1;
    }
    if (max_records <= 0) {
        max_records = 200;
    }

    int pos = snprintf(out, out_size, "{\"ev\":[");
    if (pos < 0 || (size_t)pos >= out_size) {
        return -1;
    }

    int emitted = 0;

    if (s_part != NULL && s_total_records > 0) {
        xSemaphoreTake(s_flash_mutex, portMAX_DELAY);

        uint32_t idx = s_write_idx;
        for (uint32_t n = 0; n < s_total_records && emitted < max_records; n++) {
            idx = (idx == 0) ? (s_total_records - 1) : (idx - 1);

            hist_record_t r;
            if (read_record(idx, &r) != ESP_OK || !record_valid(&r)) {
                break;  /* reached the erased gap: nothing older survives */
            }
            if (since_ts > 0 && r.ts > 0 && r.ts < since_ts) {
                break;  /* records walk backwards in time; we are done */
            }

            int n2 = snprintf(out + pos, out_size - (size_t)pos,
                "%s[%lld,%u,%u]",
                (emitted == 0) ? "" : ",",
                (long long)r.ts, (unsigned)r.type, (unsigned)r.value);

            if (n2 < 0 || (size_t)n2 >= out_size - (size_t)pos) {
                break;  /* buffer full — return what fits */
            }
            pos += n2;
            emitted++;
        }

        xSemaphoreGive(s_flash_mutex);
    }

    int n3 = snprintf(out + pos, out_size - (size_t)pos, "],\"n\":%d}", emitted);
    if (n3 < 0 || (size_t)n3 >= out_size - (size_t)pos) {
        return -1;
    }
    return pos + n3;
}

int history_samples_json(char *out, size_t out_size, int max_points, int64_t since_ts)
{
    if (out == NULL || out_size < 32) {
        return -1;
    }
    if (max_points <= 0) {
        max_points = 360;
    }

    int pos = snprintf(out, out_size, "{\"s\":[");
    if (pos < 0 || (size_t)pos >= out_size) {
        return -1;
    }

    int emitted = 0;

    if (s_samples != NULL && s_sample_count > 0) {
        xSemaphoreTake(s_sample_mutex, portMAX_DELAY);

        /* Oldest slot in the ring. */
        uint32_t start = (s_sample_count < s_sample_cap)
                       ? 0
                       : s_sample_head;

        /* Skip past anything older than since_ts. */
        uint32_t first = 0;
        while (first < s_sample_count) {
            const hist_sample_t *s = &s_samples[(start + first) % s_sample_cap];
            if (since_ts <= 0 || s->ts == 0 || s->ts >= since_ts) {
                break;
            }
            first++;
        }

        uint32_t avail = s_sample_count - first;
        uint32_t stride = (avail + (uint32_t)max_points - 1) / (uint32_t)max_points;
        if (stride == 0) {
            stride = 1;
        }

        for (uint32_t i = first; i < s_sample_count && emitted < max_points; i += stride) {
            /* Aggregate one bucket: mean RSSI/temp, but "open" wins on the
             * door bit so a short opening is never smoothed away. */
            int32_t rssi_sum = 0, temp_sum = 0;
            uint32_t used = 0;
            bool all_closed = true;
            int64_t ts = 0;

            for (uint32_t k = 0; k < stride && (i + k) < s_sample_count; k++) {
                const hist_sample_t *s = &s_samples[(start + i + k) % s_sample_cap];
                if (used == 0) {
                    ts = s->ts;
                }
                rssi_sum += s->rssi;
                temp_sum += s->temp_c10;
                if ((s->flags & SAMPLE_FLAG_CLOSED) == 0) {
                    all_closed = false;
                }
                used++;
            }

            if (used == 0) {
                continue;
            }

            int n2 = snprintf(out + pos, out_size - (size_t)pos,
                "%s[%lld,%d,%d,%d]",
                (emitted == 0) ? "" : ",",
                (long long)ts,
                all_closed ? 1 : 0,
                (int)(rssi_sum / (int32_t)used),
                (int)(temp_sum / (int32_t)used));

            if (n2 < 0 || (size_t)n2 >= out_size - (size_t)pos) {
                break;
            }
            pos += n2;
            emitted++;
        }

        xSemaphoreGive(s_sample_mutex);
    }

    int n3 = snprintf(out + pos, out_size - (size_t)pos,
        "],\"n\":%d,\"step\":%d}", emitted, APP_SAMPLE_INTERVAL_S);
    if (n3 < 0 || (size_t)n3 >= out_size - (size_t)pos) {
        return -1;
    }
    return pos + n3;
}

void history_stats(hist_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));

    out->event_capacity   = s_total_records;
    out->event_count      = s_live_count;
    out->sample_capacity  = s_sample_cap;
    out->sample_count     = s_sample_count;
    out->samples_in_psram = s_samples_psram;

    if (s_part == NULL || s_live_count == 0) {
        return;
    }

    xSemaphoreTake(s_flash_mutex, portMAX_DELAY);

    hist_record_t r;
    uint32_t newest = (s_write_idx == 0) ? (s_total_records - 1) : (s_write_idx - 1);
    if (read_record(newest, &r) == ESP_OK && record_valid(&r)) {
        out->event_newest_ts = r.ts;
    }

    uint32_t oldest = (s_write_idx + s_total_records - s_live_count) % s_total_records;
    if (read_record(oldest, &r) == ESP_OK && record_valid(&r)) {
        out->event_oldest_ts = r.ts;
    }

    xSemaphoreGive(s_flash_mutex);
}

esp_err_t history_clear(void)
{
    esp_err_t err = ESP_OK;

    if (s_part != NULL) {
        xSemaphoreTake(s_flash_mutex, portMAX_DELAY);
        err = esp_partition_erase_range(s_part, 0, s_part->size);
        if (err == ESP_OK) {
            s_write_idx  = 0;
            s_next_seq   = 1;
            s_live_count = 0;
        }
        xSemaphoreGive(s_flash_mutex);
    }

    if (s_samples != NULL) {
        xSemaphoreTake(s_sample_mutex, portMAX_DELAY);
        s_sample_head  = 0;
        s_sample_count = 0;
        xSemaphoreGive(s_sample_mutex);
    }

    ESP_LOGW(TAG, "History cleared");
    return err;
}
