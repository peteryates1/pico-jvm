#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* Prevent lwIP from redefining struct timeval */
#define LWIP_TIMEVAL_PRIVATE    0

/* System - NO_SYS=1 means bare-metal (no RTOS) */
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Protocols */
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_RAW                    1

/* Applications */
#define LWIP_MQTT                   1
#define MQTT_OUTPUT_RINGBUF_SIZE    512
#define MQTT_VAR_HEADER_BUFFER_LEN  256

/* Network interface */
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1

/* TCP tuning */
#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_UDP_PCB            8

/* Memory pool */
#define MEM_SIZE                    8000
#define PBUF_POOL_SIZE              16
#define PBUF_POOL_BUFSIZE           1600

#endif
