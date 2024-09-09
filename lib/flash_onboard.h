#pragma once

#include <hardware/flash.h>
#include <hardware/sync.h>

#include <pico/stdio.h>
#include <pico/stdlib.h>

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

int firstEmptyPage();
void eraseFlashSector();
void writeFlash(uint8_t *buf, uint16_t bufferSize);
void readFlash(uint8_t *buf, uint16_t bufferSize);
