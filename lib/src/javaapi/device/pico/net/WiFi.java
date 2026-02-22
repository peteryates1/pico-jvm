/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.net;

import java.io.IOException;

/**
 * A <code>WiFi</code> provides access to the Pico W's CYW43 WiFi module.
 * All methods are static. The WiFi module must be initialized before use
 * and connected to an access point before network operations.
 */
public class WiFi {

    /**
     * Open authentication (no password)
     */
    public static final int AUTH_OPEN = 0;

    /**
     * WPA/WPA2 mixed mode authentication
     */
    public static final int AUTH_WPA_WPA2 = 1;

    /**
     * WPA2 only authentication
     */
    public static final int AUTH_WPA2 = 2;

    /**
     * WiFi status: link down
     */
    public static final int STATUS_LINK_DOWN = 0;

    /**
     * WiFi status: connected
     */
    public static final int STATUS_JOINED = 1;

    /**
     * WiFi status: connection failed
     */
    public static final int STATUS_FAIL = -1;

    /**
     * WiFi status: no matching SSID found
     */
    public static final int STATUS_NO_MATCH = -2;

    /**
     * WiFi status: authentication failure
     */
    public static final int STATUS_BAD_AUTH = -3;

    /**
     * Initialize the WiFi module and enable station mode.
     *
     * @throws IOException if initialization fails
     */
    public static void init() throws IOException {
        int result = wifi_init();
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Connect to a WiFi access point.
     *
     * @param ssid      The SSID of the access point
     * @param password  The password (null for open networks)
     * @param auth      The authentication type (AUTH_OPEN, AUTH_WPA_WPA2, AUTH_WPA2)
     * @param timeoutMs The connection timeout in milliseconds
     *
     * @throws IOException if the connection fails
     */
    public static void connect(String ssid, String password, int auth, int timeoutMs) throws IOException {
        int result = wifi_connect(ssid, password, auth, timeoutMs);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Disconnect from the current WiFi network.
     */
    public static void disconnect() {
        wifi_disconnect();
    }

    /**
     * Get the current WiFi connection status.
     *
     * @return one of the STATUS_* constants
     */
    public static int getStatus() {
        return wifi_get_status();
    }

    /**
     * Get the current IP address as a string (e.g. "192.168.1.100").
     * Only valid after a successful connection.
     *
     * @return the IP address string, or null if not connected
     */
    public static String getIPAddress() {
        return wifi_get_ip();
    }

    /*
     * Natives for the device hardware
     */
    private static native int    wifi_init       ();
    private static native int    wifi_connect    ( String ssid, String password, int auth, int timeoutMs );
    private static native void   wifi_disconnect ();
    private static native int    wifi_get_status ();
    private static native String wifi_get_ip     ();

}
