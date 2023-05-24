# 16next

## TODO

* enable TRS MIDI
* enable TRS SPI1
* do EEPROM per-page writes...

## DONE

* fixup memory map
* update datastructure to reflect 16n config
* start out by just making it hardcoded.
* add proper serial number support
* port to RAR
* enable midiblinky
* enable powerled
* sysex sending config.
* store and load data in memory
* receive sysex data in
* test CV outs
* find MIDI jack and MIDI switch in parts bin
* enable SPI0 EEPROM on right pins
* connect to SPI0 EEPROM

---

16next is a controller for electronic music. It is 16 faders that manifest as:
* a class compliant MIDI device over USB
* a traditional MIDI device over TRS (either type A or type B)
* a device emitting monome-style I2C data over 3.5mm jack
* 16 analogue 0-5Vish control voltage outputs.

It is edited and managed via a web-based editor.

Internally, it is based around an RP2040, connected to:

* external I2C EEPROM
* a multiplexer to read multiple faders
* 16 faders
* dedicated USB-C connector

## Firmware Version

0.0.1

## REQUIREMENTS

Built with Raspberry Pi Pico SDK 1.5.0.

I've used CMake and Make to manage development; CMakeLists are set up as per the Pico recommendations. That means the Pico SDK should be somewhere on your system, described by the environment variable PICO_SDK_PATH.

I build from within VS Code, using the CMake plugin, and `arm-none-eabi-gcc`.

## IMPLEMENTATION DETAILS

A few notes on key code specifics. The Pico library is not too obtuse, and there's reasonable commenting within there.

### `lib` directory

`lib` contains:

* an implementation of Bounce2 (used for our buttons)
* an implementation of Responsive Analog Read 
* `spi_flash` as supplied in the Pico SDK examples.

### Flash memory implementation

To avoid confusion (and overwriting program code), an external I2C EEPROM chip is used as memory. This stores the config for the device.

### MIDI notes

The device reads MIDI system exclusive ("sysex") data from the editor in order to be controlled from the web; the spec is in `SYSEX_SPEC.md`. 

MIDI is enabled via TinyUSB; `tusb_config.h` configures this, and `usb_descriptors.c` is where the device name and descriptors are set up. The serial number is based on the unique identifier of the flash RAM used to store program data.

The MIDI buffer is 64 bytes long for a low-speed device. As such, sysex messages need to be processed by a little state machine to handle messages longer than 64 bytes.

### Default configuration, configuration reset

When the device fails to detect an initial configuration (ie, the second byte of the storage ram is not `0xFF`) it overwrites it with the default config.

At any point, the default config can be restored by sending sysex message TODOTODO:

### Internal LED

There's an "internal" LED on the board, at GPIO22. This is visible through the slot of fader 1, or around the usb port. (It really should have a pinhole in the side, tbh). If `midiLed` is true in the controller config, this will flash on MIDI activity. If `powerLed` is true in the controller config, this will be on permanently.

## Memory Map

| Address | Format |            Description             |
|---------|--------|------------------------------------|
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
