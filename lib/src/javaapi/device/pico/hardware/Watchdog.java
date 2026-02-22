/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * A <code>Watchdog</code> provides access to the Pico's hardware watchdog timer.
 * The watchdog will reset the system if it is not updated within the configured
 * timeout period. All methods are static.
 */
public class Watchdog {

    /**
     * Enable the watchdog timer.
     *
     * @param delayMs        The watchdog timeout in milliseconds (max ~8300ms)
     * @param pauseOnDebug   If true, the watchdog pauses when the debugger is active
     */
    public static void enable(int delayMs, boolean pauseOnDebug) {
        watchdog_enable(delayMs, pauseOnDebug ? 1 : 0);
    }

    /**
     * Update (pet/kick) the watchdog to prevent a reset.
     * This must be called periodically within the configured timeout.
     */
    public static void update() {
        watchdog_update();
    }

    /**
     * Check if the last system reset was caused by the watchdog.
     *
     * @return true if the watchdog caused the last reset
     */
    public static boolean causedReboot() {
        return watchdog_caused_reboot() != 0;
    }

    /*
     * Natives for the device hardware
     */
    private static native void watchdog_enable         ( int delayMs, int pauseOnDebug );
    private static native void watchdog_update         ();
    private static native int  watchdog_caused_reboot  ();

}
