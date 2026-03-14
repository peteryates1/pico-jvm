/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package java.io;

/**
 * An abstract representation of file and directory pathnames on the
 * SD card filesystem.
 *
 * <p>This is a subset of the JDK 6 <code>java.io.File</code> API,
 * providing path manipulation and basic file operations backed by
 * native FatFs calls.
 */
public class File {

    /**
     * The system-dependent default name-separator character.
     */
    public static final char separatorChar = '/';

    /**
     * The system-dependent default name-separator character, represented
     * as a string for convenience.
     */
    public static final String separator = "/";

    private String path;

    /**
     * Creates a new <code>File</code> instance by converting the given
     * pathname string into an abstract pathname.
     *
     * @param pathname a pathname string
     * @throws NullPointerException if <code>pathname</code> is <code>null</code>
     */
    public File(String pathname) {
        if (pathname == null) {
            throw new NullPointerException();
        }
        this.path = normalize(pathname);
    }

    /**
     * Creates a new <code>File</code> instance from a parent pathname
     * string and a child pathname string.
     *
     * @param parent the parent pathname string
     * @param child  the child pathname string
     * @throws NullPointerException if <code>child</code> is <code>null</code>
     */
    public File(String parent, String child) {
        if (child == null) {
            throw new NullPointerException();
        }
        if (parent != null && parent.length() > 0) {
            this.path = normalize(parent + separatorChar + child);
        } else {
            this.path = normalize(child);
        }
    }

    /**
     * Creates a new <code>File</code> instance from a parent abstract
     * pathname and a child pathname string.
     *
     * @param parent the parent abstract pathname
     * @param child  the child pathname string
     * @throws NullPointerException if <code>child</code> is <code>null</code>
     */
    public File(File parent, String child) {
        this(parent == null ? null : parent.getPath(), child);
    }

    private static String normalize(String p) {
        // Replace backslashes with forward slashes
        StringBuffer sb = new StringBuffer(p.length());
        for (int i = 0; i < p.length(); i++) {
            char c = p.charAt(i);
            if (c == '\\') {
                sb.append('/');
            } else {
                sb.append(c);
            }
        }
        String s = sb.toString();
        // Remove trailing slash unless it's the root
        while (s.length() > 1 && s.charAt(s.length() - 1) == '/') {
            s = s.substring(0, s.length() - 1);
        }
        return s;
    }

    /**
     * Returns the name of the file or directory denoted by this abstract
     * pathname. This is just the last name in the pathname's name sequence.
     *
     * @return the name of the file or directory
     */
    public String getName() {
        int idx = path.lastIndexOf(separatorChar);
        if (idx < 0) return path;
        return path.substring(idx + 1);
    }

    /**
     * Returns the pathname string of this abstract pathname's parent,
     * or <code>null</code> if this pathname does not name a parent directory.
     *
     * @return the pathname string of the parent directory, or <code>null</code>
     */
    public String getParent() {
        int idx = path.lastIndexOf(separatorChar);
        if (idx <= 0) return null;
        return path.substring(0, idx);
    }

    /**
     * Returns the abstract pathname of this abstract pathname's parent,
     * or <code>null</code> if this pathname does not name a parent directory.
     *
     * @return the abstract pathname of the parent directory, or <code>null</code>
     */
    public File getParentFile() {
        String p = getParent();
        if (p == null) return null;
        return new File(p);
    }

    /**
     * Converts this abstract pathname into a pathname string.
     *
     * @return the string form of this abstract pathname
     */
    public String getPath() {
        return path;
    }

    /**
     * Returns the absolute pathname string of this abstract pathname.
     * On this platform, all paths are absolute (rooted at the SD card mount).
     *
     * @return the absolute pathname string
     */
    public String getAbsolutePath() {
        if (isAbsolute()) {
            return path;
        }
        return separatorChar + path;
    }

    /**
     * Tests whether this abstract pathname is absolute.
     *
     * @return <code>true</code> if this pathname is absolute
     */
    public boolean isAbsolute() {
        return path.length() > 0 && path.charAt(0) == separatorChar;
    }

    /**
     * Tests whether the file or directory denoted by this abstract
     * pathname exists.
     *
     * @return <code>true</code> if the file or directory exists
     */
    public boolean exists() {
        return file_exists(path);
    }

    /**
     * Tests whether the file denoted by this abstract pathname is a
     * directory.
     *
     * @return <code>true</code> if this pathname denotes a directory
     */
    public boolean isDirectory() {
        return file_is_directory(path);
    }

    /**
     * Tests whether the file denoted by this abstract pathname is a
     * normal file.
     *
     * @return <code>true</code> if this pathname denotes a normal file
     */
    public boolean isFile() {
        return file_is_file(path);
    }

    /**
     * Returns the length of the file denoted by this abstract pathname.
     *
     * @return the length, in bytes, of the file, or <code>0L</code>
     *         if the file does not exist
     */
    public long length() {
        return (long) file_length(path) & 0xFFFFFFFFL;
    }

    /**
     * Deletes the file or directory denoted by this abstract pathname.
     * A directory must be empty to be deleted.
     *
     * @return <code>true</code> if the file or directory was successfully deleted
     */
    public boolean delete() {
        return file_delete(path);
    }

    /**
     * Creates the directory named by this abstract pathname.
     *
     * @return <code>true</code> if the directory was created
     */
    public boolean mkdir() {
        return file_mkdir(path);
    }

    /**
     * Creates the directory named by this abstract pathname, including
     * any necessary but nonexistent parent directories.
     *
     * @return <code>true</code> if the directory was created, along with
     *         all necessary parent directories
     */
    public boolean mkdirs() {
        if (exists()) {
            return false;
        }
        if (mkdir()) {
            return true;
        }
        File parent = getParentFile();
        if (parent != null) {
            parent.mkdirs();
            return mkdir();
        }
        return false;
    }

    /**
     * Renames the file denoted by this abstract pathname.
     *
     * @param dest the new abstract pathname for the named file
     * @return <code>true</code> if the rename succeeded
     * @throws NullPointerException if <code>dest</code> is <code>null</code>
     */
    public boolean renameTo(File dest) {
        if (dest == null) {
            throw new NullPointerException();
        }
        return file_rename(path, dest.getPath());
    }

    /**
     * Returns an array of strings naming the files and directories in
     * the directory denoted by this abstract pathname.
     *
     * @return an array of strings, or <code>null</code> if this pathname
     *         does not denote a directory or if an I/O error occurs
     */
    public String[] list() {
        String raw = file_list(path);
        if (raw == null) {
            return null;
        }
        if (raw.length() == 0) {
            return new String[0];
        }
        // Count entries (null-separated)
        int count = 1;
        for (int i = 0; i < raw.length(); i++) {
            if (raw.charAt(i) == '\0') {
                count++;
            }
        }
        String[] result = new String[count];
        int idx = 0;
        int start = 0;
        for (int i = 0; i < raw.length(); i++) {
            if (raw.charAt(i) == '\0') {
                result[idx++] = raw.substring(start, i);
                start = i + 1;
            }
        }
        result[idx] = raw.substring(start);
        return result;
    }

    /**
     * Tests this abstract pathname for equality with the given object.
     *
     * @param obj the object to be compared with this abstract pathname
     * @return <code>true</code> if the objects are the same
     */
    public boolean equals(Object obj) {
        if (obj instanceof File) {
            return path.equals(((File) obj).path);
        }
        return false;
    }

    /**
     * Computes a hash code for this abstract pathname.
     *
     * @return a hash code for this abstract pathname
     */
    public int hashCode() {
        return path.hashCode();
    }

    /**
     * Returns the pathname string of this abstract pathname.
     *
     * @return the string form of this abstract pathname
     */
    public String toString() {
        return path;
    }

    /*
     * Native methods backed by FatFs
     */
    private static native boolean file_exists(String path);
    private static native boolean file_is_directory(String path);
    private static native boolean file_is_file(String path);
    private static native int     file_length(String path);
    private static native boolean file_delete(String path);
    private static native boolean file_mkdir(String path);
    private static native boolean file_rename(String oldPath, String newPath);
    private static native String  file_list(String path);
}
