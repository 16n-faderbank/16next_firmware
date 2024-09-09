#pragma once

#include <pico/stdio.h>
#include <pico/stdlib.h>

// void processSysexBuffer();
// void copySysexStreamToBuffer(uint8_t* inputBuffer, uint8_t streamLength);
void sendByteArrayAsSysex(uint8_t messageId, uint8_t *byteArray, uint8_t byteArrayLength);
void sendCurrentConfig();
bool copySysexStreamToBuffer(uint8_t *syxBuffer, uint8_t *inputBuffer, uint8_t streamLength, uint8_t runningOffset);
