# 16next

## TODO

* port to RAR
* enable SPI0 EEPROM on right pins
* connect to SPI0 EEPROM
* enable TRS MIDI
* enable TRS SPI1

## DONE

* fixup memory map
* update datastructure to reflect 16n config
* start out by just making it hardcoded.
* add proper serial number support

---

JT8 is a MIDI controller for media composers. It is a class-compliant MIDI device, with has eight faders, which appear as up to four banks of continuous controllers, and a button to switch between them.

It is edited and managed via a web-based editor.

Internally, it is a Raspberry Pi Pico on a custom board, providing:

* external SPI flash ram
* multiplexer to read multiple faders
* controls - faders, buttons, lights
* dedicated USB-C connector

## Firmware Version

0.0.1

## REQUIREMENTS

Built with Raspberry Pi Pico SDK 1.5.0.

I've used CMake and Make to manage development; CMakeLists are set up as per the Pico recommendations. That means the Pico SDK should be somewhere on your system, described by the environment variable PICO_SDK_PATH.

I build from within VS Code, using the CMake plugin, and `arm-none-eabi-gcc`.

## IMPLEMENTATION DETAILS

A few notes on key code specifics. The Pico library is not too obtuse, and there's reasonable commenting within there.

I got quite into using `gpio_put_masked` to update the four indicator LEDs status all at once. I am not sure how wise this was, but it'll do.

### `lib` directory

`lib` contains:

* an implementation of Bounce2 (used for our buttons)
* an implementation of Responsive Analog Read (once used for faders)
* an broader Hysteresis-Filter implementation, `HystFilter`, which is used to denoise faders.
* `spi_flash` as supplied in the Pico SDK examples.

### Flash memory implementation

To avoid confusion (and overwriting program code), an external SPI Flash chip is used as memory. This stores both the config for the device, and the 'last used' bank.

The bank is not written on every button press; a counter-based check in the main loop is used to check if the bank has changed and, if it has changed, and that change is more than a second old, _then_ it is written to the flash.

### MIDI notes

The device reads MIDI system exclusive ("sysex") data from the editor in order to be controlled from the web; the spec is in `SYSEX_SPEC.md`. The device also responds to program change messages on its assigned channel, to swap between banks. (It does not transmit PC, so as not to confuse music software; it transmits a dedicated sysex to indicate to the editor the bank has changed).

MIDI is enabled via TinyUSB; `tusb_config.h` configures this, and `usb_descriptors.c` is where the device name and descriptors are set up.

The MIDI buffer is 64 bytes long for a low-speed device. As such, sysex messages need to be processed by a little state machine to handle messages longer than 64 bytes.

### Default configuration, configuration reset

When the device fails to detect an initial configuration (ie, the second byte of the storage ram is not `0xFF`) it overwrites it with the default config.

At any point, the default config can be restored:

* with the device disconnected, hold down the bank select button, and connect the device
* all four LEDs light up. This is a warning: "_if you keep holding the button, the config will be reset_"
* keep holding the button for about three seconds. All the LEDs will flash. When they stop flashing, the default config has been enabled.

### Internal LED

There's an "internal" LED on the board, at GPIO22. This is visible through the slot of fader 1, or around the usb port. (It really should have a pinhole in the side, tbh). If `midiBlinkEnabled` is true in the code, this will flash on MIDI activity.

---

## Joe's original CC Spec


> Here are the MIDI CCs I've been working with lately.  Most important is Bank 1.  Let me know if you have any questions!

Bank 1

1 - Modulation  
11 - Expression  
74 - Filter  
75  

2 - Breath  
3  
9  
18  


Bank 2

22 SF Close  
23 SF Tree  
24 SF Amb  
25 SF Out  

77 OT Close  
78 OT AB  
79 OT Tree  
80 OT Srnd  


Bank 3

16 SF Speed  
17 SF Release  
18 SF Tightness  
21 SF Vibrato  
  
3 OT Vibrato  
24 OT Con Sord  
73 OT Attack  
72 OT Release  

