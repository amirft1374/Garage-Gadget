#include "update_check.h"
#include "app_config.h"
#include "app_settings.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "update";

/* GitHub's release JSON is a few KB and we want exactly one field out of it.
 * Capturing all of it to run a full JSON parse would cost more RAM than the
 * feature is worth, so we take the first chunk and scan it — "tag_name"
 * appears near the top of the object and is unique within it. */
#define RESP_CAP        2048
#define HTTP_TIMEOUT_MS 8000

static update_info_t s_info = { .status = "not checked yet" };
static int64_t s_next_due_us = 0;
static SemaphoreHandle_t s_lock = NULL;

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} resp_capture_t;

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || evt->user_data == NULL) {
        return ESP_OK;
    }
    resp_capture_t *c = (resp_capture_t *)evt->user_data;
    size_t room = (c->cap > c->len + 1) ? (c->cap - c->len - 1) : 0;
    size_t n = ((size_t)evt->data_len < room) ? (size_t)evt->data_len : room;
    if (n > 0) {
        memcpy(c->buf + c->len, evt->data, n);
        c->len += n;
        c->buf[c->len] = '\0';
    }
    return ESP_OK;
}

/* ── Version comparison ──────────────────────────────────────────────── *
 * Tolerates a leading "v" and a missing patch or minor, so "v2.1" and
 * "2.1.0" compare equal. Anything after the third number — "-rc1", "+build"
 * — is ignored rather than guessed at: a pre-release tag should not be
 * announced as an upgrade, and treating it as equal is the quiet failure. */
static void parse_version(const char *s, int out[3])
{
    out[0] = out[1] = out[2] = 0;
    if (s == NULL) {
        return;
    }
    while (*s == 'v' || *s == 'V' || *s == ' ') {
        s++;
    }
    for (int i = 0; i < 3 && *s; i++) {
        if (*s < '0' || *s > '9') {
            break;
        }
        out[i] = (int)strtol(s, (char **)&s, 10);
        if (*s == '.') {
            s++;
        } else {
            break;
        }
    }
}

int update_check_compare(const char *a, const char *b)
{
    int va[3], vb[3];
    parse_version(a, va);
    parse_version(b, vb);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i]) {
            return (va[i] > vb[i]) ? 1 : -1;
        }
    }
    return 0;
}

/* Pulls "tag_name":"..." out of the response. */
static bool extract_tag(const char *json, char *out, size_t out_len)
{
    const char *p = strstr(json, "\"tag_name\"");
    if (p == NULL) {
        return false;
    }
    p = strchr(p + 10, '"');          /* opening quote of the value */
    if (p == NULL) {
        return false;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

static void set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_info.status, sizeof(s_info.status), fmt, ap);
    va_end(ap);
}

static esp_err_t run_check(void)
{
    char *body = calloc(1, RESP_CAP);
    if (body == NULL) {
        set_status("out of memory");
        return ESP_ERR_NO_MEM;
    }

    resp_capture_t capture = { .buf = body, .cap = RESP_CAP, .len = 0 };

    esp_http_client_config_t cfg = {
        .url               = APP_UPDATE_API_URL,
        .method            = HTTP_METHOD_GET,
        .timeout_ms        = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler     = http_event_cb,
        .user_data         = &capture,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        free(body);
        set_status("client init failed");
        return ESP_FAIL;
    }

    /* GitHub rejects requests without a User-Agent with 403, which is an
     * easy hour to lose if you assume the 403 means rate limiting. */
    esp_http_client_set_header(client, "User-Agent", APP_PRODUCT_NAME "/" APP_SW_VERSION);
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    s_info.checked = true;
    time(&s_info.checked_at);

    if (err != ESP_OK) {
        set_status("%s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Update check failed: %s", esp_err_to_name(err));
        free(body);
        return err;
    }

    if (status == 404) {
        /* The repository exists but has published no release yet. Not an
         * error, and not something to show the owner as a problem. */
        s_info.newer = false;
        s_info.latest[0] = '\0';
        set_status("no releases published yet");
        ESP_LOGI(TAG, "No releases published yet");
        free(body);
        return ESP_OK;
    }

    if (status == 403 || status == 429) {
        set_status("rate limited by GitHub (HTTP %d)", status);
        ESP_LOGW(TAG, "Rate limited (HTTP %d)", status);
        free(body);
        return ESP_FAIL;
    }

    if (status < 200 || status >= 300) {
        set_status("HTTP %d", status);
        ESP_LOGW(TAG, "Update check returned HTTP %d", status);
        free(body);
        return ESP_FAIL;
    }

    char tag[24];
    if (!extract_tag(body, tag, sizeof(tag))) {
        set_status("could not read the release tag");
        ESP_LOGW(TAG, "No tag_name in the response");
        free(body);
        return ESP_FAIL;
    }
    free(body);

    snprintf(s_info.latest, sizeof(s_info.latest), "%s", tag);
    s_info.newer = (update_check_compare(tag, APP_SW_VERSION) > 0);
    set_status("ok");

    if (s_info.newer) {
        ESP_LOGW(TAG, "Firmware %s is available (running %s)", tag, APP_SW_VERSION);
    } else {
        ESP_LOGI(TAG, "Up to date (latest %s, running %s)", tag, APP_SW_VERSION);
    }
    return ESP_OK;
}

esp_err_t update_check_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    /* First check happens on the next tick rather than here: this runs during
     * start-up, and a slow TLS handshake should not sit in front of the door
     * monitor coming up. */
    s_next_due_us = 0;
    return ESP_OK;
}

static esp_err_t locked_check(void)
{
    if (s_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;        /* another check is already running */
    }
    esp_err_t err = run_check();
    xSemaphoreGive(s_lock);
    return err;
}

void update_check_tick(void)
{
    if (!settings_get()->update_check_enabled) {
        return;
    }

    int64_t now = esp_timer_get_time();
    if (now < s_next_due_us) {
        return;
    }

    esp_err_t err = locked_check();

    /* Retry sooner after a failure, but not so much sooner that a device with
     * no route to GitHub hammers it. Both intervals are hours. */
    int64_t wait_h = (err == ESP_OK) ? APP_UPDATE_CHECK_INTERVAL_H
                                     : APP_UPDATE_CHECK_RETRY_H;
    s_next_due_us = esp_timer_get_time() + (wait_h * 3600LL * 1000000LL);
}

esp_err_t update_check_now(void)
{
    esp_err_t err = locked_check();
    s_next_due_us = esp_timer_get_time() + (APP_UPDATE_CHECK_INTERVAL_H * 3600LL * 1000000LL);
    return err;
}

const update_info_t *update_check_info(void)
{
    return &s_info;
}
