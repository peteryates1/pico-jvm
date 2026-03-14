/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package java.io;

/**
 * A file output stream is an output stream for writing data to a
 * <code>File</code> on the SD card filesystem.
 */
public class FileOutputStream extends OutputStream {

    private int fd = -1;

    /**
     * Creates a file output stream to write to the file with the
     * specified name. A new file will be created, or an existing file
     * will be truncated.
     *
     * @param name the system-dependent file name
     * @throws FileNotFoundException if the file cannot be opened
     */
    public FileOutputStream(String name) throws FileNotFoundException {
        this(name, false);
    }

    /**
     * Creates a file output stream to write to the file with the
     * specified name. If the <code>append</code> argument is <code>true</code>,
     * bytes will be written to the end of the file rather than the beginning.
     *
     * @param name   the system-dependent file name
     * @param append if <code>true</code>, bytes will be written to the end of the file
     * @throws FileNotFoundException if the file cannot be opened
     */
    public FileOutputStream(String name, boolean append) throws FileNotFoundException {
        if (name == null) {
            throw new NullPointerException();
        }
        int mode = append ? 2 : 1;
        fd = file_open(name, mode);
        if (fd < 0) {
            throw new FileNotFoundException(name);
        }
    }

    /**
     * Creates a file output stream to write to the file represented by
     * the specified <code>File</code> object.
     *
     * @param file the file to be opened for writing
     * @throws FileNotFoundException if the file cannot be opened
     */
    public FileOutputStream(File file) throws FileNotFoundException {
        this(file.getPath(), false);
    }

    /**
     * Creates a file output stream to write to the file represented by
     * the specified <code>File</code> object. If the <code>append</code>
     * argument is <code>true</code>, bytes will be written to the end
     * of the file rather than the beginning.
     *
     * @param file   the file to be opened for writing
     * @param append if <code>true</code>, bytes will be written to the end of the file
     * @throws FileNotFoundException if the file cannot be opened
     */
    public FileOutputStream(File file, boolean append) throws FileNotFoundException {
        this(file.getPath(), append);
    }

    /**
     * Writes the specified byte to this file output stream.
     *
     * @param b the byte to be written
     * @throws IOException if an I/O error occurs
     */
    public void write(int b) throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        byte[] buf = new byte[1];
        buf[0] = (byte) b;
        int n = file_write(fd, buf, 0, 1);
        if (n < 0) {
            throw new IOException("Write error");
        }
    }

    /**
     * Writes <code>len</code> bytes from the specified byte array
     * starting at offset <code>off</code> to this file output stream.
     *
     * @param b   the data
     * @param off the start offset in the data
     * @param len the number of bytes to write
     * @throws IOException if an I/O error occurs
     */
    public void write(byte[] b, int off, int len) throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        if (b == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off + len > b.length) {
            throw new IndexOutOfBoundsException();
        }
        if (len == 0) return;
        int n = file_write(fd, b, off, len);
        if (n < 0) {
            throw new IOException("Write error");
        }
    }

    /**
     * Flushes this output stream and forces any buffered output bytes
     * to be written out to the SD card.
     *
     * @throws IOException if an I/O error occurs
     */
    public void flush() throws IOException {
        if (fd < 0) {
            throw new IOException("Stream closed");
        }
        file_flush(fd);
    }

    /**
     * Closes this file output stream and releases any system resources
     * associated with this stream.
     *
     * @throws IOException if an I/O error occurs
     */
    public void close() throws IOException {
        if (fd >= 0) {
            file_flush(fd);
            file_close(fd);
            fd = -1;
        }
    }

    /*
     * Native methods backed by FatFs
     */
    private static native int  file_open(String path, int mode);
    private static native int  file_write(int fd, byte[] buf, int off, int len);
    private static native void file_flush(int fd);
    private static native void file_close(int fd);
}
