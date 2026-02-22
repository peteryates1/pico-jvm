/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * A <code>PWMChannel</code> represents a single PWM output on a GPIO pin.
 * The Pico has 8 PWM slices, each with two channels (A and B). The slice
 * and channel are determined automatically from the GPIO pin number.
 */
public class PWMChannel {

    /**
     * The GPIO pin number used for this PWM channel
     */
    private int gpioPin;

    /**
     * Create a PWM channel on the specified GPIO pin.
     *
     * @param gpioPin     The GPIO pin number
     * @param freqHz      The desired PWM frequency in Hz
     * @param dutyPercent The initial duty cycle (0-100)
     *
     * @throws IOException if the PWM channel cannot be initialized
     */
    public PWMChannel(int gpioPin, int freqHz, int dutyPercent) throws IOException {
        this.gpioPin = gpioPin;
        int result = pwm_init(gpioPin, freqHz, dutyPercent);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Set the duty cycle of this PWM channel.
     *
     * @param percent The duty cycle (0-100)
     */
    public void setDuty(int percent) {
        pwm_set_duty(this.gpioPin, percent);
    }

    /**
     * Enable the PWM output on this channel.
     */
    public void enable() {
        pwm_set_enabled(this.gpioPin, 1);
    }

    /**
     * Disable the PWM output on this channel.
     */
    public void disable() {
        pwm_set_enabled(this.gpioPin, 0);
    }

    /*
     * Natives for the device hardware
     */
    private static native int  pwm_init        ( int gpioPin, int freqHz, int dutyPercent );
    private static native void pwm_set_duty    ( int gpioPin, int dutyPercent );
    private static native void pwm_set_enabled ( int gpioPin, int enabled );

}
