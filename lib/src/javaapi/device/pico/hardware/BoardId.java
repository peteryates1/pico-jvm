/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * Access the board's unique 64-bit identifier.
 *
 * <p>On RP2040, this is read from the external flash chip's unique ID.
 * On RP2350, this is read from OTP memory.</p>
 */
public class BoardId {

    /**
     * Get the board's unique identifier as a hex string
     * (16 characters, e.g. "E6614103E72B6A25").
     *
     * @return the unique board ID as a hex string
     */
    public static String getId() {
        return board_get_id();
    }

    /**
     * Get the board's unique identifier as a byte array (8 bytes).
     *
     * @return the 8-byte unique board ID
     */
    public static byte[] getIdBytes() {
        byte[] id = new byte[8];
        board_get_id_bytes(id);
        return id;
    }

    /*
     * Natives for the device hardware
     */
    private static native String board_get_id();
    private static native void   board_get_id_bytes(byte[] buf);
}
