/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.hardware;

/**
 * Read configuration from a key=value text file stored in flash.
 * The config file is flashed separately from the firmware:
 * <pre>
 *   picotool load config.txt --offset 0x10180000
 * </pre>
 *
 * Config file format (one key=value per line):
 * <pre>
 *   wifi.ssid=MyNetwork
 *   wifi.password=MySecret
 * </pre>
 */
public class FlashConfig {

    private static final int MAX_CONFIG_SIZE = 1024;

    /**
     * Get a configuration value by key.
     *
     * @param key the configuration key (e.g. "wifi.ssid")
     * @return the value, or null if not found
     */
    public static String get(String key) {
        byte[] buf = new byte[MAX_CONFIG_SIZE];
        int len = flash_read_config(buf, MAX_CONFIG_SIZE);
        if (len <= 0) return null;

        /* Parse key=value lines */
        int lineStart = 0;
        for (int i = 0; i <= len; i++) {
            if (i == len || buf[i] == (byte)'\n' || buf[i] == (byte)'\r') {
                /* Process line from lineStart to i */
                int lineLen = i - lineStart;
                if (lineLen > 0) {
                    /* Find '=' separator */
                    int eqPos = -1;
                    for (int j = lineStart; j < i; j++) {
                        if (buf[j] == (byte)'=') {
                            eqPos = j;
                            break;
                        }
                    }
                    if (eqPos > lineStart) {
                        String k = new String(buf, lineStart, eqPos - lineStart);
                        if (k.equals(key)) {
                            return new String(buf, eqPos + 1, i - eqPos - 1);
                        }
                    }
                }
                lineStart = i + 1;
            }
        }
        return null;
    }

    /**
     * Read raw config bytes from flash.
     *
     * @param buf buffer to fill
     * @param maxLen maximum bytes to read
     * @return number of bytes read, or negative on error
     */
    private static native int flash_read_config(byte[] buf, int maxLen);
}
