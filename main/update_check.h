#pragma once

/*
 * update_check.h — "a newer firmware is available" notice.
 *
 * Asks GitHub, once a day, what the latest published release is, and compares
 * its tag against APP_SW_VERSION. If it is newer the dashboard shows a banner
 * pointing at the release page.
 *
 * It does NOT download or install anything. The device would have no way to
 * tell a genuine release from whatever answered the request, and nothing here
 * verifies a signature, so pulling an executable off the internet and running
 * it would be a bad trade for saving the owner two clicks. The existing OTA
 * page takes a .bin the owner chose; this only tells them one exists.
 *
 * Off-by-default is deliberate too: the check is an outbound call to a third
 * party that reveals the device's IP, and that should be the owner's choice.
 */

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool   checked;         /* a check has completed, successfully or not  */
    bool   newer;           /* a release newer than APP_SW_VERSION exists  */
    char   latest[24];      /* tag as published, e.g. "v2.1.0"             */
    char   status[72];      /* "ok", "no releases yet", "HTTP 403", …      */
    time_t checked_at;      /* 0 until the clock and a check have both run */
} update_info_t;

esp_err_t update_check_init(void);

/* Runs a check if one is due. Cheap to call often; the interval is enforced
 * inside. Call from a task with a few KB of stack — it does TLS. */
void update_check_tick(void);

/* Checks right now regardless of the interval, for the "Check now" button.
 * Blocks for up to a few seconds. */
esp_err_t update_check_now(void);

/* Never NULL. Treat as read-only. */
const update_info_t *update_check_info(void);

/* Compares two "1.2.3" / "v1.2.3" strings. >0 if a is newer than b, 0 if
 * equal, <0 if older. Exposed for testing. */
int update_check_compare(const char *a, const char *b);

#ifdef __cplusplus
}
#endif
