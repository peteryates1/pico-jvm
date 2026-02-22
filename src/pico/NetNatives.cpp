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

/*
 * Networking natives are stubs when using pico_cyw43_arch_none.
 * Full implementation requires pico_cyw43_arch_lwip_threadsafe_background
 * with raw lwIP callback API (BSD sockets need NO_SYS=0 / RTOS).
 */

extern "C" {

/* WiFi stubs */
int    Java_pico_net_WiFi_wifi_1init( void )        { return -1; }
int    Java_pico_net_WiFi_wifi_1connect( void )     { return -1; }
void   Java_pico_net_WiFi_wifi_1disconnect( void )  { }
int    Java_pico_net_WiFi_wifi_1get_1status( void ) { return -1; }

jobject Java_pico_net_WiFi_wifi_1get_1ip( void )
{
    KNI_StartHandles(1);
    KNI_DeclareHandle(resultHandle);
    KNI_NewStringUTF("0.0.0.0", resultHandle);
    KNI_EndHandlesAndReturnObject(resultHandle);
}

/* TCPSocket stubs */
int    Java_pico_net_TCPSocket_tcp_1connect( void )   { return -1; }
int    Java_pico_net_TCPSocket_tcp_1send( void )      { return -1; }
int    Java_pico_net_TCPSocket_tcp_1receive( void )   { return -1; }
int    Java_pico_net_TCPSocket_tcp_1available( void ) { return 0; }
void   Java_pico_net_TCPSocket_tcp_1close( void )     { }

/* UDPSocket stubs */
int    Java_pico_net_UDPSocket_udp_1bind( void )      { return -1; }
int    Java_pico_net_UDPSocket_udp_1send( void )      { return -1; }
int    Java_pico_net_UDPSocket_udp_1receive( void )   { return -1; }
void   Java_pico_net_UDPSocket_udp_1close( void )     { }

} /* extern "C" */
