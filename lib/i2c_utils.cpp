#include <pico/stdio.h>
#include <pico/stdlib.h>
#include "hardware/i2c.h"

#include "i2c_utils.h"

uint8_t device              = 0;
uint8_t port                = 0;

// master i2c specific stuff
const int ansibleI2Caddress = 0x20;
const int er301I2Caddress   = 0x31;
const int txoI2Caddress     = 0x60;
bool er301Present           = false;
bool ansiblePresent         = false;
bool txoPresent             = false;

// enumerate over all things on i2c bus. If they respond, set a flag
void scanI2Cbus() {
  int ret;
  uint8_t rxdata;

  for (uint8_t addr = 8; addr < 120; addr++) {
    ret = i2c_read_blocking(i2c1, addr, &rxdata, 1, false);

    if (ret >= 0) {
      if (addr == ansibleI2Caddress) {
        ansiblePresent = true;
      }

      if (addr == txoI2Caddress) {
        txoPresent = true;
      }

      if (addr == er301I2Caddress) {
        er301Present = true;
      }
      sleep_ms(1); // maybe unneeded?
    }
  }
}

void sendToAllI2C(uint8_t channel, uint16_t value) {
  // we send out to all three supported i2c slave devices
  // keeps the firmware simple :)

  // for 4 output devices
  port   = channel % 4;
  device = channel / 4;

  // TXo
  if (txoPresent) {
    sendi2c(txoI2Caddress, device, 0x11, port, value);
  }

  // ER-301
  if (er301Present) {
    sendi2c(er301I2Caddress, 0, 0x11, channel, value);
  }

  // ANSIBLE
  if (ansiblePresent) {
    sendi2c(ansibleI2Caddress, device << 1, 0x06, port, value);
  }
}

/*
 * Sends an i2c command out to a slave when running in master mode
 */

void sendi2c(uint8_t model, uint8_t deviceIndex, uint8_t cmd, uint8_t devicePort, int value) {

  uint8_t messageBuffer[4];
  uint16_t valueTemp;

  messageBuffer[0] = cmd;
  messageBuffer[1] = (uint8_t)devicePort;

  valueTemp        = (uint16_t)value;
  messageBuffer[2] = valueTemp >> 8;
  messageBuffer[3] = valueTemp & 0xff;

  i2c_write_blocking(i2c1, model + deviceIndex, messageBuffer, 4, false);
}
