#include "config.h"
#include "flash_onboard.h"

const uint8_t memoryMapLength = 86;

// default memorymap
// | Address | Format |            Description             |
// |---------|--------|------------------------------------|
// | 0       | 0/1    | LED on when powered                |
// | 1       | 0/1    | LED blink on MIDI data             |
// | 2       | 0/1    | Rotate controller outputs via 180º |
// | 3       | 0/1    | I2C Follower/Leader                |
// | 4,5     | 0-127  | FADERMIN lsb/msb                   |
// | 6,7     | 0-127  | FADERMAX lsb/msb                   |
// | 8       | 0/1    | Soft MIDI thru (default 0)         |
// | 9-15    |        | Currently unused                   |
// | 16-31   | 1-16   | Channel for each control (USB)     |
// | 32-47   | 1-16   | Channel for each control (TRS)     |
// | 48-63   | 0-127  | CC for each control (USB)          |
// | 64-79   | 0-127  | CC for each control (TRS)          |
// | 80-82   | 0-127  | Booleans for high-res mode (USB)   |
// | 83-85   | 0-127  | Booleans for high-res mode (TRS)   |
uint8_t defaultMemoryMap[]    = {
    0, 1, 0, 0, 0, 0, 0, 0,                                         // 0-7
    0, 0, 0, 0, 0, 0, 0, 0,                                         // 8-15
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                 // 16-31
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                 // 32-47
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, // 48-63
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, // 64-79
    0, 0, 0,                                                        // 80-82
    0, 0, 0                                                         // 83-85
};

void updateConfig(uint8_t *incomingSysex, uint8_t incomingSysexLength, ControllerConfig *cConfig) {
  // OK:
  uint8_t newMemoryMap[memoryMapLength];

  // 1) read the data that's just come in, and extract the 86 bytes of memory
  // to a variable we offset by five to strip: SYSEX_START,MFG0,MFG1,MFG2,MSG
  // and then also to strip
  // * device ID
  // * firmware MAJ/Min/POINT
  for (uint8_t i = 0; i < memoryMapLength; i++) {
    newMemoryMap[i] = incomingSysex[i + 9];
  }

  // 2) store that into memory...
  saveConfig(newMemoryMap);

  // 3) and now read that memory, loading it as data
  applyConfig(newMemoryMap, cConfig);
}

void loadConfig(ControllerConfig *cConfig, bool setDefault) {
  // read 86 bytes from internal flash
  uint8_t buf[memoryMapLength];
  readFlash(buf, memoryMapLength);
  // if the 2nd byte is unwritten, that means we should write the default
  // settings to flash
  if (setDefault && (buf[1] == 0xFF)) {
    setDefaultConfig();
    sleep_us(500);
    loadConfig(cConfig); // call yourself again, and default to read + apply
  } else {
    applyConfig(buf, cConfig);
  }
}
void applyConfig(uint8_t *conf, ControllerConfig *cConfig) {
  // take the config in a buffer and apply it to the device
  // this means you could load from RAM or just go straight from sysex.

  cConfig->powerLed  = conf[0];
  cConfig->midiLed   = conf[1];
  cConfig->rotated   = conf[2];
  cConfig->i2cLeader = conf[3];
  cConfig->midiThru  = conf[8];

  for (uint8_t i = 0; i < 16; i++) {
    cConfig->usbMidiChannels[i] = conf[16 + i];
  }
  for (uint8_t i = 0; i < 16; i++) {
    cConfig->trsMidiChannels[i] = conf[32 + i];
  }
  for (uint8_t i = 0; i < 16; i++) {
    cConfig->usbCCs[i] = conf[48 + i];
  }
  for (uint8_t i = 0; i < 16; i++) {
    cConfig->trsCCs[i] = conf[64 + i];
  }

  // extract and configure high-resolution data
  uint16_t usbHighResValue = (conf[80] & 0x7F) |
                             ((conf[81] & 0x7F) << 7) |
                             ((conf[82] & 0x03) << 14);

  uint16_t trsHighResValue = (conf[83] & 0x7F) |
                             ((conf[84] & 0x7F) << 7) |
                             ((conf[85] & 0x03) << 14);

  for (uint8_t i = 0; i < 16; i++) {
    cConfig->usbHighResolution[i] = (usbHighResValue & (1 << i)) != 0;
    cConfig->trsHighResolution[i] = (trsHighResValue & (1 << i)) != 0;
  }
}

void saveConfig(uint8_t *config) {
  writeFlash(config, memoryMapLength);
}

void setDefaultConfig() {
  eraseFlashSector();
  saveConfig(defaultMemoryMap);
}
