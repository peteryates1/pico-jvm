/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

import java.io.IOException;

/**
 * A <code>PIOStateMachine</code> represents a single state machine within
 * one of the Pico's two PIO (Programmable I/O) blocks. Each PIO block has
 * 4 state machines (0-3) that can run custom programs independently.
 */
public class PIOStateMachine {

    /**
     * PIO instance 0
     */
    public static final int PIO0 = 0;

    /**
     * PIO instance 1
     */
    public static final int PIO1 = 1;

    /**
     * The PIO instance (0 or 1)
     */
    private int pioInstance;

    /**
     * The state machine number (0-3)
     */
    private int sm;

    /**
     * Create a PIO state machine handle.
     *
     * @param pioInstance The PIO instance ({@link #PIO0} or {@link #PIO1})
     * @param sm          The state machine number (0-3)
     */
    public PIOStateMachine(int pioInstance, int sm) {
        if (pioInstance < 0 || pioInstance > 1) {
            throw new IllegalArgumentException();
        }
        if (sm < 0 || sm > 3) {
            throw new IllegalArgumentException();
        }
        this.pioInstance = pioInstance;
        this.sm = sm;
    }

    /**
     * Load a PIO program into instruction memory.
     *
     * @param instructions The PIO instructions as an int array
     * @param len          The number of instructions
     *
     * @return the offset in instruction memory where the program was loaded
     * @throws IOException if the program cannot be loaded (e.g. no space)
     */
    public int loadProgram(int[] instructions, int len) throws IOException {
        int result = pio_load_program(this.pioInstance, instructions, len);
        if (result < 0) {
            throw new IOException();
        }
        return result;
    }

    /**
     * Initialize the state machine with a loaded program.
     *
     * @param offset    The instruction memory offset (from loadProgram)
     * @param pin       The base GPIO pin
     * @param clockDiv  The clock divider (integer part, 1-65535)
     *
     * @throws IOException if initialization fails
     */
    public void init(int offset, int pin, int clockDiv) throws IOException {
        int result = pio_sm_init(this.pioInstance, this.sm, offset, pin, clockDiv);
        if (result < 0) {
            throw new IOException();
        }
    }

    /**
     * Enable or disable the state machine.
     *
     * @param enabled true to enable, false to disable
     */
    public void setEnabled(boolean enabled) {
        pio_sm_set_enabled(this.pioInstance, this.sm, enabled ? 1 : 0);
    }

    /**
     * Write a 32-bit word to the state machine TX FIFO (blocks if full).
     *
     * @param data The 32-bit value to write
     */
    public void put(int data) {
        pio_sm_put(this.pioInstance, this.sm, data);
    }

    /**
     * Read a 32-bit word from the state machine RX FIFO (blocks if empty).
     *
     * @return the 32-bit value read
     */
    public int get() {
        return pio_sm_get(this.pioInstance, this.sm);
    }

    /**
     * Check if the TX FIFO is full.
     *
     * @return true if the TX FIFO is full
     */
    public boolean isTxFull() {
        return pio_sm_is_tx_full(this.pioInstance, this.sm) != 0;
    }

    /**
     * Check if the RX FIFO is empty.
     *
     * @return true if the RX FIFO is empty
     */
    public boolean isRxEmpty() {
        return pio_sm_is_rx_empty(this.pioInstance, this.sm) != 0;
    }

    /*
     * Natives for the device hardware
     */
    private static native int  pio_load_program    ( int pioInstance, int[] instructions, int len );
    private static native int  pio_sm_init         ( int pioInstance, int sm, int offset, int pin, int clockDiv );
    private static native void pio_sm_set_enabled  ( int pioInstance, int sm, int enabled );
    private static native void pio_sm_put          ( int pioInstance, int sm, int data );
    private static native int  pio_sm_get          ( int pioInstance, int sm );
    private static native int  pio_sm_is_tx_full   ( int pioInstance, int sm );
    private static native int  pio_sm_is_rx_empty  ( int pioInstance, int sm );

}
