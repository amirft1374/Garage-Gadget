#pragma once

/*
 * web_server.h — Local web UI, setup wizard and JSON API.
 *
 * Serves two different front-ends from the same HTTP server:
 *
 *   Setup wizard  — shown while the device is unprovisioned. Reachable over
 *                   the device's own SoftAP. Walks the user through Wi-Fi,
 *                   optional InfluxDB/Grafana, optional notifications and a
 *                   mandatory admin-password change.
 *
 *   Dashboard     — shown once provisioned. Live door graphic, alarm state,
 *                   on-device history charts, terminal log, settings, OTA.
 *
 * Authentication is a bearer token obtained from POST /api/login with the
 * admin password. The password itself is never placed in a URL.
 */

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Redirects the ESP log stream into the in-memory terminal buffer. Call
 * early — before Wi-Fi — so boot messages are visible in the UI. */
void web_server_init_logging(void);

/* Starts the HTTP server on `port`. */
esp_err_t web_server_start(uint16_t port);

/* Network details rendered in the UI header. Safe to call on reconnect. */
void web_server_set_network_info(const char *ip_address, const char *mdns_hostname, uint16_t port);

/* ── Live state pushed in by the main loop ───────────────────────────── */

void web_server_set_door_closed(bool closed);
bool web_server_is_door_closed(void);

/* Seconds the door has been continuously open (0 when closed). */
void web_server_set_door_open_seconds(uint32_t seconds);

/* Alarm state for the dashboard lamps. `left_open_periods` is how many
 * auto-close delays have elapsed while open (0 = not tripped). */
void web_server_set_alarm_state(bool immediate_active, uint32_t left_open_periods);

/* Result of the optional home-server ping. */
void web_server_set_server_online(bool online);

/* Latest die temperature, in °C. */
void web_server_set_chip_temp(float temp_c);

#ifdef __cplusplus
}
#endif
