#pragma once

/*
 * led_rgb.h — Single on-board SK6812 status LED.
 *
 * One glanceable colour per device state, so a user standing in the garage
 * can tell what is wrong without opening the web UI:
 *
 *   booting   dim white
 *   setup     blue, breathing      — join the setup Wi-Fi and configure me
 *   offline   magenta, slow blink  — configured, but cannot reach Wi-Fi
 *   idle      dim green            — closed, online, nothing to report
 *   open      amber/red, fast blink
 *   alarm     red, fast blink
 */

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STATE_BOOTING = 0,
    LED_STATE_SETUP,
    LED_STATE_OFFLINE,
    LED_STATE_IDLE,
    LED_STATE_DOOR_OPEN,
    LED_STATE_ALARM,
} led_state_t;

esp_err_t led_rgb_init(void);

/* Sets the state to display. Cheap and idempotent — call it every main-loop
 * iteration with whatever the current state is. */
void led_rgb_set_state(led_state_t state);

/* Advances blink/breathe animation. Call from the main loop (~10 Hz). */
void led_rgb_tick(void);

/* Blocks for `duration_ms` showing a solid colour, then restores the
 * animated state. Used to acknowledge a relay pulse. */
void led_rgb_flash(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
