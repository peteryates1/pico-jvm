/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * A <code>SystemTimer</code> provides access to the Pico's hardware timer.
 * All methods are static as there is only one system timer.
 * The timer runs at 1MHz and provides microsecond resolution.
 */
public class SystemTimer {

    /**
     * Get the current time in milliseconds since boot.
     *
     * @return the time in milliseconds
     */
    public static int getTimeMs() {
        return timer_get_ms();
    }

    /**
     * Get the current time in microseconds since boot.
     * Note: returns the low 32 bits of the 64-bit timer.
     *
     * @return the time in microseconds (low 32 bits)
     */
    public static int getTimeUs() {
        return timer_get_us();
    }

    /**
     * Busy-wait for the specified number of microseconds.
     *
     * @param us The number of microseconds to wait
     */
    public static void delayUs(int us) {
        timer_delay_us(us);
    }

    /**
     * Busy-wait for the specified number of milliseconds.
     *
     * @param ms The number of milliseconds to wait
     */
    public static void delayMs(int ms) {
        timer_delay_us(ms * 1000);
    }

    /*
     * Natives for the device hardware
     */
    private static native int  timer_get_ms   ();
    private static native int  timer_get_us   ();
    private static native void timer_delay_us ( int us );

}
