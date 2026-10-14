#pragma once

/*
 * wifi_manager.h — Station + SoftAP lifecycle.
 *
 * Credentials come from app_settings, never from compiled-in constants, and
 * can be changed at any time from the Settings page.
 *
 * Rescue behaviour: if the configured AP cannot be joined within
 * APP_WIFI_JOIN_TIMEOUT_S the device brings its own SoftAP up alongside the
 * still-retrying station, so a mistyped password or a replaced router can
 * never leave the unit permanently unreachable. The SoftAP is torn down
 * again the moment the station gets an IP.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MGR_OFF = 0,
    WIFI_MGR_STATION,     /* joined (or joining) the user's AP        */
    WIFI_MGR_SETUP_AP,    /* our own AP only — first-time setup       */
    WIFI_MGR_RESCUE,      /* our AP up because the station cannot join */
} wifi_mgr_mode_t;

/* Fired on the Wi-Fi event task whenever the station connects or drops. */
typedef void (*wifi_mgr_event_cb_t)(bool connected, const char *ip, void *ctx);

/* Brings up netif, the default event loop and the Wi-Fi driver. */
esp_err_t wifi_manager_init(void);

/* Joins the AP named in settings. Starts the rescue watchdog. */
esp_err_t wifi_manager_start_station(void);

/* Brings up the provisioning SoftAP (in APSTA mode so scanning works).
 * SSID is "<APP_SETUP_AP_SSID_PREFIX>-XXXX" using the last MAC bytes. */
esp_err_t wifi_manager_start_setup_ap(void);

/* Re-reads credentials from settings and reconnects without a reboot.
 * Safe to call from an HTTP handler. */
esp_err_t wifi_manager_apply_new_credentials(void);

/* Blocks until the station has an IP, or the timeout expires. */
bool wifi_manager_wait_for_ip(uint32_t timeout_ms);

bool             wifi_manager_is_connected(void);
wifi_mgr_mode_t  wifi_manager_mode(void);
const char      *wifi_manager_ip(void);        /* station IP, "" if none  */
const char      *wifi_manager_ap_ip(void);     /* SoftAP IP, "" if down   */
const char      *wifi_manager_ap_ssid(void);   /* "" when the AP is down  */
int              wifi_manager_rssi(void);      /* 0 when disconnected     */

void wifi_manager_set_callback(wifi_mgr_event_cb_t cb, void *ctx);

/* Builds the address a human should open to reach this device, honouring the
 * user's published-address override and always including the scheme.
 *
 * Lives here rather than in a caller because several subsystems need the
 * same answer — notifications, and the Home Assistant device card — and two
 * copies would inevitably drift.
 *
 * Returns false when there is no usable address yet. */
bool wifi_manager_dashboard_url(char *out, size_t out_len);

/* ── Fixed LAN address ───────────────────────────────────────────────── *
 * active:   the interface is using the address set in Settings.
 * reverted: an address was set, its gateway never answered, and this boot
 *           fell back to DHCP. Both false means plain DHCP, as configured.
 *
 * The UI shows the reverted case, because a device quietly running on a
 * different address than the one in its own settings page is a bad way to
 * spend an afternoon. */
bool wifi_manager_static_active(void);
bool wifi_manager_static_reverted(void);

/* Scans for nearby networks and renders them as JSON for the setup wizard:
 *   {"aps":[{"ssid":"…","rssi":-52,"lock":true},…]}
 * Sorted strongest-first, duplicates removed. Blocks for ~2 s. */
esp_err_t wifi_manager_scan_json(char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
