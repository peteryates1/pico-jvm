/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.net;

import java.io.IOException;

/**
 * A <code>TCPSocket</code> represents a TCP client socket connection.
 * Provides a simple blocking API for TCP communication over WiFi.
 */
public class TCPSocket {

    /**
     * The native socket file descriptor
     */
    private int fd;

    /**
     * Create a TCP socket and connect to the specified host and port.
     *
     * @param host The hostname or IP address to connect to
     * @param port The TCP port number
     *
     * @throws IOException if the connection fails
     */
    public TCPSocket(String host, int port) throws IOException {
        this.fd = tcp_connect(host, port);
        if (this.fd < 0) {
            throw new IOException();
        }
    }

    /**
     * Send data over the TCP connection.
     *
     * @param data   The byte array containing data to send
     * @param offset The starting offset in the array
     * @param len    The number of bytes to send
     *
     * @return the number of bytes actually sent
     * @throws IOException if the send fails
     */
    public int send(byte[] data, int offset, int len) throws IOException {
        int result = tcp_send(this.fd, data, offset, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Receive data from the TCP connection.
     *
     * @param data   The byte array to receive into
     * @param offset The starting offset in the array
     * @param len    The maximum number of bytes to receive
     *
     * @return the number of bytes received, or -1 if the connection was closed
     * @throws IOException if the receive fails
     */
    public int receive(byte[] data, int offset, int len) throws IOException {
        int result = tcp_receive(this.fd, data, offset, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Get the number of bytes available to read without blocking.
     *
     * @return the number of bytes available
     */
    public int available() {
        return tcp_available(this.fd);
    }

    /**
     * Close the TCP connection and release resources.
     */
    public void close() {
        if (this.fd >= 0) {
            tcp_close(this.fd);
            this.fd = -1;
        }
    }

    /*
     * Natives for the device hardware
     */
    private static native int  tcp_connect   ( String host, int port );
    private static native int  tcp_send      ( int fd, byte[] data, int offset, int len );
    private static native int  tcp_receive   ( int fd, byte[] data, int offset, int len );
    private static native int  tcp_available ( int fd );
    private static native void tcp_close     ( int fd );

}
