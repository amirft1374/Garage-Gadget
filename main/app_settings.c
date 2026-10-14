#include "app_settings.h"
#include "app_config.h"

#include <cJSON.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "settings";

#define NVS_NAMESPACE "gg_cfg"

static app_settings_t s_cfg;
static bool s_loaded = false;

/* ── Field table ─────────────────────────────────────────────────────────
 * The `name` doubles as the JSON key and the NVS key, so every name must be
 * 15 characters or fewer (NVS_KEY_NAME_MAX_SIZE is 16 including the NUL).
 * Adding a field here is all that is needed for it to be loaded, saved,
 * emitted to the UI and accepted back from it.                            */
#define F(field) offsetof(app_settings_t, field), sizeof(((app_settings_t *)0)->field)

static const setting_desc_t k_fields[] = {
    /* name             kind          offset/size               secret  sanitize      min  max */

    /* An SSID may legitimately contain spaces, so only the ends are trimmed.
     * The Wi-Fi password is never touched: a password really can contain a
     * trailing space, and silently altering one would lock the user out of
     * their own network. */
    { "wifi_ssid",      SETTING_STR,  F(wifi_ssid),             false,  SAN_TRIM,     0,   0   },
    { "wifi_pass",      SETTING_STR,  F(wifi_password),         true,   SAN_NONE,     0,   0   },
    { "device_name",    SETTING_STR,  F(device_name),           false,  SAN_STRIP_WS, 0,   0   },
    { "timezone",       SETTING_STR,  F(timezone),              false,  SAN_STRIP_WS, 0,   0   },
    { "admin_pass",     SETTING_STR,  F(admin_password),        true,   SAN_NONE,     0,   0   },

    { "influx_on",      SETTING_BOOL, F(influx_enabled),        false,  SAN_NONE,     0,   0   },
    { "influx_url",     SETTING_STR,  F(influx_url),            false,  SAN_STRIP_WS, 0,   0   },
    { "influx_token",   SETTING_STR,  F(influx_token),          true,   SAN_STRIP_WS, 0,   0   },
    /* Org and bucket names are allowed to contain spaces in InfluxDB. */
    { "influx_org",     SETTING_STR,  F(influx_org),            false,  SAN_TRIM,     0,   0   },
    { "influx_bucket",  SETTING_STR,  F(influx_bucket),         false,  SAN_TRIM,     0,   0   },

    { "grafana_url",    SETTING_STR,  F(grafana_url),           false,  SAN_STRIP_WS, 0,   0   },

    { "tg_on",          SETTING_BOOL, F(tg_enabled),            false,  SAN_NONE,     0,   0   },
    { "tg_token",       SETTING_STR,  F(tg_token),              true,   SAN_STRIP_WS, 0,   0   },
    { "tg_chat",        SETTING_STR,  F(tg_chat),               false,  SAN_STRIP_WS, 0,   0   },
    /* Packed recipient list; the UI edits it as a list and writes it back
     * through the normal settings save. Whitespace around the separators is
     * meaningless, so strip it all. */
    { "tg_chats",       SETTING_STR,  F(tg_chats),              false,  SAN_TRIM,     0,   0   },

    { "ntfy_on",        SETTING_BOOL, F(ntfy_enabled),          false,  SAN_NONE,     0,   0   },
    { "ntfy_server",    SETTING_STR,  F(ntfy_server),           false,  SAN_STRIP_WS, 0,   0   },
    { "ntfy_topic",     SETTING_STR,  F(ntfy_topic),            false,  SAN_STRIP_WS, 0,   0   },

    { "notify_open",    SETTING_BOOL, F(notify_open),           false,  SAN_NONE,     0,   0   },
    { "notify_close",   SETTING_BOOL, F(notify_close),          false,  SAN_NONE,     0,   0   },
    { "notify_alarm",   SETTING_BOOL, F(notify_alarm),          false,  SAN_NONE,     0,   0   },
    { "notify_offline", SETTING_BOOL, F(notify_offline),        false,  SAN_NONE,     0,   0   },

    { "relay_ms",       SETTING_U32,  F(relay_pulse_ms),        false,  SAN_NONE,
      APP_RELAY_PULSE_MS_MIN, APP_RELAY_PULSE_MS_MAX },
    { "ac_on",          SETTING_BOOL, F(auto_close_enabled),    false,  SAN_NONE,     0,   0   },
    { "ac_locked",      SETTING_BOOL, F(auto_close_locked_off), false,  SAN_NONE,     0,   0   },
    { "ac_delay",       SETTING_U32,  F(auto_close_delay_min),  false,  SAN_NONE,     1,   120 },
    { "alarm_imm",      SETTING_BOOL, F(alarm_immediate),       false,  SAN_NONE,     0,   0   },
    { "alarm_lo",       SETTING_BOOL, F(alarm_left_open),       false,  SAN_NONE,     0,   0   },

    { "ctrl_on",        SETTING_BOOL, F(control_enabled),       false,  SAN_NONE,     0,   0   },
    { "upd_check",      SETTING_BOOL, F(update_check_enabled),  false,  SAN_NONE,     0,   0   },
    { "web_url",        SETTING_STR,  F(public_url),            false,  SAN_STRIP_WS, 0,   0   },

    { "static_ip",      SETTING_STR,  F(static_ip),             false,  SAN_STRIP_WS, 0,   0   },
    { "static_gw",      SETTING_STR,  F(static_gw),             false,  SAN_STRIP_WS, 0,   0   },
    { "static_mask",    SETTING_STR,  F(static_mask),           false,  SAN_STRIP_WS, 0,   0   },
    { "static_dns",     SETTING_STR,  F(static_dns),            false,  SAN_STRIP_WS, 0,   0   },
    /* trusted_ips is deliberately absent: it lives in RAM only, so the list
     * is empty again after any power cycle or reboot. Being outside this
     * table is what makes that true — there is no code path that could
     * accidentally persist it. */
    { "mqtt_on",        SETTING_BOOL, F(mqtt_enabled),          false,  SAN_NONE,     0,   0   },
    { "mqtt_host",      SETTING_STR,  F(mqtt_host),             false,  SAN_STRIP_WS, 0,   0   },
    { "mqtt_port",      SETTING_U32,  F(mqtt_port),             false,  SAN_NONE,     1,   65535 },
    { "mqtt_user",      SETTING_STR,  F(mqtt_user),             false,  SAN_TRIM,     0,   0   },
    { "mqtt_pass",      SETTING_STR,  F(mqtt_pass),             true,   SAN_NONE,     0,   0   },
    { "mqtt_tls",       SETTING_BOOL, F(mqtt_tls),              false,  SAN_NONE,     0,   0   },
    { "monitor_host",   SETTING_STR,  F(monitor_host),          false,  SAN_STRIP_WS, 0,   0   },
    { "provisioned",    SETTING_BOOL, F(provisioned),           false,  SAN_NONE,     0,   0   },
    { "recovery",       SETTING_BOOL, F(recovery_pending),      false,  SAN_NONE,     0,   0   },
    { "door_mv",        SETTING_U32,  F(door_threshold_mv),     false,  SAN_NONE,     100, 3100 },
    /* Calibration points are captured by /api/calibrate, which measures them
     * rather than trusting a typed-in number, so they are not editable
     * through the generic settings form. */
    { "door_cl_mv",     SETTING_U32,  F(door_closed_mv),        false,  SAN_NONE,     0,   3300 },
    { "door_op_mv",     SETTING_U32,  F(door_open_mv),          false,  SAN_NONE,     0,   3300 },
    { "door_cal",       SETTING_BOOL, F(door_cal_valid),        false,  SAN_NONE,     0,   0   },
};

#undef F

#define FIELD_COUNT (sizeof(k_fields) / sizeof(k_fields[0]))

/* Fields the generic JSON writer must never accept from a client — they have
 * dedicated, validated endpoints instead. */
static bool field_is_write_protected(const char *name)
{
    return (strcmp(name, "admin_pass") == 0)
        /* Arming the relay is its own deliberate action with its own
         * endpoint, never a field that rides along with a settings save. */
        || (strcmp(name, "ctrl_on") == 0)
        || (strcmp(name, "provisioned") == 0)
        /* Calibration is measured, never typed. */
        || (strcmp(name, "door_cl_mv") == 0)
        || (strcmp(name, "door_op_mv") == 0)
        || (strcmp(name, "door_cal") == 0)
        || (strcmp(name, "recovery") == 0);
}

static void *field_ptr(const setting_desc_t *d)
{
    return (uint8_t *)&s_cfg + d->offset;
}

/* Strict dotted quad: four decimal octets, nothing else. Deliberately not
 * inet_aton, which accepts "10", "1.2" and hex forms — generous parsing is
 * fine when reading a config file and wrong when the value decides whether
 * the device can be reached at all. */
static bool is_dotted_quad(const char *s)
{
    for (int octet = 0; octet < 4; octet++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s++ - '0');
            if (++digits > 3 || v > 255) {
                return false;
            }
        }
        if (octet < 3) {
            if (*s++ != '.') {
                return false;
            }
        }
    }
    return *s == '\0';
}

/* Guards the fields where a malformed value would cost the user their
 * connection to the device. Blank always passes: for the static-address
 * fields it is what selects DHCP. */
static bool field_rejects_value(const char *name, const char *value)
{
    bool is_addr = (strcmp(name, "static_ip") == 0)
                || (strcmp(name, "static_gw") == 0)
                || (strcmp(name, "static_mask") == 0)
                || (strcmp(name, "static_dns") == 0);

    if (!is_addr || value[0] == '\0') {
        return false;
    }
    return !is_dotted_quad(value);
}

static bool is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Cleans a value in place according to the field's policy. See sanitize_t. */
static void sanitize_str(char *s, sanitize_t how)
{
    if (how == SAN_NONE || s == NULL) {
        return;
    }

    if (how == SAN_STRIP_WS) {
        char *w = s;
        for (const char *r = s; *r != '\0'; r++) {
            if (!is_ws(*r)) {
                *w++ = *r;
            }
        }
        *w = '\0';
        return;
    }

    /* SAN_TRIM */
    char *start = s;
    while (*start != '\0' && is_ws(*start)) {
        start++;
    }
    size_t len = strlen(start);
    while (len > 0 && is_ws(start[len - 1])) {
        len--;
    }
    if (start != s) {
        memmove(s, start, len);
    }
    s[len] = '\0';
}

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void load_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));

    copy_str(s_cfg.device_name, sizeof(s_cfg.device_name), APP_DEFAULT_DEVICE_NAME);
    copy_str(s_cfg.timezone, sizeof(s_cfg.timezone), APP_DEFAULT_TZ);
    copy_str(s_cfg.admin_password, sizeof(s_cfg.admin_password), APP_DEFAULT_ADMIN_PASSWORD);
    copy_str(s_cfg.ntfy_server, sizeof(s_cfg.ntfy_server), "https://ntfy.sh");
    copy_str(s_cfg.monitor_host, sizeof(s_cfg.monitor_host), APP_DEFAULT_MONITOR_HOST);

    s_cfg.mqtt_port            = 1883;
    s_cfg.relay_pulse_ms       = APP_RELAY_PULSE_MS_DEFAULT;
    s_cfg.door_threshold_mv    = APP_DOOR_THRESHOLD_MV_DEFAULT;
    s_cfg.door_closed_mv       = 0;
    s_cfg.door_open_mv         = 0;
    s_cfg.door_cal_valid       = false;
    s_cfg.auto_close_enabled   = APP_AUTO_CLOSE_DEFAULT_ON;
    s_cfg.auto_close_delay_min = APP_AUTO_CLOSE_DEFAULT_MIN;
    s_cfg.alarm_immediate      = APP_ALARM_IMMEDIATE_DEFAULT;
    s_cfg.alarm_left_open      = APP_ALARM_LEFT_OPEN_DEFAULT;

    /* Sensible notification defaults: alarms matter, routine open/close
     * would be noisy, so they start off. */
    s_cfg.notify_alarm   = true;
    s_cfg.notify_offline = true;
    s_cfg.notify_open    = false;
    s_cfg.notify_close   = false;

    s_cfg.provisioned = false;
}

esp_err_t settings_init(void)
{
    load_defaults();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* First boot — namespace does not exist yet. Defaults stand. */
        ESP_LOGI(TAG, "No stored settings (%s); using factory defaults", esp_err_to_name(err));
        s_loaded = true;
        return ESP_OK;
    }

    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const setting_desc_t *d = &k_fields[i];
        void *dst = field_ptr(d);

        switch (d->kind) {
        case SETTING_STR: {
            size_t len = d->size;
            if (nvs_get_str(h, d->name, (char *)dst, &len) == ESP_OK) {
                /* Also clean on the way out of NVS, not just on the way in.
                 * A value stored by an older build (or by a paste that
                 * slipped through) is repaired on the next boot instead of
                 * failing mysteriously forever. */
                sanitize_str((char *)dst, d->sanitize);
            }
            break;
        }
        case SETTING_BOOL: {
            uint8_t v = 0;
            if (nvs_get_u8(h, d->name, &v) == ESP_OK) {
                *(bool *)dst = (v != 0);
            }
            break;
        }
        case SETTING_U32: {
            uint32_t v = 0;
            if (nvs_get_u32(h, d->name, &v) == ESP_OK && v >= d->min_v && v <= d->max_v) {
                *(uint32_t *)dst = v;
            }
            break;
        }
        }
    }

    nvs_close(h);
    s_loaded = true;

    /* Migration: a unit updated from single-recipient firmware has tg_chat
     * set and tg_chats empty. Carry the working value forward so Telegram
     * keeps functioning across the update without the user touching it. */
    if (s_cfg.tg_chats[0] == '\0' && s_cfg.tg_chat[0] != '\0') {
        snprintf(s_cfg.tg_chats, sizeof(s_cfg.tg_chats), "%s:Me", s_cfg.tg_chat);
        ESP_LOGI(TAG, "Migrated the existing Telegram chat ID into the recipient list");
        settings_save();
    }

    ESP_LOGI(TAG, "Settings loaded (provisioned=%s, ssid='%s', influx=%s)",
        s_cfg.provisioned ? "yes" : "no",
        s_cfg.wifi_ssid,
        s_cfg.influx_enabled ? "on" : "off");

    return ESP_OK;
}

const app_settings_t *settings_get(void)
{
    if (!s_loaded) {
        load_defaults();
        s_loaded = true;
    }
    return &s_cfg;
}

esp_err_t settings_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open for write failed: %s", esp_err_to_name(err));
        return err;
    }

    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const setting_desc_t *d = &k_fields[i];
        const void *src = field_ptr(d);

        switch (d->kind) {
        case SETTING_STR:
            nvs_set_str(h, d->name, (const char *)src);
            break;
        case SETTING_BOOL:
            nvs_set_u8(h, d->name, *(const bool *)src ? 1 : 0);
            break;
        case SETTING_U32:
            nvs_set_u32(h, d->name, *(const uint32_t *)src);
            break;
        }
    }

    err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t settings_factory_reset(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    load_defaults();
    ESP_LOGW(TAG, "Factory reset performed — all user settings erased");
    return err;
}

bool settings_check_password(const char *candidate)
{
    if (candidate == NULL) {
        return false;
    }

    /* Length-independent compare so a wrong password does not leak its
     * matching-prefix length through response timing. */
    size_t n = sizeof(s_cfg.admin_password);
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) {
        char a = s_cfg.admin_password[i];
        char b = (i < strlen(candidate)) ? candidate[i] : '\0';
        diff |= (uint8_t)(a ^ b);
    }
    return diff == 0;
}

esp_err_t settings_set_password(const char *new_password)
{
    if (new_password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strlen(new_password);
    if (len < 4 || len >= sizeof(s_cfg.admin_password)) {
        return ESP_ERR_INVALID_SIZE;
    }
    copy_str(s_cfg.admin_password, sizeof(s_cfg.admin_password), new_password);
    return settings_save();
}

int settings_apply_json(const char *json, size_t json_len)
{
    cJSON *root = cJSON_ParseWithLength(json, json_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int changed = 0;

    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const setting_desc_t *d = &k_fields[i];
        if (field_is_write_protected(d->name)) {
            continue;
        }

        cJSON *item = cJSON_GetObjectItemCaseSensitive(root, d->name);
        if (item == NULL) {
            continue;
        }

        void *dst = field_ptr(d);

        switch (d->kind) {
        case SETTING_STR: {
            if (!cJSON_IsString(item) || item->valuestring == NULL) {
                break;
            }
            /* An empty value for a secret means "leave it alone" — the UI
             * renders secrets as blank boxes and must not wipe a stored
             * token just because the user did not retype it. */
            if (d->secret && item->valuestring[0] == '\0') {
                break;
            }

            /* Clean before comparing, so re-saving a value that only differed
             * by stray whitespace is not counted as a change. */
            char clean[320];
            copy_str(clean, sizeof(clean), item->valuestring);
            sanitize_str(clean, d->sanitize);

            if (field_rejects_value(d->name, clean)) {
                ESP_LOGW(TAG, "Ignoring %s='%s': not a valid IPv4 address",
                    d->name, clean);
                break;
            }

            if (strncmp((const char *)dst, clean, d->size) != 0) {
                copy_str((char *)dst, d->size, clean);
                changed++;
            }
            break;
        }
        case SETTING_BOOL: {
            bool v;
            if (cJSON_IsBool(item)) {
                v = cJSON_IsTrue(item);
            } else if (cJSON_IsNumber(item)) {
                v = (item->valuedouble != 0);
            } else {
                break;
            }
            if (*(bool *)dst != v) {
                *(bool *)dst = v;
                changed++;
            }
            break;
        }
        case SETTING_U32: {
            if (!cJSON_IsNumber(item)) {
                break;
            }
            uint32_t v = (uint32_t)item->valuedouble;
            if (v < d->min_v || v > d->max_v) {
                break;
            }
            if (*(uint32_t *)dst != v) {
                *(uint32_t *)dst = v;
                changed++;
            }
            break;
        }
        }
    }

    cJSON_Delete(root);
    return changed;
}

/* Appends a JSON-escaped string (without surrounding quotes). Returns the
 * number of bytes written, or -1 if it would overflow. */
static int append_escaped(char *out, size_t out_size, size_t pos, const char *s)
{
    for (; *s; s++) {
        char esc[8];
        int n;
        switch (*s) {
        case '"':  n = snprintf(esc, sizeof(esc), "\\\""); break;
        case '\\': n = snprintf(esc, sizeof(esc), "\\\\"); break;
        case '\n': n = snprintf(esc, sizeof(esc), "\\n");  break;
        case '\r': n = snprintf(esc, sizeof(esc), "\\r");  break;
        case '\t': n = snprintf(esc, sizeof(esc), "\\t");  break;
        default:
            if ((unsigned char)*s < 0x20) {
                n = snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*s);
            } else {
                esc[0] = *s;
                esc[1] = '\0';
                n = 1;
            }
            break;
        }
        if (pos + (size_t)n >= out_size) {
            return -1;
        }
        memcpy(out + pos, esc, (size_t)n);
        pos += (size_t)n;
    }
    out[pos] = '\0';
    return (int)pos;
}

/* snprintf into out at pos, returning the new pos or -1 on overflow. */
static int append_fmt(char *out, size_t out_size, size_t pos, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + pos, out_size - pos, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= out_size - pos) {
        return -1;
    }
    return (int)pos + n;
}

int settings_to_json(char *out, size_t out_size)
{
    int pos = append_fmt(out, out_size, 0, "{");
    if (pos < 0) {
        return -1;
    }

    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const setting_desc_t *d = &k_fields[i];
        const void *src = field_ptr(d);

        pos = append_fmt(out, out_size, (size_t)pos, "%s\"%s\":", (i == 0) ? "" : ",", d->name);
        if (pos < 0) {
            return -1;
        }

        switch (d->kind) {
        case SETTING_STR: {
            const char *v = (const char *)src;
            if (d->secret) {
                /* Never transmit the value; report only whether one is set. */
                pos = append_fmt(out, out_size, (size_t)pos, "\"\",\"%s_set\":%s",
                    d->name, (v[0] != '\0') ? "true" : "false");
            } else {
                pos = append_fmt(out, out_size, (size_t)pos, "\"");
                if (pos < 0) {
                    return -1;
                }
                pos = append_escaped(out, out_size, (size_t)pos, v);
                if (pos < 0) {
                    return -1;
                }
                pos = append_fmt(out, out_size, (size_t)pos, "\"");
            }
            break;
        }
        case SETTING_BOOL:
            pos = append_fmt(out, out_size, (size_t)pos, "%s", *(const bool *)src ? "true" : "false");
            break;
        case SETTING_U32:
            pos = append_fmt(out, out_size, (size_t)pos, "%u", (unsigned)*(const uint32_t *)src);
            break;
        }

        if (pos < 0) {
            return -1;
        }
    }

    pos = append_fmt(out, out_size, (size_t)pos, "}");
    return pos;
}

/* ── Convenience mutators ────────────────────────────────────────────── */

esp_err_t settings_set_auto_close(bool enabled)
{
    s_cfg.auto_close_enabled = s_cfg.auto_close_locked_off ? false : enabled;
    return settings_save();
}

esp_err_t settings_set_auto_close_locked(bool locked)
{
    s_cfg.auto_close_locked_off = locked;
    s_cfg.auto_close_enabled = !locked;
    return settings_save();
}

esp_err_t settings_set_auto_close_delay(uint32_t minutes)
{
    if (minutes < 1 || minutes > 120) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg.auto_close_delay_min = minutes;
    return settings_save();
}

esp_err_t settings_set_alarm_immediate(bool engaged)
{
    s_cfg.alarm_immediate = engaged;
    return settings_save();
}

esp_err_t settings_set_alarm_left_open(bool engaged)
{
    s_cfg.alarm_left_open = engaged;
    return settings_save();
}

esp_err_t settings_set_control_enabled(bool enabled)
{
    if (s_cfg.control_enabled != enabled) {
        ESP_LOGW(TAG, "Door control %s", enabled ? "ARMED" : "DISARMED");
    }
    s_cfg.control_enabled = enabled;
    return settings_save();
}

esp_err_t settings_set_calibration(uint32_t closed_mv, uint32_t open_mv)
{
    s_cfg.door_closed_mv = closed_mv;
    s_cfg.door_open_mv   = open_mv;

    if (closed_mv == 0 || open_mv == 0) {
        s_cfg.door_cal_valid = false;
        ESP_LOGI(TAG, "Door calibration cleared; using manual threshold %" PRIu32 " mV",
            s_cfg.door_threshold_mv);
        settings_save();
        return ESP_OK;
    }

    uint32_t hi  = (closed_mv > open_mv) ? closed_mv : open_mv;
    uint32_t lo  = (closed_mv > open_mv) ? open_mv : closed_mv;
    uint32_t gap = hi - lo;

    if (gap < APP_DOOR_CAL_MIN_SEPARATION_MV) {
        s_cfg.door_cal_valid = false;
        ESP_LOGW(TAG, "Calibration rejected: closed %" PRIu32 " mV and open %" PRIu32
                      " mV are only %" PRIu32 " mV apart (need %d)",
            closed_mv, open_mv, gap, APP_DOOR_CAL_MIN_SEPARATION_MV);
        settings_save();
        return ESP_ERR_INVALID_ARG;
    }

    /* The trip point is the midpoint, which is exactly what "nearer to the
     * closed reading than the open one" means. */
    s_cfg.door_threshold_mv = lo + gap / 2;
    s_cfg.door_cal_valid = true;

    ESP_LOGI(TAG, "Door calibrated: closed %" PRIu32 " mV, open %" PRIu32
                  " mV, gap %" PRIu32 " mV, trip point %" PRIu32 " mV (%s)",
        closed_mv, open_mv, gap, s_cfg.door_threshold_mv,
        (closed_mv > open_mv) ? "closed reads higher" : "closed reads lower");

    return settings_save();
}

esp_err_t settings_mark_provisioned(void)
{
    s_cfg.provisioned = true;
    return settings_save();
}

/* Walks the packed "id:nick|id:nick" list. Returns a pointer to the start of
 * record `index`, or NULL. */
static const char *tg_chat_record(int index)
{
    const char *p = s_cfg.tg_chats;
    if (*p == '\0') {
        return NULL;
    }
    for (int i = 0; i < index; i++) {
        p = strchr(p, '|');
        if (p == NULL) {
            return NULL;
        }
        p++;
    }
    return (*p != '\0') ? p : NULL;
}

int settings_tg_chat_count(void)
{
    if (s_cfg.tg_chats[0] == '\0') {
        return 0;
    }
    int n = 1;
    for (const char *p = s_cfg.tg_chats; *p; p++) {
        if (*p == '|') {
            n++;
        }
    }
    return (n > TG_CHAT_MAX) ? TG_CHAT_MAX : n;
}

bool settings_tg_chat_get(int index, char *id_out, size_t id_len,
                          char *nick_out, size_t nick_len)
{
    const char *rec = tg_chat_record(index);
    if (rec == NULL) {
        return false;
    }

    const char *end   = strchr(rec, '|');
    const char *colon = strchr(rec, ':');
    if (end != NULL && colon != NULL && colon > end) {
        colon = NULL;   /* this record has no nickname */
    }

    size_t id_span = colon ? (size_t)(colon - rec)
                   : (end  ? (size_t)(end - rec) : strlen(rec));

    if (id_out != NULL && id_len > 0) {
        size_t n = (id_span < id_len - 1) ? id_span : id_len - 1;
        memcpy(id_out, rec, n);
        id_out[n] = '\0';
    }

    if (nick_out != NULL && nick_len > 0) {
        if (colon == NULL) {
            nick_out[0] = '\0';
        } else {
            const char *ns = colon + 1;
            size_t span = end ? (size_t)(end - ns) : strlen(ns);
            size_t n = (span < nick_len - 1) ? span : nick_len - 1;
            memcpy(nick_out, ns, n);
            nick_out[n] = '\0';
        }
    }

    return id_span > 0;
}

/* ── Trusted devices ──────────────────────────────────────────────────
 * Same packed-string approach as the Telegram recipients: one NVS value,
 * "ip:label|ip:label|...", so the list needs no storage machinery of its
 * own. Entries are added by the auth path rather than typed in. */

static const char *trusted_record(int index)
{
    const char *p = s_cfg.trusted_ips;
    if (*p == '\0') {
        return NULL;
    }
    for (int i = 0; i < index; i++) {
        p = strchr(p, '|');
        if (p == NULL) {
            return NULL;
        }
        p++;
    }
    return (*p != '\0') ? p : NULL;
}

int settings_trusted_count(void)
{
    if (s_cfg.trusted_ips[0] == '\0') {
        return 0;
    }
    int n = 1;
    for (const char *p = s_cfg.trusted_ips; *p; p++) {
        if (*p == '|') {
            n++;
        }
    }
    return (n > TRUSTED_MAX) ? TRUSTED_MAX : n;
}

bool settings_trusted_get(int index, char *ip_out, size_t ip_len,
                          char *label_out, size_t label_len)
{
    const char *rec = trusted_record(index);
    if (rec == NULL) {
        return false;
    }

    const char *end   = strchr(rec, '|');
    const char *colon = strchr(rec, ':');
    if (end != NULL && colon != NULL && colon > end) {
        colon = NULL;
    }

    size_t ip_span = colon ? (size_t)(colon - rec)
                   : (end ? (size_t)(end - rec) : strlen(rec));

    if (ip_out != NULL && ip_len > 0) {
        size_t n = (ip_span < ip_len - 1) ? ip_span : ip_len - 1;
        memcpy(ip_out, rec, n);
        ip_out[n] = '\0';
    }

    if (label_out != NULL && label_len > 0) {
        if (colon == NULL) {
            label_out[0] = '\0';
        } else {
            const char *ls = colon + 1;
            size_t span = end ? (size_t)(end - ls) : strlen(ls);
            size_t n = (span < label_len - 1) ? span : label_len - 1;
            memcpy(label_out, ls, n);
            label_out[n] = '\0';
        }
    }

    return ip_span > 0;
}

bool settings_trusted_check(const char *ip)
{
    if (ip == NULL || ip[0] == '\0') {
        return false;
    }
    int n = settings_trusted_count();
    for (int i = 0; i < n; i++) {
        char stored[TRUSTED_IP_LEN];
        if (settings_trusted_get(i, stored, sizeof(stored), NULL, 0) &&
            strcmp(stored, ip) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t settings_trusted_add(const char *ip, const char *label)
{
    if (ip == NULL || ip[0] == '\0' || strlen(ip) >= TRUSTED_IP_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    /* The separators cannot appear inside a value. IPv6 literals contain
     * colons, so those are stored without a label rather than mangled. */
    if (strchr(ip, '|') != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (settings_trusted_check(ip)) {
        return ESP_OK;   /* already trusted; nothing to do */
    }
    if (settings_trusted_count() >= TRUSTED_MAX) {
        return ESP_ERR_NO_MEM;
    }

    char clean_label[TRUSTED_LABEL_LEN] = "";
    bool ipv6 = (strchr(ip, ':') != NULL);
    if (!ipv6) {
        if (label != NULL && label[0] != '\0') {
            size_t j = 0;
            for (const char *p = label; *p && j < sizeof(clean_label) - 1; p++) {
                if (*p != '|' && *p != ':') {
                    clean_label[j++] = *p;
                }
            }
            clean_label[j] = '\0';
        } else {
            /* Default to the date it was trusted, which is the one detail
             * someone needs when deciding whether an entry is still wanted. */
            time_t now = 0;
            time(&now);
            struct tm tm_local = {};
            localtime_r(&now, &tm_local);
            if (tm_local.tm_year >= (2024 - 1900)) {
                strftime(clean_label, sizeof(clean_label), "added %d %b", &tm_local);
            }
        }
    }

    size_t used = strlen(s_cfg.trusted_ips);
    int written = snprintf(s_cfg.trusted_ips + used, sizeof(s_cfg.trusted_ips) - used,
                           "%s%s%s%s",
                           (used > 0) ? "|" : "",
                           ip,
                           clean_label[0] ? ":" : "",
                           clean_label);
    if (written < 0 || (size_t)written >= sizeof(s_cfg.trusted_ips) - used) {
        s_cfg.trusted_ips[used] = '\0';   /* roll back a truncated append */
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Trusted device added: %s (until next reboot)", ip);
    return ESP_OK;
}

esp_err_t settings_trusted_remove(const char *ip)
{
    if (ip == NULL || ip[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char rebuilt[sizeof(s_cfg.trusted_ips)] = "";
    size_t pos = 0;
    bool removed = false;
    int n = settings_trusted_count();

    for (int i = 0; i < n; i++) {
        char stored[TRUSTED_IP_LEN];
        char label[TRUSTED_LABEL_LEN];
        if (!settings_trusted_get(i, stored, sizeof(stored), label, sizeof(label))) {
            continue;
        }
        if (strcmp(stored, ip) == 0) {
            removed = true;
            continue;
        }
        int w = snprintf(rebuilt + pos, sizeof(rebuilt) - pos, "%s%s%s%s",
                         (pos > 0) ? "|" : "", stored,
                         label[0] ? ":" : "", label);
        if (w < 0 || (size_t)w >= sizeof(rebuilt) - pos) {
            break;
        }
        pos += (size_t)w;
    }

    if (!removed) {
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(s_cfg.trusted_ips, rebuilt, sizeof(s_cfg.trusted_ips));
    ESP_LOGI(TAG, "Trusted device removed: %s", ip);
    return ESP_OK;
}

esp_err_t settings_trusted_clear(void)
{
    if (s_cfg.trusted_ips[0] == '\0') {
        return ESP_OK;
    }
    s_cfg.trusted_ips[0] = '\0';
    ESP_LOGW(TAG, "Trusted device list cleared");
    return ESP_OK;
}

/* ── Password recovery ──────────────────────────────────────────────── */

esp_err_t settings_begin_recovery(void)
{
    s_cfg.recovery_pending = true;

    /* Every trusted device predates the recovery, so none of them may be
     * used to complete it — otherwise a phone left on the list could set the
     * new password without anyone touching the hardware. (The list is
     * RAM-only, so the reboot clears it regardless; doing it here makes the
     * revocation immediate rather than waiting for the restart.) */
    s_cfg.trusted_ips[0] = '\0';

    ESP_LOGW(TAG, "RECOVERY MODE armed by the hardware button. "
                  "The old password is refused; set a new one from the recovery page.");
    return settings_save();
}

bool settings_recovery_pending(void)
{
    return s_cfg.recovery_pending;
}

esp_err_t settings_complete_recovery(const char *new_password,
                                     const char *ssid,
                                     const char *wifi_password)
{
    if (new_password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strlen(new_password);
    if (len < 4 || len >= sizeof(s_cfg.admin_password)) {
        return ESP_ERR_INVALID_SIZE;
    }

    copy_str(s_cfg.admin_password, sizeof(s_cfg.admin_password), new_password);

    /* Wi-Fi is optional here: recovery exists for a forgotten password, and
     * the network is usually still fine. Only touch it if a new SSID was
     * actually supplied. */
    if (ssid != NULL && ssid[0] != '\0') {
        copy_str(s_cfg.wifi_ssid, sizeof(s_cfg.wifi_ssid), ssid);
        sanitize_str(s_cfg.wifi_ssid, SAN_TRIM);
        copy_str(s_cfg.wifi_password, sizeof(s_cfg.wifi_password),
                 (wifi_password != NULL) ? wifi_password : "");
        ESP_LOGW(TAG, "Recovery also set new Wi-Fi credentials for '%s'", s_cfg.wifi_ssid);
    }

    s_cfg.recovery_pending = false;

    ESP_LOGW(TAG, "Recovery complete: new admin password set. "
                  "Calibration, InfluxDB, notifications and history are unchanged.");
    return settings_save();
}

bool settings_have_wifi(void)
{
    return s_cfg.wifi_ssid[0] != '\0';
}
