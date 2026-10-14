#pragma once

/*
 * magnet.h — Door position sensor.
 *
 * The sensor pin is read as an analogue voltage rather than a logic level.
 * Reed switches and hall sensors wired through a pull-up or a divider often
 * do not swing all the way to the rails, and a plain digital read either
 * works by luck or does not work at all. Reading millivolts means one
 * firmware covers every wiring variant, the trip point is a setting rather
 * than a property of the hardware, and the installer can see the actual
 * voltage while adjusting it.
 *
 * At or above `door_threshold_mv` (Settings) the door is CLOSED.
 * Below it, OPEN. A hysteresis band around the threshold stops a sensor
 * resting near the trip point from chattering.
 */

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t magnet_init(void);

/* Samples the pin and returns the debounced door state.
 * Call at whatever rate you like; it is cheap and self-contained. */
bool magnet_is_closed(void);

/* Most recent sensor voltage in millivolts, for the UI's calibration
 * readout. Updated by magnet_is_closed(). */
uint32_t magnet_last_mv(void);

/* Most recent raw ADC count (0-4095 at the default 12-bit width), matching
 * the same reading as magnet_last_mv(). Shown next to the voltage in the UI
 * because the raw count is what you compare against a datasheet or a scope. */
int magnet_last_raw(void);

/* True when ADC calibration data was available, so millivolt readings are
 * factory-corrected rather than estimated from the nominal range.
 * This is about the chip's ADC accuracy, not about the door calibration. */
bool magnet_is_calibrated(void);

/* Takes a burst of readings over roughly a second and reports the average
 * plus the min/max seen, for the Calibration panel.
 *
 * The spread matters as much as the average: a sensor that wanders 130 mV
 * while the door is not moving cannot support a tight threshold, and is
 * usually a floating pin. Pass NULL for any output you do not want. */
uint32_t magnet_measure(uint32_t *out_min, uint32_t *out_max);

/* Forces the next magnet_is_closed() to classify purely on the reading,
 * discarding the sticky hysteresis state. Call after the calibration
 * changes, so the new settings take effect immediately rather than being
 * masked by the previous state. */
void magnet_reset_state(void);

#ifdef __cplusplus
}
#endif
