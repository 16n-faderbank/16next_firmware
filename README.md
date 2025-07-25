# 16next firmware

A firmware for next-gen 16n devices, such as 16nx.

16nx is a controller for electronic music. It is 16 faders that manifest as:

- a class compliant MIDI device over USB
- a traditional MIDI device over TRS (either type A or type B)
- a device emitting monome-style I2C data over 3.5mm jack
- 16 analogue 0-5Vish control voltage outputs.

It is edited and managed via a [web-based editor][editor].

## Firmware Version

3.1.1

## Introduction

This is firmware for a "next-gen" version of the 16n faderbank; the first manifestation of that is the 16nx design. It could, however, run on other boards / designs too, hence the generic name.

The firmware is written for the Raspberry Pi RP2040 chip; the 16nx board itself is a custom design, but it could be repurposed to run on almost any RP2040 board with enough pinouts.

It's coded in Pico SDK (raw C++); it does _not_ use Arduino (unlike the previous firmware). This is largely owing to when it was begun, and the state of the Arduino core - and also the author's preference for it.

> **Important note on upgrading from < 3.1.0 to 3.1.0+**
>
> If you are upgrading from a device that has already had a pre-3.1.0 firmware installed on it, you will **need** to head to the [editor][editor] immediately and edit your configuration; _some_ USB or TRS channels _might_ have set themselves to "high resolution" mode, and you will likely want to disable this.
>
> This arises because the config is derived from previously unused areas of memory, which may lead to confusion.
>
> If you are installing 3.1.0 onto a _fresh_ RP2040, the default settings should avoid this; it's only an issue when upgrading firmware.
>
> In general, you should always head to the editor to _check_ your configuration after a firmware upgrade.

## Requirements

- `pico-sdk` 1.5.0+ somewhere on your path, with the `PIDO_SDK` environment variable pointing at it
- `cmake`
- ARM Cortex dev tools - specifically, `arm-none-eabi-gcc` available on your path.

Basically: a very standard Pico SDK development toolchain.

The author recommends VS Code, with the C++ and CMake extensions available. A `.clang-format` file is included; if you have the C++ extension installed, you will be able to autoformat all files according to this on save, which will make your life more pleasant. Unless you disagree with my code preferences.

## Compilation

You'll need to pull down the [`midi_uart_lib`][trs_midi_lib] dependency before you can compile this code:

    git submodule update --recursive

With this pulled down, you should be able to Configure `cmake` (eg, using the VS Code plugin: "_CMake: Configure_"), and then Build ("_CMake: Build_").

Or, from the command line, configure:

    mkdir build
    cd build
    cmake ..

and then, from that directory:

    make 16next

This will produce the file `./build/16next.uf2` which can be flashed to your 16nx board.

## Flash storage

The RP2040 has no on-board flash memory whatsoever, and uses external flash RAM to store code. It also has no internal EEPROM. To save user data, we use the end of the onboard flash RAM.

Specifically, the code uses the last sector (4096 bytes) of onboard flash RAM as a wear-levelled 256byte storage, a bit like an EEPROM:

- you can only WRITE to a page (256 bytes) (and no less)
- you can only WRITE to erased data (ie, flip bits low)
- you can only ERASE a _sector_ (4096 bytes)
- so, as per https://www.makermatrix.com/blog/read-and-write-data-with-the-pi-pico-onboard-flash , we can use this as a form of wear levelling:
  - find the last sector in memory
  - find the first page that begins with 0xFF
  - that's the page to write to
  - the page to read from is `lastEmptyPage-1`
  - if there's no empty page, you need to erase the whole sector and write to page 0

256 bytes is fine enough for the storage we need; to store more than 256 bytes, we'd have to use a larger section of RAM.

Flashing the MCU will only delete user data _if_ the firmware is big enough to extend to that part of Flash RAM; currently, that seems unlikely.

## Code layout

- `main.cpp` is our main entry point and executable. Most code is in here.
- `main.h` is effectively a configuration file for that.
- `midi_uart_lib_config.h` configures the library we use for TRS MIDI over the UART pins.
- `tusb_config.h` and `usb_descriptors.h` configure TinyUSB, used for MIDI.
- `lib` contains:
  - an implementation of [Responsive Analog Read][rar]
  - `config.h/cpp`, which contain Structs and functions for applying configuration data to the device, and saving/loading it from RAM.
  - `flash_onboard.h/cpp` which implement storage of user data in Flash RAM
  - `i2c_utils.h/cpp` which contain functionality useful for I2C, particular Leader mode.
  - `sysex.h/cpp` which contains functions related to sysex data handling.
- `board` contains a board definition for the 16nx hardware.

## MIDI details

The device is edited from a web-based tool; this sends and receives data over MIDI system exclusive ("sysex") data; the spec is in `SYSEX_SPEC.md`.

MIDI is enabled via TinyUSB. `tusb_config.h` configures this, and `usb_descriptors.c` is where the device name and descriptors are set up. The serial number is based on the unique identifier of the flash RAM used to store program data.

The MIDI buffer is 64 bytes long for a low-speed device. As such, sysex messages need to be processed by a little state machine to handle messages longer than 64 bytes.

## Default configuration, configuration reset

When the device fails to detect an initial configuration (ie, the second byte of the storage ram is not `0xFF`) it overwrites it with the default config.

At any point, the default config can be restored by sending sysex message `0x1A` (for 1nitiAlize).

## Configuring 16nx

16nx is configured by its [web-based editor][editor]. The editor sourcecode is [available here][editor-source].

## Internal LED

There's an "internal" LED on a 16nx board, available at GPIO2. If `midiLed` is true in the controller config, this LED will flash on MIDI activity. If `powerLed` is true in the controller config, this LED will be on permanently.

## Memory Map

The user data stored in flash (and indeed, sent via sysex) has the following shape:

| Address | Format | Description                        |
| ------- | ------ | ---------------------------------- |
| 0       | 0/1    | LED on when powered                |
| 1       | 0/1    | LED blink on MIDI data             |
| 2       | 0/1    | Rotate controller outputs via 180º |
| 3       | 0/1    | I2C Master/Follower                |
| 4,5     | 0-127  | FADERMIN lsb/msb                   |
| 6,7     | 0-127  | FADERMAX lsb/msb                   |
| 8       | 0/1    | Soft MIDI thru (default 0)         |
| 9-15    |        | Currently unused                   |
| 16-31   | 0-15   | Channel for each control (USB)     |
| 32-47   | 0-15   | Channel for each control (TRS)     |
| 48-63   | 0-127  | CC for each control (USB)          |
| 64-79   | 0-127  | CC for each control (TRS)          |
| 80-82   | 0-127  | Booleans for high-res mode (USB)\* |
| 83-85   | 0-127  | Booleans for high-res mode (TRS)\* |

### High-res mode booleans

Each controller can optionally operate in high res mode, sending 14-bit data as two CCs: an "MSB" (albeit 7-bits) on (CC), and an "LSB" (again, 7-bits) on (CC+32). We store this option as a boolean. Because Sysex data can only transmit 7-bits (0x00-0x7F), we need to store this inside three bytes of data. To keep things straightforward, we'll store the high-res mode for USB and TRS as two separate values, each requiring 3 7-bit values to describe.

## Debug connector

A debug connector on the front of the board is a JST-SH connector breaking out ARM SWD (single-wire-debug) ready for connection to a [Pico Debug Probe][debugprobe]. This allows developers to use open-source debug tools (OpenOCD, [Cortex Debug](https://github.com/Marus/cortex-debug)) to debug the firmware in a more... pleasant manner than endless serial dumps.

## NB: picotool usage

As an RP2040 developer, you might wish to flash a board without having to reach for a `BOOTSEL` or `RESET` button. Unfortunately, I've had no joy enabling the method `picotool` uses to force the board into `BOOTSEL` mode, as it requires the UART to be handled over USB, and we're already using UART for our MIDI out. So I recommend making a `BOOTSEL` button easily accessible on any board you'd like to use this firmware with.

## 16next? 16nx? What's the difference

I'm using _16next_ to refer to all next-gen, RP2040, 16n-style devices. It was the original name of the project. I'm separating out the firmware and hardware repositories because they move at different rates, and have their own releases.

Also: it's highly likely that this firmware will run just fine on _multiple_ different boards. Given that, let's give it a more generic name.

## License

Licensed under the MIT License (MIT). See `LICENSE.md` for details.

[trs_midi_lib]: https://github.com/rppicomidi/midi_uart_lib
[rar]: https://github.com/dxinteractive/ResponsiveAnalogRead
[debugprobe]: https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html
[editor]: https://16n-faderbank.github.io/editor/
[editor-source]: https://github.com/16n-faderbank/16n-editor
