#include "captive_dns.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <string.h>

static const char *TAG = "captive-dns";

#define DNS_PORT        53
#define DNS_MAX_PACKET  512

static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;
static uint32_t s_answer_ip = 0;     /* network byte order */

/* Minimal DNS header, in wire order. */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

/* Walks the QNAME label sequence and returns the offset just past it, or 0
 * if it runs off the end of the packet. */
static size_t skip_qname(const uint8_t *buf, size_t len, size_t pos)
{
    while (pos < len) {
        uint8_t l = buf[pos];
        if (l == 0) {
            return pos + 1;
        }
        /* A compression pointer has no business being in a query we are
         * about to echo; refuse rather than try to follow it. */
        if ((l & 0xC0) != 0) {
            return 0;
        }
        pos += 1 + l;
    }
    return 0;
}

/* Builds a reply that echoes the question and appends one A record.
 * Returns the reply length, or 0 if the query is not one we should answer. */
static size_t build_reply(const uint8_t *req, size_t req_len,
                          uint8_t *out, size_t out_cap)
{
    if (req_len < sizeof(dns_header_t)) {
        return 0;
    }

    dns_header_t hdr;
    memcpy(&hdr, req, sizeof(hdr));

    /* Standard query, one question, not already a response. */
    uint16_t flags = ntohs(hdr.flags);
    if ((flags & 0x8000) != 0) {
        return 0;                       /* it is a response, not a query */
    }
    if (((flags >> 11) & 0x0F) != 0) {
        return 0;                       /* not a standard QUERY opcode */
    }
    if (ntohs(hdr.qd_count) != 1) {
        return 0;
    }

    size_t qend = skip_qname(req, req_len, sizeof(dns_header_t));
    if (qend == 0 || qend + 4 > req_len) {
        return 0;
    }

    uint16_t qtype = (uint16_t)((req[qend] << 8) | req[qend + 1]);
    qend += 4;                          /* past QTYPE and QCLASS */

    size_t question_len = qend - sizeof(dns_header_t);
    size_t reply_len = sizeof(dns_header_t) + question_len;

    /* Only A records get an answer. For anything else (notably AAAA, which
     * phones ask for constantly) reply with zero answers rather than
     * pointing IPv6 traffic at an IPv4 address. */
    bool answer = (qtype == 1);
    if (answer) {
        reply_len += 16;                /* name ptr 2 + type/class/ttl/len 10 + addr 4 */
    }
    if (reply_len > out_cap) {
        return 0;
    }

    dns_header_t rh;
    rh.id = hdr.id;
    /* QR=1, AA=1, RD copied from the query, RA=0, RCODE=0 */
    rh.flags = htons(0x8400 | (flags & 0x0100));
    rh.qd_count = htons(1);
    rh.an_count = htons(answer ? 1 : 0);
    rh.ns_count = 0;
    rh.ar_count = 0;

    memcpy(out, &rh, sizeof(rh));
    memcpy(out + sizeof(rh), req + sizeof(dns_header_t), question_len);

    if (answer) {
        uint8_t *a = out + sizeof(rh) + question_len;
        a[0] = 0xC0;  a[1] = 0x0C;              /* pointer to the question name */
        a[2] = 0x00;  a[3] = 0x01;              /* TYPE  A   */
        a[4] = 0x00;  a[5] = 0x01;              /* CLASS IN  */
        /* Short TTL: the moment the user leaves this network, nothing should
         * still believe the whole internet lives at 192.168.4.1. */
        a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 10;
        a[10] = 0x00; a[11] = 0x04;             /* RDLENGTH */
        memcpy(a + 12, &s_answer_ip, 4);
    }

    return reply_len;
}

static void dns_task(void *arg)
{
    (void)arg;

    int sock = -1;
    uint8_t *req = malloc(DNS_MAX_PACKET);
    uint8_t *rsp = malloc(DNS_MAX_PACKET);

    if (req == NULL || rsp == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        goto done;
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        goto done;
    }

    struct sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "bind() to port %d failed: errno %d", DNS_PORT, errno);
        goto done;
    }

    /* Short timeout so stopping is prompt rather than blocked in recvfrom. */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "Captive DNS up on port %d", DNS_PORT);

    while (s_run) {
        struct sockaddr_in from = {};
        socklen_t from_len = sizeof(from);

        int n = recvfrom(sock, req, DNS_MAX_PACKET, 0,
                         (struct sockaddr *)&from, &from_len);
        if (n <= 0) {
            continue;                   /* timeout, or a transient error */
        }

        size_t rlen = build_reply(req, (size_t)n, rsp, DNS_MAX_PACKET);
        if (rlen > 0) {
            sendto(sock, rsp, rlen, 0, (struct sockaddr *)&from, from_len);
        }
    }

done:
    if (sock >= 0) {
        close(sock);
    }
    free(req);
    free(rsp);
    ESP_LOGI(TAG, "Captive DNS stopped");
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t captive_dns_start(const char *ap_ip)
{
    if (s_task != NULL) {
        return ESP_OK;
    }
    if (ap_ip == NULL || ap_ip[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    s_answer_ip = inet_addr(ap_ip);
    if (s_answer_ip == INADDR_NONE) {
        return ESP_ERR_INVALID_ARG;
    }

    s_run = true;
    if (xTaskCreate(dns_task, "captive-dns", 3072, NULL, 4, &s_task) != pdPASS) {
        s_run = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Answering every name with %s", ap_ip);
    return ESP_OK;
}

void captive_dns_stop(void)
{
    s_run = false;      /* the task closes its socket and exits within ~1 s */
}
