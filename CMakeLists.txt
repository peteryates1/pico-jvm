#
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl) 
#

cmake_minimum_required(VERSION 3.22)

if (NOT DEFINED TARGET)
    set(TARGET PICO)
endif()

option(PICO_W "Build for Pico W with WiFi support" OFF)
option(PICO_SD "Build with SD card support" OFF)

find_package(Java 1.8 EXACT REQUIRED)
include(ExternalProject)

if (${TARGET} STREQUAL PICO)
    if (PICO_W AND NOT DEFINED PICO_BOARD)
        set(PICO_BOARD pico_w)
    endif()
    # initialize the SDK based on PICO_SDK_PATH
    # note: this must happen before project()
    include(pico_sdk_import.cmake)
endif()

project(pjvm VERSION 0.0.6)

include(CTest) 

if (${TARGET} STREQUAL PICO)
    # initialize the Raspberry Pi Pico SDK
    pico_sdk_init()
endif()

if (${TARGET} STREQUAL PICO)
    # LittleFS for internal flash filesystem (always available)
    include(FetchContent)
    FetchContent_Declare(littlefs
        GIT_REPOSITORY https://github.com/littlefs-project/littlefs.git
        GIT_TAG        v2.11.2
    )
    FetchContent_MakeAvailable(littlefs)
    add_library(littlefs STATIC
        ${littlefs_SOURCE_DIR}/lfs.c
        ${littlefs_SOURCE_DIR}/lfs_util.c
    )
    target_include_directories(littlefs PUBLIC ${littlefs_SOURCE_DIR})
    # littlefs needs the same compile flags as the rest of the firmware
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m33")
        target_compile_options(littlefs PRIVATE -mcpu=cortex-m33 -mthumb -march=armv8-m.main+fp+dsp)
    else()
        target_compile_options(littlefs PRIVATE -mcpu=cortex-m0plus -mthumb)
    endif()
endif()

if (${TARGET} STREQUAL PICO AND PICO_SD)
    include(FetchContent)
    FetchContent_Declare(FatFs_SPI
        GIT_REPOSITORY https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico.git
        GIT_TAG master
    )
    FetchContent_Populate(FatFs_SPI)
    # Build FatFs_SPI manually to exclude rtc.c (not compatible with RP2350)
    set(FATFS_SPI_DIR ${fatfs_spi_SOURCE_DIR}/FatFs_SPI)
    add_library(FatFs_SPI INTERFACE)
    target_sources(FatFs_SPI INTERFACE
        ${FATFS_SPI_DIR}/ff15/source/ffsystem.c
        ${FATFS_SPI_DIR}/ff15/source/ffunicode.c
        ${FATFS_SPI_DIR}/ff15/source/ff.c
        ${FATFS_SPI_DIR}/sd_driver/sd_spi.c
        ${FATFS_SPI_DIR}/sd_driver/demo_logging.c
        ${FATFS_SPI_DIR}/sd_driver/spi.c
        ${FATFS_SPI_DIR}/sd_driver/sd_card.c
        ${FATFS_SPI_DIR}/sd_driver/crc.c
        ${FATFS_SPI_DIR}/src/glue.c
        ${FATFS_SPI_DIR}/src/f_util.c
        ${FATFS_SPI_DIR}/src/ff_stdio.c
        ${FATFS_SPI_DIR}/src/my_debug.c
        # rtc.c excluded: uses hardware_rtc which is RP2040-only
        ${CMAKE_SOURCE_DIR}/src/pico/sd_rtc_stub.c
    )
    target_include_directories(FatFs_SPI INTERFACE
        ${FATFS_SPI_DIR}/ff15/source
        ${FATFS_SPI_DIR}/sd_driver
        ${FATFS_SPI_DIR}/include
    )
    target_link_libraries(FatFs_SPI INTERFACE
        hardware_spi
        hardware_dma
        pico_stdlib
    )
endif()

ExternalProject_Add(jvm_lib SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib
                            CMAKE_ARGS -DLIBJVM_INSTALL_DIR:STRING=${CMAKE_BINARY_DIR}
                                       -DLIBJVM_VERSION:STRING=${PROJECT_VERSION}
                                       -DCMAKE_BUILD_TYPE:STRING=MinSizeRel
                                       -DTARGET:STRING=${TARGET}
                                       -DPICO_W:BOOL=${PICO_W}
                                       -DPICO_SD:BOOL=${PICO_SD}
                                       -DCMAKE_SYSTEM_PROCESSOR:STRING=${CMAKE_SYSTEM_PROCESSOR}
                                       -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE
                            USES_TERMINAL_BUILD TRUE
                            BUILD_ALWAYS TRUE
                            BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/jvm_lib-prefix/src/jvm_lib-build/libjvm.a
)
ExternalProject_Get_property(jvm_lib BINARY_DIR)
add_library(jvm STATIC IMPORTED)
set_property(TARGET jvm PROPERTY IMPORTED_LOCATION ${BINARY_DIR}/libjvm.a)
add_dependencies(jvm jvm_lib)

if (${TARGET} STREQUAL PICO)
    set(PJVM_SOURCES src/pico/Main.cpp src/pico/OS.cpp src/pico/Natives.cpp src/pico/FSNatives.cpp)
    if (PICO_W)
        list(APPEND PJVM_SOURCES src/pico/NetNatives.cpp)
    endif()
    if (PICO_SD)
        list(APPEND PJVM_SOURCES src/pico/SDNatives.cpp)
    endif()
endif()
if (${TARGET} STREQUAL LINUX)
    set(PJVM_SOURCES src/linux/Main.cpp src/linux/OS.cpp)
endif()

add_executable(pjvm ${PJVM_SOURCES})

if (${TARGET} STREQUAL PICO)
    set(PJVM_INCLUDE_OS_DIRS lib/src/vm/os/pico ${PICO_SDK_PATH}/src/rp2_common/hardware_adc/include ${CMAKE_SOURCE_DIR}/src/pico)
endif()
if (${TARGET} STREQUAL LINUX)
    set(PJVM_INCLUDE_OS_DIRS lib/src/vm/os/linux)
endif()

target_include_directories(pjvm PRIVATE 
                    lib/src/include 
                    lib/src/vm/share/runtime
                    lib/src/vm/share/utilities
                    lib/src/vm/share/natives
                    lib/src/vm/share/memory
                    lib/src/vm/cpu/c
                    lib/src/vm/share/ROM
                    ${PJVM_INCLUDE_OS_DIRS}
)

if (${TARGET} STREQUAL PICO)
    target_compile_definitions(pjvm PRIVATE PICO)
    if (PICO_W)
        target_compile_definitions(pjvm PRIVATE PICO_W)
    endif()
    if (PICO_SD)
        target_compile_definitions(pjvm PRIVATE PICO_SD=1)
    endif()
endif()
if (${TARGET} STREQUAL LINUX)
    target_compile_definitions(pjvm PRIVATE PRODUCT)
    target_compile_options(pjvm PRIVATE -m32)
    target_link_options(pjvm PRIVATE -m32)
endif()

target_link_libraries(pjvm jvm)
target_link_directories(pjvm PRIVATE ${CMAKE_BINARY_DIR})

if (${TARGET} STREQUAL PICO)
    # Add pico_stdlib library which aggregates commonly used features
    target_link_libraries(pjvm pico_stdlib hardware_adc
                          hardware_pwm hardware_uart hardware_i2c hardware_spi
                          hardware_timer hardware_watchdog hardware_pio hardware_clocks
                          hardware_flash pico_unique_id pico_rand littlefs)
    if (PICO_W)
        target_link_libraries(pjvm pico_cyw43_arch_lwip_threadsafe_background pico_lwip_mqtt)
    endif()
    if (PICO_SD)
        target_link_libraries(pjvm FatFs_SPI)
    endif()
    # create map/bin/hex/uf2 file in addition to ELF.
    pico_add_extra_outputs(pjvm)
    pico_enable_stdio_usb(pjvm 1)
    pico_enable_stdio_uart(pjvm 1)
endif()

add_custom_command(OUTPUT main.jar.bin
                   DEPENDS test/Main.java pjvm
                   COMMAND mkdir ARGS -p ${CMAKE_BINARY_DIR}/main.dir
                   COMMAND mkdir ARGS -p ${CMAKE_BINARY_DIR}/main.dir.preverify
                   COMMAND javac ARGS -source 1.4 -target 1.4 -d ${CMAKE_BINARY_DIR}/main.dir -bootclasspath ${CMAKE_BINARY_DIR}/classes.jar
                        ${CMAKE_SOURCE_DIR}/test/Main.java
                   COMMAND ${BINARY_DIR}/preverify-prefix/src/preverify-build/preverify
                        ARGS -classpath ${CMAKE_BINARY_DIR}/classes.jar
                        -d ${CMAKE_BINARY_DIR}/main.dir.preverify
                        ${CMAKE_BINARY_DIR}/main.dir
                   COMMAND jar ARGS -cfM0 ${CMAKE_BINARY_DIR}/main.jar -C ${CMAKE_BINARY_DIR}/main.dir.preverify .
                   COMMAND ${CMAKE_SOURCE_DIR}/tools/wrapjar.sh ARGS ${CMAKE_BINARY_DIR}/main.jar ${CMAKE_BINARY_DIR}/main.jar.bin
)
add_custom_target(main ALL DEPENDS main.jar.bin)

#
# Tests currently run only on Linux
#
if (${TARGET} STREQUAL LINUX)
    add_test(Sanity ${CMAKE_BINARY_DIR}/pjvm -cp ${CMAKE_BINARY_DIR}/main.jar Main)
endif()  

#
# Build the distribution tree
#
if (${TARGET} STREQUAL PICO)
    install(FILES ${CMAKE_BINARY_DIR}/pjvm.uf2 
            DESTINATION .
            RENAME pjvm-${PROJECT_VERSION}.uf2
    )
    set(PJVM_BIN_PROGRAMS ${CMAKE_SOURCE_DIR}/tools/wrapjar.sh ${BINARY_DIR}/preverify-prefix/src/preverify-build/preverify)
    if (DEFINED HOST_BINARY_DIR)
        set(PJVM_BIN_PROGRAMS ${PJVM_BIN_PROGRAMS} ${HOST_BINARY_DIR}/pjvm)
    endif()
    install(PROGRAMS ${PJVM_BIN_PROGRAMS}
            DESTINATION bin
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/doc
            DESTINATION .
    )
    install(FILES ${CMAKE_BINARY_DIR}/classes.jar
            DESTINATION lib
    )
    set(CPACK_PACKAGE_FILE_NAME pjvm-${PROJECT_VERSION})
    include(CPack)
endif()
