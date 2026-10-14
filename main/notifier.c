#include "notifier.h"
#include "app_config.h"
#include "app_settings.h"
#include "history.h"
#include "wifi_manager.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "notifier";

#define NOTIFY_QUEUE_LEN     (12)
#define NOTIFY_TASK_STACK    (8192)
#define NOTIFY_TASK_PRIO     (4)
#define NOTIFY_HTTP_TIMEOUT  (10000)
#define NOTIFY_TITLE_LEN     (64)
#define NOTIFY_BODY_LEN      (192)
/* Delivery results carry the provider's own explanation ("chat not
 * found - open Telegram and send your bot a message first"), so they
 * need far more room than a bare status code would. */
#define NOTIFY_STATUS_LEN    (224)

typedef struct {
    notify_level_t level;
    char title[NOTIFY_TITLE_LEN];
    char body[NOTIFY_BODY_LEN];
    /* When set, the sending task posts the outcome here instead of just
     * logging it — used by the synchronous "Send test" button. */
    SemaphoreHandle_t done_sem;
    volatile esp_err_t *done_result;
    char *done_msg;
    size_t done_msg_len;
    /* Test sends run an extra getMe probe first, so the result can say
     * whether the token or the chat ID is at fault. Normal notifications
     * skip it and stay to a single request. */
    bool diagnose;
} notify_msg_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t  s_task  = NULL;
static char s_last_result[NOTIFY_STATUS_LEN] = "";
static SemaphoreHandle_t s_result_mutex = NULL;

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void set_last_result(const char *fmt, ...)
{
    if (s_result_mutex == NULL) {
        return;
    }
    va_list ap;
    xSemaphoreTake(s_result_mutex, portMAX_DELAY);
    va_start(ap, fmt);
    vsnprintf(s_last_result, sizeof(s_last_result), fmt, ap);
    va_end(ap);
    xSemaphoreGive(s_result_mutex);
}

void notifier_last_result(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (s_result_mutex == NULL) {
        out[0] = '\0';
        return;
    }
    xSemaphoreTake(s_result_mutex, portMAX_DELAY);
    strncpy(out, s_last_result, out_len - 1);
    out[out_len - 1] = '\0';
    xSemaphoreGive(s_result_mutex);
}

/* Appends `src` to `dst` with JSON string escaping applied. */
static size_t json_escape_append(char *dst, size_t dst_size, size_t pos, const char *src)
{
    for (; *src && pos + 6 < dst_size; src++) {
        switch (*src) {
        case '"':  dst[pos++] = '\\'; dst[pos++] = '"';  break;
        case '\\': dst[pos++] = '\\'; dst[pos++] = '\\'; break;
        case '\n': dst[pos++] = '\\'; dst[pos++] = 'n';  break;
        case '\r': dst[pos++] = '\\'; dst[pos++] = 'r';  break;
        case '\t': dst[pos++] = '\\'; dst[pos++] = 't';  break;
        default:
            if ((unsigned char)*src < 0x20) {
                pos += (size_t)snprintf(dst + pos, dst_size - pos, "\\u%04x", (unsigned char)*src);
            } else {
                dst[pos++] = *src;
            }
            break;
        }
    }
    dst[pos] = '\0';
    return pos;
}

/* Where to reach the dashboard, for inclusion in every notification.
 *
 * An alert saying the garage is open is only half useful; the other half is
 * being one tap away from closing it. The address itself comes from
 * wifi_manager so that this and the Home Assistant device card can never
 * disagree about where the device lives.
 *
 * Returns false when there is no address to advertise yet. */
static bool device_url(char *out, size_t out_len)
{
    return wifi_manager_dashboard_url(out, out_len);
}

static const char *level_emoji(notify_level_t level)
{
    switch (level) {
    case NOTIFY_ALARM: return "\xF0\x9F\x9A\xA8";  /* 🚨 */
    case NOTIFY_WARN:  return "\xE2\x9A\xA0";      /* ⚠  */
    default:           return "\xF0\x9F\x9A\xAA";  /* 🚪 */
    }
}

/* ntfy priority: 1 min .. 5 max */
static const char *level_ntfy_priority(notify_level_t level)
{
    switch (level) {
    case NOTIFY_ALARM: return "5";
    case NOTIFY_WARN:  return "4";
    default:           return "3";
    }
}

/* Distinct from any HTTP status or esp_err_t, so callers can tell "the URL
 * we built was not usable" from a genuine network failure. Almost always a
 * malformed token or server URL. */
#define HTTP_POST_BAD_URL (-100000)

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

/* Pulls "<key>":"<value>" out of a small JSON error body without a full
 * parse. Telegram and ntfy both report the human-readable reason this way,
 * and surfacing it is the difference between "HTTP 400" and "chat not
 * found — send your bot a message first". */
static bool json_extract(const char *json, const char *key, char *out, size_t out_len)
{
    char pat[32];
    int n = snprintf(pat, sizeof(pat), "\"%s\":", key);
    if (n < 0 || (size_t)n >= sizeof(pat)) {
        return false;
    }

    const char *p = strstr(json, pat);
    if (p == NULL) {
        return false;
    }
    p += n;
    while (*p == ' ') {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;

    size_t i = 0;
    while (*p != '\0' && *p != '"' && i + 1 < out_len) {
        if (*p == '\\' && p[1] != '\0') {
            p++;   /* keep it simple: copy the escaped char literally */
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

/* Same idea as json_extract, but for an unquoted numeric value. Telegram
 * chat ids are 64-bit and negative for groups, so this returns long long. */
static bool json_extract_num(const char *json, const char *key, long long *out)
{
    char pat[32];
    int n = snprintf(pat, sizeof(pat), "\"%s\":", key);
    if (n < 0 || (size_t)n >= sizeof(pat)) {
        return false;
    }

    const char *p = strstr(json, pat);
    if (p == NULL) {
        return false;
    }
    p += n;
    while (*p == ' ') {
        p++;
    }

    bool neg = (*p == '-');
    if (neg) {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return false;
    }

    long long v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    *out = neg ? -v : v;
    return true;
}

/* Performs one POST. Returns the HTTP status, a negative esp_err_t, or
 * HTTP_POST_BAD_URL. Any response body is copied into `resp` when given. */
static int http_post(const char *url,
                     const char *content_type,
                     const char *body,
                     const char *const *extra_headers, /* NULL-terminated k,v pairs */
                     bool skip_tls_verify,
                     char *resp, size_t resp_len)
{
    resp_capture_t capture = { .buf = resp, .cap = resp_len, .len = 0 };
    if (resp != NULL && resp_len > 0) {
        resp[0] = '\0';
    }

    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = NOTIFY_HTTP_TIMEOUT,
        .crt_bundle_attach = skip_tls_verify ? NULL : esp_crt_bundle_attach,
        .skip_cert_common_name_check = skip_tls_verify,
        .event_handler     = (resp != NULL) ? http_event_cb : NULL,
        .user_data         = (resp != NULL) ? &capture : NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        /* esp_http_client_init only returns NULL for a URL it cannot parse
         * or an allocation failure; in practice on this device it is always
         * the former, and reporting "out of memory" sends people hunting in
         * completely the wrong place. */
        ESP_LOGE(TAG, "Could not build a request for URL: %s", url);
        return HTTP_POST_BAD_URL;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    if (extra_headers != NULL) {
        for (int i = 0; extra_headers[i] != NULL && extra_headers[i + 1] != NULL; i += 2) {
            esp_http_client_set_header(client, extra_headers[i], extra_headers[i + 1]);
        }
    }

    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -(int)err;

    esp_http_client_cleanup(client);
    return status;
}

/* ── Transports ──────────────────────────────────────────────────────── */

/* A Telegram bot token is "<digits>:<35-ish base64-ish chars>". Checking the
 * shape here turns the single most common setup mistake — a token pasted
 * with a stray space or newline — into a message that says so, instead of a
 * URL parse failure deep in the HTTP client. */
static bool telegram_token_looks_valid(const char *token, char *err_out, size_t err_len)
{
    size_t len = strlen(token);

    if (len < 20) {
        snprintf(err_out, err_len, "telegram: bot token looks too short");
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)token[i];
        if (c <= ' ' || c == 0x7f) {
            snprintf(err_out, err_len,
                "telegram: bot token contains a space or line break - re-copy it from BotFather");
            return false;
        }
    }
    const char *colon = strchr(token, ':');
    if (colon == NULL || colon == token || colon[1] == '\0') {
        snprintf(err_out, err_len,
            "telegram: bot token must look like 123456789:AA... (digits, colon, key)");
        return false;
    }
    for (const char *p = token; p < colon; p++) {
        if (*p < '0' || *p > '9') {
            snprintf(err_out, err_len,
                "telegram: the part before the colon must be the bot's numeric id");
            return false;
        }
    }
    return true;
}

/* Asks Telegram to identify the bot behind the stored token.
 *
 * This separates the two failures that look identical from the outside: a
 * token Telegram will not accept, versus a good token pointing at a chat it
 * cannot post to. Only run for explicit tests.
 *
 * Returns true when the token is good, and fills `bot_name` with the bot's
 * @username so the user can confirm it is the bot they think it is. */
static bool telegram_check_token(char *bot_name, size_t name_len,
                                 char *err_out, size_t err_len)
{
    const app_settings_t *cfg = settings_get();

    char url[160];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getMe", cfg->tg_token);

    char resp[512];
    int status = http_post(url, "application/json", "{}", NULL, false, resp, sizeof(resp));

    if (status >= 200 && status < 300) {
        if (!json_extract(resp, "username", bot_name, name_len)) {
            snprintf(bot_name, name_len, "unknown");
        }
        return true;
    }

    if (status == 404) {
        /* Telegram could not parse a token out of the path at all. */
        snprintf(err_out, err_len,
            "telegram: the stored bot token is not a usable token (HTTP 404). "
            "Re-copy the whole token from BotFather - it must be digits, a colon, "
            "then the key, with no brackets or spaces.");
        return false;
    }
    if (status == 401) {
        const char *colon = strchr(cfg->tg_token, ':');
        int id_len = (colon != NULL) ? (int)(colon - cfg->tg_token) : 0;
        int key_len = (colon != NULL) ? (int)strlen(colon + 1) : 0;
        snprintf(err_out, err_len,
            "telegram: Telegram rejected this token (HTTP 401). It is bot id %.*s "
            "with a %d-character key. If you ever pressed Revoke in BotFather the "
            "id stays the same but the key changes - send /mybots to BotFather, "
            "pick the bot, and copy the current API Token.",
            id_len, cfg->tg_token, key_len);
        return false;
    }
    if (status < 0) {
        snprintf(err_out, err_len, "telegram: cannot reach Telegram (%s)",
            esp_err_to_name((esp_err_t)(-status)));
        return false;
    }

    snprintf(err_out, err_len, "telegram: getMe returned HTTP %d", status);
    return false;
}

static bool send_telegram_to(const notify_msg_t *m, const char *chat_id,
                             char *err_out, size_t err_len)
{
    const app_settings_t *cfg = settings_get();

    if (!telegram_token_looks_valid(cfg->tg_token, err_out, err_len)) {
        return false;
    }

    char url[160];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", cfg->tg_token);

    /* Body: {"chat_id":"…","text":"<emoji> <title>\n<body>","disable_notification":false} */
    char body[512];
    size_t pos = (size_t)snprintf(body, sizeof(body), "{\"chat_id\":\"");
    pos = json_escape_append(body, sizeof(body), pos, chat_id);
    pos += (size_t)snprintf(body + pos, sizeof(body) - pos, "\",\"text\":\"");
    pos = json_escape_append(body, sizeof(body), pos, level_emoji(m->level));
    pos = json_escape_append(body, sizeof(body), pos, " ");
    pos = json_escape_append(body, sizeof(body), pos, m->title);
    pos = json_escape_append(body, sizeof(body), pos, "\n");
    pos = json_escape_append(body, sizeof(body), pos, m->body);

    /* Every notification carries the dashboard address: an alert saying the
     * garage is open is only half useful without a one-tap way to close it.
     * Telegram auto-links a bare URL on its own line. */
    char dash_url[48];
    if (device_url(dash_url, sizeof(dash_url))) {
        pos = json_escape_append(body, sizeof(body), pos, "\n\n");
        pos = json_escape_append(body, sizeof(body), pos, dash_url);
    }

    snprintf(body + pos, sizeof(body) - pos, "\"}");

    char resp[256];
    int status = http_post(url, "application/json", body, NULL, false, resp, sizeof(resp));

    if (status >= 200 && status < 300) {
        snprintf(err_out, err_len, "telegram ok");
        return true;
    }

    if (status == HTTP_POST_BAD_URL) {
        snprintf(err_out, err_len, "telegram: bot token is not usable in a URL");
        return false;
    }
    if (status < 0) {
        snprintf(err_out, err_len, "telegram: %s", esp_err_to_name((esp_err_t)(-status)));
        return false;
    }

    /* Telegram always explains itself in `description`; that text is far more
     * actionable than the status code. */
    char why[160] = "";
    bool have_why = json_extract(resp, "description", why, sizeof(why));

    if (status == 401) {
        /* Telegram only says "Unauthorized". Echo back the bot id we are
         * actually using and how long the key part is, so the user can
         * compare against BotFather without us ever printing the secret. */
        const char *colon = strchr(cfg->tg_token, ':');
        int id_len = (colon != NULL) ? (int)(colon - cfg->tg_token) : 0;
        int key_len = (colon != NULL) ? (int)strlen(colon + 1) : 0;
        snprintf(err_out, err_len,
            "telegram HTTP 401: bot token rejected. We are using bot id %.*s with a "
            "%d-character key. Send /mybots to BotFather, pick this bot, and use "
            "API Token to re-copy it (or Revoke to issue a new one).",
            id_len, cfg->tg_token, key_len);
        return false;
    }

    if (have_why) {
        /* Telegram's own phrasing here is "bot can't send messages to bot",
         * which reads like a bug in the firmware. It is not: it means the
         * chat id points at a bot account — nearly always because the bot's
         * own id (the digits before the colon in the token) was pasted in as
         * the chat id. */
        if (strstr(why, "send messages to bot") != NULL ||
            strstr(why, "send messages to the bot") != NULL) {
            snprintf(err_out, err_len,
                "telegram HTTP %d: the Chat ID is a bot's ID, not yours. The number "
                "before the colon in your token is the BOT's id - you need your own. "
                "Use \"Find my chat ID\" below, or message @userinfobot.", status);
            return false;
        }
        if (status == 400 || status == 403 || strstr(why, "chat not found") != NULL) {
            snprintf(err_out, err_len,
                "telegram HTTP %d: %s - check the chat ID, and open Telegram and send "
                "your bot a message first (it cannot message you until you do)",
                status, why);
        } else {
            snprintf(err_out, err_len, "telegram HTTP %d: %s", status, why);
        }
    } else {
        snprintf(err_out, err_len, "telegram HTTP %d", status);
    }
    return false;
}

static bool send_ntfy(const notify_msg_t *m, char *err_out, size_t err_len)
{
    const app_settings_t *cfg = settings_get();

    /* Trim any trailing slash so "https://ntfy.sh/" + "topic" works. */
    char server[sizeof(cfg->ntfy_server)];
    strncpy(server, cfg->ntfy_server, sizeof(server) - 1);
    server[sizeof(server) - 1] = '\0';
    size_t slen = strlen(server);
    while (slen > 0 && server[slen - 1] == '/') {
        server[--slen] = '\0';
    }

    char endpoint[192];
    snprintf(endpoint, sizeof(endpoint), "%s/%s", server, cfg->ntfy_topic);

    /* ntfy takes the message as the raw body and the rest as headers.
     * Header values must be ASCII, so the title carries no emoji here —
     * the Tags header supplies the icon instead. */
    const char *tag = (m->level == NOTIFY_ALARM) ? "rotating_light"
                    : (m->level == NOTIFY_WARN)  ? "warning"
                                                 : "door";

    /* The Click header makes the whole ntfy notification tappable; the URL is
     * repeated in the body for desktop clients that do not surface it. */
    char dash_url[48];
    bool have_url = device_url(dash_url, sizeof(dash_url));

    char text[NOTIFY_BODY_LEN + 64];
    if (have_url) {
        snprintf(text, sizeof(text), "%s\n\n%s", m->body, dash_url);
    } else {
        snprintf(text, sizeof(text), "%s", m->body);
    }

    const char *headers[] = {
        "Title",    m->title,
        "Priority", level_ntfy_priority(m->level),
        "Tags",     tag,
        "Click",    have_url ? dash_url : "",
        NULL, NULL
    };

    /* A self-hosted ntfy on plain http:// needs no cert bundle. */
    bool plain = (strncmp(server, "http://", 7) == 0);

    char resp[256];
    int status = http_post(endpoint, "text/plain; charset=utf-8", text, headers, plain,
                           resp, sizeof(resp));

    if (status >= 200 && status < 300) {
        snprintf(err_out, err_len, "ntfy ok");
        return true;
    }

    if (status == HTTP_POST_BAD_URL) {
        snprintf(err_out, err_len, "ntfy: server URL is not valid (%s)", server);
        return false;
    }
    if (status < 0) {
        snprintf(err_out, err_len, "ntfy: %s", esp_err_to_name((esp_err_t)(-status)));
        return false;
    }

    char why[160] = "";
    if (json_extract(resp, "error", why, sizeof(why))) {
        snprintf(err_out, err_len, "ntfy HTTP %d: %s", status, why);
    } else {
        snprintf(err_out, err_len, "ntfy HTTP %d", status);
    }
    return false;
}

/* Finds the end of the JSON object that starts at `open_brace`, so a nested
 * "chat" object can be scanned without running into the next update. */
static const char *json_object_end(const char *open_brace)
{
    int depth = 0;
    bool in_str = false;

    for (const char *p = open_brace; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) {
                p++;              /* skip the escaped character */
            } else if (*p == '"') {
                in_str = false;
            }
            continue;
        }
        if (*p == '"') {
            in_str = true;
        } else if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            if (--depth == 0) {
                return p + 1;
            }
        }
    }
    return NULL;
}

int notifier_telegram_chats(char *out, size_t out_len, char *err_out, size_t err_len)
{
    const app_settings_t *cfg = settings_get();

    if (!cfg->tg_token[0]) {
        snprintf(err_out, err_len, "No bot token saved yet.");
        return -1;
    }
    if (!telegram_token_looks_valid(cfg->tg_token, err_out, err_len)) {
        return -1;
    }

    char url[160];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates", cfg->tg_token);

    const size_t cap = 8192;
    char *resp = malloc(cap);
    if (resp == NULL) {
        snprintf(err_out, err_len, "Out of memory.");
        return -1;
    }

    int status = http_post(url, "application/json", "{\"limit\":20}", NULL, false, resp, cap);

    if (status < 200 || status >= 300) {
        if (status == 401 || status == 404) {
            snprintf(err_out, err_len,
                "Telegram rejected the saved bot token (HTTP %d). Fix the token first.", status);
        } else if (status < 0) {
            snprintf(err_out, err_len, "Cannot reach Telegram (%s).",
                esp_err_to_name((esp_err_t)(-status)));
        } else {
            snprintf(err_out, err_len, "getUpdates returned HTTP %d.", status);
        }
        free(resp);
        return -1;
    }

    int pos = snprintf(out, out_len, "{\"chats\":[");
    int found = 0;
    long long seen[8];

    for (const char *p = resp; (p = strstr(p, "\"chat\":{")) != NULL; ) {
        const char *obj = p + 7;                 /* the '{' of the chat object */
        const char *end = json_object_end(obj);
        if (end == NULL) {
            break;
        }
        p = end;

        /* Copy just this chat object so the field lookups cannot wander into
         * the surrounding update. */
        size_t olen = (size_t)(end - obj);
        if (olen >= 512) {
            olen = 511;
        }
        char chat[512];
        memcpy(chat, obj, olen);
        chat[olen] = '\0';

        long long id = 0;
        if (!json_extract_num(chat, "id", &id)) {
            continue;
        }

        bool dup = false;
        for (int i = 0; i < found; i++) {
            if (seen[i] == id) { dup = true; break; }
        }
        if (dup || found >= (int)(sizeof(seen) / sizeof(seen[0]))) {
            continue;
        }

        char name[64] = "";
        char type[24] = "";
        if (!json_extract(chat, "title", name, sizeof(name))) {          /* groups */
            json_extract(chat, "first_name", name, sizeof(name));        /* people */
        }
        json_extract(chat, "type", type, sizeof(type));

        char safe[80];
        size_t sp = 0;
        for (const char *q = name; *q && sp + 2 < sizeof(safe); q++) {
            if (*q == '"' || *q == '\\') {
                safe[sp++] = '\\';
            }
            if ((unsigned char)*q >= 0x20) {
                safe[sp++] = *q;
            }
        }
        safe[sp] = '\0';

        int n = snprintf(out + pos, out_len - (size_t)pos,
            "%s{\"id\":\"%lld\",\"name\":\"%s\",\"type\":\"%s\"}",
            (found == 0) ? "" : ",", id, safe, type);
        if (n < 0 || (size_t)n >= out_len - (size_t)pos) {
            break;
        }
        pos += n;
        seen[found++] = id;
    }

    free(resp);
    snprintf(out + pos, out_len - (size_t)pos, "],\"n\":%d}", found);

    if (found == 0) {
        snprintf(err_out, err_len,
            "The token works, but nobody has messaged this bot recently. "
            "Open Telegram, find your bot, press START or send it any message, "
            "then try again.");
    }
    return found;
}

/* ── Worker ──────────────────────────────────────────────────────────── */

static esp_err_t deliver(const notify_msg_t *m, char *status_out, size_t status_len)
{
    const app_settings_t *cfg = settings_get();

    char tg_status[NOTIFY_STATUS_LEN]   = "";
    char ntfy_status[NOTIFY_STATUS_LEN] = "";
    bool tried = false;
    bool any_ok = false;

    int recipients = settings_tg_chat_count();

    if (cfg->tg_enabled && cfg->tg_token[0] && recipients > 0) {
        tried = true;

        bool token_ok = true;
        char bot_name[64] = "";
        if (m->diagnose) {
            token_ok = telegram_check_token(bot_name, sizeof(bot_name),
                                            tg_status, sizeof(tg_status));
        }

        if (token_ok) {
            int ok_count = 0;
            char first_err[NOTIFY_STATUS_LEN] = "";
            char failed_nick[TG_CHAT_NICK_LEN] = "";

            /* Deliver to each recipient independently: one bad chat id must
             * not stop the others from being told the garage is open. */
            for (int i = 0; i < recipients; i++) {
                char id[TG_CHAT_ID_LEN] = "";
                char nick[TG_CHAT_NICK_LEN] = "";
                if (!settings_tg_chat_get(i, id, sizeof(id), nick, sizeof(nick)) || id[0] == 0) {
                    continue;
                }

                char one_err[NOTIFY_STATUS_LEN] = "";
                if (send_telegram_to(m, id, one_err, sizeof(one_err))) {
                    ok_count++;
                } else if (first_err[0] == 0) {
                    strncpy(first_err, one_err, sizeof(first_err) - 1);
                    strncpy(failed_nick, nick[0] ? nick : id, sizeof(failed_nick) - 1);
                }
            }

            any_ok |= (ok_count > 0);

            if (ok_count == recipients) {
                if (m->diagnose && bot_name[0]) {
                    snprintf(tg_status, sizeof(tg_status),
                        "telegram ok - %d recipient%s, sent as @%s",
                        ok_count, (ok_count == 1) ? "" : "s", bot_name);
                } else {
                    snprintf(tg_status, sizeof(tg_status), "telegram ok (%d)", ok_count);
                }
            } else if (ok_count > 0) {
                /* Both parts are bounded so the combined message provably
                 * fits, rather than relying on the reason being short. */
                snprintf(tg_status, sizeof(tg_status),
                    "telegram: %d of %d sent; %.24s failed - %.120s",
                    ok_count, recipients, failed_nick, first_err);
            } else {
                strncpy(tg_status, first_err, sizeof(tg_status) - 1);
                tg_status[sizeof(tg_status) - 1] = 0;
            }
        }
    }

    if (cfg->ntfy_enabled && cfg->ntfy_server[0] && cfg->ntfy_topic[0]) {
        tried = true;
        any_ok |= send_ntfy(m, ntfy_status, sizeof(ntfy_status));
    }

    if (!tried) {
        snprintf(status_out, status_len, "no transport enabled");
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(status_out, status_len, "%s%s%s",
        tg_status,
        (tg_status[0] && ntfy_status[0]) ? "; " : "",
        ntfy_status);

    if (!any_ok) {
        history_log(HIST_EV_NOTIFY_FAIL, 0);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void notifier_task(void *arg)
{
    (void)arg;
    notify_msg_t m;

    while (true) {
        if (xQueueReceive(s_queue, &m, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        char status[NOTIFY_STATUS_LEN * 2];
        esp_err_t err = deliver(&m, status, sizeof(status));

        set_last_result("%s", status);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Sent '%s' (%s)", m.title, status);
        } else {
            ESP_LOGW(TAG, "Delivery failed for '%s': %s", m.title, status);
        }

        /* Hand the result back to a synchronous caller, if any. */
        if (m.done_sem != NULL) {
            if (m.done_result != NULL) {
                *m.done_result = err;
            }
            if (m.done_msg != NULL && m.done_msg_len > 0) {
                strncpy(m.done_msg, status, m.done_msg_len - 1);
                m.done_msg[m.done_msg_len - 1] = '\0';
            }
            xSemaphoreGive(m.done_sem);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t notifier_init(void)
{
    if (s_result_mutex == NULL) {
        s_result_mutex = xSemaphoreCreateMutex();
        if (s_result_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_queue == NULL) {
        s_queue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(notify_msg_t));
        if (s_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_task == NULL) {
        if (xTaskCreate(notifier_task, "notifier", NOTIFY_TASK_STACK,
                        NULL, NOTIFY_TASK_PRIO, &s_task) != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    const app_settings_t *cfg = settings_get();
    ESP_LOGI(TAG, "Notifier ready (telegram=%s, ntfy=%s)",
        cfg->tg_enabled ? "on" : "off",
        cfg->ntfy_enabled ? "on" : "off");

    return ESP_OK;
}

void notifier_reload(void)
{
    const app_settings_t *cfg = settings_get();
    ESP_LOGI(TAG, "Notification settings reloaded (telegram=%s, ntfy=%s)",
        cfg->tg_enabled ? "on" : "off",
        cfg->ntfy_enabled ? "on" : "off");
}

bool notifier_any_enabled(void)
{
    const app_settings_t *cfg = settings_get();
    bool tg   = cfg->tg_enabled   && cfg->tg_token[0]    && settings_tg_chat_count() > 0;
    bool ntfy = cfg->ntfy_enabled && cfg->ntfy_server[0] && cfg->ntfy_topic[0];
    return tg || ntfy;
}

esp_err_t notifier_send(notify_level_t level, const char *title, const char *body)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!notifier_any_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }

    notify_msg_t m = { .level = level };
    strncpy(m.title, title ? title : "Garage", sizeof(m.title) - 1);
    strncpy(m.body,  body  ? body  : "",       sizeof(m.body) - 1);

    return (xQueueSend(s_queue, &m, 0) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t notifier_sendf(notify_level_t level, const char *title, const char *fmt, ...)
{
    char body[NOTIFY_BODY_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    return notifier_send(level, title, body);
}

esp_err_t notifier_test(char *err_out, size_t err_len)
{
    if (s_queue == NULL) {
        snprintf(err_out, err_len, "notifier not started");
        return ESP_ERR_INVALID_STATE;
    }
    if (!notifier_any_enabled()) {
        snprintf(err_out, err_len, "no transport enabled");
        return ESP_ERR_INVALID_STATE;
    }

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        snprintf(err_out, err_len, "out of memory");
        return ESP_ERR_NO_MEM;
    }

    volatile esp_err_t result = ESP_FAIL;

    notify_msg_t m = {
        .level        = NOTIFY_INFO,
        .diagnose     = true,
        .done_sem     = sem,
        .done_result  = &result,
        .done_msg     = err_out,
        .done_msg_len = err_len,
    };
    strncpy(m.title, APP_PRODUCT_NAME " test", sizeof(m.title) - 1);
    strncpy(m.body,
        "If you can read this, alarms from your garage will reach you here.",
        sizeof(m.body) - 1);

    if (xQueueSend(s_queue, &m, pdMS_TO_TICKS(1000)) != pdTRUE) {
        vSemaphoreDelete(sem);
        snprintf(err_out, err_len, "queue full");
        return ESP_ERR_TIMEOUT;
    }

    /* Two transports × 10 s timeout, plus slack. */
    if (xSemaphoreTake(sem, pdMS_TO_TICKS(25000)) != pdTRUE) {
        /* The worker still holds `sem` and will give it after we return, so
         * we cannot delete it here. Leak one semaphore rather than risk a
         * use-after-free; this path only happens if the network wedges. */
        snprintf(err_out, err_len, "timed out waiting for delivery");
        return ESP_ERR_TIMEOUT;
    }

    vSemaphoreDelete(sem);
    return result;
}
