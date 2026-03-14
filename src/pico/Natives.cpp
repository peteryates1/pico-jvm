/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

#include <stdio.h>
#include <string.h>

#include "kni.h"
#include "sni.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/unique_id.h"
#include "pico/rand.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#ifdef PICO_W
#include "pico/cyw43_arch.h"
#endif

#define         ADC_GPIO_PIN_MIN        26
#define         ADC_GPIO_PIN_MAX        29

#ifdef PICO_W
int cyw43_initialized = 0;
#endif

static uart_inst_t *uart_instance(int inst) {
    return inst == 0 ? uart0 : uart1;
}

static i2c_inst_t *i2c_instance(int inst) {
    return inst == 0 ? i2c0 : i2c1;
}

static spi_inst_t *spi_instance(int inst) {
    return inst == 0 ? spi0 : spi1;
}

static PIO pio_instance(int inst) {
    return inst == 0 ? pio0 : pio1;
}

extern "C" {

/*
 * This file is the native implementation of the device specific
 * classes that access the hardware, these functions are called from
 * the Java classes. All the functions are void but they get their
 * input parameters from the JVM stack via KNI calls
 */

/* ================================================================
 * GPIOPin natives
 * ================================================================ */

/**
 * Initialize a GPIO pin
 *
 * @param The 1st parameter is the pin number
 * @param The 2nd parameter is the pin direction
 *
 */
void Java_pico_hardware_GPIOPin_gpio_1init( void )
{
    int pinNumber = KNI_GetParameterAsInt(1);
    int direction = KNI_GetParameterAsInt(2);
    gpio_init(pinNumber);
    gpio_set_dir(pinNumber, direction);
}

/**
 * Set a pin output state
 *
 * @param The 1st parameter is the pin number
 * @param The 2nd parameter is the pin state
 *
 */
void Java_pico_hardware_GPIOPin_gpio_1set( void )
{
    int pinNumber = KNI_GetParameterAsInt(1);
    int value     = KNI_GetParameterAsInt(2);
    gpio_put(pinNumber, value);
}

/**
 * Set a pin pull up or down resistor
 *
 * @param The 1st parameter is the pin number
 * @param The 2nd parameter is 1 for pull up, 0 for pull down
 *
 */
void Java_pico_hardware_GPIOPin_gpio_1set_1pull( void )
{
    int pinNumber = KNI_GetParameterAsInt(1);
    int pullUp    = KNI_GetParameterAsInt(2);
    if (pullUp) {
        gpio_pull_up(pinNumber);
    }
    else {
        gpio_pull_down(pinNumber);
    }
}

/**
 * Returns the state of the pin (high or low)
 *
 * @param The 1st parameter is the pin number
 *
 */
int Java_pico_hardware_GPIOPin_gpio_1get( void )
{
    int pinNumber = KNI_GetParameterAsInt(1);
    return gpio_get(pinNumber);
}

/* ================================================================
 * ADCChannel natives
 * ================================================================ */

/**
 * Initialize an ADC channel
 *
 * @param The 1st parameter is the channel number
 *
 */
void Java_pico_hardware_ADCChannel_adc_1init ( void )
{
    int channel = KNI_GetParameterAsInt(1);
    adc_gpio_init(ADC_GPIO_PIN_MIN + channel);
}

/**
 * Read a single sample from the ADC channel
 *
 * @param The 1st parameter is the channel number
 * @return The sample read as an integer
 *
 */
int Java_pico_hardware_ADCChannel_adc_1read ( void )
{
    int channel = KNI_GetParameterAsInt(1);
    adc_select_input(channel);
    return adc_read();
}

/* ================================================================
 * PWMChannel natives
 * ================================================================ */

/**
 * Initialize PWM on a GPIO pin
 *
 * @param 1st: GPIO pin number
 * @param 2nd: frequency in Hz
 * @param 3rd: duty cycle percent (0-100)
 * @return 0 on success, negative on error
 */
int Java_pico_hardware_PWMChannel_pwm_1init( void )
{
    int gpioPin     = KNI_GetParameterAsInt(1);
    int freqHz      = KNI_GetParameterAsInt(2);
    int dutyPercent = KNI_GetParameterAsInt(3);

    gpio_set_function(gpioPin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(gpioPin);
    uint channel = pwm_gpio_to_channel(gpioPin);

    /* Calculate wrap value for desired frequency */
    uint32_t clock = clock_get_hz(clk_sys);
    uint32_t divider16 = clock / freqHz / 4096 + (clock % (freqHz * 4096) != 0);
    if (divider16 / 16 == 0) divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / freqHz - 1;

    pwm_set_clkdiv_int_frac(slice, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice, wrap);
    pwm_set_chan_level(slice, channel, wrap * dutyPercent / 100);
    pwm_set_enabled(slice, true);

    return 0;
}

/**
 * Set PWM duty cycle
 *
 * @param 1st: GPIO pin number
 * @param 2nd: duty cycle percent (0-100)
 */
void Java_pico_hardware_PWMChannel_pwm_1set_1duty( void )
{
    int gpioPin     = KNI_GetParameterAsInt(1);
    int dutyPercent = KNI_GetParameterAsInt(2);

    uint slice = pwm_gpio_to_slice_num(gpioPin);
    uint channel = pwm_gpio_to_channel(gpioPin);
    uint32_t wrap = pwm_hw->slice[slice].top;

    pwm_set_chan_level(slice, channel, wrap * dutyPercent / 100);
}

/**
 * Enable or disable PWM on a pin
 *
 * @param 1st: GPIO pin number
 * @param 2nd: 1 to enable, 0 to disable
 */
void Java_pico_hardware_PWMChannel_pwm_1set_1enabled( void )
{
    int gpioPin = KNI_GetParameterAsInt(1);
    int enabled = KNI_GetParameterAsInt(2);

    uint slice = pwm_gpio_to_slice_num(gpioPin);
    pwm_set_enabled(slice, enabled != 0);
}

/* ================================================================
 * UARTPort natives
 * ================================================================ */

/**
 * Initialize a UART instance
 *
 * @param 1st: instance (0 or 1)
 * @param 2nd: TX pin
 * @param 3rd: RX pin
 * @param 4th: baudrate
 * @return 0 on success, negative on error
 */
int Java_pico_hardware_UARTPort_uart_1init( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int txPin    = KNI_GetParameterAsInt(2);
    int rxPin    = KNI_GetParameterAsInt(3);
    int baudrate = KNI_GetParameterAsInt(4);

    uart_inst_t *uart = uart_instance(instance);

    uart_init(uart, baudrate);
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);

    return 0;
}

/**
 * Write bytes to UART
 *
 * @param 1st: instance
 * @param 2nd: byte[] data
 * @param 3rd: length
 * @return number of bytes written, or negative on error
 */
int Java_pico_hardware_UARTPort_uart_1write( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int len      = KNI_GetParameterAsInt(3);

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);

    char *data = (char *) SNI_GetRawArrayPointer(dataHandle);
    uart_inst_t *uart = uart_instance(instance);
    uart_write_blocking(uart, (const uint8_t *)data, len);

    KNI_EndHandles();
    return len;
}

/**
 * Read bytes from UART
 *
 * @param 1st: instance
 * @param 2nd: byte[] data buffer
 * @param 3rd: max length
 * @return number of bytes read
 */
int Java_pico_hardware_UARTPort_uart_1read( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int len      = KNI_GetParameterAsInt(3);
    uart_inst_t *uart = uart_instance(instance);
    int count = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);

    char *data = (char *) SNI_GetRawArrayPointer(dataHandle);

    while (count < len && uart_is_readable(uart)) {
        data[count++] = uart_getc(uart);
    }

    KNI_EndHandles();
    return count;
}

/**
 * Check how many bytes are available to read
 *
 * @param 1st: instance
 * @return number of bytes available (0 or 1 for UART)
 */
int Java_pico_hardware_UARTPort_uart_1available( void )
{
    int instance = KNI_GetParameterAsInt(1);
    uart_inst_t *uart = uart_instance(instance);
    return uart_is_readable(uart) ? 1 : 0;
}

/**
 * Write a single byte to UART
 *
 * @param 1st: instance
 * @param 2nd: byte value
 */
void Java_pico_hardware_UARTPort_uart_1write_1byte( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int b        = KNI_GetParameterAsInt(2);
    uart_inst_t *uart = uart_instance(instance);
    uart_putc(uart, (char)b);
}

/**
 * Read a single byte from UART (blocking)
 *
 * @param 1st: instance
 * @return the byte read
 */
int Java_pico_hardware_UARTPort_uart_1read_1byte( void )
{
    int instance = KNI_GetParameterAsInt(1);
    uart_inst_t *uart = uart_instance(instance);
    return uart_getc(uart);
}

/* ================================================================
 * I2CBus natives
 * ================================================================ */

/**
 * Initialize an I2C instance
 *
 * @param 1st: instance (0 or 1)
 * @param 2nd: SDA pin
 * @param 3rd: SCL pin
 * @param 4th: baudrate
 * @return 0 on success, negative on error
 */
int Java_pico_hardware_I2CBus_i2c_1init( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int sdaPin   = KNI_GetParameterAsInt(2);
    int sclPin   = KNI_GetParameterAsInt(3);
    int baudrate = KNI_GetParameterAsInt(4);

    i2c_inst_t *i2c = i2c_instance(instance);

    i2c_init(i2c, baudrate);
    gpio_set_function(sdaPin, GPIO_FUNC_I2C);
    gpio_set_function(sclPin, GPIO_FUNC_I2C);
    gpio_pull_up(sdaPin);
    gpio_pull_up(sclPin);

    return 0;
}

/**
 * Write bytes to an I2C device
 *
 * @param 1st: instance
 * @param 2nd: 7-bit address
 * @param 3rd: byte[] data
 * @param 4th: length
 * @return number of bytes written, or negative on error
 */
int Java_pico_hardware_I2CBus_i2c_1write( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int addr     = KNI_GetParameterAsInt(2);
    int len      = KNI_GetParameterAsInt(4);
    i2c_inst_t *i2c = i2c_instance(instance);
    int result;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(3, dataHandle);

    uint8_t *data = (uint8_t *) SNI_GetRawArrayPointer(dataHandle);
    result = i2c_write_blocking(i2c, addr, data, len, false);

    KNI_EndHandles();
    return result;
}

/**
 * Read bytes from an I2C device
 *
 * @param 1st: instance
 * @param 2nd: 7-bit address
 * @param 3rd: byte[] data buffer
 * @param 4th: length
 * @return number of bytes read, or negative on error
 */
int Java_pico_hardware_I2CBus_i2c_1read( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int addr     = KNI_GetParameterAsInt(2);
    int len      = KNI_GetParameterAsInt(4);
    i2c_inst_t *i2c = i2c_instance(instance);
    int result;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(3, dataHandle);

    uint8_t *data = (uint8_t *) SNI_GetRawArrayPointer(dataHandle);
    result = i2c_read_blocking(i2c, addr, data, len, false);

    KNI_EndHandles();
    return result;
}

/**
 * Write then read from an I2C device (repeated start)
 *
 * @param 1st: instance
 * @param 2nd: 7-bit address
 * @param 3rd: byte[] write data
 * @param 4th: write length
 * @param 5th: byte[] read data buffer
 * @param 6th: read length
 * @return number of bytes read, or negative on error
 */
int Java_pico_hardware_I2CBus_i2c_1write_1read( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int addr     = KNI_GetParameterAsInt(2);
    int wLen     = KNI_GetParameterAsInt(4);
    int rLen     = KNI_GetParameterAsInt(6);
    i2c_inst_t *i2c = i2c_instance(instance);
    int result;

    KNI_StartHandles(2);
    KNI_DeclareHandle(wDataHandle);
    KNI_DeclareHandle(rDataHandle);
    KNI_GetParameterAsObject(3, wDataHandle);
    KNI_GetParameterAsObject(5, rDataHandle);

    uint8_t *wData = (uint8_t *) SNI_GetRawArrayPointer(wDataHandle);
    uint8_t *rData = (uint8_t *) SNI_GetRawArrayPointer(rDataHandle);

    /* Write with nostop=true for repeated start */
    result = i2c_write_blocking(i2c, addr, wData, wLen, true);
    if (result >= 0) {
        result = i2c_read_blocking(i2c, addr, rData, rLen, false);
    }

    KNI_EndHandles();
    return result;
}

/* ================================================================
 * SPIBus natives
 * ================================================================ */

/**
 * Initialize an SPI instance
 *
 * @param 1st: instance (0 or 1)
 * @param 2nd: SCK pin
 * @param 3rd: MOSI pin
 * @param 4th: MISO pin
 * @param 5th: baudrate
 * @return 0 on success, negative on error
 */
int Java_pico_hardware_SPIBus_spi_1init( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int sckPin   = KNI_GetParameterAsInt(2);
    int mosiPin  = KNI_GetParameterAsInt(3);
    int misoPin  = KNI_GetParameterAsInt(4);
    int baudrate = KNI_GetParameterAsInt(5);

    spi_inst_t *spi = spi_instance(instance);

    spi_init(spi, baudrate);
    gpio_set_function(sckPin,  GPIO_FUNC_SPI);
    gpio_set_function(mosiPin, GPIO_FUNC_SPI);
    gpio_set_function(misoPin, GPIO_FUNC_SPI);

    return 0;
}

/**
 * Write bytes to SPI
 *
 * @param 1st: instance
 * @param 2nd: byte[] data
 * @param 3rd: length
 * @return number of bytes written, or negative on error
 */
int Java_pico_hardware_SPIBus_spi_1write( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int len      = KNI_GetParameterAsInt(3);
    spi_inst_t *spi = spi_instance(instance);
    int result;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);

    uint8_t *data = (uint8_t *) SNI_GetRawArrayPointer(dataHandle);
    result = spi_write_blocking(spi, data, len);

    KNI_EndHandles();
    return result;
}

/**
 * Read bytes from SPI (sends zeros)
 *
 * @param 1st: instance
 * @param 2nd: byte[] data buffer
 * @param 3rd: length
 * @return number of bytes read, or negative on error
 */
int Java_pico_hardware_SPIBus_spi_1read( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int len      = KNI_GetParameterAsInt(3);
    spi_inst_t *spi = spi_instance(instance);
    int result;

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);

    uint8_t *data = (uint8_t *) SNI_GetRawArrayPointer(dataHandle);
    result = spi_read_blocking(spi, 0, data, len);

    KNI_EndHandles();
    return result;
}

/**
 * Full duplex SPI transfer
 *
 * @param 1st: instance
 * @param 2nd: byte[] TX data
 * @param 3rd: byte[] RX data buffer
 * @param 4th: length
 * @return number of bytes transferred, or negative on error
 */
int Java_pico_hardware_SPIBus_spi_1write_1read( void )
{
    int instance = KNI_GetParameterAsInt(1);
    int len      = KNI_GetParameterAsInt(4);
    spi_inst_t *spi = spi_instance(instance);
    int result;

    KNI_StartHandles(2);
    KNI_DeclareHandle(txHandle);
    KNI_DeclareHandle(rxHandle);
    KNI_GetParameterAsObject(2, txHandle);
    KNI_GetParameterAsObject(3, rxHandle);

    uint8_t *tx = (uint8_t *) SNI_GetRawArrayPointer(txHandle);
    uint8_t *rx = (uint8_t *) SNI_GetRawArrayPointer(rxHandle);
    result = spi_write_read_blocking(spi, tx, rx, len);

    KNI_EndHandles();
    return result;
}

/* ================================================================
 * SystemTimer natives
 * ================================================================ */

/**
 * Get current time in milliseconds since boot
 *
 * @return time in milliseconds (low 32 bits)
 */
int Java_pico_hardware_SystemTimer_timer_1get_1ms( void )
{
    return (int)(time_us_64() / 1000);
}

/**
 * Get current time in microseconds since boot
 *
 * @return time in microseconds (low 32 bits)
 */
int Java_pico_hardware_SystemTimer_timer_1get_1us( void )
{
    return (int)time_us_64();
}

/**
 * Busy-wait for specified microseconds
 *
 * @param 1st: microseconds to wait
 */
void Java_pico_hardware_SystemTimer_timer_1delay_1us( void )
{
    int us = KNI_GetParameterAsInt(1);
    busy_wait_us((uint64_t)us);
}

/* ================================================================
 * Watchdog natives
 * ================================================================ */

/**
 * Enable the watchdog timer
 *
 * @param 1st: delay in milliseconds
 * @param 2nd: 1 to pause on debug, 0 otherwise
 */
void Java_pico_hardware_Watchdog_watchdog_1enable( void )
{
    int delayMs      = KNI_GetParameterAsInt(1);
    int pauseOnDebug = KNI_GetParameterAsInt(2);
    watchdog_enable(delayMs, pauseOnDebug != 0);
}

/**
 * Update (pet) the watchdog
 */
void Java_pico_hardware_Watchdog_watchdog_1update( void )
{
    watchdog_update();
}

/**
 * Check if watchdog caused the last reboot
 *
 * @return 1 if watchdog caused reboot, 0 otherwise
 */
int Java_pico_hardware_Watchdog_watchdog_1caused_1reboot( void )
{
    return watchdog_caused_reboot() ? 1 : 0;
}

/* ================================================================
 * PIOStateMachine natives
 * ================================================================ */

/**
 * Load a PIO program into instruction memory
 *
 * @param 1st: PIO instance (0 or 1)
 * @param 2nd: int[] instructions
 * @param 3rd: number of instructions
 * @return offset in instruction memory, or negative on error
 */
int Java_pico_hardware_PIOStateMachine_pio_1load_1program( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int len     = KNI_GetParameterAsInt(3);
    PIO pio = pio_instance(pioInst);
    struct pio_program program;
    uint16_t instr_buf[32]; /* PIO instruction memory is 32 words max */
    int offset;

    if (len > 32) len = 32;

    KNI_StartHandles(1);
    KNI_DeclareHandle(instrHandle);
    KNI_GetParameterAsObject(2, instrHandle);

    for (int i = 0; i < len; i++) {
        instr_buf[i] = (uint16_t) KNI_GetIntArrayElement(instrHandle, i);
    }

    KNI_EndHandles();

    program.instructions = instr_buf;
    program.length = len;
    program.origin = -1;

    if (!pio_can_add_program(pio, &program)) {
        return -1;
    }

    offset = pio_add_program(pio, &program);
    return offset;
}

/**
 * Initialize a PIO state machine
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @param 3rd: program offset
 * @param 4th: base pin
 * @param 5th: clock divider (integer)
 * @return 0 on success, negative on error
 */
int Java_pico_hardware_PIOStateMachine_pio_1sm_1init( void )
{
    int pioInst  = KNI_GetParameterAsInt(1);
    int sm       = KNI_GetParameterAsInt(2);
    int offset   = KNI_GetParameterAsInt(3);
    int pin      = KNI_GetParameterAsInt(4);
    int clockDiv = KNI_GetParameterAsInt(5);

    PIO pio = pio_instance(pioInst);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 31);
    sm_config_set_clkdiv(&c, (float)clockDiv);

    if (pin >= 0) {
        pio_gpio_init(pio, pin);
        pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
        sm_config_set_set_pins(&c, pin, 1);
        sm_config_set_out_pins(&c, pin, 1);
    }

    pio_sm_init(pio, sm, offset, &c);

    return 0;
}

/**
 * Enable or disable a PIO state machine
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @param 3rd: 1 to enable, 0 to disable
 */
void Java_pico_hardware_PIOStateMachine_pio_1sm_1set_1enabled( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int sm      = KNI_GetParameterAsInt(2);
    int enabled = KNI_GetParameterAsInt(3);

    PIO pio = pio_instance(pioInst);
    pio_sm_set_enabled(pio, sm, enabled != 0);
}

/**
 * Write a word to the TX FIFO (blocking)
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @param 3rd: data word
 */
void Java_pico_hardware_PIOStateMachine_pio_1sm_1put( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int sm      = KNI_GetParameterAsInt(2);
    int data    = KNI_GetParameterAsInt(3);

    PIO pio = pio_instance(pioInst);
    pio_sm_put_blocking(pio, sm, (uint32_t)data);
}

/**
 * Read a word from the RX FIFO (blocking)
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @return data word
 */
int Java_pico_hardware_PIOStateMachine_pio_1sm_1get( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int sm      = KNI_GetParameterAsInt(2);

    PIO pio = pio_instance(pioInst);
    return (int)pio_sm_get_blocking(pio, sm);
}

/**
 * Check if TX FIFO is full
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @return 1 if full, 0 otherwise
 */
int Java_pico_hardware_PIOStateMachine_pio_1sm_1is_1tx_1full( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int sm      = KNI_GetParameterAsInt(2);

    PIO pio = pio_instance(pioInst);
    return pio_sm_is_tx_fifo_full(pio, sm) ? 1 : 0;
}

/**
 * Check if RX FIFO is empty
 *
 * @param 1st: PIO instance
 * @param 2nd: state machine number
 * @return 1 if empty, 0 otherwise
 */
int Java_pico_hardware_PIOStateMachine_pio_1sm_1is_1rx_1empty( void )
{
    int pioInst = KNI_GetParameterAsInt(1);
    int sm      = KNI_GetParameterAsInt(2);

    PIO pio = pio_instance(pioInst);
    return pio_sm_is_rx_fifo_empty(pio, sm) ? 1 : 0;
}

/* ================================================================
 * OnboardLED natives (Pico W: CYW43, regular Pico: GPIO 25)
 * ================================================================ */

/**
 * Initialize the onboard LED
 */
void Java_pico_hardware_OnboardLED_led_1init( void )
{
#ifdef PICO_W
    if (!cyw43_initialized) {
        cyw43_arch_init();
        cyw43_initialized = 1;
    }
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

/**
 * Set the onboard LED state
 *
 * @param 1st: 1 for on, 0 for off
 */
void Java_pico_hardware_OnboardLED_led_1set( void )
{
    int state = KNI_GetParameterAsInt(1);
#ifdef PICO_W
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, state != 0);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, state != 0);
#endif
}

/* ================================================================
 * FlashConfig natives
 * ================================================================ */

/* Config is stored at 1.5MB into flash */
#define CONFIG_FLASH_OFFSET  0x180000

/**
 * Read config data from flash
 *
 * @param 1st: byte[] buffer
 * @param 2nd: max length
 * @return number of bytes read, or negative on error
 */
int Java_pico_hardware_FlashConfig_flash_1read_1config( void )
{
    int maxLen = KNI_GetParameterAsInt(2);
    int len = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(1, bufHandle);

    uint8_t *buf = (uint8_t *) SNI_GetRawArrayPointer(bufHandle);
    const uint8_t *flash = (const uint8_t *)(XIP_BASE + CONFIG_FLASH_OFFSET);

    /* Read until maxLen or erased flash (0xFF) or null */
    while (len < maxLen && flash[len] != 0x00 && flash[len] != 0xFF) {
        buf[len] = flash[len];
        len++;
    }

    KNI_EndHandles();
    return len;
}

/* ================================================================
 * Flash natives (full read/write/erase)
 * ================================================================ */

/**
 * Erase a range of flash (must be sector-aligned)
 *
 * @param 1st: flash offset
 * @param 2nd: length in bytes
 */
void Java_pico_hardware_Flash_flash_1erase( void )
{
    uint32_t offset = (uint32_t)KNI_GetParameterAsInt(1);
    uint32_t length = (uint32_t)KNI_GetParameterAsInt(2);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, length);
    restore_interrupts(ints);
}

/**
 * Program (write) data to flash (must be page-aligned)
 *
 * @param 1st: flash offset
 * @param 2nd: byte[] data
 * @param 3rd: array offset
 * @param 4th: length
 */
void Java_pico_hardware_Flash_flash_1program( void )
{
    uint32_t offset = (uint32_t)KNI_GetParameterAsInt(1);
    int off = KNI_GetParameterAsInt(3);
    int len = KNI_GetParameterAsInt(4);

    KNI_StartHandles(1);
    KNI_DeclareHandle(dataHandle);
    KNI_GetParameterAsObject(2, dataHandle);

    uint8_t *data = (uint8_t *)SNI_GetRawArrayPointer(dataHandle);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(offset, data + off, len);
    restore_interrupts(ints);

    KNI_EndHandles();
}

/**
 * Read data from flash via XIP mapping
 *
 * @param 1st: flash offset
 * @param 2nd: byte[] buffer
 * @param 3rd: array offset
 * @param 4th: length
 */
void Java_pico_hardware_Flash_flash_1read( void )
{
    uint32_t offset = (uint32_t)KNI_GetParameterAsInt(1);
    int off = KNI_GetParameterAsInt(3);
    int len = KNI_GetParameterAsInt(4);

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(2, bufHandle);

    uint8_t *buf = (uint8_t *)SNI_GetRawArrayPointer(bufHandle);
    const uint8_t *flash = (const uint8_t *)(XIP_BASE + offset);
    memcpy(buf + off, flash, len);

    KNI_EndHandles();
}

/* ================================================================
 * BoardId natives
 * ================================================================ */

/**
 * Get the board unique ID as a hex string
 *
 * @return 16-character hex string
 */
jobject Java_pico_hardware_BoardId_board_1get_1id( void )
{
    char id_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(id_str, sizeof(id_str));

    KNI_StartHandles(1);
    KNI_DeclareHandle(resultHandle);
    KNI_NewStringUTF(id_str, resultHandle);
    KNI_EndHandlesAndReturnObject(resultHandle);
}

/**
 * Get the board unique ID as raw bytes
 *
 * @param 1st: byte[8] buffer
 */
void Java_pico_hardware_BoardId_board_1get_1id_1bytes( void )
{
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(1, bufHandle);

    uint8_t *buf = (uint8_t *)SNI_GetRawArrayPointer(bufHandle);
    memcpy(buf, id.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);

    KNI_EndHandles();
}

/* ================================================================
 * TempSensor natives
 * ================================================================ */

/**
 * Read the on-chip temperature in centi-degrees Celsius
 * (e.g. 2705 = 27.05 C)
 *
 * Formula: T = 27 - (V - 0.706) / 0.001721
 * where V = adc_raw * 3.3 / 4096
 *
 * In integer math (centi-degrees):
 * T_centi = 2700 - (adc_raw * 3300 - 2891776) * 100 / 70508
 */
int Java_pico_hardware_TempSensor_temp_1read( void )
{
    adc_set_temp_sensor_enabled(true);

    /* Save and restore selected input */
    uint old_input = adc_get_selected_input();

    /* Temperature sensor is input 4 on RP2040/RP2350 QFN-60 */
    adc_select_input(4);
    uint16_t raw = adc_read();

    adc_select_input(old_input);

    /* Convert to centi-Celsius using integer math */
    int numerator = (int)raw * 3300 - 2891776;
    int temp_centi = 2700 - (numerator * 100) / 70508;

    return temp_centi;
}

/* ================================================================
 * RNG natives
 * ================================================================ */

/**
 * Get a random 32-bit integer
 */
int Java_pico_hardware_RNG_rng_1get_1int( void )
{
    return (int)get_rand_32();
}

/**
 * Get a random 64-bit long
 */
long long Java_pico_hardware_RNG_rng_1get_1long( void )
{
    return (long long)get_rand_64();
}

/**
 * Fill a byte array with random bytes
 *
 * @param 1st: byte[] buffer
 * @param 2nd: length
 */
void Java_pico_hardware_RNG_rng_1get_1bytes( void )
{
    int len = KNI_GetParameterAsInt(2);

    KNI_StartHandles(1);
    KNI_DeclareHandle(bufHandle);
    KNI_GetParameterAsObject(1, bufHandle);

    uint8_t *buf = (uint8_t *)SNI_GetRawArrayPointer(bufHandle);

    /* Fill 4 bytes at a time */
    int i = 0;
    while (i + 4 <= len) {
        uint32_t r = get_rand_32();
        buf[i]     = (uint8_t)(r);
        buf[i + 1] = (uint8_t)(r >> 8);
        buf[i + 2] = (uint8_t)(r >> 16);
        buf[i + 3] = (uint8_t)(r >> 24);
        i += 4;
    }
    /* Remaining bytes */
    if (i < len) {
        uint32_t r = get_rand_32();
        while (i < len) {
            buf[i++] = (uint8_t)r;
            r >>= 8;
        }
    }

    KNI_EndHandles();
}

} /* extern "C" */
