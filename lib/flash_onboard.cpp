#include "flash_onboard.h"

const uint8_t memoryMapLength = 80;

// default memorymap
// | Address | Format |            Description             |
// |---------|--------|------------------------------------|
// | 0       | 0/1    | LED on when powered                |
// | 1       | 0/1    | LED blink on MIDI data             |
// | 2       | 0/1    | Rotate controller outputs via 180º |
// | 3       | 0/1    | I2C Master/Follower                |
// | 4,5     | 0-127  | FADERMIN lsb/msb                   |
// | 6,7     | 0-127  | FADERMAX lsb/msb                   |
// | 8       | 0/1    | Soft MIDI thru (default 0)         |
// | 9-15    |        | Currently unused                   |
// | 16-31   | 1-16   | Channel for each control (USB)     |
// | 32-47   | 1-16   | Channel for each control (TRS)     |
// | 48-63   | 0-127  | CC for each control (USB)          |
// | 64-79   | 0-127  | CC for each control (TRS)          |
uint8_t defaultMemoryMap[] = {
  0,1,0,1,0,0,0,0, // 0-7
  0,0,0,0,0,0,0,0, // 8-15
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 16-31
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 32-47
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, // 48-63
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 // 64-79
};


int firstEmptyPage() {
  int addr, *p;
  int first_empty_page = -1;
  for (int page = 0; page < FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE; page++) {
    addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);

    p = (int *)addr;
    // printf("First four bytes of page %d", page);
    // printf("( at 0x%02X) = ", p);
    // printf("%08X\n", *p);
    if (*p == -1 && first_empty_page < 0) {
      first_empty_page = page;
      // printf("First empty page is %d\n", first_empty_page);
    }
  }

  return first_empty_page;
}

void readFlash(uint8_t *buf, uint16_t bufferSize) {
  // starting at the endpoint, read each page of data
  // if the page is empty, the data is in page-1

  uint32_t page = firstEmptyPage() -1;
  if(page < 0) {
    page = 0;
  }
  // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;
  int addr;

  addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);

  uint8_t *readPointer;

  // put that bufferSize bytes of that data into buf
  for(uint8_t i = 0; i < bufferSize; i++) {
    readPointer = (uint8_t *) addr+i; // Place an int pointer at our memory-mapped address
    buf[i] = *readPointer;
  }
}

void writeFlash(uint8_t *buf, uint16_t bufferSize) {
  uint32_t page = firstEmptyPage();
   // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;

  uint8_t page_buf[FLASH_PAGE_SIZE];
  for (int i = 0; i < FLASH_PAGE_SIZE; ++i) {
    if (i < bufferSize) {
      // set first configLength bytes to config
      page_buf[i] = buf[i];
    } else {
      // and set the rest to 255
      page_buf[i] = 0xFF;
    }
  }

  int addr, *p;

  if (page < 0){
    // Serial.println("Full sector, erasing...");
    eraseFlashSector();
    page = 0;
  }
  // Serial.println("Writing to page #" + String(first_empty_page, DEC));
  uint32_t ints = save_and_disable_interrupts();
  flash_range_program(FLASH_TARGET_OFFSET + (page*FLASH_PAGE_SIZE), page_buf, FLASH_PAGE_SIZE);
  restore_interrupts (ints);
}

void eraseFlashSector() {
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
  restore_interrupts (ints);
}
