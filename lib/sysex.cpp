#include "sysex.h"

#include "config.h"
#include "flash_onboard.h"
#include "16next.h"
#include "tusb.h"

bool copySysexStreamToBuffer(uint8_t* syxBuffer, uint8_t* inputBuffer, uint8_t streamLength, uint8_t runningOffset) {
  bool sysexComplete = false;

  for(uint8_t i = 0; i < streamLength; i++) {
    syxBuffer[i+runningOffset] = inputBuffer[i];
    if(inputBuffer[i] == 0xF7) {
      sysexComplete = true;
      break;
    }
  }
  return sysexComplete;
}

void sendCurrentConfig() {
  // current Data length = memory + 3 bytes for firmware version + 1 byte for device ID
  uint8_t configDataLength = 4 + memoryMapLength;
  uint8_t currentConfigData[configDataLength];

  // read 80 bytes from internal flash
  uint8_t buf[80];
  readFlash(buf,80);

  // build a message from the version number...
  currentConfigData[0] = DEVICE_INDEX;
  currentConfigData[1] = FIRMWARE_VERSION_MAJOR;
  currentConfigData[2] = FIRMWARE_VERSION_MINOR;
  currentConfigData[3] = FIRMWARE_VERSION_POINT;

  // ... and the first 80 bytes of the external data
  for (uint8_t i = 0; i < memoryMapLength; i++) {
    currentConfigData[i+4] = buf[i];
  }

  // send as sysex; 0x0F == c0nFig
  sendByteArrayAsSysex(0x0F, currentConfigData, configDataLength);
}

void sendByteArrayAsSysex(uint8_t messageId, uint8_t *byteArray,
                          uint8_t byteArrayLength) {
  uint8_t outputMessageLength =
      1 + 3 + 1 + byteArrayLength + 1; // start/mfg/message/data/end
  uint8_t outputMessage[outputMessageLength];
  outputMessage[0] = 0xF0; // start Sysex
  outputMessage[1] = 0x7D; // MFG byte 1
  outputMessage[2] = 0x00; // MFG byte 2
  outputMessage[3] = 0x00; // MFG byte 3
  outputMessage[4] = messageId;
  for (uint8_t i = 0; i < byteArrayLength; i++) {
    uint8_t el = byteArray[i];
    outputMessage[i + 5] = el;
  }
  outputMessage[outputMessageLength - 1] = 0xF7; // end Sysex

  // how many chunks of 16 bytes is the message?
  uint8_t chunks = (outputMessageLength / 16) + 1;

  // for each chunk
  for (uint8_t chunk = 0; chunk < chunks; chunk++) {
    // offset within outputMessage
    uint8_t offset = chunk * 16;

    uint8_t chunkLength = 16;
    uint8_t tempBuf[16];
    if (chunk + 1 == chunks) {
      // we're in the final chunk so:
      chunkLength = outputMessageLength % 16;
    }
    for (uint8_t i = 0; i < chunkLength; i++) {
      tempBuf[i] = outputMessage[offset + i];
    }
    tud_midi_stream_write(0, tempBuf, chunkLength);
  }
}
