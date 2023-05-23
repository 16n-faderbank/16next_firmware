# JT8 Sysex spec

The JT8 interfaces with its editor via MIDI Sysex. This document describes the supported messages.

## `0x1F` - "1nFo"

Request for JT8 to transmit current state via sysex. No other payload.

Only ever sent from browser to JT8.

## `0x0F` - "c0nFig"

"Here is my current config." Payload of 51 bytes - 3 bytes of version number and 48 bytes of memory data.

Only sent by JT8 as an outbound message, in response to `0x1F`. 

## `0x0B` - "deBug"

Reserved to send debug data from JT8 to browser; payload length variable.

## `0x0C` - "pr0gram Change"

"Set the current bank to X"; only over sent from JT8 to browser. This exists to instruct the _editor_ to change current bank, as an alternative to native Prog Change messages - which might interfere with music software.

1 byte payload.

Only ever sent by JT8.

## `0x0E` - "c0nfig Edit"

"Here is a new complete configuration for you". Payload (other than mfg header, top/tail, etc) of 48 bytes to go straight into EEPROM, according to the memory map described in `README.md`.

Only ever sent from browser to JT8.

## `0x1A` - "1nitiAlize memory"

"Wipe the EEPROM and force factory settings". Unlikely to ever be needed. The use case is "emptying" the EEPROM of a Teensy that's previously been used for other projects, and thus has an inaccurate configuration on it.

Only ever sent from browser to JT8.
