#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t relay_init(void);

esp_err_t relay_activate_pulse(void);

uint32_t relay_last_activation_ms(void);

bool relay_has_activated(void);

#ifdef __cplusplus
}
#endif
