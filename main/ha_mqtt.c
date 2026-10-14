#include "ha_mqtt.h"
#include "app_config.h"
#include "app_settings.h"
#include "door_control.h"
#include "history.h"
#include "magnet.h"
#include "web_server.h"
#include "wifi_manager.h"

#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <mqtt_client.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ha-mqtt";

/* Topic root. Everything this device publishes lives under here, so a user
 * can watch the whole integration with one `mosquitto_sub -t 'garagegadget/#'`. */
#define T_ROOT "garagegadget"

/* HA's discovery prefix. Configurable in HA but almost never changed. */
#define HA_DISC "homeassistant"

#define TELEMETRY_MIN_INTERVAL_MS (15000)

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;
static bool s_enabled = false;
static char s_status[96] = "disabled";
static char s_id[24] = "";              /* unique per device, from the MAC */
static char s_avail_topic[64] = "";

/* Last published values, so a 10 Hz caller does not become a 10 Hz
 * publisher. The broker does not need to hear that nothing changed. */
static int  s_last_door = -1;
static int64_t s_last_telemetry_us = 0;

/* Alarm state is evaluated every main-loop pass, so publishing it
 * unconditionally meant ~20 messages a second forever — a real cost on a
 * broker and a Home Assistant sharing a small Pi, for information that
 * changes a few times a day. Publish on change only, with a slow heartbeat
 * so a subscriber that missed a retained message is not stuck forever. */
static int s_last_imm = -1;
static int s_last_lo  = -1;
static int64_t s_last_alarm_refresh_us = 0;
#define ALARM_REFRESH_INTERVAL_MS (300000)   /* 5 minutes */

static void set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_status, sizeof(s_status), fmt, ap);
    va_end(ap);
}

void ha_mqtt_status(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    strncpy(out, s_status, out_len - 1);
    out[out_len - 1] = '\0';
}

bool ha_mqtt_is_connected(void)
{
    return s_connected;
}

/* ── Topics ──────────────────────────────────────────────────────────── */

static void topic(char *out, size_t len, const char *leaf)
{
    snprintf(out, len, T_ROOT "/%s/%s", s_id, leaf);
}

static void pub(const char *leaf, const char *payload, bool retain)
{
    if (!s_connected || s_client == NULL) {
        return;
    }
    char t[80];
    topic(t, sizeof(t), leaf);
    esp_mqtt_client_publish(s_client, t, payload, 0, 0, retain ? 1 : 0);
}

/* ── Discovery ───────────────────────────────────────────────────────── *
 *
 * One retained config message per entity. Retained so Home Assistant
 * rebuilds the entities after its own restart without needing the device to
 * republish — which it would otherwise only do on reconnect, possibly days
 * later.
 *
 * Every entity shares one `device` block, so HA groups them under a single
 * device card rather than scattering a dozen loose entities. */

static void publish_discovery_entity(const char *component,
                                     const char *object_id,
                                     const char *name,
                                     const char *extra_json)
{
    const app_settings_t *cfg = settings_get();

    char cfg_topic[128];
    snprintf(cfg_topic, sizeof(cfg_topic),
        HA_DISC "/%s/%s_%s/config", component, s_id, object_id);

    char avail[80];
    snprintf(avail, sizeof(avail), T_ROOT "/%s/status", s_id);

    /* configuration_url turns into a "Visit device" link on the Home
     * Assistant device page, so the full web UI — calibration, history,
     * logs — is one tap away in the companion app. */
    char url[64];
    char url_field[96] = "";
    if (wifi_manager_dashboard_url(url, sizeof(url))) {
        snprintf(url_field, sizeof(url_field), ",\"configuration_url\":\"%s\"", url);
    }

    char payload[1000];
    int n = snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s\","
        "\"unique_id\":\"%s_%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_available\":\"online\","
        "\"payload_not_available\":\"offline\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"model\":\"Garage Gadget\","
            "\"sw_version\":\"%s\""
            "%s"
        "},"
        "%s"
        "}",
        name, s_id, object_id, avail,
        s_id, cfg->device_name, APP_PRODUCT_NAME, APP_SW_VERSION,
        url_field,
        extra_json);

    if (n < 0 || (size_t)n >= sizeof(payload)) {
        ESP_LOGW(TAG, "Discovery payload for %s truncated; skipping", object_id);
        return;
    }

    esp_mqtt_client_publish(s_client, cfg_topic, payload, 0, 0, 1 /* retain */);
}

static void publish_all_discovery(void)
{
    char base[64];
    snprintf(base, sizeof(base), T_ROOT "/%s", s_id);

    char x[700];

    /* The door itself, as a cover so HA gives it the garage icon, the right
     * voice-assistant verbs, and open/close buttons.
     *
     * The opener only accepts a single toggle pulse, so OPEN and CLOSE map
     * to the same action. HA sends whichever is the opposite of the reported
     * state, so in practice this behaves correctly; the firmware does not
     * try to guess intent. */
    snprintf(x, sizeof(x),
        "\"device_class\":\"garage\","
        "\"command_topic\":\"%s/cover/set\","
        "\"state_topic\":\"%s/cover/state\","
        "\"payload_open\":\"OPEN\",\"payload_close\":\"CLOSE\",\"payload_stop\":\"STOP\","
        "\"state_open\":\"open\",\"state_closed\":\"closed\","
        "\"optimistic\":false", base, base);
    publish_discovery_entity("cover", "door", "Garage Door", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/sensor/door_mv\","
        "\"unit_of_measurement\":\"mV\",\"device_class\":\"voltage\","
        "\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"", base);
    publish_discovery_entity("sensor", "door_mv", "Door sensor voltage", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/sensor/rssi\","
        "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
        "\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"", base);
    publish_discovery_entity("sensor", "rssi", "Wi-Fi signal", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/sensor/temp\","
        "\"unit_of_measurement\":\"\\u00b0C\",\"device_class\":\"temperature\","
        "\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"", base);
    publish_discovery_entity("sensor", "temp", "Chip temperature", x);

    /* Alarms as binary sensors: HA automations can then trigger on them
     * exactly as they would on a motion or smoke detector. */
    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/binary_sensor/alarm_immediate\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"problem\"", base);
    publish_discovery_entity("binary_sensor", "alarm_immediate", "Immediate Alarm", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/binary_sensor/alarm_left_open\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"problem\"", base);
    publish_discovery_entity("binary_sensor", "alarm_left_open", "Left Open Alarm", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/switch/auto_close\",\"command_topic\":\"%s/switch/auto_close/set\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\"", base, base);
    publish_discovery_entity("switch", "auto_close", "Automatic close", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/switch/alarm_imm_armed\","
        "\"command_topic\":\"%s/switch/alarm_imm_armed/set\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\"", base, base);
    publish_discovery_entity("switch", "alarm_imm_armed", "Immediate Alarm armed", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/switch/alarm_lo_armed\","
        "\"command_topic\":\"%s/switch/alarm_lo_armed/set\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\"", base, base);
    publish_discovery_entity("switch", "alarm_lo_armed", "Left Open Alarm armed", x);

    snprintf(x, sizeof(x),
        "\"state_topic\":\"%s/number/ac_delay\",\"command_topic\":\"%s/number/ac_delay/set\","
        "\"min\":1,\"max\":120,\"step\":1,\"unit_of_measurement\":\"min\","
        "\"mode\":\"box\",\"entity_category\":\"config\"", base, base);
    publish_discovery_entity("number", "ac_delay", "Auto-close delay", x);

    ESP_LOGI(TAG, "Published Home Assistant discovery for 10 entities");
}

/* ── Commands ────────────────────────────────────────────────────────── */

static void subscribe_all(void)
{
    char t[80];
    const char *leaves[] = {
        "cover/set",
        "switch/auto_close/set",
        "switch/alarm_imm_armed/set",
        "switch/alarm_lo_armed/set",
        "number/ac_delay/set",
    };
    for (size_t i = 0; i < sizeof(leaves) / sizeof(leaves[0]); i++) {
        topic(t, sizeof(t), leaves[i]);
        esp_mqtt_client_subscribe(s_client, t, 1);
    }
}

static bool topic_is(const char *full, int full_len, const char *leaf)
{
    char want[80];
    topic(want, sizeof(want), leaf);
    int want_len = (int)strlen(want);
    return full_len == want_len && strncmp(full, want, (size_t)want_len) == 0;
}

static void handle_command(const char *t, int t_len, const char *data, int len)
{
    char val[24] = {};
    int n = (len < (int)sizeof(val) - 1) ? len : (int)sizeof(val) - 1;
    memcpy(val, data, (size_t)n);

    if (topic_is(t, t_len, "cover/set")) {
        bool closed = web_server_is_door_closed();
        /* Refuse a command that asks for the state we are already in. The
         * opener has one button, so acting anyway would move the door the
         * wrong way — exactly the surprise to avoid on a garage door. */
        if ((strcmp(val, "OPEN") == 0 && !closed) ||
            (strcmp(val, "CLOSE") == 0 && closed)) {
            ESP_LOGI(TAG, "Ignoring %s: door is already %s", val, closed ? "closed" : "open");
            return;
        }
        if (strcmp(val, "STOP") == 0) {
            /* A pulse mid-travel is how these openers stop. */
            ESP_LOGI(TAG, "HA requested STOP");
        }
        ESP_LOGI(TAG, "Home Assistant requested %s", val);
        door_control_trigger(DOOR_SRC_OTHER);
        return;
    }

    bool on = (strcmp(val, "ON") == 0);

    if (topic_is(t, t_len, "switch/auto_close/set")) {
        if (!settings_get()->auto_close_locked_off) {
            settings_set_auto_close(on);
            history_log(HIST_EV_AUTO_CLOSE, on ? 1 : 0);
        }
    } else if (topic_is(t, t_len, "switch/alarm_imm_armed/set")) {
        settings_set_alarm_immediate(on);
    } else if (topic_is(t, t_len, "switch/alarm_lo_armed/set")) {
        settings_set_alarm_left_open(on);
    } else if (topic_is(t, t_len, "number/ac_delay/set")) {
        settings_set_auto_close_delay((uint32_t)atoi(val));
    } else {
        return;
    }

    ha_mqtt_publish_config_state();
}

/* ── Event handling ──────────────────────────────────────────────────── */

static void on_mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        set_status("connected");
        ESP_LOGI(TAG, "Connected to broker");

        esp_mqtt_client_publish(s_client, s_avail_topic, "online", 0, 1, 1);
        publish_all_discovery();
        subscribe_all();

        /* Force a full state publish: HA has just (re)created the entities
         * and they are all "unknown" until something arrives. */
        s_last_door = -1;
        s_last_imm = -1;
        s_last_lo = -1;
        s_last_alarm_refresh_us = 0;
        ha_mqtt_publish_door(web_server_is_door_closed());
        ha_mqtt_publish_config_state();
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        set_status("disconnected; retrying");
        ESP_LOGW(TAG, "Disconnected from broker");
        break;

    case MQTT_EVENT_DATA:
        handle_command(e->topic, e->topic_len, e->data, e->data_len);
        break;

    case MQTT_EVENT_ERROR:
        if (e->error_handle != NULL &&
            e->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            set_status("cannot reach broker (%s)",
                esp_err_to_name(e->error_handle->esp_transport_sock_errno));
        } else if (e->error_handle != NULL &&
                   e->error_handle->connect_return_code != MQTT_CONNECTION_ACCEPTED) {
            /* Overwhelmingly this is bad credentials, and saying so saves
             * the user from suspecting the network. */
            set_status("broker refused the connection (code %d) - check username and password",
                e->error_handle->connect_return_code);
        } else {
            set_status("connection error");
        }
        ESP_LOGW(TAG, "%s", s_status);
        break;

    default:
        break;
    }
}

/* ── Lifecycle ───────────────────────────────────────────────────────── */

static void stop_client(void)
{
    if (s_client != NULL) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_connected = false;
}

static esp_err_t start_client(void)
{
    const app_settings_t *cfg = settings_get();

    if (!cfg->mqtt_enabled || cfg->mqtt_host[0] == '\0') {
        set_status("%s", cfg->mqtt_enabled ? "no broker address set" : "disabled");
        s_enabled = false;
        return ESP_OK;
    }

    if (s_id[0] == '\0') {
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(s_id, sizeof(s_id), "garage_%02x%02x%02x", mac[3], mac[4], mac[5]);
    }
    snprintf(s_avail_topic, sizeof(s_avail_topic), T_ROOT "/%s/status", s_id);

    char uri[128];
    snprintf(uri, sizeof(uri), "%s://%s:%u",
        cfg->mqtt_tls ? "mqtts" : "mqtt", cfg->mqtt_host, (unsigned)cfg->mqtt_port);

    esp_mqtt_client_config_t mc = {};
    mc.broker.address.uri = uri;
    mc.credentials.client_id = s_id;
    if (cfg->mqtt_user[0] != '\0') {
        mc.credentials.username = cfg->mqtt_user;
        mc.credentials.authentication.password = cfg->mqtt_pass;
    }
    if (cfg->mqtt_tls) {
        mc.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }

    /* The last will is the whole reason HA can be trusted about this device.
     * Retained, so a subscriber that connects while we are offline still
     * learns we are offline rather than seeing nothing at all. */
    mc.session.last_will.topic = s_avail_topic;
    mc.session.last_will.msg = "offline";
    mc.session.last_will.qos = 1;
    mc.session.last_will.retain = 1;
    mc.session.keepalive = 30;
    mc.network.reconnect_timeout_ms = 10000;

    s_client = esp_mqtt_client_init(&mc);
    if (s_client == NULL) {
        set_status("could not create the client (bad address?)");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   on_mqtt_event, NULL);

    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        set_status("%s", esp_err_to_name(err));
        stop_client();
        return err;
    }

    s_enabled = true;
    set_status("connecting to %s:%u", cfg->mqtt_host, (unsigned)cfg->mqtt_port);
    ESP_LOGI(TAG, "Home Assistant MQTT starting: %s as '%s'", uri, s_id);
    return ESP_OK;
}

esp_err_t ha_mqtt_init(void)
{
    return start_client();
}

void ha_mqtt_reload(void)
{
    stop_client();
    start_client();
}

/* ── Publishing ──────────────────────────────────────────────────────── */

void ha_mqtt_publish_door(bool closed)
{
    if (!s_connected) {
        return;
    }
    if (s_last_door == (closed ? 1 : 0)) {
        return;
    }
    s_last_door = closed ? 1 : 0;
    pub("cover/state", closed ? "closed" : "open", true);
}

void ha_mqtt_publish_telemetry(uint32_t door_mv, int rssi, float temp_c)
{
    if (!s_connected) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (s_last_telemetry_us != 0 &&
        (now - s_last_telemetry_us) < (int64_t)TELEMETRY_MIN_INTERVAL_MS * 1000) {
        return;
    }
    s_last_telemetry_us = now;

    char v[24];
    snprintf(v, sizeof(v), "%u", (unsigned)door_mv);
    pub("sensor/door_mv", v, false);
    snprintf(v, sizeof(v), "%d", rssi);
    pub("sensor/rssi", v, false);
    snprintf(v, sizeof(v), "%.1f", (double)temp_c);
    pub("sensor/temp", v, false);
}

void ha_mqtt_publish_config_state(void)
{
    if (!s_connected) {
        return;
    }
    const app_settings_t *cfg = settings_get();

    pub("switch/auto_close",
        (cfg->auto_close_enabled && !cfg->auto_close_locked_off) ? "ON" : "OFF", true);
    pub("switch/alarm_imm_armed", cfg->alarm_immediate ? "ON" : "OFF", true);
    pub("switch/alarm_lo_armed", cfg->alarm_left_open ? "ON" : "OFF", true);

    char v[16];
    snprintf(v, sizeof(v), "%u", (unsigned)cfg->auto_close_delay_min);
    pub("number/ac_delay", v, true);
}

void ha_mqtt_publish_alarm(bool immediate_active, uint32_t left_open_periods)
{
    if (!s_connected) {
        return;
    }

    int imm = immediate_active ? 1 : 0;
    int lo  = (left_open_periods > 0) ? 1 : 0;

    int64_t now = esp_timer_get_time();
    bool due = (s_last_alarm_refresh_us == 0) ||
               ((now - s_last_alarm_refresh_us) >= (int64_t)ALARM_REFRESH_INTERVAL_MS * 1000);

    if (imm == s_last_imm && lo == s_last_lo && !due) {
        return;
    }

    s_last_imm = imm;
    s_last_lo  = lo;
    s_last_alarm_refresh_us = now;

    pub("binary_sensor/alarm_immediate", imm ? "ON" : "OFF", true);
    pub("binary_sensor/alarm_left_open", lo ? "ON" : "OFF", true);
}
