#pragma once

/*
 * door_control.h — The one place the relay is allowed to fire.
 *
 * Both the web UI and the auto-close timer route through here, so every
 * trigger is logged to history, mirrored to InfluxDB (when enabled),
 * acknowledged on the LED and — when the user asked for it — pushed to
 * their phone. Nothing else in the firmware touches relay_activate_pulse().
 */

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DOOR_SRC_WEB  = 0,   /* user pressed the button in the UI      */
    DOOR_SRC_AUTO = 1,   /* auto-close timer                       */
    DOOR_SRC_OTHER = 2,
} door_source_t;

/* Why the relay is not allowed to fire right now, if it is not. */
typedef enum {
    DOOR_ALLOWED = 0,
    DOOR_BLOCKED_DISARMED,   /* the master switch is off              */
    DOOR_BLOCKED_SETUP,      /* still being set up; see below         */
} door_block_t;

/* The single authority on whether the door may be moved. Every caller goes
 * through door_control_trigger(), which consults this first — the check is
 * exposed only so the UI can grey the button out and say why. */
door_block_t door_control_blocked(void);

/* Short human-readable reason, "" when nothing is blocking. */
const char *door_control_block_reason(door_block_t reason);

esp_err_t door_control_init(void);

/* Pulses the relay and records the event everywhere it belongs. */
esp_err_t door_control_trigger(door_source_t source);

/* Milliseconds since the relay last fired (UINT32_MAX if never). */
uint32_t door_control_ms_since_trigger(void);

#ifdef __cplusplus
}
#endif
