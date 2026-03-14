[![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/orenskl/pico-jvm/main.yml?label=build)](https://github.com/orenskl/pico-jvm/actions/workflows/main.yml)
[![GitHub Tag](https://img.shields.io/github/v/tag/orenskl/pico-jvm)](https://github.com/orenskl/pico-jvm/tags)

# Pico JVM

This is a Java virtual machine for the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/), this project is based on the [CLDC](https://en.wikipedia.org/wiki/Connected_Limited_Device_Configuration) virtual machine and more specifically on the [phoneME](https://phonej2me.github.io) project from Sun/Oracle. The [phoneME](https://phonej2me.github.io) is a very old project that currently is not maintained anymore. However I was able to find a github [repo](https://github.com/magicus/phoneME) that I used as a reference. This JVM is targeted to small embedded devices with limited resources so don't expected a full blown Java experience on [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/).

The original [phoneME](https://phonej2me.github.io) project used a Makefile based build system, I converted the build system to CMake so I can use more modern tools and integration with modern CI/CD workflows. There are currently two main targets : Linux (used mainly for debugging) and Pico.

Currently the JVM support [CLDC 1.1](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/index.html) specification which is a limited subset of the Java  J2SE specification and language.

## Features

+ Small footprint - 348KB Flash, 15KB RAM base (not including the Java heap)
+ Supports Raspberry Pi Pico (RP2040) and Pico 2 W (RP2350)
+ Java 1.4 and [CLDC 1.1](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/index.html) API
+ Standard `java.io` file I/O with internal flash filesystem (LittleFS) - no extra hardware needed
+ Optional SD card support via SPI with the same `java.io` API
+ WiFi networking on Pico W (TCP, UDP, MQTT)
+ [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) Hardware API

### Java API

#### Always available

| Package | Classes | Description |
|---|---|---|
| `pico.hardware` | `GPIOPin`, `ADCChannel`, `PWMChannel` | Digital/analog I/O |
| `pico.hardware` | `UARTPort`, `I2CBus`, `SPIBus` | Serial buses |
| `pico.hardware` | `PIOStateMachine` | Programmable I/O |
| `pico.hardware` | `SystemTimer`, `Watchdog` | Timing and watchdog |
| `pico.hardware` | `OnboardLED` | Board LED (CYW43 on Pico W) |
| `pico.hardware` | `Flash`, `FlashConfig` | Direct flash access and key-value config |
| `pico.hardware` | `BoardId` | Unique 64-bit board identifier |
| `pico.hardware` | `TempSensor` | On-chip temperature sensor |
| `pico.hardware` | `RNG` | Hardware-seeded random number generator |
| `java.io` | `File`, `FileInputStream`, `FileOutputStream`, `FileNotFoundException` | File I/O on internal flash (LittleFS) |

#### With `-DPICO_W=ON` (Pico W / Pico 2 W)

| Package | Classes | Description |
|---|---|---|
| `pico.net` | `WiFi` | WiFi station mode (connect, status, IP) |
| `pico.net` | `TCPSocket` | Blocking TCP client |
| `pico.net` | `UDPSocket` | UDP send/receive |
| `pico.net.mqtt` | `MQTTClient` | MQTT 3.1.1 client (QoS 0/1, backed by lwIP) |

#### With `-DPICO_SD=ON` (SD card via SPI)

| Package | Classes | Description |
|---|---|---|
| `pico.hardware` | `SDCard` | SD card mount/unmount |
| `java.io` | `File`, `FileInputStream`, `FileOutputStream` | File I/O on SD card (paths prefixed with `/sd/`) |

### Filesystem

The JVM includes a built-in filesystem on internal flash using [LittleFS](https://github.com/littlefs-project/littlefs), providing wear-leveled, power-loss resilient file storage with no extra hardware. This gives ~448KB on Pico (2MB flash) or ~2.4MB on Pico 2 (4MB flash).

When SD card support is enabled, files accessed via `/sd/...` paths are routed to the SD card (FAT32 via [FatFs](http://elm-chan.org/fsw/ff/)). All other paths use the internal flash filesystem.

```java
// Internal flash - always available
FileOutputStream fos = new FileOutputStream("/data.txt");
fos.write("Hello!".getBytes());
fos.close();

// SD card - requires SDCard.mount() first
SDCard.mount();  // SPI0: MISO=GP16, CS=GP17, SCK=GP18, MOSI=GP19
FileOutputStream sd = new FileOutputStream("/sd/log.csv");
```

### Flash layout

```
0x000000  Firmware (pjvm.elf)         ~348-663KB depending on options
0x100000  Application (main.jar.bin)
0x180000  FlashConfig (key=value)     4KB
0x190000  LittleFS filesystem         to end of flash
```

## Installation and setup

Download the release package from the [Releases](https://github.com/orenskl/pico-jvm/releases) page of this repository, extract the package. The package contains the following content :

```
├── bin
├── doc
├── lib
└── pjvm-X.Y.Z.uf2
```

The `bin` directory contains tools and scripts required to post process class and jar files to be able to run them on the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/). This directory also contains a Linux version of the virtual machine (`pjvm`) that may be used in development
stage to test applications on your host machine.

The `doc` directory contains the javadoc for the device specific (e.g. GPIO) classes.

The `lib` directory contains the run-time class libraries (`classes.jar`)

The `pjvm-X.Y.Z.uf2` (where X.Y.Z is the version of the firmware) is the Java Virtual Machine UF2 file, this file needs to be flashed to the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) using [picotool](https://github.com/raspberrypi/picotool). The virtual machine already contains the run time classes so these are not required to be flashed separately.

The `examples` directory in this repository contains some examples you can run with an [Ant](https://ant.apache.org) `build.xml` file. To build the examples and the following Hello World application you will need to setup the environment variable `JAVA_PICO_HOME` to point to the extracted package location.

For example if you extracted the package to `/opt/pjvm-X.Y.Z` you will need to setup the variable with the following command :

```
export JAVA_PICO_HOME=/opt/pjvm-X.Y.Z
```

You will also need [JDK 8](https://www.oracle.com/java/technologies/javase/javase8-archive-downloads.html) to build application, currently the latest versions of Java are not supported.

## Building and running a Java application

To run a Java application on the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) you will need to flash the Java virtual machine itself and than flash the Java application at the address `0x10100000`.

Lets say we have a simple hello world application :

```java
class Main {
    public static void main(String[] args) {
        System.out.println("Hello, World!"); 
    }
}
```

**NOTE : The name of the class `Main` is currently fixed as the first class that is loaded by the VM.**

1. Compile the class :

    ```
    javac -source 1.4 -target 1.4 -d main.dir -bootclasspath $JAVA_PICO_HOME/lib/classes.jar Main.java
    ```

    Make sure to setup you environment correctly so that `JAVA_PICO_HOME` points to the right place (see [here](#installation-and-setup))

2. Preverify the classes

    Before running the compiled classes on the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) we will need to Preverify them. Preverifying is the process of post processing the class files so they can be run more efficiently on the target system. Preverifying is done with the `preverify` tool in the `bin` directory of the package.

    ```
    $JAVA_PICO_HOME/bin/preverify -d main.preverify -classpath $JAVA_PICO_HOME/lib/classes.jar main.dir
    ```

3. Package the application as a JAR file :

    ```
    cd main.preverify
    jar -cfM0 ../main.jar .
    ```

4. Wrap the JAR file 

    Now we need to wrap the JAR with a header so we can run it on the Pi Pico :

    ```
    $JAVA_PICO_HOME/bin/wrapjar.sh main.jar main.jar.bin
    ```

    The `wrapjar.sh` script is located in the `bin` directory of the package.

5. Flash the binary file and reboot

    Now we can flash the application to address `0x10100000` using `picotool` :

    ```
    picotool load build/main.jar.bin --offset 10100000
    ```

    Reboot your Pi Pico and you should see `Hello, World!` on your terminal

This repository includes an `example` directory with a complete [Ant](https://ant.apache.org) `build.xml` for each example that runs all the above steps in a single command.

## Building

This project can be built on Ubuntu 22+ as the build machine, please install the following packages :

```
sudo apt-get install -y gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential gcc-multilib g++-multilib ninja-build
```

You will also need JDK 8 for this, please install it and make sure it is your default Java installation.

After cloning the project cd into it and run the the usual CMake commands. Make sure you set `PICO_SDK_PATH` to point to your Pico SDK location.

### Build targets

**Pico (RP2040) - basic:**
```
mkdir build-pico && cd build-pico
PICO_SDK_PATH=/opt/pico-sdk cmake -DTARGET=PICO ..
cmake --build .
```

**Pico W (RP2040 with WiFi):**
```
mkdir build-picow && cd build-picow
PICO_SDK_PATH=/opt/pico-sdk cmake -DTARGET=PICO -DPICO_W=ON ..
cmake --build .
```

**Pico 2 W (RP2350 with WiFi):**
```
mkdir build-pico2w && cd build-pico2w
PICO_SDK_PATH=/opt/pico-sdk cmake -DTARGET=PICO -DPICO_W=ON -DPICO_BOARD=pico2_w ..
cmake --build .
```

**With SD card support** (add to any Pico target):
```
PICO_SDK_PATH=/opt/pico-sdk cmake -DTARGET=PICO -DPICO_SD=ON ..
```

**Linux** (for development/debugging):
```
mkdir build-linux && cd build-linux
cmake -DTARGET=LINUX ..
cmake --build .
```

### CMake options

| Option | Default | Description |
|---|---|---|
| `TARGET` | `PICO` | Target platform (`PICO` or `LINUX`) |
| `PICO_W` | `OFF` | Enable WiFi/networking support (Pico W boards) |
| `PICO_SD` | `OFF` | Enable SD card filesystem via SPI |
| `PICO_BOARD` | `pico_w` (when PICO_W=ON) | Board type (set to `pico2_w` for RP2350) |

### Binary sizes

| Configuration | Flash | RAM |
|---|---|---|
| Pico (base) | 348KB | 15KB |
| Pico + SD | 386KB | 19KB |
| Pico 2 W (WiFi) | 663KB | 76KB |
| Pico 2 W (WiFi + SD) | 696KB | 80KB |

If all goes well you should end up with a `pjvm.uf2` file in your `build` directory. This file can be flashed to the Pi Pico (helper scripts can be found in the `tools` directory). The `pjvm.uf2` file is the Java VM itself and includes the system classes already romized inside it. A Java application is loaded separately into the flash of the Pi Pico at a specific address.

### SD card wiring (SPI0 defaults)

| Pico Pin | GPIO | SD Card |
|---|---|---|
| 21 | GP16 | MISO (DO) |
| 22 | GP17 | CS |
| 24 | GP18 | SCK (CLK) |
| 25 | GP19 | MOSI (DI) |
| 36 | 3V3 | VCC |
| 38 | GND | GND |

Custom pins can be passed to `SDCard.mount(spi, sck, mosi, miso, cs)`.


