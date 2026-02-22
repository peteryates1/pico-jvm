/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * A <code>I2CBus</code> represents an I2C bus on the Pico.
 * The Pico has two I2C instances (0 and 1). Each instance can be
 * mapped to specific GPIO pins for SDA and SCL. Internal pull-ups
 * are enabled on both pins.
 */
public class I2CBus {

    /**
     * The I2C instance number (0 or 1)
     */
    private int instance;

    /**
     * Create an I2C bus on the specified instance and pins.
     *
     * @param instance The I2C instance (0 or 1)
     * @param sdaPin   The GPIO pin for SDA
     * @param sclPin   The GPIO pin for SCL
     * @param baudrate The I2C clock rate in Hz (e.g. 100000 or 400000)
     *
     * @throws IOException if the I2C bus cannot be initialized
     */
    public I2CBus(int instance, int sdaPin, int sclPin, int baudrate) throws IOException {
        if (instance < 0 || instance > 1) {
            throw new IllegalArgumentException();
        }
        this.instance = instance;
        int result = i2c_init(instance, sdaPin, sclPin, baudrate);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Write data to a device on the I2C bus.
     *
     * @param addr The 7-bit I2C slave address
     * @param data The byte array to write
     * @param len  The number of bytes to write
     *
     * @return the number of bytes written
     * @throws IOException if the write fails (e.g. NACK)
     */
    public int write(int addr, byte[] data, int len) throws IOException {
        int result = i2c_write(this.instance, addr, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Read data from a device on the I2C bus.
     *
     * @param addr The 7-bit I2C slave address
     * @param data The byte array to read into
     * @param len  The number of bytes to read
     *
     * @return the number of bytes read
     * @throws IOException if the read fails (e.g. NACK)
     */
    public int read(int addr, byte[] data, int len) throws IOException {
        int result = i2c_read(this.instance, addr, data, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Write data then read data from a device on the I2C bus in a single
     * transaction (repeated start). This is common for reading registers.
     *
     * @param addr  The 7-bit I2C slave address
     * @param wData The byte array to write
     * @param wLen  The number of bytes to write
     * @param rData The byte array to read into
     * @param rLen  The number of bytes to read
     *
     * @return the number of bytes read
     * @throws IOException if the operation fails
     */
    public int writeRead(int addr, byte[] wData, int wLen, byte[] rData, int rLen) throws IOException {
        int result = i2c_write_read(this.instance, addr, wData, wLen, rData, rLen);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /*
     * Natives for the device hardware
     */
    private static native int i2c_init       ( int instance, int sdaPin, int sclPin, int baudrate );
    private static native int i2c_write      ( int instance, int addr, byte[] data, int len );
    private static native int i2c_read       ( int instance, int addr, byte[] data, int len );
    private static native int i2c_write_read ( int instance, int addr, byte[] wData, int wLen, byte[] rData, int rLen );

}
