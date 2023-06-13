#include "config.h"
#include "flash_onboard.h"

void updateConfig(uint8_t *incomingSysex, uint8_t incomingSysexLength, ControllerConfig cConfig) {
  // OK:
  uint8_t newMemoryMap[memoryMapLength];

  // 1) read the data that's just come in, and extract the 80 bytes of memory
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

void loadConfig(ControllerConfig cConfig, bool setDefault) {
  // read 80 bytes from internal flash
  uint8_t buf[memoryMapLength];
  readFlash(buf,memoryMapLength);
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
void applyConfig(uint8_t *conf, ControllerConfig cConfig) {
  // take the config in a buffer and apply it to the device
  // this means you could load from RAM or just go straight from sysex.

  cConfig.powerLed = conf[0];
  cConfig.midiLed = conf[1];
  cConfig.rotated = conf[2];
  cConfig.i2cLeader = conf[3];
  cConfig.midiThru = conf[8];

  for (uint8_t i = 0; i < 16; i++)
  {
    cConfig.usbMidiChannels[i] = conf[16+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    cConfig.trsMidiChannels[i] = conf[32+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    cConfig.usbCCs[i] = conf[48+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    cConfig.trsCCs[i] = conf[64+i];
  }
}

void saveConfig(uint8_t * config) {
  writeFlash(config,memoryMapLength);
}

void setDefaultConfig() {
  eraseFlashSector();
  saveConfig(defaultMemoryMap);
}
