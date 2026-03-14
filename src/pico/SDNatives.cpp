/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

/*
 * SD card natives: mount/unmount (pico.hardware.SDCard) and
 * FatFs file helpers called by FSNatives.cpp for /sd/ paths.
 */

#include <stdio.h>
#include <string.h>

#include "kni.h"
#include "sni.h"

#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"

/* ================================================================
 * SD FD table (internal, used by FSNatives via sd_file_* helpers)
 * ================================================================ */

#define SD_MAX_FILES    4
#define MAX_PATH_LEN    256

typedef struct {
    FIL fil;
    int in_use;
} sd_file_ctx_t;

static sd_file_ctx_t sd_files[SD_MAX_FILES];
static bool sd_mounted = false;

/* SD card hardware configuration for carlk3 library */
static spi_t sd_spi;
static sd_card_t sd_card;

/* ----------------------------------------------------------------
 * carlk3 hw_config callbacks
 * ---------------------------------------------------------------- */

extern "C" {

size_t spi_get_num(void)  { return 1; }
spi_t *spi_get_by_num(size_t num) { return (num == 0) ? &sd_spi : NULL; }
size_t sd_get_num(void)   { return 1; }
sd_card_t *sd_get_by_num(size_t num) { return (num == 0) ? &sd_card : NULL; }

} /* extern "C" for hw_config */

static int sd_alloc_fd(void)
{
    for (int i = 0; i < SD_MAX_FILES; i++) {
        if (!sd_files[i].in_use) {
            memset(&sd_files[i], 0, sizeof(sd_file_ctx_t));
            sd_files[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

static void sd_free_fd(int fd)
{
    if (fd >= 0 && fd < SD_MAX_FILES) {
        sd_files[fd].in_use = 0;
    }
}

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

/* ================================================================
 * SD file helper functions (called by FSNatives.cpp)
 * ================================================================ */

extern "C" {

int sd_file_open(const char *path, int mode)
{
    int fd = sd_alloc_fd();
    if (fd < 0) return -1;

    BYTE fa;
    switch (mode) {
        case 0: fa = FA_READ; break;
        case 1: fa = FA_WRITE | FA_CREATE_ALWAYS; break;
        case 2: fa = FA_WRITE | FA_OPEN_APPEND; break;
        default: sd_free_fd(fd); return -1;
    }

    FRESULT fr = f_open(&sd_files[fd].fil, path, fa);
    if (fr != FR_OK) {
        sd_free_fd(fd);
        return -1;
    }
    return fd;
}

int sd_file_read(int fd, uint8_t *buf, int off, int len)
{
    if (fd < 0 || fd >= SD_MAX_FILES || !sd_files[fd].in_use) return -1;
    UINT br;
    FRESULT fr = f_read(&sd_files[fd].fil, buf + off, len, &br);
    if (fr != FR_OK) return -1;
    return (int)br;
}

int sd_file_write(int fd, const uint8_t *buf, int off, int len)
{
    if (fd < 0 || fd >= SD_MAX_FILES || !sd_files[fd].in_use) return -1;
    UINT bw;
    FRESULT fr = f_write(&sd_files[fd].fil, buf + off, len, &bw);
    if (fr != FR_OK) return -1;
    return (int)bw;
}

void sd_file_flush(int fd)
{
    if (fd < 0 || fd >= SD_MAX_FILES || !sd_files[fd].in_use) return;
    f_sync(&sd_files[fd].fil);
}

void sd_file_close(int fd)
{
    if (fd < 0 || fd >= SD_MAX_FILES || !sd_files[fd].in_use) return;
    f_close(&sd_files[fd].fil);
    sd_free_fd(fd);
}

int sd_file_available(int fd)
{
    if (fd < 0 || fd >= SD_MAX_FILES || !sd_files[fd].in_use) return 0;
    FSIZE_t size = f_size(&sd_files[fd].fil);
    FSIZE_t pos  = f_tell(&sd_files[fd].fil);
    return (int)(size - pos);
}

int sd_stat_exists(const char *path)
{
    FILINFO fno;
    return (f_stat(path, &fno) == FR_OK) ? 1 : 0;
}

int sd_stat_is_dir(const char *path)
{
    FILINFO fno;
    if (f_stat(path, &fno) != FR_OK) return 0;
    return (fno.fattrib & AM_DIR) ? 1 : 0;
}

int sd_stat_is_file(const char *path)
{
    FILINFO fno;
    if (f_stat(path, &fno) != FR_OK) return 0;
    return (fno.fattrib & AM_DIR) ? 0 : 1;
}

int sd_stat_length(const char *path)
{
    FILINFO fno;
    if (f_stat(path, &fno) != FR_OK) return 0;
    return (int)fno.fsize;
}

int sd_delete(const char *path)
{
    return (f_unlink(path) == FR_OK) ? 1 : 0;
}

int sd_mkdir(const char *path)
{
    return (f_mkdir(path) == FR_OK) ? 1 : 0;
}

int sd_rename(const char *oldp, const char *newp)
{
    return (f_rename(oldp, newp) == FR_OK) ? 1 : 0;
}

int sd_list(const char *path, char *buf, int buflen)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, path);
    if (fr != FR_OK) return -1;

    int pos = 0;
    bool first = true;
    while (true) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        int nameLen = strlen(fno.fname);
        int need = (first ? 0 : 1) + nameLen;
        if (pos + need >= buflen) break;

        if (!first) buf[pos++] = '\0';
        memcpy(buf + pos, fno.fname, nameLen);
        pos += nameLen;
        first = false;
    }
    f_closedir(&dir);
    return pos;
}

/* ================================================================
 * SDCard KNI natives (pico.hardware.SDCard)
 * ================================================================ */

int Java_pico_hardware_SDCard_sd_1mount( void )
{
    int spiInst = KNI_GetParameterAsInt(1);
    int sckPin  = KNI_GetParameterAsInt(2);
    int mosiPin = KNI_GetParameterAsInt(3);
    int misoPin = KNI_GetParameterAsInt(4);
    int csPin   = KNI_GetParameterAsInt(5);

    if (sd_mounted) return 0;

    memset(&sd_spi, 0, sizeof(sd_spi));
    sd_spi.hw_inst   = spiInst == 0 ? spi0 : spi1;
    sd_spi.miso_gpio = misoPin;
    sd_spi.mosi_gpio = mosiPin;
    sd_spi.sck_gpio  = sckPin;
    sd_spi.baud_rate = 12500000;

    memset(&sd_card, 0, sizeof(sd_card));
    sd_card.pcName   = "0:";
    sd_card.spi      = &sd_spi;
    sd_card.ss_gpio  = csPin;

    FRESULT fr = f_mount(&sd_card.fatfs, sd_card.pcName, 1);
    if (fr != FR_OK) {
        printf("SD mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        return -1;
    }

    sd_mounted = true;
    return 0;
}

void Java_pico_hardware_SDCard_sd_1unmount( void )
{
    if (!sd_mounted) return;

    for (int i = 0; i < SD_MAX_FILES; i++) {
        if (sd_files[i].in_use) {
            f_close(&sd_files[i].fil);
            sd_files[i].in_use = 0;
        }
    }

    f_unmount(sd_card.pcName);
    sd_mounted = false;
}

int Java_pico_hardware_SDCard_sd_1is_1mounted( void )
{
    return sd_mounted ? 1 : 0;
}

} /* extern "C" */
