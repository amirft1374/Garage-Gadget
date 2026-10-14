/*
 * Garage Gadget — ESP32-S3 garage door monitor and controller.
 *
 * Responsibilities of this file:
 *   - bring the subsystems up in a safe order
 *   - own the door state machine: open/close detection, the two alarms,
 *     and the auto-close timer
 *
 * Everything the user can configure lives in NVS (app_settings.h); nothing
 * here is customer-specific, so one firmware image serves every unit.
 *
 * Door safety comes first: the polling loop below starts before Wi-Fi is up
 * and keeps running whether or not the network, InfluxDB or the
 * notification services are reachable.
 */

#include <driver/gpio.h>
#include <driver/temperature_sensor.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <lwip/ip_addr.h>
#include <lwip/netdb.h>
#include <mdns.h>
#include <nvs_flash.h>
#include <ping/ping_sock.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_config.h"
#include "app_settings.h"
#include "door_control.h"
#include "ha_mqtt.h"
#include "history.h"
#include "influx_uploader.h"
#include "magnet.h"
#include "notifier.h"
#include "update_check.h"
#include "web_server.h"
#include "wifi_manager.h"

extern "C" {
#include "led_rgb.h"
}

static const char *TAG = "garage";

static temperature_sensor_handle_t s_temp_sensor = nullptr;
static volatile bool s_network_ready = false;

/* ── Time ────────────────────────────────────────────────────────────── */

static void apply_timezone(void)
{
    const char *tz = settings_get()->timezone;
    setenv("TZ", (tz[0] != '\0') ? tz : APP_DEFAULT_TZ, 1);
    tzset();
}

static void sync_time_with_sntp(void)
{
    ESP_LOGI(TAG, "Starting SNTP time sync");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    /* Poll mode re-syncs on its own from here; we only wait for the first
     * fix so that timestamps written to history are real. */
    for (int i = 0; i < 20; i++) {
        time_t now = 0;
        struct tm tm_utc = {};
        time(&now);
        gmtime_r(&now, &tm_utc);
        if (tm_utc.tm_year >= (2024 - 1900)) {
            apply_timezone();
            char ts[40];
            struct tm tm_local = {};
            localtime_r(&now, &tm_local);
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z", &tm_local);
            ESP_LOGI(TAG, "Clock synced: %s", ts);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "SNTP did not sync within 20s; timestamps stay relative until it does");
}

/* ── Home-server ping monitor (optional) ─────────────────────────────── */

static void on_ping_success(esp_ping_handle_t hdl, void *args) { web_server_set_server_online(true); }
static void on_ping_timeout(esp_ping_handle_t hdl, void *args) { web_server_set_server_online(false); }
static void on_ping_end(esp_ping_handle_t hdl, void *args)     { esp_ping_delete_session(hdl); }

static void server_monitor_task(void *arg)
{
    (void)arg;

    while (true) {
        const char *host = settings_get()->monitor_host;

        if (host[0] == '\0' || !wifi_manager_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(APP_SERVER_PING_INTERVAL_S * 1000));
            continue;
        }

        /* Resolve every cycle so a DHCP change is picked up. */
        struct hostent *he = gethostbyname(host);
        if (he == nullptr || he->h_addr == nullptr) {
            web_server_set_server_online(false);
        } else {
            ip_addr_t target = {};
            memcpy(&target.u_addr.ip4, he->h_addr, sizeof(ip4_addr_t));
            IP_SET_TYPE_VAL(target, IPADDR_TYPE_V4);

            esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
            cfg.target_addr = target;
            cfg.count = 1;
            cfg.timeout_ms = APP_SERVER_PING_TIMEOUT_MS;
            cfg.task_stack_size = 4096;
            cfg.task_prio = 2;

            esp_ping_callbacks_t cbs = {};
            cbs.on_ping_success = on_ping_success;
            cbs.on_ping_timeout = on_ping_timeout;
            cbs.on_ping_end = on_ping_end;

            esp_ping_handle_t hdl;
            if (esp_ping_new_session(&cfg, &cbs, &hdl) == ESP_OK) {
                esp_ping_start(hdl);   /* on_ping_end deletes the session */
            } else {
                web_server_set_server_online(false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_SERVER_PING_INTERVAL_S * 1000));
    }
}

/* ── Deferred network bring-up ───────────────────────────────────────
 * Runs off the main loop so door monitoring never waits on Wi-Fi. */

/* The update check does a TLS handshake, so it gets its own task rather than
 * borrowing one sized for something else. Lowest useful priority: nothing
 * about the door waits on it. */
static void update_check_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_network_ready) {
            update_check_tick();
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static void network_services_task(void *arg)
{
    (void)arg;

    while (!wifi_manager_wait_for_ip(15000)) {
        if (wifi_manager_mode() == WIFI_MGR_SETUP_AP) {
            /* Nothing to wait for: the device has never been configured. */
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        ESP_LOGW(TAG, "Still waiting for an IP address…");
    }

    const app_settings_t *cfg = settings_get();

    ESP_LOGI(TAG, "Network up on %s", wifi_manager_ip());
    web_server_set_network_info(wifi_manager_ip(), cfg->device_name, APP_WEB_SERVER_PORT);

    sync_time_with_sntp();

    /* Now that the clock is real, the boot record is worth writing. */
    history_log(HIST_EV_BOOT, 0);

    influx_uploader_reload();
    ha_mqtt_init();
    update_check_init();
    s_network_ready = true;

    ESP_LOGI(TAG, "Dashboard: http://%s  ·  http://%s.local",
        wifi_manager_ip(), cfg->device_name);

    if (cfg->notify_offline) {
        notifier_sendf(NOTIFY_INFO, "Garage online",
            "%s is up at %s. Door is currently %s.",
            cfg->device_name, wifi_manager_ip(),
            web_server_is_door_closed() ? "closed" : "OPEN");
    }

    vTaskDelete(nullptr);
}

/* Keeps the UI's IP line honest across reconnects. */
static void on_wifi_change(bool connected, const char *ip, void *ctx)
{
    (void)ctx;
    if (connected) {
        web_server_set_network_info(ip, settings_get()->device_name, APP_WEB_SERVER_PORT);
    }
}

/* ── Password recovery button ────────────────────────────────────────── *
 *
 * Holding the button to ground for APP_RECOVERY_HOLD_S arms recovery mode.
 *
 * Checked continuously while running rather than at power-up, because
 * GPIO0 is a strapping pin: held low at reset the chip enters USB download
 * mode and this firmware never runs. Ten seconds of deliberate holding
 * proves the same physical access and cannot happen by accident.
 *
 * Runs from the main loop, so it keeps working even with no network. */

static void recovery_button_init(void)
{
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << APP_RECOVERY_GPIO);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;     /* the button pulls it to ground */
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGW(TAG, "Recovery button unavailable on GPIO%d", APP_RECOVERY_GPIO);
    }
}

/* Returns true once, at the moment the hold completes. */
static bool recovery_button_held(uint32_t now_ms)
{
    static uint32_t pressed_since_ms = 0;
    static bool     already_fired = false;
    static uint32_t last_log_s = 0;

    bool down = (gpio_get_level((gpio_num_t)APP_RECOVERY_GPIO) == 0);

    if (!down) {
        if (pressed_since_ms != 0 && !already_fired) {
            ESP_LOGI(TAG, "Recovery button released early; nothing changed");
        }
        pressed_since_ms = 0;
        already_fired = false;
        last_log_s = 0;
        return false;
    }

    if (pressed_since_ms == 0) {
        pressed_since_ms = now_ms;
        ESP_LOGW(TAG, "Recovery button pressed — hold %d s to reset the admin password",
            APP_RECOVERY_HOLD_S);
        return false;
    }

    if (already_fired) {
        return false;   /* wait for a release before it can fire again */
    }

    uint32_t held_s = (now_ms - pressed_since_ms) / 1000U;

    /* Count down out loud, so someone watching the serial log or the LED can
     * tell the hold is being registered. */
    if (held_s != last_log_s) {
        last_log_s = held_s;
        if (held_s > 0 && held_s < (uint32_t)APP_RECOVERY_HOLD_S) {
            ESP_LOGW(TAG, "Recovery in %" PRIu32 " s… (release to cancel)",
                (uint32_t)APP_RECOVERY_HOLD_S - held_s);
        }
    }

    if (held_s >= (uint32_t)APP_RECOVERY_HOLD_S) {
        already_fired = true;
        return true;
    }
    return false;
}

/* ── Alarm + door state machine ──────────────────────────────────────── */

struct door_state_t {
    bool     last_closed;
    uint32_t open_since_ms;      /* 0 when closed                        */
    bool     immediate_fired;    /* immediate alarm raised this opening   */
    uint32_t left_open_periods;  /* delay periods elapsed while open      */
};

static void handle_door_transition(door_state_t *st, bool closed)
{
    const app_settings_t *cfg = settings_get();

    if (closed) {
        st->open_since_ms = 0;

        history_log(HIST_EV_DOOR_CLOSED, 0);
        influx_uploader_write_event("door_closed", "1");

        /* Clear whatever was raised during this opening. The event names
         * (including the historical "ImmidiateAlarm" spelling) are kept
         * exactly as they were so existing Grafana alerts keep matching. */
        if (cfg->alarm_immediate && st->immediate_fired) {
            influx_uploader_write_event("ImmidiateAlarm", "0");
            history_log(HIST_EV_ALARM_IMMEDIATE, 0);
        }
        if (cfg->alarm_left_open && st->left_open_periods > 0) {
            influx_uploader_write_event("LeftOpenAlarm", "0");
            history_log(HIST_EV_ALARM_LEFT_OPEN, 0);
        }

        bool was_alarming = st->immediate_fired || (st->left_open_periods > 0);
        st->immediate_fired = false;
        st->left_open_periods = 0;

        if (cfg->notify_close || (was_alarming && cfg->notify_alarm)) {
            notifier_send(NOTIFY_INFO, "Garage closed", "The garage door is now closed.");
        }
    } else {
        st->open_since_ms = esp_log_timestamp();
        st->left_open_periods = 0;

        history_log(HIST_EV_DOOR_OPENED, 0);
        influx_uploader_write_event("door_opened", "1");

        if (cfg->notify_open) {
            notifier_send(NOTIFY_INFO, "Garage opened", "The garage door has just opened.");
        }
    }

    ESP_LOGI(TAG, "Garage door is %s", closed ? "CLOSED" : "OPEN");
}

static void evaluate_alarms(door_state_t *st, bool closed, uint32_t now_ms)
{
    const app_settings_t *cfg = settings_get();

    if (closed) {
        return;
    }

    /* Immediate Alarm — fires as soon as the door is seen open while armed.
     * Checked on state rather than on the transition so that arming the
     * alarm with the door already open still raises it. */
    if (cfg->alarm_immediate && !st->immediate_fired) {
        st->immediate_fired = true;

        influx_uploader_write_event("ImmidiateAlarm", "1");
        history_log(HIST_EV_ALARM_IMMEDIATE, 1);
        ESP_LOGW(TAG, "Immediate Alarm: door is open");

        if (cfg->notify_alarm) {
            notifier_send(NOTIFY_ALARM, "Garage door OPEN",
                "The Immediate Alarm is armed and the garage door is open.");
        }
    }

    /* Left Open Alarm — one step per elapsed delay period, so a door left
     * open all afternoon escalates rather than alerting once and going
     * quiet. */
    if (cfg->alarm_left_open && st->open_since_ms > 0) {
        uint32_t elapsed_ms = now_ms - st->open_since_ms;
        uint32_t delay_ms = cfg->auto_close_delay_min * 60U * 1000U;
        uint32_t periods = (delay_ms > 0) ? (elapsed_ms / delay_ms) : 0;

        if (periods > st->left_open_periods) {
            st->left_open_periods = periods;

            char val[12];
            snprintf(val, sizeof(val), "%" PRIu32, periods);
            influx_uploader_write_event("LeftOpenAlarm", val);
            history_log(HIST_EV_ALARM_LEFT_OPEN, (uint8_t)((periods > 255) ? 255 : periods));

            ESP_LOGW(TAG, "Left Open Alarm: open for %" PRIu32 " delay period(s)", periods);

            if (cfg->notify_alarm) {
                notifier_sendf(NOTIFY_ALARM, "Garage left open",
                    "The door has been open for %" PRIu32 " minute(s).",
                    elapsed_ms / 60000U);
            }
        }
    }
}

static void maybe_auto_close(door_state_t *st, bool closed, uint32_t now_ms)
{
    const app_settings_t *cfg = settings_get();

    if (closed || cfg->auto_close_locked_off || !cfg->auto_close_enabled) {
        return;
    }

    /* Give up before announcing anything. The rate limit below keys off the
     * last trigger, and a door that has never moved has no last trigger, so
     * without this the "auto-closing" warning repeats every loop at a gate
     * that is always going to refuse. */
    door_block_t blocked = door_control_blocked();
    if (blocked != DOOR_ALLOWED) {
        static door_block_t s_last_logged = DOOR_ALLOWED;
        if (blocked != s_last_logged) {
            s_last_logged = blocked;
            ESP_LOGW(TAG, "Auto-close wanted to run but the door is held: %s",
                door_control_block_reason(blocked));
        }
        return;
    }
    if (st->open_since_ms == 0) {
        return;
    }

    uint32_t delay_ms = cfg->auto_close_delay_min * 60U * 1000U;
    uint32_t open_ms = now_ms - st->open_since_ms;

    /* Also require the relay to have been idle for a full delay, so a door
     * that is genuinely stuck is not pulsed every loop iteration. */
    if (open_ms < delay_ms || door_control_ms_since_trigger() < delay_ms) {
        return;
    }

    ESP_LOGW(TAG, "Door open for %" PRIu32 " min — auto-closing", open_ms / 60000U);
    door_control_trigger(DOOR_SRC_AUTO);
}

static led_state_t pick_led_state(const door_state_t *st, bool closed)
{
    if (!settings_get()->provisioned || wifi_manager_mode() == WIFI_MGR_SETUP_AP) {
        return LED_STATE_SETUP;
    }
    if (st->immediate_fired || st->left_open_periods > 0) {
        return LED_STATE_ALARM;
    }
    if (!closed) {
        return LED_STATE_DOOR_OPEN;
    }
    if (!wifi_manager_is_connected()) {
        return LED_STATE_OFFLINE;
    }
    return LED_STATE_IDLE;
}

/* ── Entry point ─────────────────────────────────────────────────────── */

extern "C" void app_main(void)
{
    /* The HTTP server logs an ECONNRESET every time a phone locks and RSTs
     * its keep-alive sockets, and an ENOBUFS burst when the browser
     * reconnects them all at once. Both are expected and drown out
     * everything else in the Terminal tab. */
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd", ESP_LOG_ERROR);
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    /* ── Storage first: everything else reads its configuration from it ── */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(settings_init());
    apply_timezone();

    web_server_init_logging();

    const app_settings_t *cfg = settings_get();
    ESP_LOGI(TAG, "%s v%s starting (%s)", APP_PRODUCT_NAME, APP_SW_VERSION,
        cfg->provisioned ? "configured" : "NOT YET CONFIGURED");

    /* ── Hardware ──
     *
     * Only the door sensor and the relay are load-bearing. The status LED is
     * a convenience: if its driver refuses to start — wrong LED model for
     * the board, RMT channel unavailable — the unit must still monitor and
     * control the door. Aborting here would turn a cosmetic fault into a
     * boot loop on a device whose whole job is to be watching the garage. */
    esp_err_t led_err = led_rgb_init();
    if (led_err != ESP_OK) {
        ESP_LOGE(TAG, "Status LED unavailable (%s) — continuing without it",
            esp_err_to_name(led_err));
    } else {
        led_rgb_set_state(LED_STATE_BOOTING);
    }

    ESP_ERROR_CHECK(magnet_init());
    ESP_ERROR_CHECK(door_control_init());
    recovery_button_init();

    temperature_sensor_config_t temp_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&temp_cfg, &s_temp_sensor) == ESP_OK) {
        temperature_sensor_enable(s_temp_sensor);
    } else {
        ESP_LOGW(TAG, "Chip temperature sensor unavailable");
        s_temp_sensor = nullptr;
    }

    /* ── On-device storage and outbound services ── */
    if (history_init() != ESP_OK) {
        ESP_LOGE(TAG, "History storage failed to start; the device still works, "
                      "but the History tab will be empty");
    }
    ESP_ERROR_CHECK(notifier_init());
    ESP_ERROR_CHECK(influx_uploader_init());

    /* ── Network ── */
    ESP_ERROR_CHECK(wifi_manager_init());
    wifi_manager_set_callback(on_wifi_change, nullptr);

    if (settings_recovery_pending()) {
        /* Recovery always raises the hotspot, immediately and regardless of
         * whether the stored network is reachable. Someone is standing at
         * the device having just held the button; making them wait out the
         * 45 s rescue timeout — or discover that the network they are
         * recovering from no longer exists — would be the wrong answer.
         * The station is still attempted, so a working network keeps the
         * device reachable on the LAN as well. */
        ESP_ERROR_CHECK(wifi_manager_start_setup_ap());
        if (settings_have_wifi()) {
            ESP_ERROR_CHECK(wifi_manager_start_station());
        }
        ESP_LOGW(TAG, "RECOVERY: join Wi-Fi '%s' (password '%s') and open http://%s",
            wifi_manager_ap_ssid(), APP_SETUP_AP_PASSWORD, wifi_manager_ap_ip());
    } else if (cfg->provisioned && settings_have_wifi()) {
        ESP_ERROR_CHECK(wifi_manager_start_station());
    } else {
        ESP_ERROR_CHECK(wifi_manager_start_setup_ap());
        ESP_LOGW(TAG, "First-time setup: join Wi-Fi '%s' (password '%s') and open http://%s",
            wifi_manager_ap_ssid(), APP_SETUP_AP_PASSWORD, wifi_manager_ap_ip());
    }

    if (mdns_init() == ESP_OK) {
        mdns_hostname_set(cfg->device_name);
        mdns_instance_name_set(APP_PRODUCT_NAME);
        mdns_service_add(nullptr, "_http", "_tcp", APP_WEB_SERVER_PORT, nullptr, 0);
    } else {
        ESP_LOGW(TAG, "mDNS unavailable; reach the device by IP address");
    }

    /* The UI must be reachable before the network settles — that is exactly
     * how a misconfigured unit gets fixed. */
    ESP_ERROR_CHECK(web_server_start(APP_WEB_SERVER_PORT));
    web_server_set_network_info(wifi_manager_ip(), cfg->device_name, APP_WEB_SERVER_PORT);

    xTaskCreate(network_services_task, "net-svc", 5120, nullptr, 4, nullptr);
    xTaskCreate(update_check_task, "upd-check", 6144, nullptr, 2, nullptr);
    xTaskCreate(server_monitor_task, "srv-monitor", 4096, nullptr, 3, nullptr);

    /* ── Door state machine ── */
    door_state_t st = {};
    st.last_closed = magnet_is_closed();
    st.open_since_ms = st.last_closed ? 0 : esp_log_timestamp();

    web_server_set_door_closed(st.last_closed);
    ESP_LOGI(TAG, "Garage door is %s at startup", st.last_closed ? "CLOSED" : "OPEN");

    uint32_t last_telemetry_ms = 0;
    uint32_t last_sample_ms = 0;

    while (true) {
        bool closed = magnet_is_closed();
        uint32_t now_ms = esp_log_timestamp();

        if (recovery_button_held(now_ms)) {
            settings_begin_recovery();
            /* Acknowledge unmistakably: three long white flashes. */
            for (int i = 0; i < 3; i++) {
                led_rgb_flash(120, 120, 120, 250);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            ESP_LOGW(TAG, "Rebooting into recovery mode");
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_restart();
        }

        web_server_set_door_closed(closed);
        web_server_set_door_open_seconds(
            (!closed && st.open_since_ms > 0) ? ((now_ms - st.open_since_ms) / 1000U) : 0U);

        if (closed != st.last_closed) {
            st.last_closed = closed;
            handle_door_transition(&st, closed);
        }

        evaluate_alarms(&st, closed, now_ms);
        web_server_set_alarm_state(st.immediate_fired, st.left_open_periods);
        ha_mqtt_publish_door(closed);
        ha_mqtt_publish_alarm(st.immediate_fired, st.left_open_periods);
        maybe_auto_close(&st, closed, now_ms);

        /* Per-second: die temperature and the InfluxDB telemetry pair. */
        if ((now_ms - last_telemetry_ms) >= APP_INFLUX_TELEMETRY_MS) {
            last_telemetry_ms = now_ms;

            if (s_temp_sensor != nullptr) {
                float temp_c = 0.0f;
                if (temperature_sensor_get_celsius(s_temp_sensor, &temp_c) == ESP_OK) {
                    web_server_set_chip_temp(temp_c);
                }
            }

            if (wifi_manager_is_connected()) {
                influx_uploader_write_wifi_rssi((int32_t)wifi_manager_rssi());
            }
            influx_uploader_write_garage_door_position(closed ? 1 : 0);

            /* Diagnostics for the shipped Grafana health row. Coalesced
             * here, uploaded on the uploader's own slower cadence. */
            float health_temp = 0.0f;
            if (s_temp_sensor != nullptr) {
                temperature_sensor_get_celsius(s_temp_sensor, &health_temp);
            }
            influx_uploader_write_device_health(
                health_temp,
                (uint32_t)(esp_timer_get_time() / 1000000LL),
                (uint32_t)esp_get_free_heap_size());

            ha_mqtt_publish_telemetry(magnet_last_mv(), wifi_manager_rssi(), health_temp);
        }

        /* Per-minute: one point into the on-device sample trace. */
        if ((now_ms - last_sample_ms) >= (APP_SAMPLE_INTERVAL_S * 1000U)) {
            last_sample_ms = now_ms;

            float temp_c = 0.0f;
            if (s_temp_sensor != nullptr) {
                temperature_sensor_get_celsius(s_temp_sensor, &temp_c);
            }
            history_sample(closed, wifi_manager_rssi(), temp_c);
        }

        led_rgb_set_state(pick_led_state(&st, closed));
        led_rgb_tick();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
