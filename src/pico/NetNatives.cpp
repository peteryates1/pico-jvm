/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

#include <stdio.h>
#include <string.h>

#include "kni.h"
#include "sni.h"

#include "pico/cyw43_arch.h"

/* lwIP headers only available when using a lwIP-enabled cyw43_arch variant */
#if CYW43_LWIP
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#endif

#include "pico/stdlib.h"

#define MAX_SSID_LEN    64
#define MAX_PASS_LEN    64
#define MAX_IP_LEN      16

/**
 * Helper: copy a Java String to a C char buffer
 */
static int kni_string_to_cstr(jobject strHandle, char *buf, int bufLen)
{
    jint len = KNI_GetStringLength(strHandle);
    if (len < 0 || len >= bufLen) return -1;

    jchar jchars[128];
    if (len > 128) len = 128;
    KNI_GetStringRegion(strHandle, 0, len, jchars);

    for (int i = 0; i < len; i++) {
        buf[i] = (char)jchars[i];
    }
    buf[len] = '\0';
    return len;
}

/* ================================================================
 * Socket FD table and helpers
 * ================================================================ */

#if CYW43_LWIP

#define MAX_SOCKETS       8
#define TCP_RECV_BUF_SIZE 2048

enum sock_type  { SOCK_UNUSED = 0, SOCK_TCP = 1, SOCK_UDP = 2 };
enum sock_state { STATE_NONE = 0, STATE_CONNECTING, STATE_CONNECTED, STATE_CLOSED, STATE_ERROR };

typedef struct {
    int type;
    union { struct tcp_pcb *tcp; struct udp_pcb *udp; } pcb;

    volatile int  state;
    volatile int  error;

    /* TCP ring buffer for received bytes */
    uint8_t       recv_buf[TCP_RECV_BUF_SIZE];
    volatile int  recv_head;
    volatile int  recv_tail;
    volatile bool recv_eof;

    /* UDP: single pending datagram */
    struct pbuf  *udp_pbuf;
    ip_addr_t     udp_src_addr;
    u16_t         udp_src_port;
} socket_ctx_t;

static socket_ctx_t sockets[MAX_SOCKETS];

static int alloc_socket(int type)
{
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].type == SOCK_UNUSED) {
            memset(&sockets[i], 0, sizeof(socket_ctx_t));
            sockets[i].type = type;
            return i;
        }
    }
    return -1;
}

static void free_socket(int fd)
{
    if (fd >= 0 && fd < MAX_SOCKETS) {
        if (sockets[fd].udp_pbuf) {
            pbuf_free(sockets[fd].udp_pbuf);
        }
        memset(&sockets[fd], 0, sizeof(socket_ctx_t));
    }
}

static int recv_buf_used(socket_ctx_t *s)
{
    return (s->recv_head - s->recv_tail + TCP_RECV_BUF_SIZE) % TCP_RECV_BUF_SIZE;
}

static int recv_buf_free(socket_ctx_t *s)
{
    /* Leave one byte unused to distinguish full from empty */
    return TCP_RECV_BUF_SIZE - 1 - recv_buf_used(s);
}

/* ----------------------------------------------------------------
 * DNS resolver (shared by TCP connect and UDP send)
 * ---------------------------------------------------------------- */

static volatile bool dns_done;
static ip_addr_t     dns_result;

static void dns_cb(const char *name, const ip_addr_t *addr, void *arg)
{
    (void)name; (void)arg;
    if (addr) {
        dns_result = *addr;
    } else {
        ip_addr_set_any(0, &dns_result);
    }
    dns_done = true;
}

/**
 * Resolve hostname to IP address.
 * Returns 0 on success (result in *out_addr), -1 on failure.
 */
static int resolve_host(const char *host, ip_addr_t *out_addr)
{
    /* Try IP literal first */
    if (ipaddr_aton(host, out_addr)) return 0;

    dns_done = false;
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(host, out_addr, dns_cb, NULL);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) return 0;            /* Cached result */
    if (err != ERR_INPROGRESS) return -1;   /* Error */

    /* Wait for async DNS callback */
    while (!dns_done) sleep_ms(1);

    if (ip_addr_isany(&dns_result)) return -1;
    *out_addr = dns_result;
    return 0;
}

/* ----------------------------------------------------------------
 * TCP callbacks (called from lwIP background context)
 * ---------------------------------------------------------------- */

static err_t on_tcp_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)pcb;
    socket_ctx_t *s = (socket_ctx_t *)arg;
    if (err == ERR_OK) {
        s->state = STATE_CONNECTED;
    } else {
        s->state = STATE_ERROR;
        s->error = err;
    }
    return ERR_OK;
}

static err_t on_tcp_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    socket_ctx_t *s = (socket_ctx_t *)arg;

    if (p == NULL) {
        /* Remote closed the connection */
        s->recv_eof = true;
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        s->state = STATE_ERROR;
        s->error = err;
        return err;
    }

    /* Copy as much as fits into ring buffer */
    int avail = recv_buf_free(s);
    int to_copy = (int)p->tot_len;
    if (to_copy > avail) to_copy = avail;

    if (to_copy > 0) {
        /* Copy from pbuf into ring buffer, handling wraparound */
        int head = s->recv_head;
        int first = TCP_RECV_BUF_SIZE - head;
        if (first > to_copy) first = to_copy;

        pbuf_copy_partial(p, s->recv_buf + head, first, 0);
        if (to_copy > first) {
            pbuf_copy_partial(p, s->recv_buf, to_copy - first, first);
        }
        s->recv_head = (head + to_copy) % TCP_RECV_BUF_SIZE;

        /* Acknowledge received data to lwIP for flow control */
        tcp_recved(pcb, to_copy);
    }

    pbuf_free(p);
    return ERR_OK;
}

static void on_tcp_err(void *arg, err_t err)
{
    socket_ctx_t *s = (socket_ctx_t *)arg;
    s->state = STATE_ERROR;
    s->error = err;
    /* lwIP has already freed the pcb when this callback fires */
    s->pcb.tcp = NULL;
}

/* ----------------------------------------------------------------
 * UDP callback (called from lwIP background context)
 * ---------------------------------------------------------------- */

static void on_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                         const ip_addr_t *addr, u16_t port)
{
    (void)pcb;
    socket_ctx_t *s = (socket_ctx_t *)arg;

    if (s->udp_pbuf != NULL) {
        /* Previous datagram not yet consumed — drop incoming */
        pbuf_free(p);
        return;
    }

    s->udp_pbuf = p;
    s->udp_src_addr = *addr;
    s->udp_src_port = port;
}

#endif /* CYW43_LWIP */

extern "C" {

/* ================================================================
 * WiFi natives
 * ================================================================ */

extern int cyw43_initialized;

int Java_pico_net_WiFi_wifi_1init( void )
{
    if (!cyw43_initialized) {
        int result = cyw43_arch_init();
        if (result != 0) return -1;
        cyw43_initialized = 1;
    }
    cyw43_arch_enable_sta_mode();
    return 0;
}

int Java_pico_net_WiFi_wifi_1connect( void )
{
    int auth      = KNI_GetParameterAsInt(3);
    int timeoutMs = KNI_GetParameterAsInt(4);

    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];

    KNI_StartHandles(2);
    KNI_DeclareHandle(ssidHandle);
    KNI_DeclareHandle(passHandle);
    KNI_GetParameterAsObject(1, ssidHandle);
    KNI_GetParameterAsObject(2, passHandle);

    kni_string_to_cstr(ssidHandle, ssid, MAX_SSID_LEN);
    kni_string_to_cstr(passHandle, password, MAX_PASS_LEN);

    KNI_EndHandles();

    uint32_t cyw_auth;
    switch (auth) {
        case 0: cyw_auth = CYW43_AUTH_OPEN; break;
        case 1: cyw_auth = CYW43_AUTH_WPA2_MIXED_PSK; break;
        case 2: cyw_auth = CYW43_AUTH_WPA2_AES_PSK; break;
        default: cyw_auth = CYW43_AUTH_WPA2_AES_PSK; break;
    }

    int result = cyw43_arch_wifi_connect_timeout_ms(ssid, password, cyw_auth, timeoutMs);
    return result == 0 ? 0 : -1;
}

void Java_pico_net_WiFi_wifi_1disconnect( void )
{
    cyw43_arch_disable_sta_mode();
}

int Java_pico_net_WiFi_wifi_1get_1status( void )
{
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
}

jobject Java_pico_net_WiFi_wifi_1get_1ip( void )
{
    KNI_StartHandles(1);
    KNI_DeclareHandle(resultHandle);

#if CYW43_LWIP
    struct netif *netif = netif_list;
    if (netif != NULL && netif_is_up(netif)) {
        char ip_str[MAX_IP_LEN];
        const ip4_addr_t *addr = netif_ip4_addr(netif);
        snprintf(ip_str, MAX_IP_LEN, "%d.%d.%d.%d",
                 ip4_addr1(addr), ip4_addr2(addr),
                 ip4_addr3(addr), ip4_addr4(addr));
        KNI_NewStringUTF(ip_str, resultHandle);
    } else
#endif
    {
        KNI_NewStringUTF("0.0.0.0", resultHandle);
    }

    KNI_EndHandlesAndReturnObject(resultHandle);
}

/* ================================================================
 * TCPSocket natives
 * ================================================================ */

int Java_pico_net_TCPSocket_tcp_1connect( void )
{
#if CYW43_LWIP
    int port = KNI_GetParameterAsInt(2);

    char host[128];
    KNI_StartHandles(1);
    KNI_DeclareHandle(hostHandle);
    KNI_GetParameterAsObject(1, hostHandle);
    kni_string_to_cstr(hostHandle, host, sizeof(host));
    KNI_EndHandles();

    /* Resolve hostname */
    ip_addr_t addr;
    if (resolve_host(host, &addr) != 0) return -1;

    /* Allocate socket */
    int fd = alloc_socket(SOCK_TCP);
    if (fd < 0) return -1;

    socket_ctx_t *s = &sockets[fd];
    s->state = STATE_CONNECTING;

    /* Create PCB and initiate connection */
    cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        cyw43_arch_lwip_end();
        free_socket(fd);
        return -1;
    }
    s->pcb.tcp = pcb;
    tcp_arg(pcb, s);
    tcp_recv(pcb, on_tcp_recv);
    tcp_err(pcb, on_tcp_err);

    err_t err = tcp_connect(pcb, &addr, port, on_tcp_connected);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        cyw43_arch_lwip_begin();
        tcp_abort(pcb);
        cyw43_arch_lwip_end();
        free_socket(fd);
        return -1;
    }

    /* Spin-wait for connection to complete */
    while (s->state == STATE_CONNECTING) {
        sleep_ms(1);
    }

    if (s->state != STATE_CONNECTED) {
        /* Error during connect — pcb may already be freed by on_tcp_err */
        if (s->pcb.tcp) {
            cyw43_arch_lwip_begin();
            tcp_abort(s->pcb.tcp);
            cyw43_arch_lwip_end();
        }
        free_socket(fd);
        return -1;
    }

    return fd;
#else
    return -1;
#endif
}

int Java_pico_net_TCPSocket_tcp_1send( void )
{
#if CYW43_LWIP
    int fd     = KNI_GetParameterAsInt(1);
    int offset = KNI_GetParameterAsInt(3);
    int len    = KNI_GetParameterAsInt(4);

    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_TCP) return -1;
    socket_ctx_t *s = &sockets[fd];
    if (s->state != STATE_CONNECTED || !s->pcb.tcp) return -1;

    int result = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);

    cyw43_arch_lwip_begin();
    int sndbuf = tcp_sndbuf(s->pcb.tcp);
    int to_send = len;
    if (to_send > sndbuf) to_send = sndbuf;

    if (to_send > 0) {
        err_t err = tcp_write(s->pcb.tcp, data + offset, to_send, TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK) {
            tcp_output(s->pcb.tcp);
            result = to_send;
        } else {
            result = -1;
        }
    }
    cyw43_arch_lwip_end();

    KNI_EndHandles();

    return result;
#else
    return -1;
#endif
}

int Java_pico_net_TCPSocket_tcp_1receive( void )
{
#if CYW43_LWIP
    int fd     = KNI_GetParameterAsInt(1);
    int offset = KNI_GetParameterAsInt(3);
    int len    = KNI_GetParameterAsInt(4);

    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_TCP) return -1;
    socket_ctx_t *s = &sockets[fd];

    /* Wait until data available, EOF, or error */
    while (recv_buf_used(s) == 0 && !s->recv_eof && s->state != STATE_ERROR) {
        sleep_ms(1);
    }

    if (s->state == STATE_ERROR) return -1;

    int avail = recv_buf_used(s);
    if (avail == 0 && s->recv_eof) return 0;  /* EOF */

    int to_copy = len;
    if (to_copy > avail) to_copy = avail;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);

    /* Copy from ring buffer, handling wraparound */
    int tail = s->recv_tail;
    int first = TCP_RECV_BUF_SIZE - tail;
    if (first > to_copy) first = to_copy;

    memcpy(data + offset, s->recv_buf + tail, first);
    if (to_copy > first) {
        memcpy(data + offset + first, s->recv_buf, to_copy - first);
    }
    s->recv_tail = (tail + to_copy) % TCP_RECV_BUF_SIZE;

    KNI_EndHandles();

    return to_copy;
#else
    return -1;
#endif
}

int Java_pico_net_TCPSocket_tcp_1available( void )
{
#if CYW43_LWIP
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_TCP) return 0;
    return recv_buf_used(&sockets[fd]);
#else
    return 0;
#endif
}

void Java_pico_net_TCPSocket_tcp_1close( void )
{
#if CYW43_LWIP
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_TCP) return;

    socket_ctx_t *s = &sockets[fd];
    if (s->pcb.tcp) {
        cyw43_arch_lwip_begin();
        /* Clear callbacks first to prevent use-after-free */
        tcp_arg(s->pcb.tcp, NULL);
        tcp_recv(s->pcb.tcp, NULL);
        tcp_err(s->pcb.tcp, NULL);

        err_t err = tcp_close(s->pcb.tcp);
        if (err != ERR_OK) {
            tcp_abort(s->pcb.tcp);
        }
        cyw43_arch_lwip_end();
    }
    free_socket(fd);
#endif
}

/* ================================================================
 * UDPSocket natives
 * ================================================================ */

int Java_pico_net_UDPSocket_udp_1bind( void )
{
#if CYW43_LWIP
    int localPort = KNI_GetParameterAsInt(1);

    int fd = alloc_socket(SOCK_UDP);
    if (fd < 0) return -1;

    socket_ctx_t *s = &sockets[fd];

    cyw43_arch_lwip_begin();
    struct udp_pcb *pcb = udp_new();
    if (!pcb) {
        cyw43_arch_lwip_end();
        free_socket(fd);
        return -1;
    }

    err_t err = udp_bind(pcb, IP_ADDR_ANY, localPort);
    if (err != ERR_OK) {
        udp_remove(pcb);
        cyw43_arch_lwip_end();
        free_socket(fd);
        return -1;
    }

    s->pcb.udp = pcb;
    s->state = STATE_CONNECTED;
    udp_recv(pcb, on_udp_recv, s);
    cyw43_arch_lwip_end();

    return fd;
#else
    return -1;
#endif
}

int Java_pico_net_UDPSocket_udp_1send( void )
{
#if CYW43_LWIP
    int fd   = KNI_GetParameterAsInt(1);
    int port = KNI_GetParameterAsInt(3);
    int len  = KNI_GetParameterAsInt(5);

    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_UDP) return -1;
    socket_ctx_t *s = &sockets[fd];
    if (!s->pcb.udp) return -1;

    char host[128];
    int result = -1;

    KNI_StartHandles(2);
    KNI_DeclareHandle(hostHandle);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, hostHandle);
    KNI_GetParameterAsObject(4, dataHandle);

    kni_string_to_cstr(hostHandle, host, sizeof(host));
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);

    /* Resolve destination */
    ip_addr_t addr;
    if (resolve_host(host, &addr) == 0) {
        cyw43_arch_lwip_begin();
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        if (p) {
            memcpy(p->payload, data, len);
            err_t err = udp_sendto(s->pcb.udp, p, &addr, port);
            pbuf_free(p);
            if (err == ERR_OK) result = len;
        }
        cyw43_arch_lwip_end();
    }

    KNI_EndHandles();

    return result;
#else
    return -1;
#endif
}

int Java_pico_net_UDPSocket_udp_1receive( void )
{
#if CYW43_LWIP
    int fd  = KNI_GetParameterAsInt(1);
    int len = KNI_GetParameterAsInt(3);

    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_UDP) return -1;
    socket_ctx_t *s = &sockets[fd];

    /* Wait until a datagram arrives */
    while (s->udp_pbuf == NULL && s->state != STATE_ERROR) {
        sleep_ms(1);
    }

    if (s->udp_pbuf == NULL) return -1;

    int result = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);

    cyw43_arch_lwip_begin();
    result = (int)s->udp_pbuf->tot_len;
    if (result > len) result = len;
    pbuf_copy_partial(s->udp_pbuf, data, result, 0);
    pbuf_free(s->udp_pbuf);
    s->udp_pbuf = NULL;
    cyw43_arch_lwip_end();

    KNI_EndHandles();

    return result;
#else
    return -1;
#endif
}

void Java_pico_net_UDPSocket_udp_1close( void )
{
#if CYW43_LWIP
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_SOCKETS || sockets[fd].type != SOCK_UDP) return;

    socket_ctx_t *s = &sockets[fd];
    if (s->pcb.udp) {
        cyw43_arch_lwip_begin();
        udp_remove(s->pcb.udp);
        cyw43_arch_lwip_end();
    }
    free_socket(fd);
#endif
}

/* ================================================================
 * MQTT client natives (pico.net.mqtt.MQTTClient)
 * Uses lwIP mqtt_client (apps/mqtt)
 * ================================================================ */

#if CYW43_LWIP && LWIP_MQTT

#include "lwip/apps/mqtt.h"

#define MAX_MQTT_CLIENTS  2
#define MQTT_MSG_BUF_SIZE 1024
#define MQTT_TOPIC_MAX    128

/* Single incoming message buffer (topic + payload) */
typedef struct {
    char   topic[MQTT_TOPIC_MAX];
    int    topic_len;
    uint8_t payload[MQTT_MSG_BUF_SIZE];
    int    payload_len;
    int    payload_total;
    volatile bool ready;     /* complete message waiting to be consumed */
} mqtt_msg_t;

typedef struct {
    mqtt_client_t *client;
    int in_use;
    volatile int conn_status; /* 0 = accepted, 256 = disconnected, etc */
    volatile bool conn_done;  /* connect callback has fired */
    volatile int sub_result;  /* subscribe result: 0=ok, <0=err */
    volatile bool sub_done;
    volatile int pub_result;  /* publish result: 0=ok, <0=err */
    volatile bool pub_done;

    /* Incoming message - double buffer: one being filled, one ready */
    mqtt_msg_t msg;
    /* Temp topic storage while data callback accumulates payload */
    char  incoming_topic[MQTT_TOPIC_MAX];
    int   incoming_topic_len;
    int   incoming_payload_pos;
} mqtt_ctx_t;

static mqtt_ctx_t mqtt_clients[MAX_MQTT_CLIENTS];

static int alloc_mqtt(void)
{
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (!mqtt_clients[i].in_use) {
            memset(&mqtt_clients[i], 0, sizeof(mqtt_ctx_t));
            mqtt_clients[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

static void free_mqtt(int h)
{
    if (h >= 0 && h < MAX_MQTT_CLIENTS) {
        if (mqtt_clients[h].client) {
            mqtt_client_free(mqtt_clients[h].client);
        }
        memset(&mqtt_clients[h], 0, sizeof(mqtt_ctx_t));
    }
}

/* ---- lwIP MQTT callbacks (run in lwIP context) ---- */

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                                mqtt_connection_status_t status)
{
    (void)client;
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)arg;
    ctx->conn_status = (int)status;
    ctx->conn_done = true;
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)arg;
    /* Store topic for upcoming data callbacks */
    int tlen = strlen(topic);
    if (tlen >= MQTT_TOPIC_MAX) tlen = MQTT_TOPIC_MAX - 1;
    memcpy(ctx->incoming_topic, topic, tlen);
    ctx->incoming_topic[tlen] = '\0';
    ctx->incoming_topic_len = tlen;
    ctx->incoming_payload_pos = 0;
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)arg;

    if (data != NULL && len > 0) {
        int space = MQTT_MSG_BUF_SIZE - ctx->incoming_payload_pos;
        int to_copy = (int)len;
        if (to_copy > space) to_copy = space;
        if (to_copy > 0) {
            memcpy(ctx->msg.payload + ctx->incoming_payload_pos, data, to_copy);
            ctx->incoming_payload_pos += to_copy;
        }
    }

    if (flags & MQTT_DATA_FLAG_LAST) {
        /* Message complete — publish to ready slot if not already occupied */
        if (!ctx->msg.ready) {
            memcpy(ctx->msg.topic, ctx->incoming_topic, ctx->incoming_topic_len);
            ctx->msg.topic[ctx->incoming_topic_len] = '\0';
            ctx->msg.topic_len = ctx->incoming_topic_len;
            ctx->msg.payload_len = ctx->incoming_payload_pos;
            ctx->msg.ready = true;
        }
        /* else: drop, previous message not yet consumed */
        ctx->incoming_payload_pos = 0;
    }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)arg;
    ctx->sub_result = (result == ERR_OK) ? 0 : -1;
    ctx->sub_done = true;
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)arg;
    ctx->pub_result = (result == ERR_OK) ? 0 : -1;
    ctx->pub_done = true;
}

#endif /* CYW43_LWIP && LWIP_MQTT */

/* ---- MQTT native functions ---- */

/**
 * Connect to an MQTT broker.
 *
 * @param 1st: broker hostname
 * @param 2nd: port
 * @param 3rd: client ID
 * @param 4th: username (may be null)
 * @param 5th: password (may be null)
 * @return handle (>=0) on success, -1 on error
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1connect( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int port = KNI_GetParameterAsInt(2);

    char broker[128];
    char clientId[64];
    char user[64];
    char pass[64];
    bool has_user = false;
    bool has_pass = false;

    KNI_StartHandles(4);
    KNI_DeclareHandle(brokerHandle);
    KNI_DeclareHandle(clientIdHandle);
    KNI_DeclareHandle(userHandle);
    KNI_DeclareHandle(passHandle);
    KNI_GetParameterAsObject(1, brokerHandle);
    KNI_GetParameterAsObject(3, clientIdHandle);
    KNI_GetParameterAsObject(4, userHandle);
    KNI_GetParameterAsObject(5, passHandle);

    kni_string_to_cstr(brokerHandle, broker, sizeof(broker));
    kni_string_to_cstr(clientIdHandle, clientId, sizeof(clientId));

    if (KNI_GetStringLength(userHandle) >= 0) {
        kni_string_to_cstr(userHandle, user, sizeof(user));
        has_user = true;
    }
    if (KNI_GetStringLength(passHandle) >= 0) {
        kni_string_to_cstr(passHandle, pass, sizeof(pass));
        has_pass = true;
    }

    KNI_EndHandles();

    /* Resolve broker hostname */
    ip_addr_t addr;
    if (resolve_host(broker, &addr) != 0) return -1;

    int h = alloc_mqtt();
    if (h < 0) return -1;

    mqtt_ctx_t *ctx = &mqtt_clients[h];

    cyw43_arch_lwip_begin();
    ctx->client = mqtt_client_new();
    cyw43_arch_lwip_end();

    if (!ctx->client) {
        free_mqtt(h);
        return -1;
    }

    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.client_id = clientId;
    ci.client_user = has_user ? user : NULL;
    ci.client_pass = has_pass ? pass : NULL;
    ci.keep_alive = 60;

    ctx->conn_done = false;
    ctx->conn_status = -1;

    cyw43_arch_lwip_begin();
    /* Set incoming message callbacks */
    mqtt_set_inpub_callback(ctx->client, mqtt_incoming_publish_cb,
                            mqtt_incoming_data_cb, ctx);
    err_t err = mqtt_client_connect(ctx->client, &addr, port,
                                    mqtt_connection_cb, ctx, &ci);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        free_mqtt(h);
        return -1;
    }

    /* Wait for connection result */
    while (!ctx->conn_done) {
        sleep_ms(1);
    }

    if (ctx->conn_status != MQTT_CONNECT_ACCEPTED) {
        free_mqtt(h);
        return -1;
    }

    return h;
#else
    return -1;
#endif
}

/**
 * Publish a message.
 *
 * @param 1st: handle
 * @param 2nd: topic string
 * @param 3rd: byte[] payload
 * @param 4th: payload length
 * @param 5th: qos
 * @param 6th: retain
 * @return 0 on success, -1 on error
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1publish( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h      = KNI_GetParameterAsInt(1);
    int len    = KNI_GetParameterAsInt(4);
    int qos    = KNI_GetParameterAsInt(5);
    int retain = KNI_GetParameterAsInt(6);

    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return -1;
    mqtt_ctx_t *ctx = &mqtt_clients[h];
    if (!mqtt_client_is_connected(ctx->client)) return -1;

    char topic[128];
    err_t err;

    KNI_StartHandles(2);
    KNI_DeclareHandle(topicHandle);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, topicHandle);
    KNI_GetParameterAsObject(3, dataHandle);

    kni_string_to_cstr(topicHandle, topic, sizeof(topic));
    uint8_t *payload = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);

    ctx->pub_done = false;
    ctx->pub_result = -1;

    cyw43_arch_lwip_begin();
    err = mqtt_publish(ctx->client, topic, payload, len,
                       qos, retain, mqtt_pub_request_cb, ctx);
    cyw43_arch_lwip_end();

    KNI_EndHandles();

    if (err != ERR_OK) return -1;

    /* For QoS 1, wait for PUBACK; for QoS 0, callback fires immediately */
    while (!ctx->pub_done) {
        sleep_ms(1);
    }

    return ctx->pub_result;
#else
    return -1;
#endif
}

/**
 * Subscribe to a topic.
 *
 * @param 1st: handle
 * @param 2nd: topic string
 * @param 3rd: qos
 * @return 0 on success, -1 on error
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1subscribe( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h   = KNI_GetParameterAsInt(1);
    int qos = KNI_GetParameterAsInt(3);

    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return -1;
    mqtt_ctx_t *ctx = &mqtt_clients[h];
    if (!mqtt_client_is_connected(ctx->client)) return -1;

    char topic[128];

    KNI_StartHandles(1);
    KNI_DeclareHandle(topicHandle);
    KNI_GetParameterAsObject(2, topicHandle);
    kni_string_to_cstr(topicHandle, topic, sizeof(topic));
    KNI_EndHandles();

    ctx->sub_done = false;
    ctx->sub_result = -1;

    cyw43_arch_lwip_begin();
    err_t err = mqtt_sub_unsub(ctx->client, topic, qos,
                               mqtt_sub_request_cb, ctx, 1);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) return -1;

    /* Wait for SUBACK */
    while (!ctx->sub_done) {
        sleep_ms(1);
    }

    return ctx->sub_result;
#else
    return -1;
#endif
}

/**
 * Unsubscribe from a topic.
 *
 * @param 1st: handle
 * @param 2nd: topic string
 * @return 0 on success, -1 on error
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1unsubscribe( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h = KNI_GetParameterAsInt(1);

    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return -1;
    mqtt_ctx_t *ctx = &mqtt_clients[h];
    if (!mqtt_client_is_connected(ctx->client)) return -1;

    char topic[128];

    KNI_StartHandles(1);
    KNI_DeclareHandle(topicHandle);
    KNI_GetParameterAsObject(2, topicHandle);
    kni_string_to_cstr(topicHandle, topic, sizeof(topic));
    KNI_EndHandles();

    ctx->sub_done = false;
    ctx->sub_result = -1;

    cyw43_arch_lwip_begin();
    err_t err = mqtt_sub_unsub(ctx->client, topic, 0,
                               mqtt_sub_request_cb, ctx, 0);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) return -1;

    while (!ctx->sub_done) {
        sleep_ms(1);
    }

    return ctx->sub_result;
#else
    return -1;
#endif
}

/**
 * Receive the next incoming message.
 *
 * @param 1st: handle
 * @param 2nd: byte[] topicBuf
 * @param 3rd: byte[] payloadBuf
 * @param 4th: timeoutMs
 * @return >0: (topicLen << 16 | payloadLen), 0 = timeout, -1 = error
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1receive( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h         = KNI_GetParameterAsInt(1);
    int timeoutMs = KNI_GetParameterAsInt(4);

    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return -1;
    mqtt_ctx_t *ctx = &mqtt_clients[h];

    /* Wait for a message or timeout */
    int waited = 0;
    while (!ctx->msg.ready) {
        if (!mqtt_client_is_connected(ctx->client)) return -1;
        if (timeoutMs > 0 && waited >= timeoutMs) return 0;
        sleep_ms(1);
        waited++;
    }

    int topicLen = ctx->msg.topic_len;
    int payloadLen = ctx->msg.payload_len;
    int result;

    KNI_StartHandles(2);
    KNI_DeclareHandle(topicBufHandle);
    KNI_DeclareHandle(payloadBufHandle);
    KNI_GetParameterAsObject(2, topicBufHandle);
    KNI_GetParameterAsObject(3, payloadBufHandle);

    uint8_t *topicBuf = (uint8_t *)SNI_GetRawArrayPointer(topicBufHandle);
    uint8_t *payloadBuf = (uint8_t *)SNI_GetRawArrayPointer(payloadBufHandle);

    /* Copy topic */
    int tCopy = topicLen;
    if (tCopy > KNI_GetArrayLength(topicBufHandle)) {
        tCopy = KNI_GetArrayLength(topicBufHandle);
    }
    memcpy(topicBuf, ctx->msg.topic, tCopy);

    /* Copy payload */
    int pCopy = payloadLen;
    if (pCopy > KNI_GetArrayLength(payloadBufHandle)) {
        pCopy = KNI_GetArrayLength(payloadBufHandle);
    }
    memcpy(payloadBuf, ctx->msg.payload, pCopy);

    /* Mark consumed */
    ctx->msg.ready = false;

    result = (tCopy << 16) | (pCopy & 0xFFFF);

    KNI_EndHandles();

    return result;
#else
    return -1;
#endif
}

/**
 * Check if connected.
 *
 * @param 1st: handle
 * @return 1 if connected, 0 otherwise
 */
int Java_pico_net_mqtt_MQTTClient_mqtt_1is_1connected( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h = KNI_GetParameterAsInt(1);
    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return 0;
    return mqtt_client_is_connected(mqtt_clients[h].client) ? 1 : 0;
#else
    return 0;
#endif
}

/**
 * Disconnect and free resources.
 *
 * @param 1st: handle
 */
void Java_pico_net_mqtt_MQTTClient_mqtt_1disconnect( void )
{
#if CYW43_LWIP && LWIP_MQTT
    int h = KNI_GetParameterAsInt(1);
    if (h < 0 || h >= MAX_MQTT_CLIENTS || !mqtt_clients[h].in_use) return;

    mqtt_ctx_t *ctx = &mqtt_clients[h];
    if (ctx->client && mqtt_client_is_connected(ctx->client)) {
        cyw43_arch_lwip_begin();
        mqtt_disconnect(ctx->client);
        cyw43_arch_lwip_end();
    }
    free_mqtt(h);
#endif
}

} /* extern "C" */
