/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * A <code>UARTPort</code> represents a UART serial port on the Pico.
 * The Pico has two UART instances (0 and 1). Each instance can be
 * mapped to specific GPIO pins for TX and RX.
 */
public class UARTPort {

    /**
     * The UART instance number (0 or 1)
     */
    private int instance;

    /**
     * Create a UART port on the specified instance and pins.
     *
     * @param instance The UART instance (0 or 1)
     * @param txPin    The GPIO pin for TX
     * @param rxPin    The GPIO pin for RX
     * @param baudrate The baud rate
     *
     * @throws IOException if the UART cannot be initialized
     */
    public UARTPort(int instance, int txPin, int rxPin, int baudrate) throws IOException {
        if (instance < 0 || instance > 1) {
            throw new IllegalArgumentException();
        }
        this.instance = instance;
        int result = uart_init(instance, txPin, rxPin, baudrate);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Write a block of data to the UART.
     *
     * @param data The byte array to write
     * @param len  The number of bytes to write
     *
     * @throws IOException if the write fails
     */
    public void write(byte[] data, int len) throws IOException {
        int result = uart_write(this.instance, data, len);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Read a block of data from the UART.
     *
     * @param data The byte array to read into
     * @param len  The number of bytes to read
     *
     * @return the number of bytes actually read
     * @throws IOException if the read fails
     */
    public int read(byte[] data, int len) throws IOException {
        int result = uart_read(this.instance, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Return the number of bytes available to read without blocking.
     *
     * @return the number of bytes available
     */
    public int available() {
        return uart_available(this.instance);
    }

    /**
     * Write a single byte to the UART.
     *
     * @param b The byte to write
     */
    public void writeByte(int b) {
        uart_write_byte(this.instance, b);
    }

    /**
     * Read a single byte from the UART (blocks until data is available).
     *
     * @return the byte read
     */
    public int readByte() {
        return uart_read_byte(this.instance);
    }

    /*
     * Natives for the device hardware
     */
    private static native int  uart_init       ( int instance, int txPin, int rxPin, int baudrate );
    private static native int  uart_write      ( int instance, byte[] data, int len );
    private static native int  uart_read       ( int instance, byte[] data, int len );
    private static native int  uart_available  ( int instance );
    private static native void uart_write_byte ( int instance, int b );
    private static native int  uart_read_byte  ( int instance );

}
