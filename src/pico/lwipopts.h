#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* Prevent lwIP from redefining struct timeval */
#define LWIP_TIMEVAL_PRIVATE    0

/* System - NO_SYS=1 means bare-metal (no RTOS) */
/* BSD sockets and netconn require NO_SYS=0, so we disable them */
/* TCP/UDP use the raw lwIP callback API instead */
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Protocols */
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1

/* Network interface */
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1

/* Memory */
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_NETBUF             8
#define PBUF_POOL_SIZE              24

#endif
