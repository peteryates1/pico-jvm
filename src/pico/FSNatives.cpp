/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

/*
 * Unified filesystem natives for java.io.File, FileInputStream, FileOutputStream.
 *
 * Internal flash filesystem (LittleFS) is always available.
 * When PICO_SD is enabled, paths starting with "/sd/" are routed to
 * the FatFs SD card backend in SDNatives.cpp; all other paths use LittleFS.
 */

#include <stdio.h>
#include <string.h>

#include "kni.h"
#include "sni.h"

#include "hardware/flash.h"
#include "hardware/sync.h"

#include "lfs.h"

/* ================================================================
 * LittleFS flash backend configuration
 * ================================================================ */

/*
 * Flash layout:
 *   0x000000 - firmware (~640KB)
 *   0x100000 - main.jar.bin
 *   0x180000 - FlashConfig (4KB)
 *   0x190000 - LittleFS filesystem (to end of flash)
 *
 * This gives ~448KB on 2MB Pico, ~2.4MB on 4MB Pico 2.
 */
#define LFS_FLASH_OFFSET    (0x190000)
#ifndef LFS_FLASH_SIZE
#define LFS_FLASH_SIZE      (PICO_FLASH_SIZE_BYTES - LFS_FLASH_OFFSET)
#endif
#define LFS_BLOCK_SIZE      FLASH_SECTOR_SIZE   /* 4096 */
#define LFS_BLOCK_COUNT     (LFS_FLASH_SIZE / LFS_BLOCK_SIZE)

/* Static buffers to avoid malloc */
static uint8_t lfs_read_buf[FLASH_PAGE_SIZE];
static uint8_t lfs_prog_buf[FLASH_PAGE_SIZE];
static uint8_t lfs_lookahead_buf[16];

static lfs_t lfs;
static bool lfs_mounted = false;

/* ---- LittleFS block device callbacks ---- */

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;
    uint32_t addr = LFS_FLASH_OFFSET + block * LFS_BLOCK_SIZE + off;
    memcpy(buffer, (const void *)(XIP_BASE + addr), size);
    return LFS_ERR_OK;
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size)
{
    (void)c;
    uint32_t addr = LFS_FLASH_OFFSET + block * LFS_BLOCK_SIZE + off;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(addr, (const uint8_t *)buffer, size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    uint32_t addr = LFS_FLASH_OFFSET + block * LFS_BLOCK_SIZE;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(addr, LFS_BLOCK_SIZE);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int lfs_flash_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}

static const struct lfs_config lfs_cfg = {
    .context = NULL,
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = lfs_flash_sync,

    .read_size      = 1,
    .prog_size      = FLASH_PAGE_SIZE,
    .block_size     = LFS_BLOCK_SIZE,
    .block_count    = LFS_BLOCK_COUNT,
    .block_cycles   = 500,
    .cache_size     = FLASH_PAGE_SIZE,
    .lookahead_size = sizeof(lfs_lookahead_buf),

    .read_buffer      = lfs_read_buf,
    .prog_buffer      = lfs_prog_buf,
    .lookahead_buffer = lfs_lookahead_buf,
};

/**
 * Mount the LittleFS filesystem. Format on first use.
 * Called from JVM startup (Main.cpp) or lazily on first file operation.
 */
static void lfs_ensure_mounted(void)
{
    if (lfs_mounted) return;

    int err = lfs_mount(&lfs, &lfs_cfg);
    if (err != LFS_ERR_OK) {
        /* Not formatted yet — format and retry */
        lfs_format(&lfs, &lfs_cfg);
        err = lfs_mount(&lfs, &lfs_cfg);
        if (err != LFS_ERR_OK) {
            printf("LittleFS mount failed: %d\n", err);
            return;
        }
    }
    lfs_mounted = true;
}

/* ================================================================
 * File descriptor table (shared between LittleFS and SD/FatFs)
 * ================================================================ */

#define MAX_FILES       4
#define MAX_PATH_LEN    256

enum fs_type { FS_NONE = 0, FS_LFS = 1, FS_FATFS = 2 };

typedef struct {
    int      in_use;
    int      fs;         /* FS_LFS or FS_FATFS */
    lfs_file_t lfs_file; /* used when fs == FS_LFS */
#ifdef PICO_SD
    /* FatFs FIL is managed in SDNatives.cpp via sd_file_* functions */
    int      sd_fd;      /* SD FD from SDNatives, used when fs == FS_FATFS */
#endif
} file_ctx_t;

static file_ctx_t files[MAX_FILES];

static int alloc_fd(void)
{
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].in_use) {
            memset(&files[i], 0, sizeof(file_ctx_t));
            files[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

static void free_fd(int fd)
{
    if (fd >= 0 && fd < MAX_FILES) {
        files[fd].in_use = 0;
        files[fd].fs = FS_NONE;
    }
}

#ifdef PICO_SD
/* SD card file operations — implemented in SDNatives.cpp */
extern "C" {
    int  sd_file_open(const char *path, int mode);
    int  sd_file_read(int fd, uint8_t *buf, int off, int len);
    int  sd_file_write(int fd, const uint8_t *buf, int off, int len);
    void sd_file_flush(int fd);
    void sd_file_close(int fd);
    int  sd_file_available(int fd);

    int  sd_stat_exists(const char *path);
    int  sd_stat_is_dir(const char *path);
    int  sd_stat_is_file(const char *path);
    int  sd_stat_length(const char *path);
    int  sd_delete(const char *path);
    int  sd_mkdir(const char *path);
    int  sd_rename(const char *oldp, const char *newp);
    int  sd_list(const char *path, char *buf, int buflen);
}

#define SD_PREFIX     "/sd/"
#define SD_PREFIX_LEN 4

static bool is_sd_path(const char *path)
{
    return strncmp(path, SD_PREFIX, SD_PREFIX_LEN) == 0;
}

/* Strip the /sd prefix, return the path relative to SD root */
static const char *sd_rel_path(const char *path)
{
    /* "/sd/foo" -> "/foo", "/sd" -> "/" */
    const char *rel = path + 3; /* skip "/sd" */
    if (*rel == '\0') return "/";
    return rel;
}
#endif

/**
 * Helper: copy a Java String parameter to a C char buffer.
 */
static int kni_string_to_path(jobject strHandle, char *buf, int bufLen)
{
    jint len = KNI_GetStringLength(strHandle);
    if (len < 0 || len >= bufLen) return -1;

    jchar jchars[MAX_PATH_LEN];
    if (len > MAX_PATH_LEN) len = MAX_PATH_LEN;
    KNI_GetStringRegion(strHandle, 0, len, jchars);

    for (int i = 0; i < len; i++) {
        buf[i] = (char)jchars[i];
    }
    buf[len] = '\0';
    return len;
}

extern "C" {

/* ================================================================
 * File natives (java.io.File)
 * ================================================================ */

int Java_java_io_File_file_1exists( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_stat_exists(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    struct lfs_info info;
    return (lfs_stat(&lfs, path, &info) == LFS_ERR_OK) ? 1 : 0;
}

int Java_java_io_File_file_1is_1directory( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_stat_is_dir(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    struct lfs_info info;
    if (lfs_stat(&lfs, path, &info) != LFS_ERR_OK) return 0;
    return (info.type == LFS_TYPE_DIR) ? 1 : 0;
}

int Java_java_io_File_file_1is_1file( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_stat_is_file(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    struct lfs_info info;
    if (lfs_stat(&lfs, path, &info) != LFS_ERR_OK) return 0;
    return (info.type == LFS_TYPE_REG) ? 1 : 0;
}

int Java_java_io_File_file_1length( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_stat_length(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    struct lfs_info info;
    if (lfs_stat(&lfs, path, &info) != LFS_ERR_OK) return 0;
    return (int)info.size;
}

int Java_java_io_File_file_1delete( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_delete(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    return (lfs_remove(&lfs, path) == LFS_ERR_OK) ? 1 : 0;
}

int Java_java_io_File_file_1mkdir( void )
{
    char path[MAX_PATH_LEN];
    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) return sd_mkdir(sd_rel_path(path));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    return (lfs_mkdir(&lfs, path) == LFS_ERR_OK) ? 1 : 0;
}

int Java_java_io_File_file_1rename( void )
{
    char oldPath[MAX_PATH_LEN];
    char newPath[MAX_PATH_LEN];

    KNI_StartHandles(2);
    KNI_DeclareHandle(oldHandle);
    KNI_DeclareHandle(newHandle);
    KNI_GetParameterAsObject(1, oldHandle);
    KNI_GetParameterAsObject(2, newHandle);
    kni_string_to_path(oldHandle, oldPath, MAX_PATH_LEN);
    kni_string_to_path(newHandle, newPath, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(oldPath) && is_sd_path(newPath))
        return sd_rename(sd_rel_path(oldPath), sd_rel_path(newPath));
#endif
    lfs_ensure_mounted();
    if (!lfs_mounted) return 0;
    return (lfs_rename(&lfs, oldPath, newPath) == LFS_ERR_OK) ? 1 : 0;
}

/**
 * List directory contents.
 * Returns a null-separated string of entry names, or NULL on error.
 */
jobject Java_java_io_File_file_1list( void )
{
    char path[MAX_PATH_LEN];
    char buf[1024];
    int pos = 0;
    bool valid = false;

    KNI_StartHandles(2);
    KNI_DeclareHandle(pathHandle);
    KNI_DeclareHandle(resultHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);

#ifdef PICO_SD
    if (is_sd_path(path)) {
        int n = sd_list(sd_rel_path(path), buf, sizeof(buf));
        if (n > 0) { pos = n; valid = true; }
        else if (n == 0) { valid = true; }
        /* n < 0: valid stays false -> null result */
    } else
#endif
    {
        lfs_ensure_mounted();
        if (lfs_mounted) {
            lfs_dir_t dir;
            if (lfs_dir_open(&lfs, &dir, path) == LFS_ERR_OK) {
                valid = true;
                bool first = true;
                struct lfs_info info;
                while (lfs_dir_read(&lfs, &dir, &info) > 0) {
                    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
                        continue;

                    int nameLen = strlen(info.name);
                    int need = (first ? 0 : 1) + nameLen;
                    if (pos + need >= (int)sizeof(buf)) break;

                    if (!first) {
                        buf[pos++] = '\0';
                    }
                    memcpy(buf + pos, info.name, nameLen);
                    pos += nameLen;
                    first = false;
                }
                lfs_dir_close(&lfs, &dir);
            }
        }
    }

    if (!valid) {
        KNI_ReleaseHandle(resultHandle);
    } else if (pos == 0) {
        KNI_NewStringUTF("", resultHandle);
    } else {
        jchar jbuf[1024];
        for (int i = 0; i < pos; i++) {
            jbuf[i] = (jchar)(unsigned char)buf[i];
        }
        KNI_NewString(jbuf, pos, resultHandle);
    }

    KNI_EndHandlesAndReturnObject(resultHandle);
}

/* ================================================================
 * FileInputStream natives (java.io.FileInputStream)
 * ================================================================ */

/**
 * Open a file.
 * @param 1st: path string
 * @param 2nd: mode (0=read, 1=write+truncate, 2=write+append)
 * @return file descriptor, or -1 on error
 */
int Java_java_io_FileInputStream_file_1open( void )
{
    int mode = KNI_GetParameterAsInt(2);
    char path[MAX_PATH_LEN];

    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) {
        int sd_fd = sd_file_open(sd_rel_path(path), mode);
        if (sd_fd < 0) return -1;
        int fd = alloc_fd();
        if (fd < 0) { sd_file_close(sd_fd); return -1; }
        files[fd].fs = FS_FATFS;
        files[fd].sd_fd = sd_fd;
        return fd;
    }
#endif

    lfs_ensure_mounted();
    if (!lfs_mounted) return -1;

    int fd = alloc_fd();
    if (fd < 0) return -1;

    int flags;
    switch (mode) {
        case 0: flags = LFS_O_RDONLY; break;
        case 1: flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC; break;
        case 2: flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND; break;
        default: free_fd(fd); return -1;
    }

    int err = lfs_file_open(&lfs, &files[fd].lfs_file, path, flags);
    if (err != LFS_ERR_OK) {
        free_fd(fd);
        return -1;
    }
    files[fd].fs = FS_LFS;
    return fd;
}

/**
 * Read bytes from a file.
 */
int Java_java_io_FileInputStream_file_1read( void )
{
    int fd  = KNI_GetParameterAsInt(1);
    int off = KNI_GetParameterAsInt(3);
    int len = KNI_GetParameterAsInt(4);

    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return -1;

    int result = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(2, bufHandle);
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(bufHandle);

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) {
        result = sd_file_read(files[fd].sd_fd, data + off, 0, len);
    } else
#endif
    {
        lfs_ssize_t r = lfs_file_read(&lfs, &files[fd].lfs_file, data + off, len);
        result = (r < 0) ? -1 : (int)r;
    }

    KNI_EndHandles();
    return result;
}

/**
 * Get number of bytes remaining in file.
 */
int Java_java_io_FileInputStream_file_1available( void )
{
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return 0;

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) return sd_file_available(files[fd].sd_fd);
#endif

    lfs_soff_t size = lfs_file_size(&lfs, &files[fd].lfs_file);
    lfs_soff_t pos  = lfs_file_tell(&lfs, &files[fd].lfs_file);
    if (size < 0 || pos < 0) return 0;
    return (int)(size - pos);
}

/**
 * Close a file.
 */
void Java_java_io_FileInputStream_file_1close( void )
{
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return;

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) {
        sd_file_close(files[fd].sd_fd);
        free_fd(fd);
        return;
    }
#endif

    lfs_file_close(&lfs, &files[fd].lfs_file);
    free_fd(fd);
}

/* ================================================================
 * FileOutputStream natives (java.io.FileOutputStream)
 * ================================================================ */

/**
 * Open a file for writing.
 */
int Java_java_io_FileOutputStream_file_1open( void )
{
    int mode = KNI_GetParameterAsInt(2);
    char path[MAX_PATH_LEN];

    KNI_StartHandles(1);
    KNI_DeclareHandle(pathHandle);
    KNI_GetParameterAsObject(1, pathHandle);
    kni_string_to_path(pathHandle, path, MAX_PATH_LEN);
    KNI_EndHandles();

#ifdef PICO_SD
    if (is_sd_path(path)) {
        int sd_fd = sd_file_open(sd_rel_path(path), mode);
        if (sd_fd < 0) return -1;
        int fd = alloc_fd();
        if (fd < 0) { sd_file_close(sd_fd); return -1; }
        files[fd].fs = FS_FATFS;
        files[fd].sd_fd = sd_fd;
        return fd;
    }
#endif

    lfs_ensure_mounted();
    if (!lfs_mounted) return -1;

    int fd = alloc_fd();
    if (fd < 0) return -1;

    int flags;
    switch (mode) {
        case 0: flags = LFS_O_RDONLY; break;
        case 1: flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC; break;
        case 2: flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND; break;
        default: free_fd(fd); return -1;
    }

    int err = lfs_file_open(&lfs, &files[fd].lfs_file, path, flags);
    if (err != LFS_ERR_OK) {
        free_fd(fd);
        return -1;
    }
    files[fd].fs = FS_LFS;
    return fd;
}

/**
 * Write bytes to a file.
 */
int Java_java_io_FileOutputStream_file_1write( void )
{
    int fd  = KNI_GetParameterAsInt(1);
    int off = KNI_GetParameterAsInt(3);
    int len = KNI_GetParameterAsInt(4);

    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return -1;

    int result = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(2, bufHandle);
    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(bufHandle);

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) {
        result = sd_file_write(files[fd].sd_fd, data + off, 0, len);
    } else
#endif
    {
        lfs_ssize_t w = lfs_file_write(&lfs, &files[fd].lfs_file, data + off, len);
        result = (w < 0) ? -1 : (int)w;
    }

    KNI_EndHandles();
    return result;
}

/**
 * Flush (sync) a file.
 */
void Java_java_io_FileOutputStream_file_1flush( void )
{
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return;

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) {
        sd_file_flush(files[fd].sd_fd);
        return;
    }
#endif

    lfs_file_sync(&lfs, &files[fd].lfs_file);
}

/**
 * Close a file (output stream).
 */
void Java_java_io_FileOutputStream_file_1close( void )
{
    int fd = KNI_GetParameterAsInt(1);
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) return;

#ifdef PICO_SD
    if (files[fd].fs == FS_FATFS) {
        sd_file_close(files[fd].sd_fd);
        free_fd(fd);
        return;
    }
#endif

    lfs_file_close(&lfs, &files[fd].lfs_file);
    free_fd(fd);
}

} /* extern "C" */
