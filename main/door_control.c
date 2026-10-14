#include "door_control.h"
#include "app_settings.h"
#include "history.h"
#include "influx_uploader.h"
#include "led_rgb.h"
#include "notifier.h"
#include "relay.h"
#include "wifi_manager.h"

#include <esp_log.h>

static const char *TAG = "door";

static bool s_ever_triggered = false;

esp_err_t door_control_init(void)
{
    return relay_init();
}

static const char *source_name(door_source_t source)
{
    switch (source) {
    case DOOR_SRC_WEB:  return "web";
    case DOOR_SRC_AUTO: return "auto_close";
    default:            return "other";
    }
}

door_block_t door_control_blocked(void)
{
    /* Setup comes first, because during setup the master switch has never
     * been touched and the door must not move regardless of it.
     *
     * An unprovisioned unit has no calibration, so the sensor reads whatever
     * the floating pin feels like — usually "open". Auto-close is on by
     * default. That combination will drive a real opener while the installer
     * is stood under the door with a screwdriver, which is how this was
     * found. The hotspot being up means the same thing: someone is still
     * configuring it. */
    if (!settings_get()->provisioned || settings_recovery_pending()) {
        return DOOR_BLOCKED_SETUP;
    }

    wifi_mgr_mode_t mode = wifi_manager_mode();
    if (mode == WIFI_MGR_SETUP_AP) {
        return DOOR_BLOCKED_SETUP;
    }

    /* WIFI_MGR_RESCUE is deliberately allowed: that is a configured unit
     * whose router went away, and refusing to shut someone's garage because
     * their Wi-Fi is down would be its own kind of failure. */

    if (!settings_get()->control_enabled) {
        return DOOR_BLOCKED_DISARMED;
    }

    return DOOR_ALLOWED;
}

const char *door_control_block_reason(door_block_t reason)
{
    switch (reason) {
    case DOOR_BLOCKED_DISARMED:
        return "Door control is switched off";
    case DOOR_BLOCKED_SETUP:
        return "Device is still being set up";
    default:
        return "";
    }
}

esp_err_t door_control_trigger(door_source_t source)
{
    door_block_t blocked = door_control_blocked();
    if (blocked != DOOR_ALLOWED) {
        ESP_LOGW(TAG, "Refused door trigger from %s: %s",
            source_name(source), door_control_block_reason(blocked));
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = relay_activate_pulse();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Relay pulse failed (%s): %s", source_name(source), esp_err_to_name(err));
        return err;
    }

    s_ever_triggered = true;
    ESP_LOGI(TAG, "Door triggered by %s", source_name(source));

    history_log(HIST_EV_RELAY, (uint8_t)source);
    influx_uploader_write_event("relay_activated", source_name(source));

    /* Short blue acknowledgement so someone standing at the door sees the
     * command land even if the opener itself is slow to react. */
    led_rgb_flash(0, 0, 90, 250);

    if (source == DOOR_SRC_AUTO) {
        const app_settings_t *cfg = settings_get();
        if (cfg->notify_alarm) {
            notifier_send(NOTIFY_WARN, "Garage auto-closing",
                "The door was left open past the auto-close delay, so it has been triggered to close.");
        }
    }

    return ESP_OK;
}

uint32_t door_control_ms_since_trigger(void)
{
    if (!s_ever_triggered && !relay_has_activated()) {
        return UINT32_MAX;
    }
    return esp_log_timestamp() - relay_last_activation_ms();
}
