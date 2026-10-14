#pragma once

/*
 * app_config.h — Compile-time defaults for Garage Gadget.
 *
 * NOTHING SECRET LIVES HERE.
 *
 * Every user-specific value (Wi-Fi, InfluxDB, Grafana, notification tokens,
 * admin password) is entered by the end user during first-time setup and is
 * stored in NVS.  See app_settings.h.
 *
 * The values below are only the factory defaults a brand-new (or
 * factory-reset) device starts from.
 */

/* ── Product identity ────────────────────────────────────────────────── */
#define APP_SW_VERSION              "2.0.0"
#define APP_PRODUCT_NAME            "Garage Gadget"

/* ── Project home ───────────────────────────────────────── *
 * Shown in the setup wizard and the dashboard footer so an owner can always
 * find the source, the documentation and the releases for the firmware they
 * are actually running.                                                   */
#define APP_REPO_URL        "https://github.com/amirft1374/Garage-Gadget"
#define APP_RELEASES_URL    APP_REPO_URL "/releases/latest"

/* The release the device asks GitHub about. Only the tag is read from the
 * reply; the link shown to the owner is APP_RELEASES_URL, built here rather
 * than parsed out of the response — one less thing to get wrong, and it
 * stays right even when the JSON changes shape. */
#define APP_UPDATE_API_URL  "https://api.github.com/repos/amirft1374/Garage-Gadget/releases/latest"

/* How often to ask, in hours, and how long to wait after a failure.
 * Unauthenticated GitHub API calls are limited to 60 per hour per IP, which
 * a daily check never approaches even with several units on one address. */
#define APP_UPDATE_CHECK_INTERVAL_H (24)
#define APP_UPDATE_CHECK_RETRY_H    (4)

/* Default mDNS/device name. The last 4 hex digits of the MAC are appended
 * to the SoftAP SSID so several units on one bench stay distinguishable.  */
#define APP_DEFAULT_DEVICE_NAME     "garage"

/* ── First-time setup (SoftAP provisioning portal) ───────────────────── */
/* SSID becomes "<prefix>-XXXX" where XXXX = last 2 bytes of the MAC.      */
#define APP_SETUP_AP_SSID_PREFIX    "GarageGadget"
/* WPA2 passphrase for the setup AP. Must be >= 8 chars.                   */
#define APP_SETUP_AP_PASSWORD       "garagesetup"
/* Setup portal (and normal web UI) TCP port.                              */
#define APP_WEB_SERVER_PORT         (80)
/* If Wi-Fi credentials are stored but the AP cannot be joined within this
 * many seconds, fall back to the setup portal so the user is never locked
 * out after moving the device or changing routers.                        */
#define APP_WIFI_JOIN_TIMEOUT_S     (45)

/* How long the setup AP lingers after the station successfully joins.
 *
 * Tearing it down the instant we get an IP kicks the phone that just
 * finished the wizard off the network mid-page, which reads as "setup
 * failed" even though it worked. Keeping it up briefly lets the wizard's
 * final screen poll for and display the device's new address before the
 * user has to switch networks.                                            */
#define APP_SETUP_AP_GRACE_S        (180)

/* ── Password recovery button ────────────────────────────────────────── *
 * Holding this input to ground for APP_RECOVERY_HOLD_S puts the device into
 * recovery mode: the admin password is discarded and a new one must be set,
 * with the option of new Wi-Fi credentials at the same time. Nothing else is
 * touched — calibration, InfluxDB, notifications and the event history all
 * survive.
 *
 * Deliberately checked while running, NOT at power-up. GPIO0 is a strapping
 * pin: held low at reset the ESP32-S3 enters USB download mode and this
 * firmware never executes, so a "hold during power-on" gesture could not
 * work. Holding it for ten seconds on a running device proves exactly the
 * same thing — physical access — and always works.
 *
 * On a dev board this is the BOOT button. In a product, bring it out to a
 * pinhole so it needs a paperclip and cannot be pressed by accident.        */
#define APP_RECOVERY_GPIO       (0)
#define APP_RECOVERY_HOLD_S     (10)

/* ── Security ────────────────────────────────────────────────────────── */
/* Factory admin password. The setup wizard forces the user to change it.  */
#define APP_DEFAULT_ADMIN_PASSWORD  "admin"
/* A client IP stays "verified" (no passcode re-prompt) for this long.     */
#define APP_AUTH_SESSION_MINUTES    (720)

/* ── Door sensor ─────────────────────────────────────────────────────── */
/* The sensor pin is sampled as an analogue voltage, not a logic level, so
 * the same firmware copes with reed switches, hall-effect sensors and
 * resistor-divider wiring whose "closed" level is nowhere near 3.3 V.
 *
 * At or above this many millivolts the door reads CLOSED. Adjustable at
 * runtime from Settings; the live voltage is shown next to it so the user
 * can pick a threshold by watching the real numbers.                      */
#define APP_DOOR_THRESHOLD_MV_DEFAULT (1800)

/* Hysteresis band, in millivolts, applied either side of the threshold.
 * Without it a sensor resting near the trip point chatters between states
 * and fills the event log with phantom open/close pairs.                  */
#define APP_DOOR_HYSTERESIS_MV        (150)

/* Each reading is the median of this many samples, which rejects the
 * occasional wild ADC sample without smearing a real transition.          */
#define APP_DOOR_ADC_SAMPLES          (5)

/* ── Two-point calibration ───────────────────────────────────────────── */
/* Samples taken when capturing a calibration point. Spread over roughly a
 * second so the figure reflects real noise rather than one lucky instant. */
#define APP_DOOR_CAL_SAMPLES          (64)

/* The two captured points must differ by at least this much for the
 * calibration to be trusted. Below it the sensor is not actually
 * distinguishing the two states — usually a floating pin or a missing
 * pull-up — and accepting it would produce a door state that flickers.    */
#define APP_DOOR_CAL_MIN_SEPARATION_MV (120)

/* Dead-band as a fraction of the measured separation: gap / this. A quarter
 * of the gap is wide enough to absorb normal noise without making a real
 * transition sluggish.                                                    */
#define APP_DOOR_CAL_HYST_DIVISOR      (4)
#define APP_DOOR_CAL_HYST_MIN_MV       (30)
#define APP_DOOR_CAL_HYST_MAX_MV       (300)

/* ── Relay pulse ─────────────────────────────────────────────────────── *
 * How long the opener's button is held, in milliseconds. Openers vary: most
 * want a short tap, a few need longer to register. Runtime-configurable
 * because it is the kind of thing that only reveals itself once the unit is
 * wired to the actual opener.
 *
 * The upper bound is deliberate. A held button is not just a long tap to
 * many openers — several interpret a multi-second hold as a request to
 * enter programming or limit-setting mode, which is not something a web
 * form should be able to trigger by accident.                             */
#define APP_RELAY_PULSE_MS_DEFAULT  (100)
#define APP_RELAY_PULSE_MS_MIN      (20)
#define APP_RELAY_PULSE_MS_MAX      (2000)

/* ── Door behaviour defaults ─────────────────────────────────────────── */
#define APP_AUTO_CLOSE_DEFAULT_ON   true
#define APP_AUTO_CLOSE_DEFAULT_MIN  (5)
#define APP_ALARM_IMMEDIATE_DEFAULT false
#define APP_ALARM_LEFT_OPEN_DEFAULT true

/* ── Server monitor (ping) ───────────────────────────────────────────── */
/* Empty by default — the user may set a host to watch in Settings.        */
#define APP_DEFAULT_MONITOR_HOST    ""
#define APP_SERVER_PING_INTERVAL_S  (30)
#define APP_SERVER_PING_TIMEOUT_MS  (3000)

/* ── Wi-Fi reconnection ──────────────────────────────────────────────── */
/* Backoff between reconnect attempts, in seconds. Starts at MIN and
 * doubles up to MAX so a brief router reboot recovers in seconds while a
 * genuinely absent AP does not hammer the radio.                          */
#define APP_WIFI_RECONNECT_MIN_S    (5)
#define APP_WIFI_RECONNECT_MAX_S    (300)

/* ── On-device history ───────────────────────────────────────────────── */
/* Periodic sample cadence (RAM-resident trace) in seconds.                */
#define APP_SAMPLE_INTERVAL_S       (60)
/* How many days of 1-minute samples to keep in RAM.                       */
#define APP_SAMPLE_RETENTION_DAYS   (10)
/* Events older than this are pruned from the flash log on rollover.       */
#define APP_EVENT_RETENTION_DAYS    (60)

/* ── Telemetry ───────────────────────────────────────────────────────── */
/* Cadence for InfluxDB RSSI / door-position points, in milliseconds.      */
#define APP_INFLUX_TELEMETRY_MS     (1000)

/* ── Local timezone (POSIX TZ string) ────────────────────────────────── */
/* Editable from Settings; this is only the factory default.               */
#define APP_DEFAULT_TZ              "EST5EDT,M3.2.0/2,M11.1.0/2"
