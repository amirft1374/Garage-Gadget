#include "magnet.h"
#include "app_config.h"
#include "app_settings.h"

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <string.h>

static const char *TAG = "magnet";

/* GPIO6 is ADC1_CH5 on the ESP32-S3.
 *
 * ADC1 specifically: ADC2 is shared with the Wi-Fi radio and its reads fail
 * with ESP_ERR_TIMEOUT whenever Wi-Fi is active, which for this product is
 * always. */
#define MAGNET_ADC_UNIT    ADC_UNIT_1
#define MAGNET_ADC_CHANNEL ADC_CHANNEL_5

/* 12 dB attenuation gives roughly 0–3.1 V of usable range, which is what a
 * 3.3 V sensor needs. Lower attenuations top out below the 1.8 V trip
 * point. */
#define MAGNET_ADC_ATTEN   ADC_ATTEN_DB_12

static adc_oneshot_unit_handle_t s_adc = nullptr;
static adc_cali_handle_t s_cali = nullptr;
static bool s_calibrated = false;

static uint32_t s_last_mv = 0;
static int s_last_raw = 0;
/* Sticky state: with hysteresis the reading alone does not determine the
 * answer, the previous state does too. */
static bool s_closed = true;
static bool s_have_state = false;

esp_err_t magnet_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = MAGNET_ADC_UNIT;
    unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = MAGNET_ADC_ATTEN;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_oneshot_config_channel(s_adc, MAGNET_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Curve fitting is the ESP32-S3's calibration scheme. It needs eFuse
     * calibration data, which is present on essentially all production
     * parts; if it is missing we fall back to a linear estimate, which is
     * good to a few percent — fine for a threshold comparison. */
    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id = MAGNET_ADC_UNIT;
    cali_cfg.chan = MAGNET_ADC_CHANNEL;
    cali_cfg.atten = MAGNET_ADC_ATTEN;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK) {
        s_calibrated = true;
    } else {
        ESP_LOGW(TAG, "No ADC calibration data in eFuse; using an uncalibrated estimate");
        s_calibrated = false;
    }

    /* Seed the sticky state from the real sensor so the first
     * magnet_is_closed() does not invent a transition. */
    s_have_state = false;
    (void)magnet_is_closed();

    ESP_LOGI(TAG, "Door sensor on ADC1 ch%d: %" PRIu32 " mV, %s (threshold %" PRIu32 " mV%s)",
        (int)MAGNET_ADC_CHANNEL,
        s_last_mv,
        s_closed ? "CLOSED" : "OPEN",
        settings_get()->door_threshold_mv,
        s_calibrated ? "" : ", uncalibrated");

    return ESP_OK;
}

/* Median of a small sample set: one spurious reading cannot move the
 * result, unlike a mean. */
static int median_of(int *v, int n)
{
    for (int i = 1; i < n; i++) {
        int key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
    return v[n / 2];
}

bool magnet_is_closed(void)
{
    if (s_adc == nullptr) {
        return s_closed;
    }

    int samples[APP_DOOR_ADC_SAMPLES];
    int n = 0;

    for (int i = 0; i < APP_DOOR_ADC_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, MAGNET_ADC_CHANNEL, &raw) == ESP_OK) {
            samples[n++] = raw;
        }
    }

    if (n == 0) {
        /* Keep reporting the last known state rather than inventing a
         * transition off a failed read. */
        return s_closed;
    }

    int raw = median_of(samples, n);
    s_last_raw = raw;

    int mv = 0;
    if (s_calibrated) {
        if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) {
            mv = (raw * 3100) / 4095;
        }
    } else {
        mv = (raw * 3100) / 4095;
    }

    s_last_mv = (uint32_t)((mv < 0) ? 0 : mv);

    const app_settings_t *cfg = settings_get();

    /* Two decision modes.
     *
     * Calibrated: the installer measured the reading in each state, so
     * "closed" simply means the reading is nearer the closed reference than
     * the open one — which is the midpoint, in whichever direction the
     * sensor happens to move. A sensor that reads lower when shut is handled
     * identically to one that reads higher.
     *
     * Uncalibrated: fall back to the manual threshold, where higher means
     * closed. */
    uint32_t trip = cfg->door_threshold_mv;
    uint32_t hyst = APP_DOOR_HYSTERESIS_MV;
    bool closed_is_higher = true;

    if (cfg->door_cal_valid) {
        closed_is_higher = (cfg->door_closed_mv > cfg->door_open_mv);

        uint32_t hi = closed_is_higher ? cfg->door_closed_mv : cfg->door_open_mv;
        uint32_t lo = closed_is_higher ? cfg->door_open_mv : cfg->door_closed_mv;
        uint32_t gap = hi - lo;

        trip = lo + gap / 2;

        /* Scale the dead-band to the measured separation rather than using a
         * fixed number of millivolts: a sensor that swings 2 V can afford a
         * wide band, one that swings 300 mV cannot. */
        hyst = gap / APP_DOOR_CAL_HYST_DIVISOR;
        if (hyst < APP_DOOR_CAL_HYST_MIN_MV) hyst = APP_DOOR_CAL_HYST_MIN_MV;
        if (hyst > APP_DOOR_CAL_HYST_MAX_MV) hyst = APP_DOOR_CAL_HYST_MAX_MV;
    }

    /* Where the reading sits relative to the trip point, expressed as
     * "how closed-ward is it", so the hysteresis logic below is written once
     * regardless of sensor polarity. */
    int32_t toward_closed = closed_is_higher
                          ? ((int32_t)s_last_mv - (int32_t)trip)
                          : ((int32_t)trip - (int32_t)s_last_mv);

    if (!s_have_state) {
        /* First reading: no previous state to be sticky about. */
        s_closed = (toward_closed >= 0);
        s_have_state = true;
    } else if (s_closed) {
        /* Currently closed — must move clearly open-ward to flip. */
        if (toward_closed < -(int32_t)hyst) {
            s_closed = false;
        }
    } else {
        /* Currently open — must move clearly closed-ward to flip. */
        if (toward_closed > (int32_t)hyst) {
            s_closed = true;
        }
    }

    return s_closed;
}

uint32_t magnet_last_mv(void)
{
    return s_last_mv;
}

int magnet_last_raw(void)
{
    return s_last_raw;
}

bool magnet_is_calibrated(void)
{
    return s_calibrated;
}

void magnet_reset_state(void)
{
    s_have_state = false;
}

uint32_t magnet_measure(uint32_t *out_min, uint32_t *out_max)
{
    if (s_adc == nullptr) {
        if (out_min) *out_min = 0;
        if (out_max) *out_max = 0;
        return 0;
    }

    uint32_t lo = UINT32_MAX, hi = 0;
    uint64_t sum = 0;
    uint32_t n = 0;

    for (int i = 0; i < APP_DOOR_CAL_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, MAGNET_ADC_CHANNEL, &raw) != ESP_OK) {
            continue;
        }

        int mv = 0;
        if (!s_calibrated || adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) {
            mv = (raw * 3100) / 4095;
        }
        if (mv < 0) {
            mv = 0;
        }

        uint32_t v = (uint32_t)mv;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        sum += v;
        n++;

        /* Spread the burst over about a second so the min/max reflects real
         * noise, not one instant. */
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (n == 0) {
        if (out_min) *out_min = 0;
        if (out_max) *out_max = 0;
        return 0;
    }

    if (out_min) *out_min = lo;
    if (out_max) *out_max = hi;
    return (uint32_t)(sum / n);
}
