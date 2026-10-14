#include "relay.h"
#include "app_config.h"
#include "app_settings.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char *TAG = "relay";

/* GPIO18, not 19. On the ESP32-S3 GPIO19 is USB D-: with the USB Serial/JTAG
 * console enabled (it is, see CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG)
 * driving it as a relay output fights the USB peripheral for the same pad. */
#define RELAY_GPIO (GPIO_NUM_18)

/* Pulse length comes from settings so it can be tuned against the actual
 * opener without a rebuild; see APP_RELAY_PULSE_MS_* for the bounds and why
 * the upper one exists. */
static uint32_t pulse_ms(void)
{
    uint32_t ms = settings_get()->relay_pulse_ms;
    if (ms < APP_RELAY_PULSE_MS_MIN) ms = APP_RELAY_PULSE_MS_MIN;
    if (ms > APP_RELAY_PULSE_MS_MAX) ms = APP_RELAY_PULSE_MS_MAX;
    return ms;
}

static SemaphoreHandle_t s_relay_mutex = NULL;
static uint32_t s_last_activation_ms = 0;
static bool s_has_activated = false;

static esp_err_t relay_release_idle(void)
{
    esp_err_t ret = gpio_set_level(RELAY_GPIO, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    return gpio_set_direction(RELAY_GPIO, GPIO_MODE_INPUT);
}

esp_err_t relay_init(void)
{
    if (s_relay_mutex == NULL) {
        s_relay_mutex = xSemaphoreCreateMutex();
        if (s_relay_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << RELAY_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    s_last_activation_ms = esp_log_timestamp();
    s_has_activated = false;

    return ESP_OK;
}

esp_err_t relay_activate_pulse(void)
{
    if (s_relay_mutex == NULL) {
        esp_err_t init_ret = relay_init();
        if (init_ret != ESP_OK) {
            return init_ret;
        }
    }

    if (xSemaphoreTake(s_relay_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    if (ret == ESP_OK) {
        ret = gpio_set_level(RELAY_GPIO, 0);
    }

    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(pulse_ms()));
        ret = relay_release_idle();
    }
    else {
        relay_release_idle();
    }

    if (ret == ESP_OK) {
        s_last_activation_ms = esp_log_timestamp();
        s_has_activated = true;
        ESP_LOGI(TAG, "Relay pulse triggered (%u ms)", (unsigned)pulse_ms());
    }

    xSemaphoreGive(s_relay_mutex);
    return ret;
}

uint32_t relay_last_activation_ms(void)
{
    return s_last_activation_ms;
}

bool relay_has_activated(void)
{
    return s_has_activated;
}
