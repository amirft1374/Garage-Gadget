#include "wifi_manager.h"
#include "app_config.h"
#include "app_settings.h"
#include "captive_dns.h"
#include "history.h"

#include <esp_check.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <ping/ping_sock.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "wifi";

static const int BIT_GOT_IP    = BIT0;
static const int BIT_PING_DONE = BIT1;

static EventGroupHandle_t s_events = nullptr;
static esp_netif_t *s_netif_sta = nullptr;
static esp_netif_t *s_netif_ap  = nullptr;

static wifi_mgr_mode_t s_mode = WIFI_MGR_OFF;
static char s_ip[16]      = "";
static char s_ap_ip[16]   = "";
static char s_ap_ssid[33] = "";
static bool s_driver_started = false;
static bool s_ap_up = false;

static wifi_mgr_event_cb_t s_cb = nullptr;
static void *s_cb_ctx = nullptr;

/* Reconnect backoff, in seconds, doubling from MIN to MAX. */
static uint32_t s_backoff_s = APP_WIFI_RECONNECT_MIN_S;
/* Tick at which a lingering setup/rescue AP should be shut down; 0 = none. */
static TickType_t s_ap_stop_at = 0;
/* Tick at which the current join attempt started, for the rescue watchdog. */
static TickType_t s_join_started = 0;
static bool s_ever_connected = false;
static TaskHandle_t s_watchdog_task = nullptr;

/* Static-address state. s_static_active means we configured the interface
 * by hand; s_static_reverted means we tried and the gateway never answered,
 * so this boot is running on DHCP instead. The UI reports the difference —
 * silently ignoring the address the user typed would be worse than the
 * typo. */
static bool s_static_active = false;
static bool s_static_reverted = false;
static TaskHandle_t s_verify_task = nullptr;

static esp_err_t start_ap_locked(void);
static esp_err_t stop_ap_locked(void);
static void static_ip_verify_task(void *arg);

/* ── Event handling ──────────────────────────────────────────────────── */

static const char *disconnect_reason_to_str(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:            return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_FAIL:              return "AUTH_FAIL (wrong password?)";
    case WIFI_REASON_ASSOC_EXPIRE:           return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_FAIL:             return "ASSOC_FAIL";
    case WIFI_REASON_NO_AP_FOUND:            return "NO_AP_FOUND (wrong SSID or out of range?)";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT (wrong password?)";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:      return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:        return "CONNECTION_FAIL";
    case WIFI_REASON_BEACON_TIMEOUT:         return "BEACON_TIMEOUT";
    default:                                 return "UNKNOWN";
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *e = (wifi_event_sta_disconnected_t *)data;

        bool was_connected = (xEventGroupGetBits(s_events) & BIT_GOT_IP) != 0;
        xEventGroupClearBits(s_events, BIT_GOT_IP);
        s_ip[0] = '\0';

        if (was_connected) {
            ESP_LOGW(TAG, "Disconnected (reason %u: %s)",
                (unsigned)e->reason, disconnect_reason_to_str(e->reason));
            history_log(HIST_EV_WIFI_LOST, e->reason);
            if (s_cb) {
                s_cb(false, "", s_cb_ctx);
            }
        }

        /* Retry with backoff; the watchdog task performs the actual retry so
         * we never block the event task. */
        return;
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));

        s_backoff_s = APP_WIFI_RECONNECT_MIN_S;
        s_ever_connected = true;
        xEventGroupSetBits(s_events, BIT_GOT_IP);

        ESP_LOGI(TAG, "Station got IP %s", s_ip);
        history_log(HIST_EV_WIFI_UP, 0);

        /* A hand-set address needs proving; a DHCP lease is proof in itself,
         * because something answered to give it to us. */
        if (s_static_active && s_verify_task == nullptr) {
            xTaskCreate(static_ip_verify_task, "ip-verify", 3072, nullptr, 3,
                &s_verify_task);
        }

        /* The setup/rescue AP has done its job, but do not yank it away
         * immediately: whoever just finished the wizard is still connected
         * to it and is about to be told where the device now lives. The
         * watchdog takes it down once the grace period expires. */
        if (s_ap_up && s_ap_stop_at == 0) {
            s_ap_stop_at = xTaskGetTickCount() + pdMS_TO_TICKS(APP_SETUP_AP_GRACE_S * 1000);
            ESP_LOGI(TAG, "Station is up; setup AP '%s' stays available for %ds so you can "
                          "read the new address, then shuts down",
                s_ap_ssid, APP_SETUP_AP_GRACE_S);
        }

        if (s_cb) {
            s_cb(true, s_ip, s_cb_ctx);
        }
        return;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        auto *e = (wifi_event_ap_staconnected_t *)data;
        ESP_LOGI(TAG, "Setup client joined: " MACSTR, MAC2STR(e->mac));
        return;
    }
}

/* ── Rescue / reconnect watchdog ─────────────────────────────────────── */

static void watchdog_task(void *arg)
{
    (void)arg;
    bool logged_offline = false;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (s_mode != WIFI_MGR_STATION && s_mode != WIFI_MGR_RESCUE) {
            continue;
        }

        /* Retire a lingering setup AP once its grace period is up, but only
         * while the station is genuinely connected — and never while setup or
         * recovery is still unfinished, since the hotspot is the only way the
         * person standing at the device can complete it. */
        bool setup_in_progress = settings_recovery_pending() || !settings_get()->provisioned;

        if (s_ap_up && s_ap_stop_at != 0 && wifi_manager_is_connected() &&
            !setup_in_progress &&
            (int32_t)(xTaskGetTickCount() - s_ap_stop_at) >= 0) {
            ESP_LOGI(TAG, "Grace period over; shutting down the setup AP");
            stop_ap_locked();
            s_ap_stop_at = 0;
            s_mode = WIFI_MGR_STATION;
        }

        if (wifi_manager_is_connected()) {
            logged_offline = false;
            continue;
        }

        /* Station went away again: cancel any pending AP shutdown. */
        s_ap_stop_at = 0;

        TickType_t elapsed_ticks = xTaskGetTickCount() - s_join_started;
        uint32_t elapsed_s = (uint32_t)(elapsed_ticks * portTICK_PERIOD_MS / 1000);

        /* No hotspot outside setup and recovery.
         *
         * A configured unit that simply cannot see its network — router
         * rebooting, power cut, out of range — must not start broadcasting an
         * access point. It would sit there indefinitely offering a way in,
         * unattended, for what is usually a temporary fault. The station just
         * keeps retrying on the backoff below.
         *
         * Getting back in after a genuine lock-out is the recovery button's
         * job: holding it raises the hotspot deliberately, with someone
         * standing at the device. */
        if (!s_ap_up && elapsed_s >= APP_WIFI_JOIN_TIMEOUT_S && !logged_offline) {
            logged_offline = true;
            ESP_LOGW(TAG, "Cannot join '%s'. Still retrying; no hotspot will be started. "
                          "Hold the recovery button for %ds if you need to change the network.",
                settings_get()->wifi_ssid, APP_RECOVERY_HOLD_S);
        }

        /* Retry the station join on a doubling backoff. */
        if (elapsed_s >= s_backoff_s) {
            ESP_LOGI(TAG, "Retrying Wi-Fi join (backoff %us)", (unsigned)s_backoff_s);
            esp_wifi_connect();
            s_join_started = xTaskGetTickCount();

            s_backoff_s *= 2;
            if (s_backoff_s > APP_WIFI_RECONNECT_MAX_S) {
                s_backoff_s = APP_WIFI_RECONNECT_MAX_S;
            }
        }
    }
}

/* ── Setup / init ────────────────────────────────────────────────────── */

esp_err_t wifi_manager_init(void)
{
    if (s_events == nullptr) {
        s_events = xEventGroupCreate();
        if (s_events == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (s_netif_sta == nullptr) {
        s_netif_sta = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, nullptr),
        TAG, "wifi event register failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, nullptr),
        TAG, "ip event register failed");

    /* Credentials live in our own NVS namespace; do not let the driver keep
     * a second, stale copy of them. */
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "set storage failed");

    return ESP_OK;
}

/* Builds "GarageGadget-A1B2" from the station MAC. */
static void build_ap_ssid(char *out, size_t out_size)
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_size, "%s-%02X%02X", APP_SETUP_AP_SSID_PREFIX, mac[4], mac[5]);
}

static esp_err_t start_ap_locked(void)
{
    if (s_ap_up) {
        return ESP_OK;
    }

    if (s_netif_ap == nullptr) {
        s_netif_ap = esp_netif_create_default_wifi_ap();
    }

    build_ap_ssid(s_ap_ssid, sizeof(s_ap_ssid));

    wifi_config_t ap_cfg = {};
    strlcpy((char *)ap_cfg.ap.ssid, s_ap_ssid, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    strlcpy((char *)ap_cfg.ap.password, APP_SETUP_AP_PASSWORD, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = (strlen(APP_SETUP_AP_PASSWORD) >= 8)
                       ? WIFI_AUTH_WPA2_PSK
                       : WIFI_AUTH_OPEN;
    ap_cfg.ap.channel = 1;

    /* APSTA, not AP: the setup wizard needs to scan for the user's network,
     * and the rescue portal must keep retrying the station join. */
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set APSTA failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "set AP config failed");

    if (!s_driver_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
        s_driver_started = true;
    }

    esp_netif_ip_info_t ip = {};
    if (esp_netif_get_ip_info(s_netif_ap, &ip) == ESP_OK) {
        snprintf(s_ap_ip, sizeof(s_ap_ip), IPSTR, IP2STR(&ip.ip));
    }

    s_ap_up = true;

    /* Make the phone open the setup page by itself.
     *
     * Android, iOS and Windows each fetch a known URL on joining a network
     * to decide whether internet access is real. Resolving every name to
     * ourselves means those probes land on our web server, which answers
     * with a redirect — and the client responds by showing "Sign in to
     * network" and opening the setup page.
     *
     * DHCP option 114 (RFC 8910) would state the portal address explicitly,
     * but esp_netif does not expose it in ESP-IDF 5.3. The DNS approach
     * works on every client that matters, so it is not worth pinning the
     * project to a newer IDF for. */
    captive_dns_start(s_ap_ip);

    ESP_LOGW(TAG, "Setup AP up: SSID '%s', password '%s', http://%s",
        s_ap_ssid, APP_SETUP_AP_PASSWORD, s_ap_ip);

    return ESP_OK;
}

static esp_err_t stop_ap_locked(void)
{
    if (!s_ap_up) {
        return ESP_OK;
    }

    /* Stop hijacking DNS the instant the portal is gone. Resolving every
     * name to ourselves is right for a provisioning portal and actively
     * harmful anywhere else. */
    captive_dns_stop();

    s_ap_up = false;
    s_ap_ip[0] = '\0';
    s_ap_ssid[0] = '\0';
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

esp_err_t wifi_manager_start_setup_ap(void)
{
    ESP_RETURN_ON_ERROR(start_ap_locked(), TAG, "setup AP failed");
    s_mode = WIFI_MGR_SETUP_AP;
    return ESP_OK;
}

static esp_err_t apply_station_config(void)
{
    const app_settings_t *cfg = settings_get();

    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid,     cfg->wifi_ssid,     sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, cfg->wifi_password, sizeof(sta.sta.password));
    /* Accept whatever the router offers, including open networks. */
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;

    return esp_wifi_set_config(WIFI_IF_STA, &sta);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Fixed LAN address
 *
 * Optional, and off unless the user fills it in. A DHCP reservation on the
 * router is still the better tool for the job, but not every user can
 * configure their router, and some need the device to hold an address a
 * port-forward or a Home Assistant entry already points at.
 *
 * The risk is obvious: a wrong address, or the right address on the wrong
 * subnet, associates with the Wi-Fi perfectly and is then unreachable. The
 * normal join watchdog does not catch that, because nothing failed to join.
 * So after the interface comes up we ping the gateway, and if it never
 * answers we drop the static address and take a DHCP lease for this boot.
 * The setting is left alone — it is still shown, still wrong, and now
 * fixable from a device the user can actually reach.
 * ═══════════════════════════════════════════════════════════════════════ */

static volatile uint32_t s_gw_replies = 0;

static void on_gw_ping_reply(esp_ping_handle_t hdl, void *args)
{
    (void)hdl; (void)args;
    /* Spelled out rather than ++: a read-modify-write on a volatile is
     * deprecated in C++20 because it looks atomic and is not. Only the ping
     * callback writes this, so a plain store is correct and honest. */
    s_gw_replies = s_gw_replies + 1;
}

static void on_gw_ping_end(esp_ping_handle_t hdl, void *args)
{
    (void)args;
    esp_ping_delete_session(hdl);
    xEventGroupSetBits(s_events, BIT_PING_DONE);
}

static void revert_to_dhcp(const char *why)
{
    ESP_LOGE(TAG, "Fixed address unusable (%s) — taking a DHCP lease instead. "
                  "The address in Settings is unchanged; correct it and reboot.", why);

    s_static_active = false;
    s_static_reverted = true;

    esp_netif_dhcpc_start(s_netif_sta);

    /* Re-associate so the lease is negotiated from a clean start rather than
     * on top of an interface that still believes it has an address. */
    s_ip[0] = '\0';
    xEventGroupClearBits(s_events, BIT_GOT_IP);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

static void static_ip_verify_task(void *arg)
{
    (void)arg;

    /* Let ARP and the AP's bridge settle; pinging the instant the link comes
     * up produces a false negative often enough to matter. */
    vTaskDelay(pdMS_TO_TICKS(2500));

    const app_settings_t *cfg = settings_get();
    ip_addr_t target = {};

    if (s_static_active && ipaddr_aton(cfg->static_gw, &target)) {
        esp_ping_config_t pc = ESP_PING_DEFAULT_CONFIG();
        pc.target_addr = target;
        pc.count       = 4;
        pc.interval_ms = 500;
        pc.timeout_ms  = 1000;

        esp_ping_callbacks_t cbs = {};
        cbs.on_ping_success = on_gw_ping_reply;
        cbs.on_ping_end     = on_gw_ping_end;

        s_gw_replies = 0;
        xEventGroupClearBits(s_events, BIT_PING_DONE);

        esp_ping_handle_t hdl = nullptr;
        if (esp_ping_new_session(&pc, &cbs, &hdl) == ESP_OK) {
            esp_ping_start(hdl);
            /* on_gw_ping_end deletes the session and sets the bit. The
             * timeout is only a backstop against a session that never ends. */
            xEventGroupWaitBits(s_events, BIT_PING_DONE, pdTRUE, pdTRUE,
                pdMS_TO_TICKS(10000));

            if (s_gw_replies == 0) {
                revert_to_dhcp("gateway did not answer");
            } else {
                ESP_LOGI(TAG, "Fixed address %s confirmed: gateway %s answered "
                              "%u of 4 pings",
                    cfg->static_ip, cfg->static_gw, (unsigned)s_gw_replies);
            }
        } else {
            /* Cannot test it, so do not claim it works — but do not throw a
             * working configuration away over a failed self-check either. */
            ESP_LOGW(TAG, "Could not ping-test the gateway; keeping %s",
                cfg->static_ip);
        }
    }

    s_verify_task = nullptr;
    vTaskDelete(nullptr);
}

/* Configures the station interface before it associates: by hand if the user
 * has set an address, by DHCP otherwise. Called on every join, so clearing
 * the fields and rebooting goes back to DHCP. */
static esp_err_t apply_address_config(void)
{
    const app_settings_t *cfg = settings_get();

    s_static_active = false;
    s_static_reverted = false;

    /* A gateway is not optional. Without one the device reaches its own
     * subnet and nothing else: no clock, no Telegram, no broker. Treating a
     * missing gateway as "use DHCP" is the kinder reading of an incomplete
     * form than half-applying it. */
    if (cfg->static_ip[0] == '\0' || cfg->static_gw[0] == '\0') {
        if (cfg->static_ip[0] != '\0') {
            ESP_LOGW(TAG, "Fixed address %s ignored: no gateway set", cfg->static_ip);
        }
        esp_netif_dhcpc_start(s_netif_sta);     /* no-op if already running */
        return ESP_OK;
    }

    esp_netif_ip_info_t ip = {};
    ip.ip.addr = ipaddr_addr(cfg->static_ip);
    ip.gw.addr = ipaddr_addr(cfg->static_gw);
    ip.netmask.addr = (cfg->static_mask[0] != '\0')
        ? ipaddr_addr(cfg->static_mask)
        : ipaddr_addr("255.255.255.0");

    if (ip.ip.addr == IPADDR_NONE || ip.gw.addr == IPADDR_NONE ||
        ip.netmask.addr == IPADDR_NONE) {
        ESP_LOGE(TAG, "Fixed address fields do not parse; using DHCP");
        esp_netif_dhcpc_start(s_netif_sta);
        return ESP_OK;
    }

    /* The DHCP client has to be out of the way before the address can be set
     * by hand, or it will overwrite it the moment a lease arrives. */
    esp_err_t err = esp_netif_dhcpc_stop(s_netif_sta);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "Could not stop the DHCP client: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_set_ip_info(s_netif_sta, &ip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not set %s: %s — using DHCP",
            cfg->static_ip, esp_err_to_name(err));
        esp_netif_dhcpc_start(s_netif_sta);
        return ESP_OK;
    }

    /* With DHCP off nothing supplies a resolver, and every outbound feature
     * here is addressed by name. Blank means "the gateway", which is right
     * on essentially every home network. */
    esp_netif_dns_info_t dns = {};
    const char *resolver = (cfg->static_dns[0] != '\0') ? cfg->static_dns : cfg->static_gw;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(resolver);
    esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_MAIN, &dns);

    s_static_active = true;
    ESP_LOGI(TAG, "Fixed address %s/%s via %s, DNS %s",
        cfg->static_ip, cfg->static_mask[0] ? cfg->static_mask : "255.255.255.0",
        cfg->static_gw, resolver);
    return ESP_OK;
}

bool wifi_manager_static_active(void)   { return s_static_active; }
bool wifi_manager_static_reverted(void) { return s_static_reverted; }

esp_err_t wifi_manager_start_station(void)
{
    if (!settings_have_wifi()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ap_up) {
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA failed");
    }

    if (!s_driver_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
        s_driver_started = true;
    }

    ESP_RETURN_ON_ERROR(apply_station_config(), TAG, "set STA config failed");

    /* Before associating, not after: a DHCP lease that arrives first would
     * have to be torn back down again. */
    apply_address_config();

    s_mode = s_ap_up ? WIFI_MGR_RESCUE : WIFI_MGR_STATION;
    s_backoff_s = APP_WIFI_RECONNECT_MIN_S;
    s_join_started = xTaskGetTickCount();

    ESP_LOGI(TAG, "Joining '%s'", settings_get()->wifi_ssid);
    esp_wifi_connect();

    if (s_watchdog_task == nullptr) {
        xTaskCreate(watchdog_task, "wifi-wd", 3072, nullptr, 3, &s_watchdog_task);
    }

    return ESP_OK;
}

esp_err_t wifi_manager_apply_new_credentials(void)
{
    if (!settings_have_wifi()) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Applying new credentials for '%s'", settings_get()->wifi_ssid);

    xEventGroupClearBits(s_events, BIT_GOT_IP);
    s_ip[0] = '\0';

    esp_wifi_disconnect();
    ESP_RETURN_ON_ERROR(apply_station_config(), TAG, "set STA config failed");
    apply_address_config();

    s_backoff_s = APP_WIFI_RECONNECT_MIN_S;
    s_join_started = xTaskGetTickCount();

    if (s_mode == WIFI_MGR_SETUP_AP) {
        /* Leaving provisioning: keep our AP up until the station is proven
         * to work, then the IP event tears it down. */
        s_mode = WIFI_MGR_RESCUE;
        if (s_watchdog_task == nullptr) {
            xTaskCreate(watchdog_task, "wifi-wd", 3072, nullptr, 3, &s_watchdog_task);
        }
    }

    return esp_wifi_connect();
}

/* ── Accessors ───────────────────────────────────────────────────────── */

bool wifi_manager_wait_for_ip(uint32_t timeout_ms)
{
    if (s_events == nullptr) {
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(
        s_events, BIT_GOT_IP, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & BIT_GOT_IP) != 0;
}

bool wifi_manager_is_connected(void)
{
    return s_events != nullptr && (xEventGroupGetBits(s_events) & BIT_GOT_IP) != 0;
}

wifi_mgr_mode_t wifi_manager_mode(void) { return s_mode; }
const char *wifi_manager_ip(void)       { return s_ip; }
const char *wifi_manager_ap_ip(void)    { return s_ap_ip; }
const char *wifi_manager_ap_ssid(void)  { return s_ap_ssid; }

int wifi_manager_rssi(void)
{
    if (!wifi_manager_is_connected()) {
        return 0;
    }
    int rssi = 0;
    return (esp_wifi_sta_get_rssi(&rssi) == ESP_OK) ? rssi : 0;
}

bool wifi_manager_dashboard_url(char *out, size_t out_len)
{
    const app_settings_t *cfg = settings_get();

    /* An explicitly published address wins: the DHCP lease is only the right
     * answer for a plain LAN install, and it changes. */
    if (cfg->public_url[0] != '\0') {
        if (strncmp(cfg->public_url, "http://", 7) == 0 ||
            strncmp(cfg->public_url, "https://", 8) == 0) {
            snprintf(out, out_len, "%s", cfg->public_url);
        } else {
            /* Never publish a bare host: a browser in HTTPS-only mode
             * rewrites it to https:// and then cannot connect. */
            snprintf(out, out_len, "http://%s", cfg->public_url);
        }
        return true;
    }

    if (s_ip[0] == '\0') {
        return false;
    }
    if (APP_WEB_SERVER_PORT == 80) {
        snprintf(out, out_len, "http://%s", s_ip);
    } else {
        snprintf(out, out_len, "http://%s:%d", s_ip, APP_WEB_SERVER_PORT);
    }
    return true;
}

void wifi_manager_set_callback(wifi_mgr_event_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
}

/* ── Scan ────────────────────────────────────────────────────────────── */

esp_err_t wifi_manager_scan_json(char *out, size_t out_size)
{
    if (out == nullptr || out_size < 16) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_scan_config_t scan_cfg = {};
    scan_cfg.show_hidden = false;

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true /* block */);
    if (err != ESP_OK) {
        snprintf(out, out_size, "{\"aps\":[],\"error\":\"%s\"}", esp_err_to_name(err));
        return err;
    }

    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    if (found > 24) {
        found = 24;
    }

    wifi_ap_record_t *records = (wifi_ap_record_t *)calloc(found, sizeof(wifi_ap_record_t));
    if (records == nullptr) {
        esp_wifi_clear_ap_list();
        snprintf(out, out_size, "{\"aps\":[]}");
        return ESP_ERR_NO_MEM;
    }

    esp_wifi_scan_get_ap_records(&found, records);

    int pos = snprintf(out, out_size, "{\"aps\":[");
    int emitted = 0;

    for (uint16_t i = 0; i < found; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (ssid[0] == '\0') {
            continue;
        }

        /* esp_wifi returns records strongest-first, so the first sighting of
         * an SSID is the best one; drop later duplicates from mesh/repeaters. */
        bool dup = false;
        for (uint16_t j = 0; j < i; j++) {
            if (strcmp(ssid, (const char *)records[j].ssid) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }

        /* SSIDs are user-controlled: escape quotes and backslashes so a
         * hostile network name cannot break out of the JSON string. */
        char safe[68];
        size_t sp = 0;
        for (const char *p = ssid; *p && sp + 2 < sizeof(safe); p++) {
            if (*p == '"' || *p == '\\') {
                safe[sp++] = '\\';
            }
            if ((unsigned char)*p < 0x20) {
                continue;
            }
            safe[sp++] = *p;
        }
        safe[sp] = '\0';

        int n = snprintf(out + pos, out_size - (size_t)pos,
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"lock\":%s}",
            (emitted == 0) ? "" : ",",
            safe,
            records[i].rssi,
            (records[i].authmode == WIFI_AUTH_OPEN) ? "false" : "true");

        if (n < 0 || (size_t)n >= out_size - (size_t)pos) {
            break;
        }
        pos += n;
        emitted++;
    }

    free(records);
    esp_wifi_clear_ap_list();

    snprintf(out + pos, out_size - (size_t)pos, "],\"n\":%d}", emitted);
    return ESP_OK;
}
