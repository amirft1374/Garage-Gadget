#pragma once

/*
 * history.h — On-device data retention, for users who run no InfluxDB and
 * no Grafana.
 *
 * Two tiers, matching how the data is actually used:
 *
 *   Events  — door opened/closed, relay pulses, alarms, boots, Wi-Fi drops.
 *             Written to a dedicated 1 MB flash partition as a wear-levelled
 *             ring of fixed-size records. Survives reboots and power cuts.
 *             ~65k records; at a busy 100 events/day that is well over a
 *             year, so the configured 60-day window always fits.
 *
 *   Samples — door state + RSSI + die temperature, once a minute, held in a
 *             PSRAM ring covering 10 days. Deliberately volatile: this is
 *             chart texture, not a record, and keeping it out of flash means
 *             no write-wear from routine polling.
 *
 * Both tiers are readable from the web UI without any external service.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HIST_EV_BOOT = 0,
    HIST_EV_DOOR_OPENED,
    HIST_EV_DOOR_CLOSED,
    HIST_EV_RELAY,             /* value: 0=web 1=auto-close 2=other        */
    HIST_EV_ALARM_IMMEDIATE,   /* value: 1=raised 0=cleared                */
    HIST_EV_ALARM_LEFT_OPEN,   /* value: number of delay periods elapsed   */
    HIST_EV_WIFI_LOST,
    HIST_EV_WIFI_UP,
    HIST_EV_AUTO_CLOSE,        /* value: 1=enabled 0=disabled              */
    HIST_EV_SETTINGS,          /* value: 1=changed 2=factory reset         */
    HIST_EV_NOTIFY_FAIL,       /* value: transport id                      */
    HIST_EV_COUNT
} hist_event_t;

typedef struct {
    uint32_t event_count;      /* live records in the flash ring           */
    uint32_t event_capacity;
    int64_t  event_oldest_ts;
    int64_t  event_newest_ts;
    uint32_t sample_count;     /* live samples in the RAM ring             */
    uint32_t sample_capacity;
    bool     samples_in_psram;
} hist_stats_t;

/* Opens the `history` flash partition, recovers the ring head, and allocates
 * the RAM sample ring. Safe to call before Wi-Fi/SNTP: records written with
 * an unsynced clock carry ts=0 and are labelled as such in the UI. */
esp_err_t history_init(void);

/* Appends one event. Cheap (a single 16-byte flash write, plus a 4 KB erase
 * once every 256 records) and safe to call from any task. */
void history_log(hist_event_t type, uint8_t value);

/* Records one periodic sample into the RAM ring. Call once per
 * APP_SAMPLE_INTERVAL_S. */
void history_sample(bool door_closed, int rssi, float temp_c);

/* Human-readable event name, for the UI and for Influx event tags. */
const char *history_event_name(hist_event_t type);

/* Newest-first JSON array of events:
 *   {"n":12,"ev":[[ts,type,value],...]}
 * `max_records` caps the result; `since_ts` (0 = no bound) drops anything
 * older. Returns bytes written, or -1 on overflow. */
int history_events_json(char *out, size_t out_size, int max_records, int64_t since_ts);

/* Oldest-first JSON of the sample trace, decimated to at most `max_points`
 * buckets so the response stays small enough to render:
 *   {"n":360,"s":[[ts,door,rssi,temp10],...]}
 * `door` is 1 if the door was closed for the whole bucket, 0 if it was open
 * at any point in it (so a brief opening is never averaged away).
 * Returns bytes written, or -1 on overflow. */
int history_samples_json(char *out, size_t out_size, int max_points, int64_t since_ts);

/* Storage occupancy, for the UI's "Local storage" panel. */
void history_stats(hist_stats_t *out);

/* Erases the flash event ring and empties the RAM sample ring. */
esp_err_t history_clear(void);

#ifdef __cplusplus
}
#endif
