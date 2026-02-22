/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.net;

import java.io.IOException;

/**
 * A <code>UDPSocket</code> represents a UDP socket bound to a local port.
 * Provides a simple blocking API for UDP communication over WiFi.
 */
public class UDPSocket {

    /**
     * The native socket file descriptor
     */
    private int fd;

    /**
     * Create a UDP socket bound to the specified local port.
     *
     * @param localPort The local port to bind to (0 for any available port)
     *
     * @throws IOException if the socket cannot be created or bound
     */
    public UDPSocket(int localPort) throws IOException {
        this.fd = udp_bind(localPort);
        if (this.fd < 0) {
            throw new IOException();
        }
    }

    /**
     * Send a UDP datagram to the specified host and port.
     *
     * @param host The destination hostname or IP address
     * @param port The destination port number
     * @param data The byte array containing data to send
     * @param len  The number of bytes to send
     *
     * @return the number of bytes sent
     * @throws IOException if the send fails
     */
    public int send(String host, int port, byte[] data, int len) throws IOException {
        int result = udp_send(this.fd, host, port, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Receive a UDP datagram (blocks until data arrives).
     *
     * @param data The byte array to receive into
     * @param len  The maximum number of bytes to receive
     *
     * @return the number of bytes received
     * @throws IOException if the receive fails
     */
    public int receive(byte[] data, int len) throws IOException {
        int result = udp_receive(this.fd, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Close the UDP socket and release resources.
     */
    public void close() {
        if (this.fd >= 0) {
            udp_close(this.fd);
            this.fd = -1;
        }
    }

    /*
     * Natives for the device hardware
     */
    private static native int  udp_bind    ( int localPort );
    private static native int  udp_send    ( int fd, String host, int port, byte[] data, int len );
    private static native int  udp_receive ( int fd, byte[] data, int len );
    private static native void udp_close   ( int fd );

}
