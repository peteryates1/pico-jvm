/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * Hardware-seeded random number generator.
 *
 * <p>Uses the Pico SDK's <code>pico_rand</code> module which combines
 * multiple entropy sources (hardware RNG on RP2350, ring oscillator,
 * timer) to seed a 128-bit PRNG.</p>
 *
 * <p>This provides higher quality randomness than
 * <code>java.util.Random</code>, which uses a simple linear
 * congruential generator.</p>
 */
public class RNG {

    /**
     * Get a random 32-bit integer.
     *
     * @return a random int
     */
    public static int nextInt() {
        return rng_get_int();
    }

    /**
     * Get a random integer in the range [0, bound).
     *
     * @param bound the upper bound (exclusive), must be positive
     * @return a random int between 0 (inclusive) and bound (exclusive)
     * @throws IllegalArgumentException if bound is not positive
     */
    public static int nextInt(int bound) {
        if (bound <= 0) {
            throw new IllegalArgumentException();
        }
        // Use masking for power-of-2 bounds, modulo otherwise
        int r = rng_get_int();
        if (r < 0) r = -r;
        if (r < 0) r = 0; // handle Integer.MIN_VALUE
        return r % bound;
    }

    /**
     * Get a random long (64-bit) value.
     *
     * @return a random long
     */
    public static long nextLong() {
        return rng_get_long();
    }

    /**
     * Fill a byte array with random bytes.
     *
     * @param bytes the array to fill
     */
    public static void nextBytes(byte[] bytes) {
        if (bytes == null) {
            throw new NullPointerException();
        }
        rng_get_bytes(bytes, bytes.length);
    }

    /*
     * Natives
     */
    private static native int  rng_get_int();
    private static native long rng_get_long();
    private static native void rng_get_bytes(byte[] buf, int len);
}
