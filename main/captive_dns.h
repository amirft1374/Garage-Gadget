#pragma once

/*
 * captive_dns.h — Tiny DNS responder for the setup hotspot.
 *
 * Answers every A query with the device's own SoftAP address. That is what
 * makes a phone pop the setup page open by itself: Android, iOS and Windows
 * all probe a known URL on joining a network, and when the answer is not the
 * one they expect they show a "Sign in to network" notification and open a
 * browser at whatever the portal returns.
 *
 * Only ever runs while the setup or recovery hotspot is up. Resolving every
 * name to ourselves is exactly right for a provisioning portal and exactly
 * wrong anywhere else, so it is started and stopped with the AP rather than
 * left running.
 */

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the responder on UDP 53, answering with `ap_ip` (dotted quad).
 * Idempotent: a second call while running is a no-op. */
esp_err_t captive_dns_start(const char *ap_ip);

/* Stops it. Safe to call when it is not running. */
void captive_dns_stop(void);

#ifdef __cplusplus
}
#endif
