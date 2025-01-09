#pragma once

#include <pico/stdio.h>
#include <pico/stdlib.h>

/*
 * Data structure containing all elements of controller config.
 */
struct ControllerConfig {
  bool powerLed;
  bool midiLed;
  bool rotated;
  bool i2cLeader;
  uint32_t faderMin;
  uint32_t faderMax;
  bool midiThru;
  uint8_t usbMidiChannels[16];
  uint8_t usbCCs[16];
  uint8_t trsMidiChannels[16];
  uint8_t trsCCs[16];
  bool usbHighResolution[16];
  bool trsHighResolution[16];
};

extern const uint8_t memoryMapLength;
extern uint8_t defaultMemoryMap[];

void updateConfig(uint8_t *incomingSysex, uint8_t incomingSysexLength, ControllerConfig *cConfig);
void loadConfig(ControllerConfig *cConfig, bool setDefault = false);
void applyConfig(uint8_t *config, ControllerConfig *cConfig);
void saveConfig(uint8_t *config);
void setDefaultConfig();
