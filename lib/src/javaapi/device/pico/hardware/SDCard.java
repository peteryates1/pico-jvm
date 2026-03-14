/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * An <code>SDCard</code> provides access to an SD card connected via SPI.
 * The SD card must be mounted before any file I/O operations can be performed.
 *
 * <p>Default SPI0 pin assignment: MISO=GP16, CS=GP17, SCK=GP18, MOSI=GP19.
 */
public class SDCard {

    /** Default SPI instance (SPI0) */
    public static final int DEFAULT_SPI  = 0;

    /** Default SCK pin (GP18) */
    public static final int DEFAULT_SCK  = 18;

    /** Default MOSI pin (GP19) */
    public static final int DEFAULT_MOSI = 19;

    /** Default MISO pin (GP16) */
    public static final int DEFAULT_MISO = 16;

    /** Default CS pin (GP17) */
    public static final int DEFAULT_CS   = 17;

    /**
     * Mount the SD card using default SPI0 pins.
     *
     * @throws IOException if the SD card cannot be mounted
     */
    public static void mount() throws IOException {
        mount(DEFAULT_SPI, DEFAULT_SCK, DEFAULT_MOSI, DEFAULT_MISO, DEFAULT_CS);
    }

    /**
     * Mount the SD card using the specified SPI pins.
     *
     * @param spi  the SPI instance (0 or 1)
     * @param sck  the SCK pin
     * @param mosi the MOSI pin
     * @param miso the MISO pin
     * @param cs   the CS pin
     * @throws IOException if the SD card cannot be mounted
     */
    public static void mount(int spi, int sck, int mosi, int miso, int cs) throws IOException {
        int result = sd_mount(spi, sck, mosi, miso, cs);
        if (result < 0) {
            throw new IOException("SD card mount failed");
        }
    }

    /**
     * Unmount the SD card.
     *
     * @throws IOException if the SD card cannot be unmounted
     */
    public static void unmount() throws IOException {
        sd_unmount();
    }

    /**
     * Check whether the SD card is currently mounted.
     *
     * @return <code>true</code> if the SD card is mounted
     */
    public static boolean isMounted() {
        return sd_is_mounted() != 0;
    }

    /*
     * Natives for the device hardware
     */
    private static native int  sd_mount(int spi, int sck, int mosi, int miso, int cs);
    private static native void sd_unmount();
    private static native int  sd_is_mounted();
}
