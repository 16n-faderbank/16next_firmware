/*
 * 16next Header file
 *
*/

/*
 * Data structure containing all elements of controller config.
*/
struct ControllerConfig {
  bool powerLed;
  bool midiLed;
  bool rotated;
  bool i2cFollower;
  uint32_t faderMin;
  uint32_t faderMax;
  bool midiThru;
  uint8_t usbMidiChannels[16];
  uint8_t usbCCs[16];
  uint8_t trsMidiChannel[16];
  uint8_t trsCCs[16];
};

/*
 * Functions appearing in 16next.cpp
*/
void midi_read_task();
void processSysexBuffer();
void copySysexStreamToBuffer(uint8_t* inputBuffer, uint8_t streamLength);
void updateControls(bool force=false);
void loadConfig(bool setDefault=false);
void applyConfig(uint8_t* config);
void saveConfig(uint8_t* config);
void setDefaultConfig();
void sendByteArrayAsSysex(uint8_t messageId, uint8_t* byteArray, uint8_t byteArrayLength);
void sendCurrentConfig();
void updateConfig(uint8_t* incomingSysex, uint8_t incomingSysexLength);
