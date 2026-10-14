#pragma once

/*
 * notifier.h — Serverless push notifications.
 *
 * The device talks straight to a public push endpoint over HTTPS; there is
 * no broker, no cloud account of ours, and nothing for the user to host.
 *
 *   Telegram — POST to api.telegram.org/bot<token>/sendMessage. Real push to
 *              phone and desktop, free, and the user already has the app.
 *   ntfy     — POST to ntfy.sh/<topic> (or a self-hosted ntfy). No account
 *              at all: pick a hard-to-guess topic, subscribe in the app.
 *
 * Both are enabled independently, so a user can run either, both, or none.
 * Delivery happens on a worker task: a notification never blocks the door
 * logic, and a dead network degrades to a logged failure.
 */

#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOTIFY_INFO = 0,   /* routine: door opened/closed, device booted   */
    NOTIFY_WARN,       /* attention: Wi-Fi recovered, auto-close fired */
    NOTIFY_ALARM,      /* urgent: door left open, immediate alarm      */
} notify_level_t;

/* Starts the delivery task. Call once after settings_init(). */
esp_err_t notifier_init(void);

/* Re-reads the notification settings. Call after the user saves Settings. */
void notifier_reload(void);

/* Queues a notification. Returns ESP_OK if it was accepted for delivery,
 * ESP_ERR_INVALID_STATE if no transport is enabled, or ESP_ERR_TIMEOUT if
 * the queue is full. Never blocks. */
esp_err_t notifier_send(notify_level_t level, const char *title, const char *body);

/* Same, with printf-style formatting for the body. */
esp_err_t notifier_sendf(notify_level_t level, const char *title, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Sends a test message synchronously so the Settings page can report a real
 * result. Blocks for up to ~15 s. `err_out` receives a short human-readable
 * status either way. */
esp_err_t notifier_test(char *err_out, size_t err_len);

/* True when at least one transport is configured and enabled. */
bool notifier_any_enabled(void);

/* Asks Telegram which chats have recently messaged this bot, and renders
 * them as JSON for the Settings page:
 *   {"chats":[{"id":"123456789","name":"John","type":"private"},...]}
 *
 * This exists because picking the chat ID by hand is the single most
 * error-prone step in the whole setup: the bot's own id sits right there in
 * the token, and using it produces a "bot can't send messages to bot" error
 * that explains nothing. Reading it back from a real message the user just
 * sent cannot be confused with anything else.
 *
 * Requires a valid token and at least one recent message to the bot.
 * Returns the number of chats found, or -1 on failure with `err_out` set.
 * Blocks for up to ~10 s. */
int notifier_telegram_chats(char *out, size_t out_len, char *err_out, size_t err_len);

/* Copies the outcome of the most recent delivery attempt into `out`
 * (e.g. "telegram ok", "ntfy HTTP 404"). Empty before the first attempt. */
void notifier_last_result(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
