/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * A <code>OnboardLED</code> controls the onboard LED on the Pico or Pico W.
 * On the regular Pico this is GPIO 25. On the Pico W this is the CYW43
 * wireless chip LED (WL_GPIO0).
 */
public class OnboardLED {

    /**
     * Create and initialize the onboard LED.
     */
    public OnboardLED() {
        led_init();
    }

    /**
     * Turn the LED on.
     */
    public void on() {
        led_set(1);
    }

    /**
     * Turn the LED off.
     */
    public void off() {
        led_set(0);
    }

    /*
     * Natives for the device hardware
     */
    private static native void led_init ();
    private static native void led_set  ( int state );

}
