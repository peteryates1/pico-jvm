/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * On-chip temperature sensor. Uses the ADC's built-in temperature
 * sensor channel to read the die temperature.
 *
 * <p>The temperature sensor is on ADC input 4 (RP2040/RP2350 QFN-60)
 * or ADC input 8 (RP2350 QFN-80).</p>
 */
public class TempSensor {

    /**
     * Read the on-chip temperature in degrees Celsius, multiplied by 100
     * (e.g. 2705 = 27.05 C). This avoids floating point.
     *
     * <p>The ADC is temporarily reconfigured to read the temperature
     * sensor channel, then restored.</p>
     *
     * @return the temperature in centi-degrees Celsius
     */
    public static int readCentiCelsius() {
        return temp_read();
    }

    /*
     * Natives for the device hardware
     */
    private static native int temp_read();
}
