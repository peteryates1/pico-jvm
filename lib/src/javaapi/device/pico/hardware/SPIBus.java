/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * A <code>SPIBus</code> represents an SPI bus on the Pico.
 * The Pico has two SPI instances (0 and 1). Each instance can be
 * mapped to specific GPIO pins for SCK, MOSI, and MISO. Chip select
 * is managed separately via a GPIOPin.
 */
public class SPIBus {

    /**
     * The SPI instance number (0 or 1)
     */
    private int instance;

    /**
     * Create an SPI bus on the specified instance and pins.
     *
     * @param instance The SPI instance (0 or 1)
     * @param sckPin   The GPIO pin for SCK (clock)
     * @param mosiPin  The GPIO pin for MOSI (TX)
     * @param misoPin  The GPIO pin for MISO (RX)
     * @param baudrate The SPI clock rate in Hz
     *
     * @throws IOException if the SPI bus cannot be initialized
     */
    public SPIBus(int instance, int sckPin, int mosiPin, int misoPin, int baudrate) throws IOException {
        if (instance < 0 || instance > 1) {
            throw new IllegalArgumentException();
        }
        this.instance = instance;
        int result = spi_init(instance, sckPin, mosiPin, misoPin, baudrate);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Write data to the SPI bus.
     *
     * @param data The byte array to write
     * @param len  The number of bytes to write
     *
     * @return the number of bytes written
     * @throws IOException if the write fails
     */
    public int write(byte[] data, int len) throws IOException {
        int result = spi_write(this.instance, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Read data from the SPI bus. Sends zeros while reading.
     *
     * @param data The byte array to read into
     * @param len  The number of bytes to read
     *
     * @return the number of bytes read
     * @throws IOException if the read fails
     */
    public int read(byte[] data, int len) throws IOException {
        int result = spi_read(this.instance, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Simultaneously write and read data on the SPI bus (full duplex).
     *
     * @param tx  The byte array to write
     * @param rx  The byte array to read into
     * @param len The number of bytes to transfer
     *
     * @return the number of bytes transferred
     * @throws IOException if the transfer fails
     */
    public int writeRead(byte[] tx, byte[] rx, int len) throws IOException {
        int result = spi_write_read(this.instance, tx, rx, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /*
     * Natives for the device hardware
     */
    private static native int spi_init       ( int instance, int sckPin, int mosiPin, int misoPin, int baudrate );
    private static native int spi_write      ( int instance, byte[] data, int len );
    private static native int spi_read       ( int instance, byte[] data, int len );
    private static native int spi_write_read ( int instance, byte[] tx, byte[] rx, int len );

}
