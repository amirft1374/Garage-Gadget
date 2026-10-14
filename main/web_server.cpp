#include "update_check.h"
#include "web_server.h"
#include "app_config.h"
#include "app_settings.h"
#include "door_control.h"
#include "ha_mqtt.h"
#include "history.h"
#include "influx_uploader.h"
#include "magnet.h"
#include "notifier.h"
#include "web_assets.h"
#include "web_help.h"
#include "wifi_manager.h"

#include <esp_check.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <lwip/sockets.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

static const char *TAG = "web";

/* ── In-memory log ring, mirrored to the Terminal tab ────────────────── */

#define LOG_BUFFER_SIZE (6144)

static char s_log_buffer[LOG_BUFFER_SIZE];
static size_t s_log_len = 0;
static SemaphoreHandle_t s_log_mutex = nullptr;
static vprintf_like_t s_prev_vprintf = nullptr;

/* ── Live state, pushed in by the main loop ──────────────────────────── */

static volatile bool s_door_closed = true;
static volatile uint32_t s_open_seconds = 0;
static volatile bool s_immediate_tripped = false;
static volatile uint32_t s_left_open_periods = 0;
static volatile bool s_server_online = false;
static volatile bool s_server_known = false;
static volatile float s_chip_temp_c = 0.0f;
static volatile bool s_chip_temp_known = false;

static char s_ui_ip[16] = "";
static char s_ui_mdns[64] = "";
static uint16_t s_ui_port = 0;

/* ── Sessions ─────────────────────────────────────────────────────────
 * A bearer token beats the old ?code=<password> scheme: the secret never
 * lands in a URL, so it stays out of server logs, browser history and any
 * Referer header. Tokens are RAM-only and die with a reboot.            */

#define SESSION_MAX 8
#define TOKEN_HEX_LEN 32

typedef struct {
    char    token[TOKEN_HEX_LEN + 1];
    char    ip[46];
    int64_t expires_us;
    bool    used;
} session_t;

static session_t s_sessions[SESSION_MAX];
static SemaphoreHandle_t s_session_mutex = nullptr;

static void make_token(char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < TOKEN_HEX_LEN; i += 8) {
        uint32_t r = esp_random();
        for (int b = 0; b < 8 && (i + b) < TOKEN_HEX_LEN; b++) {
            out[i + b] = hex[(r >> (b * 4)) & 0xF];
        }
    }
    out[TOKEN_HEX_LEN] = '\0';
}

static bool get_client_ip(httpd_req_t *req, char *out_ip, size_t out_len)
{
    int sock = httpd_req_to_sockfd(req);
    if (sock < 0) {
        return false;
    }

    struct sockaddr_storage addr = {};
    socklen_t len = sizeof(addr);
    if (getpeername(sock, (struct sockaddr *)&addr, &len) != 0) {
        return false;
    }

    if (addr.ss_family == AF_INET) {
        auto *s4 = (struct sockaddr_in *)&addr;
        return lwip_inet_ntop(AF_INET, &s4->sin_addr, out_ip, out_len) != nullptr;
    }

    if (addr.ss_family == AF_INET6) {
        auto *s6 = (struct sockaddr_in6 *)&addr;
        const uint8_t *b = s6->sin6_addr.s6_addr;
        /* ESP-IDF's dual-stack server presents IPv4 peers as ::ffff:a.b.c.d */
        bool v4mapped = true;
        for (int i = 0; i < 10; i++) {
            if (b[i] != 0) { v4mapped = false; break; }
        }
        if (v4mapped && b[10] == 0xff && b[11] == 0xff) {
            return lwip_inet_ntop(AF_INET, &b[12], out_ip, out_len) != nullptr;
        }
        return lwip_inet_ntop(AF_INET6, &s6->sin6_addr, out_ip, out_len) != nullptr;
    }

    return false;
}

static void session_create(const char *ip, char *token_out)
{
    make_token(token_out);

    int64_t now = esp_timer_get_time();
    int64_t expiry = now + (int64_t)APP_AUTH_SESSION_MINUTES * 60LL * 1000000LL;

    xSemaphoreTake(s_session_mutex, portMAX_DELAY);

    /* Reuse a free slot, else evict whichever session expires soonest. */
    int slot = -1;
    for (int i = 0; i < SESSION_MAX; i++) {
        if (!s_sessions[i].used || s_sessions[i].expires_us < now) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < SESSION_MAX; i++) {
            if (s_sessions[i].expires_us < s_sessions[slot].expires_us) {
                slot = i;
            }
        }
    }

    strncpy(s_sessions[slot].token, token_out, TOKEN_HEX_LEN);
    s_sessions[slot].token[TOKEN_HEX_LEN] = '\0';
    strncpy(s_sessions[slot].ip, ip ? ip : "", sizeof(s_sessions[slot].ip) - 1);
    s_sessions[slot].ip[sizeof(s_sessions[slot].ip) - 1] = '\0';
    s_sessions[slot].expires_us = expiry;
    s_sessions[slot].used = true;

    xSemaphoreGive(s_session_mutex);
}

static void session_clear_all(void)
{
    xSemaphoreTake(s_session_mutex, portMAX_DELAY);
    memset(s_sessions, 0, sizeof(s_sessions));
    xSemaphoreGive(s_session_mutex);
}

static bool session_valid(const char *token)
{
    if (token == nullptr || token[0] == '\0') {
        return false;
    }

    int64_t now = esp_timer_get_time();
    bool ok = false;

    xSemaphoreTake(s_session_mutex, portMAX_DELAY);
    for (int i = 0; i < SESSION_MAX; i++) {
        if (!s_sessions[i].used) {
            continue;
        }
        if (s_sessions[i].expires_us < now) {
            s_sessions[i].used = false;
            continue;
        }
        if (strcmp(s_sessions[i].token, token) == 0) {
            ok = true;
            break;
        }
    }
    xSemaphoreGive(s_session_mutex);

    return ok;
}

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *body)
{
    if (status != nullptr) {
        httpd_resp_set_status(req, status);
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_401(httpd_req_t *req)
{
    return send_json(req, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
}

/* True when the request carries a valid session token. Falls back to
 * ?code=<admin password> so scripts and curl still work. */
static bool check_auth(httpd_req_t *req)
{
    char token[TOKEN_HEX_LEN + 8] = {};
    if (httpd_req_get_hdr_value_str(req, "X-Auth-Token", token, sizeof(token)) == ESP_OK) {
        if (session_valid(token)) {
            return true;
        }
    }

    /* While a recovery is armed nothing else is reachable: not the stored
     * password, not a live session, not a trusted address. Otherwise a
     * still-valid session could quietly complete the recovery without the
     * physical button ever being involved. */
    if (settings_recovery_pending()) {
        return false;
    }

    char client_ip[TRUSTED_IP_LEN] = {};
    bool have_ip = get_client_ip(req, client_ip, sizeof(client_ip));

    /* A device that has already proved it knows the password is not asked
     * again. The trust is bound to the address, so it dies when the lease
     * moves, when the password changes, or when the owner removes it. */
    if (have_ip && settings_trusted_check(client_ip)) {
        return true;
    }

    char query[192] = {};
    char code[64] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "code", code, sizeof(code)) == ESP_OK &&
            settings_check_password(code)) {
            if (have_ip) {
                settings_trusted_add(client_ip, NULL);
            }
            return true;
        }
    }

    /* Log the path but never the query string — it may hold the password. */
    char path[64] = {};
    const char *q = strchr(req->uri, '?');
    size_t plen = q ? (size_t)(q - req->uri) : strlen(req->uri);
    if (plen >= sizeof(path)) {
        plen = sizeof(path) - 1;
    }
    memcpy(path, req->uri, plen);

    ESP_LOGW(TAG, "Auth denied: %s -> %s", have_ip ? client_ip : "unknown", path);

    return false;
}

#define REQUIRE_AUTH(req) do { if (!check_auth(req)) return send_401(req); } while (0)

/* ── Small helpers ───────────────────────────────────────────────────── */

/* Reads the whole request body into a NUL-terminated heap buffer.
 * Caller frees. Returns nullptr on error or if the body exceeds `max`. */
static char *read_body(httpd_req_t *req, size_t max)
{
    if (req->content_len == 0 || (size_t)req->content_len > max) {
        return nullptr;
    }

    char *buf = (char *)malloc((size_t)req->content_len + 1);
    if (buf == nullptr) {
        return nullptr;
    }

    size_t got = 0;
    while (got < (size_t)req->content_len) {
        int n = httpd_req_recv(req, buf + got, (size_t)req->content_len - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            free(buf);
            return nullptr;
        }
        got += (size_t)n;
    }

    buf[got] = '\0';
    return buf;
}

static bool query_flag(httpd_req_t *req, const char *key, bool fallback)
{
    char query[128] = {};
    char val[8] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return fallback;
    }
    if (httpd_query_key_value(query, key, val, sizeof(val)) != ESP_OK) {
        return fallback;
    }
    return strcmp(val, "0") != 0;
}

static long query_long(httpd_req_t *req, const char *key, long fallback)
{
    char query[128] = {};
    char val[24] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return fallback;
    }
    if (httpd_query_key_value(query, key, val, sizeof(val)) != ESP_OK) {
        return fallback;
    }
    return strtol(val, nullptr, 10);
}

/* Appends `src` to `dst` with JSON escaping. Used for user-supplied strings
 * (device name, Grafana URL) that end up in status responses. */
static void json_escape_into(char *dst, size_t dst_size, const char *src)
{
    size_t p = 0;
    for (; *src && p + 7 < dst_size; src++) {
        if (*src == '"' || *src == '\\') {
            dst[p++] = '\\';
            dst[p++] = *src;
        } else if ((unsigned char)*src < 0x20) {
            p += (size_t)snprintf(dst + p, dst_size - p, "\\u%04x", (unsigned char)*src);
        } else {
            dst[p++] = *src;
        }
    }
    dst[p] = '\0';
}

/* Runs `esp_restart` shortly after the current response has flushed. */
static void deferred_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(800));
    ESP_LOGW(TAG, "Restarting on request");
    esp_restart();
}

static void schedule_restart(void)
{
    xTaskCreate(deferred_restart_task, "restart", 2048, nullptr, 5, nullptr);
}

/* Applies new Wi-Fi credentials after the response has flushed, so the
 * client always sees the acknowledgement before the link drops. */
static void deferred_wifi_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_manager_apply_new_credentials();
    vTaskDelete(nullptr);
}

static void schedule_wifi_reconnect(void)
{
    xTaskCreate(deferred_wifi_task, "wifi-apply", 4096, nullptr, 5, nullptr);
}

/* ── Log capture ─────────────────────────────────────────────────────── */

static void format_timestamp(char *out, size_t out_size)
{
    time_t now = 0;
    struct tm tm_local = {};
    time(&now);
    localtime_r(&now, &tm_local);

    if (tm_local.tm_year < (2024 - 1900)) {
        snprintf(out, out_size, "--:--:--");
        return;
    }
    strftime(out, out_size, "%H:%M:%S", &tm_local);
}

static void log_append(const char *msg, size_t len)
{
    if (len == 0 || s_log_mutex == nullptr) {
        return;
    }

    xSemaphoreTake(s_log_mutex, portMAX_DELAY);

    if (len >= LOG_BUFFER_SIZE) {
        msg += (len - (LOG_BUFFER_SIZE - 1));
        len = LOG_BUFFER_SIZE - 1;
    }

    if (s_log_len + len >= LOG_BUFFER_SIZE) {
        size_t keep = LOG_BUFFER_SIZE - 1 - len;
        memmove(s_log_buffer, s_log_buffer + (s_log_len - keep), keep);
        s_log_len = keep;
    }

    memcpy(s_log_buffer + s_log_len, msg, len);
    s_log_len += len;
    s_log_buffer[s_log_len] = '\0';

    xSemaphoreGive(s_log_mutex);
}

static int log_vprintf(const char *fmt, va_list args)
{
    char line[256];
    char prefix[16];
    char ts[16];

    format_timestamp(ts, sizeof(ts));
    int prefix_len = snprintf(prefix, sizeof(prefix), "[%s] ", ts);

    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);

    if (len > 0) {
        if ((size_t)len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        if (prefix_len > 0) {
            log_append(prefix, (size_t)prefix_len);
        }
        log_append(line, (size_t)len);
    }

    return s_prev_vprintf ? s_prev_vprintf(fmt, args) : vprintf(fmt, args);
}

void web_server_init_logging(void)
{
    if (s_log_mutex == nullptr) {
        s_log_mutex = xSemaphoreCreateMutex();
    }
    if (s_session_mutex == nullptr) {
        s_session_mutex = xSemaphoreCreateMutex();
    }
    s_prev_vprintf = esp_log_set_vprintf(log_vprintf);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — static assets
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Asset versioning ─────────────────────────────────────────────────
 *
 * An ETag alone is not enough. A browser that once cached /app.js under a
 * long max-age does not revalidate at all until that expires, so changing
 * the response headers has no effect on the very clients that need it — the
 * page keeps running old JavaScript against a new API, silently.
 *
 * Versioning the URL fixes it properly: index.html is served no-store, so it
 * is always fresh, and it points at /app.js?v=<id>. When the assets change
 * the id changes, the URL changes, and no cache on earth has an entry for
 * it. The stale copy is simply never consulted again.
 *
 * The id is derived from the asset lengths, so it changes exactly when the
 * assets do — no build-time plumbing, and identical firmware serves an
 * identical id. */
static char s_asset_ver[20] = "0";
static char k_asset_etag[40] = "\"0\"";

static void compute_asset_version(void)
{
    uint32_t h = 2166136261u;                    /* FNV-1a over the sizes */
    size_t lens[2] = { strlen(k_app_css), strlen(k_app_js) };
    for (size_t i = 0; i < 2; i++) {
        for (size_t b = 0; b < sizeof(size_t); b++) {
            h ^= (uint32_t)((lens[i] >> (b * 8)) & 0xFF);
            h *= 16777619u;
        }
    }
    snprintf(s_asset_ver, sizeof(s_asset_ver), "%s-%08" PRIx32, APP_SW_VERSION, h);
    snprintf(k_asset_etag, sizeof(k_asset_etag), "\"%s\"", s_asset_ver);
}

/* Serves a static asset with revalidation rather than a blind max-age.
 *
 * A long max-age meant that after a firmware update the browser kept running
 * the previous UI against the new API — stale hints, missing controls, and no
 * obvious way for the user to know why. With an ETag the browser asks each
 * page load and normally gets a 24-byte 304, but picks up new assets
 * immediately after an update. */

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/html");

    /* An unprovisioned device always lands on the wizard, whichever
     * interface the request arrived on. */
    const char *page = settings_recovery_pending() ? k_recovery_html
                     : (settings_get()->provisioned ? k_index_html : k_setup_html);

    /* Splice the current asset version into every __AV__ marker on the way
     * out, so the page always asks for the assets this firmware ships. */
    const char *p = page;
    const char *m;
    while ((m = strstr(p, "__AV__")) != nullptr) {
        if (m > p) {
            httpd_resp_send_chunk(req, p, m - p);
        }
        httpd_resp_send_chunk(req, s_asset_ver, HTTPD_RESP_USE_STRLEN);
        p = m + 6;
    }
    httpd_resp_send_chunk(req, p, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, nullptr, 0);
}

static esp_err_t send_asset(httpd_req_t *req, const char *type, const char *body)
{
    char inm[64] = {};
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm, sizeof(inm)) == ESP_OK &&
        strcmp(inm, k_asset_etag) == 0) {
        httpd_resp_set_status(req, "304 Not Modified");
        httpd_resp_set_hdr(req, "ETag", k_asset_etag);
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        return httpd_resp_send(req, nullptr, 0);
    }

    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "ETag", k_asset_etag);
    /* Safe to cache hard: the URL carries the version, so a new build is a
     * new URL rather than a stale hit. */
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=604800, immutable");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t css_handler(httpd_req_t *req)
{
    return send_asset(req, "text/css", k_app_css);
}

static esp_err_t js_handler(httpd_req_t *req)
{
    return send_asset(req, "application/javascript", k_app_js);
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, nullptr, 0);
}

/* Browsers pre-flight with HEAD; answering avoids a 405 and the socket
 * churn that follows it. */
static esp_err_t head_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, nullptr, 0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — public status
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Help ─────────────────────────────────────────────────────────────
 * Served without authentication on purpose: the recovery instructions are
 * needed exactly when someone cannot sign in, and none of this content is
 * specific to the unit. Topics are fetched one at a time so a 38 KB manual
 * never has to be pushed through the socket in one response. */

static esp_err_t help_index_handler(httpd_req_t *req)
{
    const size_t cap = 2048;
    char *buf = (char *)malloc(cap);
    if (buf == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    int pos = snprintf(buf, cap, "{\"topics\":[");
    for (int i = 0; i < k_help_topic_count; i++) {
        int n = snprintf(buf + pos, cap - (size_t)pos,
            "%s{\"i\":%d,\"c\":\"%s\",\"t\":\"%s\"}",
            (i == 0) ? "" : ",", i,
            k_help_topics[i].category, k_help_topics[i].title);
        if (n < 0 || (size_t)n >= cap - (size_t)pos) {
            break;
        }
        pos += n;
    }
    snprintf(buf + pos, cap - (size_t)pos, "],\"n\":%d}", k_help_topic_count);

    /* Revalidate rather than cache blindly: a stale help page cannot be
     * corrected by the device, and this content changes with the firmware. */
    httpd_resp_set_hdr(req, "ETag", k_asset_etag);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return ret;
}

static esp_err_t help_page_handler(httpd_req_t *req)
{
    long id = query_long(req, "i", -1);
    if (id < 0 || id >= k_help_topic_count) {
        return send_json(req, "404 Not Found", "{\"error\":\"no such topic\"}");
    }

    char inm[64] = {};
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm, sizeof(inm)) == ESP_OK &&
        strcmp(inm, k_asset_etag) == 0) {
        httpd_resp_set_status(req, "304 Not Modified");
        httpd_resp_set_hdr(req, "ETag", k_asset_etag);
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        return httpd_resp_send(req, nullptr, 0);
    }

    httpd_resp_set_hdr(req, "ETag", k_asset_etag);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, k_help_topics[id].html, HTTPD_RESP_USE_STRLEN);
}

/* The whole manual as one Markdown file.
 *
 * Content-Disposition makes the browser save it rather than render it, and
 * the device name is in the filename so several units do not all produce
 * "help.md" in the same downloads folder. */
static esp_err_t help_markdown_handler(httpd_req_t *req)
{
    char disp[96];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s-help.md\"",
        settings_get()->device_name);

    httpd_resp_set_type(req, "text/markdown; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition", disp);
    httpd_resp_set_hdr(req, "ETag", k_asset_etag);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, k_help_markdown, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    const app_settings_t *cfg = settings_get();

    char who[TRUSTED_IP_LEN] = {};
    bool client_trusted = get_client_ip(req, who, sizeof(who))
                       && settings_trusted_check(who);

    char notify_last[224] = "";
    notifier_last_result(notify_last, sizeof(notify_last));

    char mqtt_stat[96] = "";
    ha_mqtt_status(mqtt_stat, sizeof(mqtt_stat));
    char safe_mqtt[200];
    json_escape_into(safe_mqtt, sizeof(safe_mqtt), mqtt_stat);

    char safe_name[80];
    char safe_grafana[340];
    char safe_notify[460];
    json_escape_into(safe_name, sizeof(safe_name), cfg->device_name);
    json_escape_into(safe_grafana, sizeof(safe_grafana), cfg->grafana_url);
    json_escape_into(safe_notify, sizeof(safe_notify), notify_last);

    /* Sized for the worst case: a 160-char Grafana URL and a 96-char notifier
     * status, both of which can grow ~2x under JSON escaping. snprintf would
     * truncate rather than overflow, but truncated JSON breaks the UI. */
    char body[1600];
    int len = snprintf(body, sizeof(body),
        "{\"doorClosed\":%s,\"openSeconds\":%" PRIu32 ","
        "\"autoClose\":%s,\"autoCloseLocked\":%s,\"autoCloseDelayMin\":%" PRIu32 ","
        "\"alarmImmediate\":%s,\"alarmLeftOpen\":%s,"
        "\"immediateTripped\":%s,\"leftOpenPeriods\":%" PRIu32 ","
        "\"serverOnline\":%s,\"serverKnown\":%s,\"monitorEnabled\":%s,"
        "\"tempC\":%.1f,\"tempKnown\":%s,"
        "\"wifiConnected\":%s,\"rssi\":%d,\"uptimeSec\":%lld,"
        "\"influxActive\":%s,\"notifyEnabled\":%s,\"notifyLast\":\"%s\","
        "\"doorMv\":%" PRIu32 ",\"doorRaw\":%d,"
        "\"doorThresholdMv\":%" PRIu32 ",\"doorCalibrated\":%s,"
        "\"doorClosedMv\":%" PRIu32 ",\"doorOpenMv\":%" PRIu32 ",\"doorCalValid\":%s,"
        "\"trusted\":%s,\"recovery\":%s,\"mqtt\":\"%s\","
        "\"controlEnabled\":%s,\"controlBlocked\":\"%s\","
        "\"deviceName\":\"%s\",\"grafanaUrl\":\"%s\",\"version\":\"%s\"}",
        s_door_closed ? "true" : "false",
        s_open_seconds,
        (cfg->auto_close_enabled && !cfg->auto_close_locked_off) ? "true" : "false",
        cfg->auto_close_locked_off ? "true" : "false",
        cfg->auto_close_delay_min,
        cfg->alarm_immediate ? "true" : "false",
        cfg->alarm_left_open ? "true" : "false",
        s_immediate_tripped ? "true" : "false",
        s_left_open_periods,
        s_server_online ? "true" : "false",
        s_server_known ? "true" : "false",
        (cfg->monitor_host[0] != '\0') ? "true" : "false",
        (double)s_chip_temp_c,
        s_chip_temp_known ? "true" : "false",
        wifi_manager_is_connected() ? "true" : "false",
        wifi_manager_rssi(),
        (long long)(esp_timer_get_time() / 1000000LL),
        influx_uploader_is_active() ? "true" : "false",
        notifier_any_enabled() ? "true" : "false",
        safe_notify,
        magnet_last_mv(),
        magnet_last_raw(),
        cfg->door_threshold_mv,
        magnet_is_calibrated() ? "true" : "false",
        cfg->door_closed_mv,
        cfg->door_open_mv,
        cfg->door_cal_valid ? "true" : "false",
        client_trusted ? "true" : "false",
        settings_recovery_pending() ? "true" : "false",
        safe_mqtt,
        cfg->control_enabled ? "true" : "false",
        door_control_block_reason(door_control_blocked()),
        safe_name,
        safe_grafana,
        APP_SW_VERSION);

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, len);
}

static esp_err_t network_handler(httpd_req_t *req)
{
    char safe_mdns[140];
    json_escape_into(safe_mdns, sizeof(safe_mdns), s_ui_mdns);

    const char *mode = "offline";
    switch (wifi_manager_mode()) {
    case WIFI_MGR_STATION:  mode = "station"; break;
    case WIFI_MGR_SETUP_AP: mode = "setup";   break;
    case WIFI_MGR_RESCUE:   mode = "rescue";  break;
    default: break;
    }

    char body[420];
    int len = snprintf(body, sizeof(body),
        "{\"ip\":\"%s\",\"mdns\":\"%s\",\"port\":%u,"
        "\"mode\":\"%s\",\"apIp\":\"%s\",\"apSsid\":\"%s\","
        "\"staticIp\":\"%s\",\"staticOn\":%s,\"staticRevert\":%s}",
        s_ui_ip, safe_mdns, (unsigned)s_ui_port,
        mode, wifi_manager_ap_ip(), wifi_manager_ap_ssid(),
        settings_get()->static_ip,
        wifi_manager_static_active() ? "true" : "false",
        wifi_manager_static_reverted() ? "true" : "false");

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, len);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — authentication
 * ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t login_handler(httpd_req_t *req)
{
    char *body = read_body(req, 512);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    cJSON *pw = root ? cJSON_GetObjectItemCaseSensitive(root, "password") : nullptr;
    bool ok = cJSON_IsString(pw) && settings_check_password(pw->valuestring);
    cJSON_Delete(root);

    char ip[46] = "";
    get_client_ip(req, ip, sizeof(ip));

    if (!ok) {
        ESP_LOGW(TAG, "Failed login from %s", ip[0] ? ip : "unknown");
        /* Slow brute-force attempts without blocking the whole server. */
        vTaskDelay(pdMS_TO_TICKS(750));
        return send_401(req);
    }

    char token[TOKEN_HEX_LEN + 1];
    session_create(ip, token);

    /* First successful sign-in from this address makes it trusted, which is
     * the whole point: the household's own phones stop being challenged. */
    if (ip[0] != '\0') {
        settings_trusted_add(ip, NULL);
    }
    ESP_LOGI(TAG, "Login OK from %s (now trusted)", ip[0] ? ip : "unknown");

    char resp[96];
    snprintf(resp, sizeof(resp), "{\"token\":\"%s\",\"ttlMin\":%d}", token, APP_AUTH_SESSION_MINUTES);
    return send_json(req, nullptr, resp);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — door and behaviour
 * ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t door_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    door_block_t blocked = door_control_blocked();
    if (blocked != DOOR_ALLOWED) {
        /* 409, not 500: nothing failed. The device is working exactly as
         * configured and the answer is a reason, not an error. */
        char body[160];
        snprintf(body, sizeof(body), "{\"triggered\":false,\"reason\":\"%s\"}",
            door_control_block_reason(blocked));
        return send_json(req, "409 Conflict", body);
    }

    if (door_control_trigger(DOOR_SRC_WEB) != ESP_OK) {
        return send_json(req, "500 Internal Server Error",
            "{\"triggered\":false,\"reason\":\"relay did not fire\"}");
    }
    return send_json(req, nullptr, "{\"triggered\":true}");
}

/* The update notice and the project's home.
 *
 * Unauthenticated on purpose: it is the same information printed on the
 * dashboard header that anyone on the LAN can already read, it reveals
 * nothing about the installation, and the banner has to render before the
 * owner has signed in. The check itself is a privileged action — see below. */
static esp_err_t update_info_handler(httpd_req_t *req)
{
    const update_info_t *u = update_check_info();

    char safe_latest[48], safe_status[160];
    json_escape_into(safe_latest, sizeof(safe_latest), u->latest);
    json_escape_into(safe_status, sizeof(safe_status), u->status);

    char body[520];
    int len = snprintf(body, sizeof(body),
        "{\"enabled\":%s,\"checked\":%s,\"newer\":%s,"
        "\"current\":\"%s\",\"latest\":\"%s\",\"status\":\"%s\","
        "\"checkedAt\":%lld,\"repoUrl\":\"%s\",\"releaseUrl\":\"%s\"}",
        settings_get()->update_check_enabled ? "true" : "false",
        u->checked ? "true" : "false",
        u->newer ? "true" : "false",
        APP_SW_VERSION, safe_latest, safe_status,
        (long long)u->checked_at,
        APP_REPO_URL, APP_RELEASES_URL);

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, len);
}

/* Forces a check now. Authenticated, because it makes the device reach out to
 * a third party on demand and an unauthenticated version would let anyone on
 * the LAN burn the owner's GitHub rate limit. */
static esp_err_t update_check_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    if (!settings_get()->update_check_enabled) {
        return send_json(req, "409 Conflict",
            "{\"ok\":false,\"status\":\"update checks are switched off\"}");
    }

    update_check_now();
    const update_info_t *u = update_check_info();

    char safe_status[160];
    json_escape_into(safe_status, sizeof(safe_status), u->status);

    char body[300];
    snprintf(body, sizeof(body),
        "{\"ok\":%s,\"newer\":%s,\"latest\":\"%s\",\"status\":\"%s\"}",
        (strcmp(u->status, "ok") == 0) ? "true" : "false",
        u->newer ? "true" : "false", u->latest, safe_status);
    return send_json(req, nullptr, body);
}

/* Arms or disarms the relay. Its own endpoint rather than a settings field:
 * this decides whether a garage door can move, and changing it should be a
 * deliberate action, not something that rides along with a form save. */
static esp_err_t control_enable_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    bool enabled = query_flag(req, "enabled", false);
    settings_set_control_enabled(enabled);

    history_log(HIST_EV_SETTINGS, enabled ? 1 : 0);
    influx_uploader_write_event("door_control", enabled ? "armed" : "disarmed");
    ha_mqtt_publish_config_state();

    ESP_LOGW(TAG, "Door control %s from the web UI", enabled ? "ARMED" : "DISARMED");

    char body[160];
    snprintf(body, sizeof(body), "{\"controlEnabled\":%s,\"controlBlocked\":\"%s\"}",
        enabled ? "true" : "false",
        door_control_block_reason(door_control_blocked()));
    return send_json(req, nullptr, body);
}

static esp_err_t auto_close_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    const app_settings_t *cfg = settings_get();
    if (cfg->auto_close_locked_off) {
        return send_json(req, "409 Conflict", "{\"error\":\"locked off\"}");
    }

    bool enabled = query_flag(req, "enabled", true);
    if (enabled != cfg->auto_close_enabled) {
        settings_set_auto_close(enabled);
        history_log(HIST_EV_AUTO_CLOSE, enabled ? 1 : 0);
        influx_uploader_write_event("auto_close_flipped", enabled ? "enabled" : "disabled");
    }

    ha_mqtt_publish_config_state();
    return send_json(req, nullptr,
        settings_get()->auto_close_enabled ? "{\"autoClose\":true}" : "{\"autoClose\":false}");
}

static esp_err_t auto_close_lock_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    bool locked = query_flag(req, "enabled", false);
    if (locked != settings_get()->auto_close_locked_off) {
        settings_set_auto_close_locked(locked);
        history_log(HIST_EV_AUTO_CLOSE, locked ? 0 : 1);
        influx_uploader_write_event("auto_close_locked", locked ? "on" : "off");
        ESP_LOGW(TAG, "Automatic close %s", locked ? "LOCKED OFF" : "unlocked");
    }

    ha_mqtt_publish_config_state();
    return send_json(req, nullptr,
        locked ? "{\"autoCloseLocked\":true}" : "{\"autoCloseLocked\":false}");
}

static esp_err_t auto_close_delay_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    long minutes = query_long(req, "minutes", 0);
    if (settings_set_auto_close_delay((uint32_t)minutes) != ESP_OK) {
        return send_json(req, "400 Bad Request", "{\"error\":\"minutes must be 1-120\"}");
    }

    ESP_LOGI(TAG, "Auto-close delay set to %ld min", minutes);

    ha_mqtt_publish_config_state();

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"autoCloseDelayMin\":%" PRIu32 "}",
        settings_get()->auto_close_delay_min);
    return send_json(req, nullptr, resp);
}

/* ── On-demand alarm state report ─────────────────────────────────────
 *
 * The shipped Grafana rules read the *last* `system_event` point for each
 * alarm and stay firing until a zero arrives. The device normally sends
 * that zero when the door closes — but only while the alarm is still
 * armed, because the clearing branch in main.cpp reads:
 *
 *     if (cfg->alarm_immediate && st->immediate_fired) { ...write "0"... }
 *
 * Disarm an alarm while it is firing and that branch never runs again. No
 * clearing point is ever sent, and the server alert hangs firing with
 * nothing on the device able to resolve it — the operator is left editing
 * the database or silencing the rule by hand.
 *
 * This republishes the current truth for every alarm on demand. Crucially
 * it sends an explicit zero for an alarm that is disarmed: a disarmed
 * alarm cannot be firing, and being able to say so is the whole point. */
static esp_err_t alarm_report_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    const app_settings_t *cfg = settings_get();

    /* Armed *and* tripped is the only state that is genuinely firing.
     * Everything else — disarmed, or armed but quiet — reports zero. */
    bool     immediate_firing = cfg->alarm_immediate && s_immediate_tripped;
    uint32_t left_open_period = cfg->alarm_left_open ? s_left_open_periods : 0;

    char left_open_val[12];
    snprintf(left_open_val, sizeof(left_open_val), "%" PRIu32, left_open_period);

    /* Event names keep their original spelling, historical typo included,
     * so rules written against the old firmware keep matching. */
    esp_err_t e_imm = influx_uploader_write_event("ImmidiateAlarm",
        immediate_firing ? "1" : "0");
    esp_err_t e_lo  = influx_uploader_write_event("LeftOpenAlarm", left_open_val);

    ESP_LOGI(TAG, "Alarm state report: ImmidiateAlarm=%s LeftOpenAlarm=%s",
        immediate_firing ? "1" : "0", left_open_val);

    /* An event write is only queued, so there is no delivery result to
     * report yet; what we can state honestly is whether it was accepted and
     * whether Influx is even switched on. Saying "sent" with InfluxDB
     * disabled would be a lie the user acts on. */
    bool queued = (e_imm == ESP_OK) && (e_lo == ESP_OK);

    const char *detail;
    if (!cfg->influx_enabled) {
        detail = "InfluxDB is turned off — nothing was sent";
    } else if (!queued) {
        detail = "Upload queue is full — try again in a moment";
    } else {
        detail = "Queued for InfluxDB";
    }

    char resp[220];
    snprintf(resp, sizeof(resp),
        "{\"ok\":%s,\"detail\":\"%s\",\"immediate\":%s,\"leftOpen\":%" PRIu32 "}",
        (cfg->influx_enabled && queued) ? "true" : "false",
        detail,
        immediate_firing ? "1" : "0",
        left_open_period);

    return send_json(req, nullptr, resp);
}

static esp_err_t alarm_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char query[128] = {};
    char which[16] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "alarm", which, sizeof(which));
    }

    bool engaged = query_flag(req, "engaged", false);

    if (strcmp(which, "immediate") == 0) {
        settings_set_alarm_immediate(engaged);
        ESP_LOGI(TAG, "Immediate alarm %s", engaged ? "engaged" : "disengaged");
    } else if (strcmp(which, "left_open") == 0) {
        settings_set_alarm_left_open(engaged);
        ESP_LOGI(TAG, "Left-open alarm %s", engaged ? "engaged" : "disengaged");
    } else {
        return send_json(req, "400 Bad Request", "{\"error\":\"unknown alarm\"}");
    }

    ha_mqtt_publish_config_state();

    const app_settings_t *cfg = settings_get();
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"alarmImmediate\":%s,\"alarmLeftOpen\":%s}",
        cfg->alarm_immediate ? "true" : "false",
        cfg->alarm_left_open ? "true" : "false");
    return send_json(req, nullptr, resp);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — history
 * ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t history_events_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    long limit = query_long(req, "limit", 200);
    long since = query_long(req, "since", 0);
    if (limit < 1 || limit > 400) {
        limit = 200;
    }

    const size_t cap = 16384;
    char *buf = (char *)malloc(cap);
    if (buf == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    int len = history_events_json(buf, cap, (int)limit, (int64_t)since);
    esp_err_t ret;
    if (len < 0) {
        ret = send_json(req, "500 Internal Server Error", "{\"error\":\"encode failed\"}");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, buf, len);
    }

    free(buf);
    return ret;
}

static esp_err_t history_samples_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    long points = query_long(req, "points", 360);
    long since = query_long(req, "since", 0);
    if (points < 10 || points > 600) {
        points = 360;
    }

    const size_t cap = 24576;
    char *buf = (char *)malloc(cap);
    if (buf == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    int len = history_samples_json(buf, cap, (int)points, (int64_t)since);
    esp_err_t ret;
    if (len < 0) {
        ret = send_json(req, "500 Internal Server Error", "{\"error\":\"encode failed\"}");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, buf, len);
    }

    free(buf);
    return ret;
}

static esp_err_t history_stats_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    hist_stats_t st;
    history_stats(&st);

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"eventCount\":%" PRIu32 ",\"eventCapacity\":%" PRIu32 ","
        "\"eventOldest\":%lld,\"eventNewest\":%lld,"
        "\"sampleCount\":%" PRIu32 ",\"sampleCapacity\":%" PRIu32 ",\"samplesInPsram\":%s}",
        st.event_count, st.event_capacity,
        (long long)st.event_oldest_ts, (long long)st.event_newest_ts,
        st.sample_count, st.sample_capacity,
        st.samples_in_psram ? "true" : "false");

    return send_json(req, nullptr, resp);
}

static esp_err_t history_clear_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    history_clear();
    ESP_LOGW(TAG, "On-device history cleared by user");
    return send_json(req, nullptr, "{\"cleared\":true}");
}

static esp_err_t logs_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    if (s_log_mutex == nullptr) {
        return httpd_resp_send(req, "", 0);
    }

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/plain");

    xSemaphoreTake(s_log_mutex, portMAX_DELAY);
    esp_err_t ret = httpd_resp_send(req, s_log_buffer, HTTPD_RESP_USE_STRLEN);
    xSemaphoreGive(s_log_mutex);

    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — settings
 * ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    const size_t cap = 3072;
    char *buf = (char *)malloc(cap);
    if (buf == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    int len = settings_to_json(buf, cap);
    esp_err_t ret;
    if (len < 0) {
        ret = send_json(req, "500 Internal Server Error", "{\"error\":\"encode failed\"}");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, buf, len);
    }

    free(buf);
    return ret;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char *body = read_body(req, 4096);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    int changed = settings_apply_json(body, strlen(body));
    free(body);

    if (changed < 0) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    settings_save();
    history_log(HIST_EV_SETTINGS, 1);

    /* Push the new configuration into the subsystems that cache it. */
    influx_uploader_reload();
    notifier_reload();
    ha_mqtt_reload();

    ESP_LOGI(TAG, "Settings updated (%d field(s))", changed);

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"saved\":true,\"changed\":%d}", changed);
    return send_json(req, nullptr, resp);
}

/* Wi-Fi is its own endpoint because saving it has a side effect the generic
 * settings save must not have: dropping the connection the caller is on. */
static esp_err_t wifi_post_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char *body = read_body(req, 1024);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    int changed = settings_apply_json(body, strlen(body));
    free(body);

    if (changed < 0) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }
    if (!settings_have_wifi()) {
        return send_json(req, "400 Bad Request", "{\"error\":\"ssid required\"}");
    }

    settings_save();
    history_log(HIST_EV_SETTINGS, 1);
    influx_uploader_reload();   /* the SSID is an Influx tag */

    ESP_LOGW(TAG, "Wi-Fi credentials changed; reconnecting to '%s'", settings_get()->wifi_ssid);

    esp_err_t ret = send_json(req, nullptr, "{\"saved\":true,\"reconnecting\":true}");
    schedule_wifi_reconnect();
    return ret;
}

static esp_err_t password_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char *body = read_body(req, 512);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (root == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    cJSON *cur = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *next = cJSON_GetObjectItemCaseSensitive(root, "next");

    if (!cJSON_IsString(cur) || !settings_check_password(cur->valuestring)) {
        cJSON_Delete(root);
        vTaskDelay(pdMS_TO_TICKS(750));
        return send_json(req, "403 Forbidden", "{\"error\":\"wrong password\"}");
    }

    if (!cJSON_IsString(next) || settings_set_password(next->valuestring) != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "400 Bad Request", "{\"error\":\"password must be 4-32 characters\"}");
    }

    cJSON_Delete(root);

    /* Every existing session was issued against the old password, and so was
     * every entry on the trusted list. Changing the password has to mean
     * something: both are discarded. */
    session_clear_all();
    settings_trusted_clear();
    history_log(HIST_EV_SETTINGS, 1);
    ESP_LOGW(TAG, "Admin password changed; all sessions invalidated");

    return send_json(req, nullptr, "{\"changed\":true}");
}

static esp_err_t notify_test_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char detail[224] = "";
    esp_err_t err = notifier_test(detail, sizeof(detail));

    char safe[460];
    json_escape_into(safe, sizeof(safe), detail);

    char resp[540];
    snprintf(resp, sizeof(resp), "{\"ok\":%s,\"detail\":\"%s\"}",
        (err == ESP_OK) ? "true" : "false", safe);
    return send_json(req, nullptr, resp);
}

/* Two-point door-sensor calibration.
 *
 *   POST /api/calibrate?point=closed  measure and store the shut reading
 *   POST /api/calibrate?point=open    measure and store the open reading
 *   POST /api/calibrate?point=clear   forget the calibration
 *
 * Measuring beats asking the installer to type a number: the value that
 * matters is what this board reads on this wiring, and the burst also
 * reports how much the reading wanders, which is the fastest way to spot a
 * sensor that is not actually connected. */
static esp_err_t calibrate_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char query[64] = {};
    char point[16] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "point", point, sizeof(point));
    }

    const app_settings_t *cfg = settings_get();
    uint32_t closed_mv = cfg->door_closed_mv;
    uint32_t open_mv   = cfg->door_open_mv;

    uint32_t measured = 0, lo = 0, hi = 0;

    if (strcmp(point, "clear") == 0) {
        settings_set_calibration(0, 0);
        magnet_reset_state();
        ESP_LOGW(TAG, "Door calibration cleared by user");
    } else if (strcmp(point, "closed") == 0 || strcmp(point, "open") == 0) {
        /* Takes about a second — the burst is deliberately spread out. */
        measured = magnet_measure(&lo, &hi);
        if (measured == 0) {
            return send_json(req, "500 Internal Server Error",
                "{\"error\":\"sensor read failed\"}");
        }
        if (strcmp(point, "closed") == 0) {
            closed_mv = measured;
        } else {
            open_mv = measured;
        }
        settings_set_calibration(closed_mv, open_mv);
        magnet_reset_state();
        ESP_LOGI(TAG, "Calibration point '%s' captured: %" PRIu32 " mV (spread %" PRIu32 " mV)",
            point, measured, hi - lo);
    } else {
        return send_json(req, "400 Bad Request",
            "{\"error\":\"point must be closed, open or clear\"}");
    }

    cfg = settings_get();

    uint32_t gap = 0;
    if (cfg->door_closed_mv > 0 && cfg->door_open_mv > 0) {
        gap = (cfg->door_closed_mv > cfg->door_open_mv)
            ? (cfg->door_closed_mv - cfg->door_open_mv)
            : (cfg->door_open_mv - cfg->door_closed_mv);
    }

    char resp[384];
    snprintf(resp, sizeof(resp),
        "{\"measured\":%" PRIu32 ",\"spread\":%" PRIu32 ","
        "\"closedMv\":%" PRIu32 ",\"openMv\":%" PRIu32 ","
        "\"gap\":%" PRIu32 ",\"thresholdMv\":%" PRIu32 ",\"valid\":%s,"
        "\"minGap\":%d}",
        measured, (measured > 0) ? (hi - lo) : 0,
        cfg->door_closed_mv, cfg->door_open_mv,
        gap, cfg->door_threshold_mv,
        cfg->door_cal_valid ? "true" : "false",
        APP_DOOR_CAL_MIN_SEPARATION_MV);

    return send_json(req, nullptr, resp);
}

/* Lists the chats that have recently messaged this bot, so the user can pick
 * their chat ID instead of guessing it. */
static esp_err_t telegram_chats_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    const size_t cap = 1024;
    char *list = (char *)malloc(cap);
    if (list == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    char err[224] = "";
    int n = notifier_telegram_chats(list, cap, err, sizeof(err));

    esp_err_t ret;
    if (n < 0) {
        char safe[460];
        json_escape_into(safe, sizeof(safe), err);
        char resp[540];
        snprintf(resp, sizeof(resp), "{\"chats\":[],\"n\":0,\"error\":\"%s\"}", safe);
        ret = send_json(req, nullptr, resp);
    } else if (n == 0) {
        char safe[460];
        json_escape_into(safe, sizeof(safe), err);
        char resp[540];
        snprintf(resp, sizeof(resp), "{\"chats\":[],\"n\":0,\"hint\":\"%s\"}", safe);
        ret = send_json(req, nullptr, resp);
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, list, HTTPD_RESP_USE_STRLEN);
    }

    free(list);
    return ret;
}

/* ── Trusted devices ─────────────────────────────────────────────────
 *
 *   GET  /api/trusted                     list, plus which one is you
 *   POST /api/trusted?action=add&ip=..&label=..
 *   POST /api/trusted?action=remove&ip=..
 *   POST /api/trusted?action=clear
 */
static esp_err_t trusted_list_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char me[TRUSTED_IP_LEN] = {};
    get_client_ip(req, me, sizeof(me));

    char body[900];
    int pos = snprintf(body, sizeof(body), "{\"you\":\"%s\",\"max\":%d,\"devices\":[", me, TRUSTED_MAX);

    int n = settings_trusted_count();
    int emitted = 0;
    for (int i = 0; i < n; i++) {
        char ip[TRUSTED_IP_LEN] = {};
        char label[TRUSTED_LABEL_LEN] = {};
        if (!settings_trusted_get(i, ip, sizeof(ip), label, sizeof(label))) {
            continue;
        }
        char safe_ip[TRUSTED_IP_LEN * 2];
        char safe_label[TRUSTED_LABEL_LEN * 2];
        json_escape_into(safe_ip, sizeof(safe_ip), ip);
        json_escape_into(safe_label, sizeof(safe_label), label);

        int w = snprintf(body + pos, sizeof(body) - (size_t)pos,
            "%s{\"ip\":\"%s\",\"label\":\"%s\",\"self\":%s}",
            (emitted == 0) ? "" : ",", safe_ip, safe_label,
            (strcmp(ip, me) == 0) ? "true" : "false");
        if (w < 0 || (size_t)w >= sizeof(body) - (size_t)pos) {
            break;
        }
        pos += w;
        emitted++;
    }

    snprintf(body + pos, sizeof(body) - (size_t)pos, "],\"n\":%d}", emitted);
    return send_json(req, nullptr, body);
}

static esp_err_t trusted_edit_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char query[256] = {};
    char action[16] = {};
    char ip[TRUSTED_IP_LEN] = {};
    char label[TRUSTED_LABEL_LEN] = {};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "action", action, sizeof(action));
        httpd_query_key_value(query, "ip", ip, sizeof(ip));
        httpd_query_key_value(query, "label", label, sizeof(label));
    }

    esp_err_t err;
    if (strcmp(action, "clear") == 0) {
        err = settings_trusted_clear();
    } else if (strcmp(action, "remove") == 0) {
        err = settings_trusted_remove(ip);
        if (err == ESP_ERR_NOT_FOUND) {
            return send_json(req, "404 Not Found", "{\"error\":\"not on the list\"}");
        }
    } else if (strcmp(action, "add") == 0) {
        err = settings_trusted_add(ip, label[0] ? label : nullptr);
        if (err == ESP_ERR_NO_MEM) {
            return send_json(req, "409 Conflict", "{\"error\":\"the list is full\"}");
        }
        if (err == ESP_ERR_INVALID_ARG) {
            return send_json(req, "400 Bad Request", "{\"error\":\"not a usable address\"}");
        }
    } else {
        return send_json(req, "400 Bad Request",
            "{\"error\":\"action must be add, remove or clear\"}");
    }

    if (err != ESP_OK) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"could not save\"}");
    }

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"n\":%d}", settings_trusted_count());
    return send_json(req, nullptr, resp);
}

static esp_err_t scan_handler(httpd_req_t *req)
{
    /* Open while unprovisioned so the setup wizard can list networks before
     * any password exists; protected afterwards. */
    if (settings_get()->provisioned) {
        REQUIRE_AUTH(req);
    }

    const size_t cap = 2560;
    char *buf = (char *)malloc(cap);
    if (buf == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    wifi_manager_scan_json(buf, cap);

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);

    free(buf);
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Handlers — setup wizard, maintenance, OTA
 * ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t setup_complete_handler(httpd_req_t *req)
{
    /* Open only while the device has never been set up. Once provisioned,
     * this route is the same as any other privileged one — otherwise a
     * rescue portal would be a way around the admin password. */
    if (settings_get()->provisioned) {
        REQUIRE_AUTH(req);
    }

    char *body = read_body(req, 4096);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    /* The wizard sends the password under its own key so it never travels
     * through the generic (write-protected) settings path. */
    cJSON *root = cJSON_Parse(body);
    if (root == nullptr) {
        free(body);
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    cJSON *pw = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(pw) || strlen(pw->valuestring) < 4) {
        cJSON_Delete(root);
        free(body);
        return send_json(req, "400 Bad Request", "{\"error\":\"password must be 4-32 characters\"}");
    }

    char new_password[64];
    strncpy(new_password, pw->valuestring, sizeof(new_password) - 1);
    new_password[sizeof(new_password) - 1] = '\0';
    cJSON_Delete(root);

    int changed = settings_apply_json(body, strlen(body));
    free(body);

    if (changed < 0) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    if (!settings_have_wifi()) {
        return send_json(req, "400 Bad Request", "{\"error\":\"ssid required\"}");
    }

    if (settings_set_password(new_password) != ESP_OK) {
        return send_json(req, "400 Bad Request", "{\"error\":\"password must be 4-32 characters\"}");
    }

    settings_mark_provisioned();
    history_log(HIST_EV_SETTINGS, 1);

    influx_uploader_reload();
    notifier_reload();

    ESP_LOGW(TAG, "Setup complete — joining '%s' as '%s'",
        settings_get()->wifi_ssid, settings_get()->device_name);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"apSsid\":\"%s\"}", wifi_manager_ap_ssid());
    esp_err_t ret = send_json(req, nullptr, resp);

    schedule_wifi_reconnect();
    return ret;
}

/* Completes a recovery. Open only while one is armed — which can only
 * happen by holding the button on the device — and it closes itself the
 * moment a new password is set. */
static esp_err_t recovery_complete_handler(httpd_req_t *req)
{
    if (!settings_recovery_pending()) {
        return send_json(req, "409 Conflict", "{\"error\":\"no recovery is in progress\"}");
    }

    char *body = read_body(req, 1024);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (root == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    cJSON *pw   = cJSON_GetObjectItemCaseSensitive(root, "password");
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *wpw  = cJSON_GetObjectItemCaseSensitive(root, "wifi_pass");

    const char *new_pw    = cJSON_IsString(pw)   ? pw->valuestring   : nullptr;
    const char *new_ssid  = cJSON_IsString(ssid) ? ssid->valuestring : "";
    const char *new_wifipw = cJSON_IsString(wpw) ? wpw->valuestring  : "";

    bool changing_wifi = (new_ssid[0] != '\0');

    /* If the station is down, the stored network is unusable — recovering
     * without supplying a working one would leave the device reachable only
     * over its hotspot, which disappears on the next reboot. Enforced here,
     * not just in the page, because the page is not the only possible
     * client. */
    if (!wifi_manager_is_connected() && !changing_wifi) {
        cJSON_Delete(root);
        return send_json(req, "400 Bad Request",
            "{\"error\":\"this device cannot reach the stored Wi-Fi network, "
            "so new Wi-Fi details are required\"}");
    }

    esp_err_t err = settings_complete_recovery(new_pw, new_ssid, new_wifipw);
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_SIZE) {
        return send_json(req, "400 Bad Request",
            "{\"error\":\"password must be 4-32 characters\"}");
    }
    if (err != ESP_OK) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"could not save\"}");
    }

    session_clear_all();
    history_log(HIST_EV_SETTINGS, 1);
    ESP_LOGW(TAG, "Recovery finished from the web page");

    char resp[160];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"reconnecting\":%s,\"url\":\"http://%s\"}",
        changing_wifi ? "true" : "false", s_ui_ip);
    esp_err_t ret = send_json(req, nullptr, resp);

    if (changing_wifi) {
        schedule_wifi_reconnect();
    }
    return ret;
}

static esp_err_t reboot_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    esp_err_t ret = send_json(req, nullptr, "{\"rebooting\":true}");
    schedule_restart();
    return ret;
}

static esp_err_t factory_reset_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    char *body = read_body(req, 512);
    if (body == nullptr) {
        return send_json(req, "400 Bad Request", "{\"error\":\"bad body\"}");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    cJSON *pw = root ? cJSON_GetObjectItemCaseSensitive(root, "password") : nullptr;
    bool ok = cJSON_IsString(pw) && settings_check_password(pw->valuestring);
    cJSON_Delete(root);

    if (!ok) {
        vTaskDelay(pdMS_TO_TICKS(750));
        return send_json(req, "403 Forbidden", "{\"error\":\"wrong password\"}");
    }

    ESP_LOGW(TAG, "FACTORY RESET requested — erasing all user settings");
    history_log(HIST_EV_SETTINGS, 2);

    settings_factory_reset();
    session_clear_all();

    esp_err_t ret = send_json(req, nullptr, "{\"reset\":true}");
    schedule_restart();
    return ret;
}

static esp_err_t ota_handler(httpd_req_t *req)
{
    REQUIRE_AUTH(req);

    if (req->content_len == 0) {
        return send_json(req, "400 Bad Request", "{\"error\":\"empty body\"}");
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (part == nullptr) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"no OTA partition\"}");
    }

    ESP_LOGW(TAG, "OTA: %d bytes -> partition '%s'", req->content_len, part->label);

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error", "{\"error\":\"ota begin failed\"}");
    }

    const int BUF_SIZE = 4096;
    char *buf = (char *)malloc(BUF_SIZE);
    if (buf == nullptr) {
        esp_ota_abort(handle);
        return send_json(req, "500 Internal Server Error", "{\"error\":\"out of memory\"}");
    }

    int remaining = req->content_len;
    bool failed = false;

    while (remaining > 0) {
        int want = (remaining < BUF_SIZE) ? remaining : BUF_SIZE;
        int got = httpd_req_recv(req, buf, want);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (got <= 0) {
            ESP_LOGE(TAG, "OTA recv error: %d", got);
            failed = true;
            break;
        }
        if (esp_ota_write(handle, buf, got) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed");
            failed = true;
            break;
        }
        remaining -= got;
    }

    free(buf);

    if (failed) {
        esp_ota_abort(handle);
        return send_json(req, "500 Internal Server Error", "{\"error\":\"receive or write error\"}");
    }

    err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error", "{\"error\":\"image validation failed\"}");
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error", "{\"error\":\"set boot partition failed\"}");
    }

    ESP_LOGW(TAG, "OTA complete — rebooting into the new firmware");
    esp_err_t ret = send_json(req, nullptr, "{\"success\":true}");
    schedule_restart();
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Server bring-up
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Captive portal ───────────────────────────────────────────────────
 *
 * Android, iOS and Windows each fetch a known URL on joining a network and
 * decide from the reply whether there is a portal in the way. Answering
 * anything other than what they expect is what produces the "Sign in to
 * network" notification and auto-opens our setup page.
 *
 * Only while the setup or recovery hotspot is the point of the exercise.
 * On a configured device this must not fire: a 404 on the LAN should stay a
 * 404, or a mistyped API path would silently redirect instead of erroring. */
static bool captive_portal_active(void)
{
    return settings_recovery_pending() || !settings_get()->provisioned;
}

/* The setup page's address on the hotspot. The port is spelled out only when
 * it is not 80 — a captive-portal probe arrives on 80 by definition, and a
 * bare host is what the printed instructions say. */
static void portal_url(char *out, size_t out_len)
{
    const char *ap = wifi_manager_ap_ip();
    const char *host = (ap[0] != '\0') ? ap : "192.168.4.1";

    if (APP_WEB_SERVER_PORT == 80) {
        snprintf(out, out_len, "http://%s/", host);
    } else {
        snprintf(out, out_len, "http://%s:%u/", host, (unsigned)APP_WEB_SERVER_PORT);
    }
}

/* 302 rather than 307: the probes are GETs and every captive-portal detector
 * in the wild understands a plain Found. The body matters too — some clients
 * show it in an embedded view instead of following the header. */
static esp_err_t send_portal_redirect(httpd_req_t *req)
{
    char url[56];
    portal_url(url, sizeof(url));

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", url);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/html");

    char body[240];
    int n = snprintf(body, sizeof(body),
        "<!doctype html><meta http-equiv=\"refresh\" content=\"0;url=%s\">"
        "<title>Set up Garage Gadget</title>"
        "<p>Continue to <a href=\"%s\">%s</a></p>", url, url, url);

    ESP_LOGI(TAG, "Captive portal: redirecting %s to %s", req->uri, url);
    return httpd_resp_send(req, body, n);
}

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;

    if (!captive_portal_active()) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"not found\"}", HTTPD_RESP_USE_STRLEN);
    }

    return send_portal_redirect(req);
}

/* A request too large for the parser. During setup, send the wizard rather
 * than an error: whoever is standing there with a phone cannot act on
 * "header fields are too large", and bouncing them to the page is both
 * truthful and useful. On a configured device it stays an error, because by
 * then it means a real request failed and hiding that would be worse. */
static esp_err_t too_large_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;

    if (captive_portal_active()) {
        return send_portal_redirect(req);
    }

    httpd_resp_set_status(req, "431 Request Header Fields Too Large");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"error\":\"request headers too large\"}",
        HTTPD_RESP_USE_STRLEN);
}

typedef struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
} route_t;

static const route_t k_routes[] = {
    { "/",                      HTTP_GET,  index_handler            },
    { "/app.css",               HTTP_GET,  css_handler              },
    { "/app.js",                HTTP_GET,  js_handler               },
    { "/favicon.ico",           HTTP_GET,  favicon_handler          },

    { "/api/status",            HTTP_GET,  status_handler           },
    { "/api/network",           HTTP_GET,  network_handler          },
    { "/api/logs",              HTTP_GET,  logs_handler             },
    { "/api/settings",          HTTP_GET,  settings_get_handler     },
    { "/api/scan",              HTTP_GET,  scan_handler             },
    { "/api/history/events",    HTTP_GET,  history_events_handler   },
    { "/api/history/samples",   HTTP_GET,  history_samples_handler  },
    { "/api/history/stats",     HTTP_GET,  history_stats_handler    },
    { "/api/help",              HTTP_GET,  help_index_handler       },
    { "/api/help/page",         HTTP_GET,  help_page_handler        },
    { "/api/help/markdown",     HTTP_GET,  help_markdown_handler    },

    { "/api/login",             HTTP_POST, login_handler            },
    { "/api/door",              HTTP_POST, door_handler             },
    { "/api/control-enable",    HTTP_POST, control_enable_handler   },
    { "/api/update-info",       HTTP_GET,  update_info_handler      },
    { "/api/update-check",      HTTP_POST, update_check_handler     },
    { "/api/auto-close",        HTTP_POST, auto_close_handler       },
    { "/api/auto-close-lock",   HTTP_POST, auto_close_lock_handler  },
    { "/api/auto-close-delay",  HTTP_POST, auto_close_delay_handler },
    { "/api/alarm",             HTTP_POST, alarm_handler            },
    { "/api/alarm-report",      HTTP_POST, alarm_report_handler     },
    { "/api/settings",          HTTP_POST, settings_post_handler    },
    { "/api/wifi",              HTTP_POST, wifi_post_handler        },
    { "/api/password",          HTTP_POST, password_handler         },
    { "/api/notify-test",       HTTP_POST, notify_test_handler      },
    { "/api/calibrate",         HTTP_POST, calibrate_handler        },
    { "/api/trusted",           HTTP_GET,  trusted_list_handler     },
    { "/api/trusted",           HTTP_POST, trusted_edit_handler     },
    { "/api/telegram-chats",    HTTP_POST, telegram_chats_handler   },
    { "/api/setup/complete",    HTTP_POST, setup_complete_handler   },
    { "/api/recovery/complete", HTTP_POST, recovery_complete_handler },
    { "/api/history/clear",     HTTP_POST, history_clear_handler    },
    { "/api/factory-reset",     HTTP_POST, factory_reset_handler    },
    { "/api/reboot",            HTTP_POST, reboot_handler           },
    { "/api/update",            HTTP_POST, ota_handler              },

    { "/",                      HTTP_HEAD, head_handler             },
    { "/api/status",            HTTP_HEAD, head_handler             },
};

#define ROUTE_COUNT (sizeof(k_routes) / sizeof(k_routes[0]))

esp_err_t web_server_start(uint16_t port)
{
    httpd_handle_t server = nullptr;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = port;
    config.max_uri_handlers = ROUTE_COUNT + 2;
    /* The page polls several endpoints in parallel; the default pool of 7
     * fills up and the server starts refusing connections. lwIP is
     * configured for 16 sockets, and httpd reserves 3 for itself. */
    config.max_open_sockets = 9;
    config.recv_wait_timeout = 15;   /* generous, for large OTA uploads */
    config.send_wait_timeout = 10;
    config.stack_size = 8192;        /* TLS-free, but JSON encoding is chatty */
    config.keep_alive_enable = true;
    config.keep_alive_idle = 5;
    config.keep_alive_interval = 5;
    config.keep_alive_count = 3;
    config.lru_purge_enable = true;  /* evict the oldest idle socket rather than reject */

    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "httpd_start failed");

    for (size_t i = 0; i < ROUTE_COUNT; i++) {
        httpd_uri_t uri = {};
        uri.uri = k_routes[i].uri;
        uri.method = k_routes[i].method;
        uri.handler = k_routes[i].handler;
        uri.user_ctx = nullptr;

        esp_err_t err = httpd_register_uri_handler(server, &uri);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s: %s", k_routes[i].uri, esp_err_to_name(err));
            return err;
        }
    }

    /* Everything unrouted becomes the captive-portal redirect while the
     * hotspot is up, and an ordinary 404 otherwise. */
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);
    httpd_register_err_handler(server, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,
        too_large_handler);
    httpd_register_err_handler(server, HTTPD_414_URI_TOO_LONG, too_large_handler);

    s_ui_port = port;
    compute_asset_version();
    ESP_LOGI(TAG, "Web server listening on port %u (%u routes), assets v%s",
        (unsigned)port, (unsigned)ROUTE_COUNT, s_asset_ver);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 * State setters
 * ═══════════════════════════════════════════════════════════════════════ */

void web_server_set_network_info(const char *ip_address, const char *mdns_hostname, uint16_t port)
{
    if (ip_address != nullptr) {
        strncpy(s_ui_ip, ip_address, sizeof(s_ui_ip) - 1);
        s_ui_ip[sizeof(s_ui_ip) - 1] = '\0';
    }
    if (mdns_hostname != nullptr) {
        strncpy(s_ui_mdns, mdns_hostname, sizeof(s_ui_mdns) - 1);
        s_ui_mdns[sizeof(s_ui_mdns) - 1] = '\0';
    }
    if (port != 0) {
        s_ui_port = port;
    }
}

void web_server_set_door_closed(bool closed)     { s_door_closed = closed; }
bool web_server_is_door_closed(void)             { return s_door_closed; }
void web_server_set_door_open_seconds(uint32_t s){ s_open_seconds = s; }
void web_server_set_server_online(bool online)   { s_server_online = online; s_server_known = true; }
void web_server_set_chip_temp(float temp_c)      { s_chip_temp_c = temp_c; s_chip_temp_known = true; }

void web_server_set_alarm_state(bool immediate_active, uint32_t left_open_periods)
{
    s_immediate_tripped = immediate_active;
    s_left_open_periods = left_open_periods;
}
