#include "local_flash.h"

/*
 * use last PAGE of program flash as a virtual EEPROM.
 * this incorporates simple wear levelling:
 *    - write will look for a page, and if the first byte isn't 0xFF,
 *      it'll iterate through pages til it finds an empty one.
*/

void writeFlash(uint32_t eepromLocation, uint8_t *buf, uint16_t bufferSize) {
   // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;
  int addr, *p;
  int first_empty_page = -1;
  for(unsigned int page = 0; page < FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; page++){
    addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);
    p = (int *)addr;
    // Serial.print("First four bytes of page " + String(page, DEC) );
    // Serial.print("( at 0x" + (String(int(p), HEX)) + ") = ");
    // Serial.println(*p);
    if( *p == -1 && first_empty_page < 0){
      first_empty_page = page;
      // Serial.println("First empty page is #" + String(first_empty_page, DEC));
    }
  }

  if (first_empty_page < 0){
    // Serial.println("Full sector, erasing...");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    first_empty_page = 0;
    restore_interrupts (ints);
  }
  // Serial.println("Writing to page #" + String(first_empty_page, DEC));
  uint32_t ints = save_and_disable_interrupts();
  flash_range_program(FLASH_TARGET_OFFSET + (first_empty_page*FLASH_PAGE_SIZE), (uint8_t *)buf, FLASH_PAGE_SIZE);
  restore_interrupts (ints);
}

void readFlash(uint32_t eepromLocation, uint8_t *buf, uint16_t bufferSize) {
  // TODO:
  // starting at the endpoint, read each page of data
  // if the page is empty, the data is in page-1

  // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;
  int addr, *p;
  int last_data_page = -1;
  for(unsigned int page = 0; page < FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; page++){
    addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);
    p = (int *)addr;
    // Serial.print("First four bytes of page " + String(page, DEC) );
    // Serial.print("( at 0x" + (String(int(p), HEX)) + ") = ");
    // Serial.println(*p);
    if( *p == -1 && last_data_page < 0){
      last_data_page = page-1;
      // Serial.println("First empty page is #" + String(first_empty_page, DEC));
    }
  }

  // otherwise it's the last page
  if (last_data_page < 0){
    last_data_page = (FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE)-1;
  }

  addr = XIP_BASE + FLASH_TARGET_OFFSET + (last_data_page * FLASH_PAGE_SIZE);

  uint8_t *readPointer;

  // put that 80 bytes of that data into buf
  for(uint8_t i = 0; i < bufferSize; i++) {
    readPointer = (uint8_t *) addr+i; // Place an int pointer at our memory-mapped address
    buf[i] = *readPointer;
  }
}
