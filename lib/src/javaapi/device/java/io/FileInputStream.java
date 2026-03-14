/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package java.io;

/**
 * A <code>FileInputStream</code> obtains input bytes from a file on the
 * SD card filesystem.
 */
public class FileInputStream extends InputStream {

    private int fd = -1;

    /**
     * Creates a <code>FileInputStream</code> by opening a connection to
     * an actual file named by the path name <code>name</code>.
     *
     * @param name the system-dependent file name
     * @throws FileNotFoundException if the file does not exist or cannot be opened
     */
    public FileInputStream(String name) throws FileNotFoundException {
        if (name == null) {
            throw new NullPointerException();
        }
        fd = file_open(name, 0);
        if (fd < 0) {
            throw new FileNotFoundException(name);
        }
    }

    /**
     * Creates a <code>FileInputStream</code> by opening a connection to
     * an actual file named by the <code>File</code> object <code>file</code>.
     *
     * @param file the file to be opened for reading
     * @throws FileNotFoundException if the file does not exist or cannot be opened
     */
    public FileInputStream(File file) throws FileNotFoundException {
        this(file.getPath());
    }

    /**
     * Reads the next byte of data from this input stream.
     *
     * @return the next byte of data, or <code>-1</code> if the end of the file is reached
     * @throws IOException if an I/O error occurs
     */
    public int read() throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        byte[] buf = new byte[1];
        int n = file_read(fd, buf, 0, 1);
        if (n <= 0) return -1;
        return buf[0] & 0xFF;
    }

    /**
     * Reads up to <code>len</code> bytes of data from this input stream
     * into an array of bytes.
     *
     * @param b   the buffer into which the data is read
     * @param off the start offset in the destination array <code>b</code>
     * @param len the maximum number of bytes read
     * @return the total number of bytes read, or <code>-1</code> if
     *         the end of the file is reached
     * @throws IOException if an I/O error occurs
     */
    public int read(byte[] b, int off, int len) throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        if (b == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off + len > b.length) {
            throw new IndexOutOfBoundsException();
        }
        if (len == 0) return 0;
        int n = file_read(fd, b, off, len);
        if (n == 0) return -1;
        if (n < 0) throw new IOException("Read error");
        return n;
    }

    /**
     * Returns an estimate of the number of remaining bytes that can be
     * read from this input stream without blocking.
     *
     * @return an estimate of the number of remaining bytes
     * @throws IOException if an I/O error occurs
     */
    public int available() throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        return file_available(fd);
    }

    /**
     * Skips over and discards <code>n</code> bytes of data from the
     * input stream.
     *
     * @param n the number of bytes to be skipped
     * @return the actual number of bytes skipped
     * @throws IOException if an I/O error occurs
     */
    public long skip(long n) throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        return super.skip(n);
    }

    /**
     * Closes this file input stream and releases any system resources
     * associated with the stream.
     *
     * @throws IOException if an I/O error occurs
     */
    public void close() throws IOException {
        if (fd >= 0) {
            file_close(fd);
            fd = -1;
        }
    }

    /*
     * Native methods backed by FatFs
     */
    private static native int  file_open(String path, int mode);
    private static native int  file_read(int fd, byte[] buf, int off, int len);
    private static native int  file_available(int fd);
    private static native void file_close(int fd);
}
