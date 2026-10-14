#pragma once

/*
 * ha_mqtt.h — Home Assistant integration over MQTT.
 *
 * The device connects out to the user's broker and publishes MQTT Discovery
 * payloads, so Home Assistant builds the entities itself: a garage-door
 * cover, the door voltage, Wi-Fi signal, chip temperature, the auto-close
 * and alarm switches, and the auto-close delay. No YAML on the user's side.
 *
 * Outbound connection on purpose. Home Assistant never has to reach the
 * device, which keeps working when the ESP is on an IoT VLAN, behind a
 * guest-network client isolation rule, or on a changing DHCP lease — all of
 * which break the polling approach.
 *
 * A retained last-will on the availability topic means HA marks the device
 * unavailable within seconds of it dropping off, rather than showing a
 * stale door state indefinitely. That matters more here than for most
 * integrations: a stale "closed" is actively misleading.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the client if MQTT is enabled in settings. Safe to call when it is
 * not — the module then does nothing until enabled. Call after Wi-Fi. */
esp_err_t ha_mqtt_init(void);

/* Re-reads the broker settings and reconnects. Call after the user saves
 * the Settings page. */
void ha_mqtt_reload(void);

/* True when connected to the broker. */
bool ha_mqtt_is_connected(void);

/* Short human-readable status for the Settings page: "connected",
 * "disabled", "connecting", or the last error. */
void ha_mqtt_status(char *out, size_t out_len);

/* Publishes the current door position. Cheap and idempotent: call whenever
 * the state is known, it only sends when the value actually changed. */
void ha_mqtt_publish_door(bool closed);

/* Publishes the periodic diagnostics. Rate-limited internally. */
void ha_mqtt_publish_telemetry(uint32_t door_mv, int rssi, float temp_c);

/* Publishes the switch/number states that the user can also change from the
 * web UI, so Home Assistant stays in step with changes made there. */
void ha_mqtt_publish_config_state(void);

/* Publishes an alarm transition as an HA event-style state. */
void ha_mqtt_publish_alarm(bool immediate_active, uint32_t left_open_periods);

#ifdef __cplusplus
}
#endif
