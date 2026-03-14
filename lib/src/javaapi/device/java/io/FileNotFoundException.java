/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package java.io;

/**
 * Signals that an attempt to open the file denoted by a specified
 * pathname has failed.
 */
public class FileNotFoundException extends IOException {

    /**
     * Constructs a <code>FileNotFoundException</code> with
     * <code>null</code> as its error detail message.
     */
    public FileNotFoundException() {
        super();
    }

    /**
     * Constructs a <code>FileNotFoundException</code> with the
     * specified detail message.
     *
     * @param s the detail message.
     */
    public FileNotFoundException(String s) {
        super(s);
    }
}
