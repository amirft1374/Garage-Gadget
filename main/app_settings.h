#pragma once

/*
 * app_settings.h — All user configuration, persisted in NVS.
 *
 * Nothing user-specific is compiled in.  Nothing here is
 * baked into the firmware: one binary serves every customer, and each unit
 * is configured through the setup wizard / Settings tab.
 *
 * Threading: settings_get() hands out a pointer to the live struct, which is
 * only mutated from the web-server task.  Readers copy scalars out; anyone
 * needing a consistent multi-field snapshot should take settings_lock().
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* ── Network ─────────────────────────────────────────────────────── */
    char     wifi_ssid[33];
    char     wifi_password[65];
    char     device_name[32];        /* mDNS hostname + Influx device tag  */
    char     timezone[48];           /* POSIX TZ string                    */

    /* ── Security ────────────────────────────────────────────────────── */
    char     admin_password[33];

    /* ── InfluxDB (optional) ─────────────────────────────────────────── */
    bool     influx_enabled;
    char     influx_url[160];
    char     influx_token[256];
    char     influx_org[64];
    char     influx_bucket[64];

    /* ── Grafana (link only — the ESP never talks to Grafana) ────────── */
    char     grafana_url[160];

    /* ── Notifications: Telegram ─────────────────────────────────────── *
     * Multiple recipients, each with a nickname, stored as one packed
     * string: "id:nickname|id:nickname|...". Keeping the list in a single
     * NVS value means it rides the existing settings plumbing — load, save,
     * JSON out and JSON in — with no special cases anywhere.
     *
     * `tg_chat` is the old single-recipient field, retained only so that an
     * existing unit's working configuration survives a firmware update: it
     * is migrated into tg_chats on first load and then ignored. */
    bool     tg_enabled;
    char     tg_token[64];
    char     tg_chat[32];        /* legacy, migrated on load               */
    char     tg_chats[320];      /* "id:nick|id:nick|..."                  */

    /* ── Notifications: ntfy ─────────────────────────────────────────── */
    bool     ntfy_enabled;
    char     ntfy_server[96];        /* e.g. https://ntfy.sh              */
    char     ntfy_topic[64];

    /* ── What to notify about ────────────────────────────────────────── */
    bool     notify_open;
    bool     notify_close;
    bool     notify_alarm;
    bool     notify_offline;         /* Wi-Fi recovered / device booted   */

    /* ── Door behaviour ──────────────────────────────────────────────── */
    /* How long the relay holds the opener's button closed, in ms. */
    uint32_t relay_pulse_ms;
    bool     auto_close_enabled;
    bool     auto_close_locked_off;  /* hard override; hides the toggle    */
    uint32_t auto_close_delay_min;
    bool     alarm_immediate;
    bool     alarm_left_open;

    /* ── Door sensor ─────────────────────────────────────────────────── *
     * The sensor pin is read as an analogue voltage rather than a logic
     * level, so one firmware works with reed switches, hall sensors and
     * resistor-divider wiring that never quite reach a clean 0/3.3 V.
     *
     * Two ways to decide open vs closed:
     *
     *  Calibrated (preferred) — the installer captures the actual reading
     *  with the door shut and again with it open. Classification is then
     *  "whichever reference the reading is nearer to", the trip point is the
     *  midpoint, and the dead-band scales with the measured separation. This
     *  copes with either polarity: a sensor that reads *lower* when closed
     *  works exactly as well as one that reads higher.
     *
     *  Manual fallback — a plain threshold, with closed meaning "at or above
     *  it". Used until a calibration exists.                             */
    uint32_t door_threshold_mv;   /* manual / derived trip point, mV       */
    uint32_t door_closed_mv;      /* captured reading, door shut           */
    uint32_t door_open_mv;        /* captured reading, door open           */
    bool     door_cal_valid;      /* both points captured and far enough apart */

    /* ── Update notice ───────────────────────────────────── *
     * When true the device asks GitHub once a day whether a newer release
     * exists, and says so on the dashboard. It never downloads or installs
     * anything — see update_check.h for why.
     *
     * Defaults to false. The check is an outbound request to a third party
     * that reveals the device's public IP, and a garage opener should not
     * start talking to the internet on its own account without being asked.
     * The setup wizard offers it explicitly.                              */
    bool     update_check_enabled;

    /* ── Relay master switch ───────────────────────────────── *
     * While false the relay is never energised, by any route: the web button,
     * auto-close, Home Assistant, MQTT. Nothing.
     *
     * Defaults to false on a brand-new or factory-reset device, and that
     * default is the point. A unit on the bench is wired to a real opener
     * often enough, and during setup it has no calibration, so the door
     * reads "open", auto-close fires, and the door moves while someone is
     * stood next to it with a screwdriver. The installer turns this on once
     * the sensor is calibrated and the unit is where it belongs.
     *
     * Deliberately persisted, so a power cut cannot silently re-arm a door
     * that was switched off on purpose.                                   */
    bool     control_enabled;

    /* ── Published dashboard address ─────────────────────────────────── *
     * What the device tells people to open, e.g. in notifications. Blank
     * means "use whatever IP DHCP gave me", which is right for a plain LAN
     * install. Set it when the device is reached by some other name: a
     * static hostname, a reverse proxy, a VPN address, or a forwarded port
     * that does not match the one it listens on. */
    char     public_url[96];

    /* ── Fixed LAN address (optional) ────────────────────────────────── *
     * Blank static_ip means DHCP, which is the right answer for most
     * installs. Set it when the device must always be at the same address:
     * a Home Assistant entry, a bookmark or a port-forward that cannot
     * follow a lease around.
     *
     * A router-side DHCP reservation does the same job with nothing stored
     * here, and is still the more robust option — it survives the device
     * being moved to another network. This exists for the case where the
     * router cannot do reservations, or is not the user's to configure.
     *
     * Applied when the station comes up, so a change takes effect on the
     * next reboot. static_gw is required alongside static_ip; a blank mask
     * means 255.255.255.0 and a blank DNS means "ask the gateway".
     *
     * Safety net: if the gateway does not answer after the interface comes
     * up, the device gives up on the static address and falls back to DHCP
     * for that boot — see wifi_manager.cpp. A typo costs a reboot, not a
     * trip to the garage with a USB cable.                               */
    char     static_ip[16];
    char     static_gw[16];
    char     static_mask[16];
    char     static_dns[16];

    /* ── Trusted devices ─────────────────────────────────────────────── *
     * Clients that have already proved they know the admin password, so a
     * phone that lives in the house is not asked for it on every visit.
     * Packed as "ip:label|ip:label|...".
     *
     * RAM only, never written to NVS: the list is empty again after every
     * power cycle or reboot, so trust must be re-earned with the password.
     * That also bounds the risk below — a stale entry cannot outlive the
     * uptime it was created in.
     *
     * The trust is bound to an IP address, which is worth understanding:
     * while the device is up, if DHCP hands that address to a different
     * machine, the new one inherits the trust. Reserve addresses for anything
     * listed here. The list is also cleared whenever the password changes,
     * so changing the password really does lock everyone out again.       */
    char     trusted_ips[360];

    /* ── Home Assistant via MQTT (optional) ──────────────────────────── *
     * The device publishes HA discovery payloads on connect, so Home
     * Assistant builds the entities itself — no YAML on the user's side.
     * Commands arrive on the same connection, so there is no need for HA to
     * be able to reach the device, which matters on segmented networks. */
    bool     mqtt_enabled;
    char     mqtt_host[64];      /* broker hostname or IP                 */
    uint32_t mqtt_port;          /* 1883 plain, 8883 TLS                  */
    char     mqtt_user[48];
    char     mqtt_pass[64];
    bool     mqtt_tls;

    /* ── Home-server ping monitor (optional) ─────────────────────────── */
    char     monitor_host[64];

    /* ── Lifecycle ───────────────────────────────────────────────────── */
    bool     provisioned;            /* setup wizard completed             */

    /* Set by holding the recovery button. While true the normal password is
     * refused everywhere and only the recovery page is reachable, so a
     * half-finished recovery cannot leave the device wide open. Cleared when
     * a new password is set. Survives a reboot on purpose: pressing the
     * button and then losing power must not silently re-lock the owner out. */
    bool     recovery_pending;
} app_settings_t;

/* Field descriptor kinds, used by the generic JSON get/set plumbing. */
typedef enum {
    SETTING_STR = 0,
    SETTING_BOOL,
    SETTING_U32,
} setting_kind_t;

/* How much of the user's typing to clean up on the way in.
 *
 * Credentials get pasted from chat apps, and a stray space or newline that
 * comes along for the ride would otherwise be stored verbatim and produce a
 * baffling failure much later (a bot token with a space in it does not even
 * survive URL parsing). Anything that can never legitimately contain
 * whitespace is stripped of all of it. */
typedef enum {
    SAN_NONE = 0,   /* store exactly what was sent (passwords)          */
    SAN_TRIM,       /* drop leading/trailing whitespace only            */
    SAN_STRIP_WS,   /* remove every whitespace character                */
} sanitize_t;

typedef struct {
    const char    *name;    /* JSON key, also the NVS key (<=15 chars) */
    setting_kind_t kind;
    size_t         offset;  /* offsetof() into app_settings_t          */
    size_t         size;    /* buffer size for SETTING_STR             */
    bool           secret;  /* never echoed back to a client           */
    sanitize_t     sanitize;
    uint32_t       min_v;   /* SETTING_U32 range (inclusive)           */
    uint32_t       max_v;
} setting_desc_t;

/* Loads settings from NVS, filling anything missing with factory defaults.
 * Safe to call once at boot before Wi-Fi is up. */
esp_err_t settings_init(void);

/* Live settings. Never NULL after settings_init(). */
const app_settings_t *settings_get(void);

/* Serialises every field to NVS. Call after a batch of settings_apply_json
 * updates. Returns ESP_OK even if individual keys were unchanged. */
esp_err_t settings_save(void);

/* Restores factory defaults and erases the NVS namespace. The caller is
 * expected to reboot afterwards. */
esp_err_t settings_factory_reset(void);

/* Constant-time-ish comparison against the stored admin password. */
bool settings_check_password(const char *candidate);

/* Replaces the admin password. Rejects empty or >32-char values. */
esp_err_t settings_set_password(const char *new_password);

/* Applies a JSON object of {field: value} pairs onto the live settings.
 * Unknown keys are ignored; type mismatches are rejected per-field.
 * Secret string fields are left untouched when the supplied value is the
 * empty string, so the UI can render a blank box without wiping a token.
 * Returns the number of fields actually changed, or -1 on parse failure. */
int settings_apply_json(const char *json, size_t json_len);

/* Renders the full settings object as JSON. Secret fields are emitted as
 * "" plus a companion "<field>_set": true/false so the UI can show
 * "configured" without ever transmitting the value.
 * Returns bytes written (excluding NUL), or -1 if the buffer is too small. */
int settings_to_json(char *out, size_t out_size);

/* Convenience mutators used by the dashboard toggles (each saves to NVS). */
esp_err_t settings_set_auto_close(bool enabled);
esp_err_t settings_set_auto_close_locked(bool locked);
esp_err_t settings_set_auto_close_delay(uint32_t minutes);
esp_err_t settings_set_alarm_immediate(bool engaged);
esp_err_t settings_set_alarm_left_open(bool engaged);

/* Arms or disarms the relay entirely. Persisted immediately: this is a
 * safety state, and it must survive a power cut in whichever position the
 * installer left it. See control_enabled. */
esp_err_t settings_set_control_enabled(bool enabled);
esp_err_t settings_mark_provisioned(void);

/* Stores a two-point door-sensor calibration and derives the trip point.
 *
 * Pass 0 for either reading to clear the calibration and fall back to the
 * manual threshold. The calibration is marked valid only when the two points
 * are at least APP_DOOR_CAL_MIN_SEPARATION_MV apart — a sensor that reads
 * roughly the same in both states is not measuring the door, and pretending
 * otherwise would give a state that flickers.
 *
 * Returns ESP_OK when a valid calibration was stored, ESP_ERR_INVALID_ARG
 * when the points were too close together (the values are still recorded so
 * the UI can show what was measured), or a save error. */
esp_err_t settings_set_calibration(uint32_t closed_mv, uint32_t open_mv);

/* True when Wi-Fi credentials exist — i.e. we can try station mode. */
bool settings_have_wifi(void);

/* ── Telegram recipients ──────────────────────────────────────────────
 * Accessors over the packed tg_chats string. Nicknames are cosmetic; the id
 * is what Telegram needs. */

#define TG_CHAT_MAX      8
#define TG_CHAT_ID_LEN   24
#define TG_CHAT_NICK_LEN 32

/* Number of configured recipients. */
int settings_tg_chat_count(void);

/* Reads recipient `index`. Either output may be NULL. Returns false when the
 * index is out of range. */
bool settings_tg_chat_get(int index,
                          char *id_out, size_t id_len,
                          char *nick_out, size_t nick_len);

/* ── Trusted devices ──────────────────────────────────────────────────
 * An address that has authenticated once is remembered, so the household's
 * own phones are not challenged on every visit. */

#define TRUSTED_MAX       8
#define TRUSTED_IP_LEN    46    /* fits an IPv6 literal */
#define TRUSTED_LABEL_LEN 32

int  settings_trusted_count(void);

/* Reads entry `index`. Either output may be NULL. */
bool settings_trusted_get(int index,
                          char *ip_out, size_t ip_len,
                          char *label_out, size_t label_len);

/* True when `ip` is on the list. Cheap enough to call per request. */
bool settings_trusted_check(const char *ip);

/* Adds `ip` if it is not already present and there is room. `label` may be
 * NULL, in which case the current date is used. Saves to NVS.
 * Returns ESP_OK, ESP_ERR_NO_MEM when the list is full, or ESP_ERR_INVALID_ARG. */
esp_err_t settings_trusted_add(const char *ip, const char *label);

/* Removes one entry by address. Saves. */
esp_err_t settings_trusted_remove(const char *ip);

/* Empties the list. Called on password change as well as from the UI —
 * changing the password must genuinely revoke everyone. */
esp_err_t settings_trusted_clear(void);

/* ── Password recovery ────────────────────────────────────────────────
 * Entered by holding the recovery button; see APP_RECOVERY_GPIO. */

/* Puts the device into recovery mode and drops every trusted device, so a
 * recovery cannot be completed from a session that predates it. */
esp_err_t settings_begin_recovery(void);

bool settings_recovery_pending(void);

/* Leaves recovery: installs `new_password`, and replaces the Wi-Fi
 * credentials when `ssid` is non-empty. Everything else — calibration,
 * InfluxDB, Grafana, notifications, the event history — is untouched.
 * Returns ESP_ERR_INVALID_SIZE if the password is not 4-32 characters. */
esp_err_t settings_complete_recovery(const char *new_password,
                                     const char *ssid,
                                     const char *wifi_password);

#ifdef __cplusplus
}
#endif
