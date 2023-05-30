#include "local_flash.h"

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
