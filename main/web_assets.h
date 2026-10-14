#pragma once

/*
 * web_assets.h — Static front-end payloads.
 *
 * Kept out of web_server.cpp so the request-handling logic stays readable,
 * and split into separate CSS/JS resources so the browser caches them
 * instead of re-downloading the whole UI on every poll.
 *
 * All three live in flash (.rodata); none of it costs RAM.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Dashboard shell, served at "/" once the device is provisioned. */
extern const char k_index_html[];

/* Stylesheet, served at "/app.css". */
extern const char k_app_css[];

/* Dashboard logic, served at "/app.js". */
extern const char k_app_js[];

/* Self-contained first-time setup wizard, served at "/" while the device is
 * unprovisioned. Inlines its own CSS/JS so a half-configured device never
 * depends on a second request succeeding. */
extern const char k_setup_html[];

/* Shown at "/" while the recovery button has armed a password reset.
 * Self-contained for the same reason as the wizard: a locked-out owner must
 * not depend on a second request succeeding. */
extern const char k_recovery_html[];

#ifdef __cplusplus
}
#endif
