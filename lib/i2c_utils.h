#pragma once

#include <pico/stdio.h>
#include <pico/stdlib.h>

void scanI2Cbus();
void sendToAllI2C(uint8_t channel, uint16_t value);
void sendi2c(uint8_t model, uint8_t deviceIndex, uint8_t cmd, uint8_t devicePort, int value);
