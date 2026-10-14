#pragma once

/*
 * influx_uploader.h — Optional InfluxDB v2 telemetry.
 *
 * Entirely opt-in. When the user has not configured InfluxDB (the default
 * out of the box), every call here is a cheap no-op and all data lives in
 * the on-device history instead — see history.h.
 *
 * Configuration is read from app_settings, so the user can point the device
 * at a bucket, change the token, or switch it off again at runtime without
 * reflashing.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the upload worker. Safe to call even when Influx is disabled —
 * the task idles until the user enables it. */
esp_err_t influx_uploader_init(void);

/* Re-reads URL/token/org/bucket from settings and drops any pooled
 * connection. Call after the user saves the Settings page. */
void influx_uploader_reload(void);

/* True when the uploader is enabled and fully configured. */
bool influx_uploader_is_active(void);

/* Latest per-second telemetry values. These are coalesced: calling more
 * often than the upload cadence simply overwrites the pending value. */
esp_err_t influx_uploader_write_wifi_rssi(int32_t rssi);
esp_err_t influx_uploader_write_garage_door_position(int32_t closed);

/* Diagnostics for the shipped Grafana dashboard's health row. Coalesced
 * like the values above, but uploaded on a slower cadence — this is trend
 * data, not something anyone watches per-second. */
esp_err_t influx_uploader_write_device_health(float temp_c, uint32_t uptime_s, uint32_t heap_free);

/* One-off event point. Queued; returns ESP_ERR_TIMEOUT if the queue is
 * full, ESP_OK when accepted or when Influx is disabled. */
esp_err_t influx_uploader_write_event(const char *event_name, const char *event_value);

/* Copies a short human-readable status ("ok", "HTTP 401", "disabled")
 * describing the most recent write attempt. */
void influx_uploader_last_result(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
