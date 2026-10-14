#include "influx_uploader.h"
#include "app_config.h"
#include "app_settings.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "influx";

#define INFLUX_UPLOAD_TASK_STACK (8192)
#define INFLUX_UPLOAD_TASK_PRIO  (4)
#define INFLUX_UPLOAD_QUEUE_LEN  (24)
#define INFLUX_BACKOFF_MIN_MS    (2000)
#define INFLUX_BACKOFF_MAX_MS    (60000)

typedef struct {
    char event_name[48];
    char event_value[48];
} influx_msg_t;

static bool  s_active = false;                 /* enabled AND configured    */
static char  s_write_endpoint[460];
static char  s_auth_header[300];
static char  s_device[48];
static char  s_ssid[64];
static char  s_last_result[48] = "disabled";

static QueueHandle_t      s_upload_queue    = nullptr;
static TaskHandle_t       s_upload_task     = nullptr;
static SemaphoreHandle_t  s_telemetry_mutex = nullptr;
static SemaphoreHandle_t  s_config_mutex    = nullptr;
static esp_http_client_handle_t s_http_client = nullptr;

static bool    s_have_rssi = false;
static bool    s_have_door = false;
static int32_t s_latest_rssi = 0;
static int32_t s_latest_door_closed = 1;

static bool     s_have_health = false;
static float    s_latest_temp_c = 0.0f;
static uint32_t s_latest_uptime_s = 0;
static uint32_t s_latest_heap_free = 0;

/* Device health is trend data — once every 30 s is plenty. */
#define INFLUX_HEALTH_INTERVAL_MS (30000)

static TickType_t s_backoff_until = 0;
static uint32_t   s_backoff_ms = 0;

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void set_result(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_last_result, sizeof(s_last_result), fmt, ap);
    va_end(ap);
}

void influx_uploader_last_result(char *out, size_t out_len)
{
    if (out == nullptr || out_len == 0) {
        return;
    }
    strncpy(out, s_last_result, out_len - 1);
    out[out_len - 1] = '\0';
}

/* ── HTTP plumbing ───────────────────────────────────────────────────── */

static void drop_http_client(void)
{
    if (s_http_client != nullptr) {
        esp_http_client_cleanup(s_http_client);
        s_http_client = nullptr;
    }
}

static esp_err_t ensure_http_client(void)
{
    if (s_http_client != nullptr) {
        return ESP_OK;
    }

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = s_write_endpoint;
    http_cfg.method = HTTP_METHOD_POST;
    http_cfg.timeout_ms = 6000;
    http_cfg.keep_alive_enable = true;
    /* Plain-HTTP self-hosted Influx is common on a home LAN; only attach the
     * cert bundle when we actually speak TLS. */
    if (strncmp(s_write_endpoint, "https://", 8) == 0) {
        http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    }

    s_http_client = esp_http_client_init(&http_cfg);
    if (s_http_client == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(s_http_client, "Authorization", s_auth_header);
    esp_http_client_set_header(s_http_client, "Content-Type", "text/plain; charset=utf-8");
    esp_http_client_set_header(s_http_client, "Accept", "application/json");
    return ESP_OK;
}

static void reset_backoff(void)
{
    s_backoff_ms = 0;
    s_backoff_until = 0;
}

static void increase_backoff(void)
{
    s_backoff_ms = (s_backoff_ms == 0)
                 ? INFLUX_BACKOFF_MIN_MS
                 : ((s_backoff_ms * 2 > INFLUX_BACKOFF_MAX_MS) ? INFLUX_BACKOFF_MAX_MS : s_backoff_ms * 2);
    s_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(s_backoff_ms);
}

static esp_err_t influx_write_line(const char *line)
{
    if (!s_active) {
        return ESP_OK;  /* disabled — silently accepted, history has it */
    }

    if (s_backoff_until != 0 && xTaskGetTickCount() < s_backoff_until) {
        return ESP_ERR_TIMEOUT;
    }

    xSemaphoreTake(s_config_mutex, portMAX_DELAY);

    esp_err_t err = ensure_http_client();
    if (err != ESP_OK) {
        increase_backoff();
        set_result("client init failed");
        xSemaphoreGive(s_config_mutex);
        return err;
    }

    esp_http_client_set_post_field(s_http_client, line, (int)strlen(line));
    err = esp_http_client_perform(s_http_client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        set_result("%s", esp_err_to_name(err));
        drop_http_client();
        increase_backoff();
        xSemaphoreGive(s_config_mutex);
        return err;
    }

    int status = esp_http_client_get_status_code(s_http_client);
    xSemaphoreGive(s_config_mutex);

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "Influx write returned HTTP %d", status);
        set_result("HTTP %d", status);
        increase_backoff();
        return ESP_FAIL;
    }

    set_result("ok");
    reset_backoff();
    return ESP_OK;
}

/* ── Worker ──────────────────────────────────────────────────────────── */

static void influx_upload_task(void *arg)
{
    (void)arg;

    influx_msg_t msg = {};
    char line[400];
    TickType_t last_telemetry_tick = xTaskGetTickCount();
    TickType_t last_health_tick = xTaskGetTickCount();

    while (true) {
        if (xQueueReceive(s_upload_queue, &msg, pdMS_TO_TICKS(200)) == pdTRUE && s_active) {
            time_t now = 0;
            time(&now);

            const char *value = (msg.event_value[0] != '\0') ? msg.event_value : "1";
            snprintf(line, sizeof(line),
                "system_event,device=%s,SSID=%s,event=%s value=\"%s\" %lld",
                s_device[0] ? s_device : "ESP32",
                s_ssid[0] ? s_ssid : "unknown",
                msg.event_name, value, (long long)now);

            esp_err_t err = influx_write_line(line);
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Event upload failed: %s", esp_err_to_name(err));
            }
        }

        TickType_t now_tick = xTaskGetTickCount();
        if (s_active && (now_tick - last_telemetry_tick) >= pdMS_TO_TICKS(APP_INFLUX_TELEMETRY_MS)) {
            bool have_rssi = false, have_door = false;
            int32_t rssi = 0, door_closed = 1;

            xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
            have_rssi   = s_have_rssi;
            have_door   = s_have_door;
            rssi        = s_latest_rssi;
            door_closed = s_latest_door_closed;
            xSemaphoreGive(s_telemetry_mutex);

            time_t now = 0;
            time(&now);

            if (have_rssi) {
                snprintf(line, sizeof(line),
                    "wifi_status_garage_door,device=%s,SSID=%s rssi=%ldi %lld",
                    s_device[0] ? s_device : "ESP32",
                    s_ssid[0] ? s_ssid : "unknown",
                    (long)rssi, (long long)now);
                influx_write_line(line);
            }

            if (have_door) {
                snprintf(line, sizeof(line),
                    "garage\\ door\\ position,device=%s,SSID=%s value=%ldi %lld",
                    s_device[0] ? s_device : "ESP32",
                    s_ssid[0] ? s_ssid : "unknown",
                    (long)door_closed, (long long)now);
                influx_write_line(line);
            }

            last_telemetry_tick = now_tick;
        }

        if (s_active && (now_tick - last_health_tick) >= pdMS_TO_TICKS(INFLUX_HEALTH_INTERVAL_MS)) {
            last_health_tick = now_tick;

            bool have_health = false;
            float temp_c = 0.0f;
            uint32_t uptime_s = 0, heap_free = 0;

            xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
            have_health = s_have_health;
            temp_c      = s_latest_temp_c;
            uptime_s    = s_latest_uptime_s;
            heap_free   = s_latest_heap_free;
            xSemaphoreGive(s_telemetry_mutex);

            if (have_health) {
                time_t now = 0;
                time(&now);
                snprintf(line, sizeof(line),
                    "garage_device,device=%s,SSID=%s temp_c=%.1f,uptime_s=%lui,heap_free=%lui %lld",
                    s_device[0] ? s_device : "ESP32",
                    s_ssid[0] ? s_ssid : "unknown",
                    (double)temp_c,
                    (unsigned long)uptime_s,
                    (unsigned long)heap_free,
                    (long long)now);
                influx_write_line(line);
            }
        }

        if (!s_active) {
            /* Nothing to do; keep the task cheap while Influx is off. */
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/* ── Configuration ───────────────────────────────────────────────────── */

void influx_uploader_reload(void)
{
    const app_settings_t *cfg = settings_get();

    xSemaphoreTake(s_config_mutex, portMAX_DELAY);

    bool configured = cfg->influx_enabled
                   && cfg->influx_url[0]
                   && cfg->influx_token[0]
                   && cfg->influx_org[0]
                   && cfg->influx_bucket[0];

    /* Trim a trailing slash so "http://host:8086/" builds a clean path. */
    char url[sizeof(cfg->influx_url)];
    copy_str(url, sizeof(url), cfg->influx_url);
    size_t ulen = strlen(url);
    while (ulen > 0 && url[ulen - 1] == '/') {
        url[--ulen] = '\0';
    }

    snprintf(s_write_endpoint, sizeof(s_write_endpoint),
        "%s/api/v2/write?org=%s&bucket=%s&precision=s",
        url, cfg->influx_org, cfg->influx_bucket);

    snprintf(s_auth_header, sizeof(s_auth_header), "Token %s", cfg->influx_token);

    copy_str(s_device, sizeof(s_device), cfg->device_name);
    copy_str(s_ssid,   sizeof(s_ssid),   cfg->wifi_ssid);

    /* Any config change invalidates the pooled connection. */
    drop_http_client();
    reset_backoff();

    s_active = configured;
    if (!configured) {
        set_result("%s", cfg->influx_enabled ? "incomplete config" : "disabled");
    } else {
        set_result("configured");
    }

    xSemaphoreGive(s_config_mutex);

    ESP_LOGI(TAG, "Influx %s", configured ? "enabled" : "disabled");
}

bool influx_uploader_is_active(void)
{
    return s_active;
}

esp_err_t influx_uploader_init(void)
{
    if (s_telemetry_mutex == nullptr) {
        s_telemetry_mutex = xSemaphoreCreateMutex();
        if (s_telemetry_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_config_mutex == nullptr) {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_upload_queue == nullptr) {
        s_upload_queue = xQueueCreate(INFLUX_UPLOAD_QUEUE_LEN, sizeof(influx_msg_t));
        if (s_upload_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    influx_uploader_reload();

    if (s_upload_task == nullptr) {
        if (xTaskCreate(influx_upload_task, "influx-upload", INFLUX_UPLOAD_TASK_STACK,
                        nullptr, INFLUX_UPLOAD_TASK_PRIO, &s_upload_task) != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

/* ── Writers ─────────────────────────────────────────────────────────── */

esp_err_t influx_uploader_write_wifi_rssi(int32_t rssi)
{
    if (!s_active || s_telemetry_mutex == nullptr) {
        return ESP_OK;
    }
    xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
    s_latest_rssi = rssi;
    s_have_rssi = true;
    xSemaphoreGive(s_telemetry_mutex);
    return ESP_OK;
}

esp_err_t influx_uploader_write_garage_door_position(int32_t closed)
{
    if (!s_active || s_telemetry_mutex == nullptr) {
        return ESP_OK;
    }
    xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
    s_latest_door_closed = (closed != 0) ? 1 : 0;
    s_have_door = true;
    xSemaphoreGive(s_telemetry_mutex);
    return ESP_OK;
}

esp_err_t influx_uploader_write_device_health(float temp_c, uint32_t uptime_s, uint32_t heap_free)
{
    if (!s_active || s_telemetry_mutex == nullptr) {
        return ESP_OK;
    }
    xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
    s_latest_temp_c    = temp_c;
    s_latest_uptime_s  = uptime_s;
    s_latest_heap_free = heap_free;
    s_have_health = true;
    xSemaphoreGive(s_telemetry_mutex);
    return ESP_OK;
}

esp_err_t influx_uploader_write_event(const char *event_name, const char *event_value)
{
    if (!s_active || s_upload_queue == nullptr) {
        return ESP_OK;
    }
    if (event_name == nullptr || event_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    influx_msg_t msg = {};
    copy_str(msg.event_name, sizeof(msg.event_name), event_name);
    copy_str(msg.event_value, sizeof(msg.event_value), event_value);

    return (xQueueSend(s_upload_queue, &msg, 0) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}
