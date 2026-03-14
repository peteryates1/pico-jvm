/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * Low-level flash memory access. Provides erase, program, and read
 * operations on the on-board flash (XIP) memory.
 *
 * <p>Flash is organized in 4KB sectors (erase unit) and 256-byte pages
 * (program unit). All offsets are relative to the start of flash
 * (0x10000000 on RP2040/RP2350), not absolute addresses.</p>
 *
 * <p><b>Warning:</b> Flash operations temporarily disable XIP. Do not
 * call from interrupt handlers or while the other core is executing
 * from flash.</p>
 */
public class Flash {

    /** Flash sector size: 4096 bytes (minimum erase unit) */
    public static final int SECTOR_SIZE = 4096;

    /** Flash page size: 256 bytes (minimum program unit) */
    public static final int PAGE_SIZE = 256;

    /** Flash block size: 65536 bytes */
    public static final int BLOCK_SIZE = 65536;

    /**
     * Erase a range of flash memory. Both offset and length must be
     * aligned to {@link #SECTOR_SIZE} (4096 bytes).
     *
     * @param offset the flash offset to start erasing (must be sector-aligned)
     * @param length the number of bytes to erase (must be sector-aligned)
     * @throws IOException if the parameters are not sector-aligned
     */
    public static void erase(int offset, int length) throws IOException {
        if ((offset & (SECTOR_SIZE - 1)) != 0 || (length & (SECTOR_SIZE - 1)) != 0) {
            throw new IOException("Offset and length must be sector-aligned (4096)");
        }
        flash_erase(offset, length);
    }

    /**
     * Program (write) data to flash. The offset must be aligned to
     * {@link #PAGE_SIZE} (256 bytes) and the data length must be a
     * multiple of 256.
     *
     * <p>The target region must be erased first via {@link #erase}.</p>
     *
     * @param offset the flash offset to start writing (must be page-aligned)
     * @param data   the data to write
     * @param off    offset into the data array
     * @param len    number of bytes to write (must be a multiple of 256)
     * @throws IOException if the parameters are not page-aligned
     */
    public static void program(int offset, byte[] data, int off, int len) throws IOException {
        if ((offset & (PAGE_SIZE - 1)) != 0 || (len & (PAGE_SIZE - 1)) != 0) {
            throw new IOException("Offset and length must be page-aligned (256)");
        }
        if (data == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off + len > data.length) {
            throw new IndexOutOfBoundsException();
        }
        flash_program(offset, data, off, len);
    }

    /**
     * Read data from flash memory via XIP (execute-in-place) mapping.
     *
     * @param offset the flash offset to start reading
     * @param data   the buffer to read into
     * @param off    offset into the buffer
     * @param len    number of bytes to read
     */
    public static void read(int offset, byte[] data, int off, int len) {
        if (data == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off + len > data.length) {
            throw new IndexOutOfBoundsException();
        }
        flash_read(offset, data, off, len);
    }

    /*
     * Natives for the device hardware
     */
    private static native void flash_erase(int offset, int length);
    private static native void flash_program(int offset, byte[] data, int off, int len);
    private static native void flash_read(int offset, byte[] data, int off, int len);
}
