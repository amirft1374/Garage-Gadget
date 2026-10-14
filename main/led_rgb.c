#include "led_rgb.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>
#include <driver/rmt_tx.h>

static const char *TAG = "led-rgb";

#define LED_STRIP_GPIO          (GPIO_NUM_48)
#define LED_STRIP_RESOLUTION_HZ (10 * 1000 * 1000)

#define BLINK_FAST_MS   (300)
#define BLINK_SLOW_MS   (1200)
#define BREATHE_MS      (2400)

static led_strip_handle_t s_strip = NULL;
static led_state_t s_state = LED_STATE_BOOTING;
static TickType_t  s_phase_start = 0;
static bool s_suspended = false;   /* true while led_rgb_flash() holds the LED */

static void write_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_strip == NULL) {
        return;
    }
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

/* Returns 0..255 following a triangle wave over `period_ms`. */
static uint8_t breathe_level(uint32_t period_ms)
{
    uint32_t elapsed = (uint32_t)((xTaskGetTickCount() - s_phase_start) * portTICK_PERIOD_MS);
    uint32_t pos = elapsed % period_ms;
    uint32_t half = period_ms / 2;
    uint32_t up = (pos < half) ? pos : (period_ms - pos);
    return (uint8_t)((up * 255u) / half);
}

/* Returns true during the "on" half of a blink of `period_ms`. */
static bool blink_on(uint32_t period_ms)
{
    uint32_t elapsed = (uint32_t)((xTaskGetTickCount() - s_phase_start) * portTICK_PERIOD_MS);
    return ((elapsed / (period_ms / 2)) % 2) == 0;
}

static void apply_state(void)
{
    if (s_strip == NULL || s_suspended) {
        return;
    }

    switch (s_state) {
    case LED_STATE_BOOTING:
        write_pixel(20, 20, 20);
        break;

    case LED_STATE_SETUP: {
        uint8_t lvl = breathe_level(BREATHE_MS);
        write_pixel(0, 0, (uint8_t)(lvl / 3));   /* blue, gentle */
        break;
    }

    case LED_STATE_OFFLINE:
        if (blink_on(BLINK_SLOW_MS)) {
            write_pixel(60, 0, 60);              /* magenta */
        } else {
            write_pixel(0, 0, 0);
        }
        break;

    case LED_STATE_IDLE:
        write_pixel(0, 12, 0);                   /* dim green */
        break;

    case LED_STATE_DOOR_OPEN:
        if (blink_on(BLINK_FAST_MS)) {
            write_pixel(80, 80, 0);              /* amber */
        } else {
            write_pixel(80, 0, 0);               /* red */
        }
        break;

    case LED_STATE_ALARM:
        if (blink_on(BLINK_FAST_MS)) {
            write_pixel(120, 0, 0);
        } else {
            write_pixel(0, 0, 0);
        }
        break;
    }
}

esp_err_t led_rgb_init(void)
{
    if (s_strip != NULL) {
        return ESP_OK;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RESOLUTION_HZ,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LED strip: %s", esp_err_to_name(err));
        return err;
    }

    s_state = LED_STATE_BOOTING;
    s_phase_start = xTaskGetTickCount();
    apply_state();

    return ESP_OK;
}

void led_rgb_set_state(led_state_t state)
{
    if (state != s_state) {
        s_state = state;
        /* Restart the animation phase so a transition is visible immediately
         * rather than mid-blink. */
        s_phase_start = xTaskGetTickCount();
        apply_state();
    }
}

void led_rgb_tick(void)
{
    apply_state();
}

void led_rgb_flash(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms)
{
    if (s_strip == NULL) {
        return;
    }

    s_suspended = true;
    write_pixel(r, g, b);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    s_suspended = false;

    s_phase_start = xTaskGetTickCount();
    apply_state();
}
