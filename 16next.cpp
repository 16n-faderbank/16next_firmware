/**
 * 16Next P1 firmware
 *
 * See README.md for pinout / circuit configuration
 * Tom Armitage [tom@infovore.org]
 */

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "bsp/board.h"
#include "tusb.h"

#include "16next.h"
#include "lib/ResponsiveAnalogRead.h"
#include "lib/spi_flash.h"

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_POINT 1 

#define FIRST_MUX_PIN 18
#define MUX_PIN_COUNT 4 
#define ADC_PIN 26
#define INTERNAL_LED_PIN 22

// 2 for now, we're being cheapskates
#define FADER_COUNT 2

// we're using SPI1, which is here:
// #define SPI1_SCK_PIN 10
// #define SPI1_TX_PIN 11
// #define SPI1_RX_PIN 12
// #define SPI1_CS_PIN 13

#define CONTROL_POLL_TIMEOUT 10 // ms

#define MIDI_BLINK_DURATION 5000 // us

uint8_t page_buf[FLASH_PAGE_SIZE];
const uint32_t target_addr = 0;
absolute_time_t updateControlsAt;
absolute_time_t midiActivityLightOffAt;
bool midiActivity = false;
bool midiBlinkEnabled = false;

bool shouldSendControlUpdate = false;
absolute_time_t sendForcedUpdateAt;

// bool controllerRotated = false;
// uint8_t enabledBanks = 3;
// uint8_t midiChannel = 0;
// uint8_t ccs[] = {1,  11, 74, 75, 2, 3,  9,  18, 22, 23, 24, 25, 77, 78, 79, 80,
                //  16, 17, 18, 21, 3, 24, 73, 72, 32, 33, 34, 35, 36, 37, 38, 39};

ControllerConfig controller; // struct to hold controller config

uint8_t sysexBuffer[128]; // 128 bytes to store incoming sysex in
bool isReadingSysex = false;
uint8_t sysexOffset = 0; // where in the buffer we start writing to.

#define MIDI_INPUT_BUFFER 64

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
// | 16-31   | 0-15   | Channel for each control (USB)     |
// | 32-47   | 0-15   | Channel for each control (TRS)     |
// | 48-63   | 0-127  | CC for each control (USB)          |
// | 64-79   | 0-127  | CC for each control (TRS)          |
uint8_t memoryMapLength = 80;
uint8_t defaultMemoryMap[] = {
  0,1,0,0,0,0,0,0, // 0-7
  0,0,0,0,0,0,0,0, // 8-15
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 16-31
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 32-47
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, // 48-63
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 // 64-79
};

int faderLookup[] = {7,6,5,4,3,2,1,0,8,9,10,11,12,13,14,15}; // this faders to Mux positions, ie,
                                  // fader 6 is on mux input 0,
                                  // fader 4 is on mux input 1
int previousValues[16];
int muxMask;

ResponsiveAnalogRead *analog[FADER_COUNT];  // array of filters to smooth analog read.

int main() {
  board_init();

  stdio_init_all();

  // Enable SPI 1 at 1 MHz and connect to GPIOs
  // spi_init(spi1, 1000 * 1000);
  // gpio_set_function(SPI_RX_PIN, GPIO_FUNC_SPI);
  // gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
  // gpio_set_function(SPI_TX_PIN, GPIO_FUNC_SPI);

  // Make the SPI pins available to picotool
  // bi_decl(bi_3pins_with_func(SPI_RX_PIN, SPI_TX_PIN, SPI_SCK_PIN, GPIO_FUNC_SPI));

  // Chip select is active-low, so we'll initialise it to a driven-high state
  // gpio_init(SPI_CS_PIN);
  // gpio_put(SPI_CS_PIN, 1);
  // gpio_set_dir(SPI_CS_PIN, GPIO_OUT);
  // Make the CS pin available to picotool
  // bi_decl(bi_1pin_with_name(SPI_CS_PIN, "SPI CS"));

  // setup default controller config, before we have memory working
  controller.midiLed = true;
  controller.powerLed = false;
  controller.rotated = false;
  controller.i2cFollower = false;
  controller.midiThru = false;
  for (uint8_t i = 0; i < FADER_COUNT; i++)
  {
    controller.usbMidiChannels[i] = 0;
  }
  for (uint8_t i = 0; i < FADER_COUNT; i++)
  {
    controller.usbCCs[i] = 32+i;
  }
  for (uint8_t i = 0; i < FADER_COUNT; i++)
  {
    controller.trsMidiChannel[i] = 0;
  }
  for (uint8_t i = 0; i < FADER_COUNT; i++)
  {
    controller.trsCCs[i] = 32+i;
  }
  
  // init ADC0 on GPIO26
  adc_init();
  adc_gpio_init(ADC_PIN);
  adc_select_input(0);

  // setup mux pins
  for (int i = 0; i < MUX_PIN_COUNT; i++) {
    muxMask |= 1 << i + FIRST_MUX_PIN;
  }
  gpio_init_mask(muxMask);
  gpio_set_dir_out_masked(muxMask);

  // setup internal led
  gpio_init(INTERNAL_LED_PIN);
  gpio_set_dir(INTERNAL_LED_PIN, GPIO_OUT);

  // setup analog read buckets
  for (int i = 0; i < FADER_COUNT; i++) {
    analog[i] = new ResponsiveAnalogRead(0, true, .0001);
    analog[i]->setActivityThreshold(32);
    analog[i]->enableEdgeSnap();
  }

  tusb_init();

  gpio_put(INTERNAL_LED_PIN, 0);

  // loadConfig(true); // write default config if byte 1 is 0xFF

  // end setup

  // begin infinite loop
  while (true) {
    // do the TinyUSB task and midi task - as fast as you can.
    tud_task();
    midi_read_task();

    gpio_put(INTERNAL_LED_PIN, midiBlinkEnabled && midiActivity);

    if(absolute_time_diff_us(midiActivityLightOffAt, get_absolute_time()) > 0) {
      midiActivity = false;
    }

    if(shouldSendControlUpdate && absolute_time_diff_us(sendForcedUpdateAt, get_absolute_time()) > 0) {
      // we've received a sysex "give me your config request" recently
      // so we should send the state of all controls whether they've changed
      // or not
      updateControls(true);
      shouldSendControlUpdate = false;
    }

    // if it's time to update
    int shouldUpdate =
        absolute_time_diff_us(get_absolute_time(), updateControlsAt) < 0;

    if (!shouldUpdate) {
      // skip to the next main loop
      continue;
    }

    updateControlsAt = make_timeout_time_ms(CONTROL_POLL_TIMEOUT);

    updateControls();
  }
  // end infinite loop
}

void midi_read_task() {
  uint8_t inputBuffer[MIDI_INPUT_BUFFER];
  uint8_t streamLength;
  while (tud_midi_available()) {
    streamLength = tud_midi_stream_read(inputBuffer, MIDI_INPUT_BUFFER);
    midiActivity = true;
    midiActivityLightOffAt = make_timeout_time_us(MIDI_BLINK_DURATION);
  }

  if(isReadingSysex) {
    // keep doing sysex stuff
    copySysexStreamToBuffer(inputBuffer, streamLength);
    return;
  }

  // BEGIN SYSEX HANDLER
  if (inputBuffer[0] == 0xF0 && inputBuffer[1] == 0x7D && inputBuffer[2] == 0x00 && inputBuffer[3] == 0x00) {
    // it's a sysex message and it's for us

    // start the process of reading it
    isReadingSysex = true;
    sysexOffset = 0;

    // blank the buffer;
    for(uint8_t i = 0; i < 128; i++) {
      sysexBuffer[i] = 0x00;
    }

    copySysexStreamToBuffer(inputBuffer, streamLength);
    return;
  }
  // END SYSEX HANDLER

  // null out inputbuffer
  // for (int i = 0; i < MIDI_INPUT_BUFFER; i++) {
  //   inputBuffer[i] = 0;
  // }
}

void copySysexStreamToBuffer(uint8_t* inputBuffer, uint8_t streamLength) {
  bool shouldProcess = false;
  for(uint8_t i = 0; i < streamLength; i++) {
    sysexBuffer[i+sysexOffset] = inputBuffer[i];
    if(inputBuffer[i] == 0xF7) {
      shouldProcess = true;
      break;
    }
  }
  if(shouldProcess) {
    // we saw an 0xF7, sysex is over, time to process
    processSysexBuffer();
  } else {
    // we still haven't seen the end of message, glue it on the end
    // and process the next 64-byte chunk
    sysexOffset += streamLength;
  }
}

void processSysexBuffer() {
  isReadingSysex = false;
  switch (sysexBuffer[4]) {
  case 0x1F:
    // 0x1F == tell me your 1nFo
    // sendCurrentConfig(); TODO
    break;
  case 0x0E:
    // 0x0E == c0nfig Edit
    // updateConfig(sysexBuffer, 128); TODO
    break;
  }
}

// void sendCurrentConfig() {
//   // current Data length = memory + 3 bytes for firmware version
//   uint8_t configDataLength = 3 + memoryMapLength;
//   uint8_t currentConfigData[configDataLength];

//   // read 256 bytes from external flash
//   flash_read(spi1, SPI_CS_PIN, target_addr, page_buf, FLASH_PAGE_SIZE);

//   // build a message from the version number...
//   currentConfigData[0] = FIRMWARE_VERSION_MAJOR;
//   currentConfigData[1] = FIRMWARE_VERSION_MINOR;
//   currentConfigData[2] = FIRMWARE_VERSION_POINT;

//   // ... and the first 48 bytes of the external data
//   for (uint8_t i = 0; i < memoryMapLength; i++) {
//     currentConfigData[i+3] = page_buf[i];
//   }

//   // send as sysex; 0x0F == c0nFig
//   sendByteArrayAsSysex(0x0F, currentConfigData, configDataLength);

//   // send the current state of the faders in 1s time.
//   shouldSendControlUpdate = true;
//   sendForcedUpdateAt = make_timeout_time_ms(1000);
// }

// void updateConfig(uint8_t *incomingSysex, uint8_t incomingSysexLength) {
//   // OK:
//   uint8_t newMemoryMap[memoryMapLength];

//   // 1) read the data that's just come in, and extract the 48 bytes of memory
//   // to a variable we offset by five to strip: SYSEX_START,MFG0,MFG1,MFG2,MSG
//   for (uint8_t i = 0; i < memoryMapLength; i++) {
//     newMemoryMap[i] = incomingSysex[i + 5];
//   }

//   // 2) store that into memory...
//   saveConfig(newMemoryMap);

//   // 3) and now read that memory, loading it as data
//   applyConfig(newMemoryMap);
// }

// void sendByteArrayAsSysex(uint8_t messageId, uint8_t *byteArray,
//                           uint8_t byteArrayLength) {
//   uint8_t outputMessageLength =
//       1 + 3 + 1 + byteArrayLength + 1; // start/mfg/message/data/end
//   uint8_t outputMessage[outputMessageLength];
//   outputMessage[0] = 0xF0; // start Sysex
//   outputMessage[1] = 0x7D; // MFG byte 1
//   outputMessage[2] = 0x00; // MFG byte 2
//   outputMessage[3] = 0x00; // MFG byte 3
//   outputMessage[4] = messageId;
//   for (uint8_t i = 0; i < byteArrayLength; i++) {
//     uint8_t el = byteArray[i];
//     outputMessage[i + 5] = el;
//   }
//   outputMessage[outputMessageLength - 1] = 0xF7; // end Sysex

//   // how many chunks of 16 bytes is the message?
//   uint8_t chunks = (outputMessageLength / 16) + 1;

//   // for each chunk
//   for (uint8_t chunk = 0; chunk < chunks; chunk++) {
//     // offset within outputMessage
//     uint8_t offset = chunk * 16;

//     if (chunk + 1 == chunks) {
//       // we're in the final chunk so:
//       uint8_t chunkLength = outputMessageLength % 16;
//       uint8_t tempBuf[chunkLength];
//       for (uint8_t i = 0; i < chunkLength; i++) {
//         tempBuf[i] = outputMessage[offset + i];
//       }
//       tud_midi_stream_write(0, tempBuf, chunkLength);
//     } else {
//       uint8_t tempBuf[16];
//       for (uint8_t i = 0; i < 16; i++) {
//         tempBuf[i] = outputMessage[offset + i];
//       }
//       tud_midi_stream_write(0, tempBuf, 16);
//     }
//   }
// }

void updateControls(bool force) {
  if(force) {
    // "force" only happens when connecting via sysex initially
    // ie, it's for the 'first load' of the editor. So we can lock up for 1ms.
    busy_wait_us(1000);
  }
  for (int i = 0; i < FADER_COUNT; i++) {
    // convert our number to binary, and turn it into a valid output mask
    // do this by working out a binary representation of channel ID
    uint controllerId = faderLookup[i] << FIRST_MUX_PIN;
    gpio_put_masked(muxMask, controllerId);

    busy_wait_us(10); // wait for mux pins to swap

    uint16_t rawAdcValue = adc_read();
    analog[i]->update(rawAdcValue);

    if (analog[i]->hasChanged() || force) {
      if(force) {
        // if we're being asked to update all our values, we _really_ would like a read, please.
        analog[i]->update(rawAdcValue);
      }

      // test the scaled version against the previous CC.
      uint8_t outputValue = analog[i]->getValue() >> 5;
      if ((outputValue != previousValues[i]) || force) {
        previousValues[i] = outputValue;
        uint8_t controllerIndex = i; // TODO is this right?
        if(controller.rotated) {
          controllerIndex = FADER_COUNT + 15 - i;
          outputValue = 127-outputValue;
        }

        // Send CC on channel 0 for appropriate channel
        uint8_t cc[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex]), controller.usbCCs[controllerIndex],
                         outputValue};
        uint8_t cable_num = 0;
        tud_midi_stream_write(cable_num, cc, 3);

        midiActivity = true;
        midiActivityLightOffAt = make_timeout_time_us(MIDI_BLINK_DURATION);
      }
    }
  }
}

// void loadConfig(bool setDefault) {
//   // read 256 bytes from external flash
//   flash_read(spi1, SPI_CS_PIN, target_addr, page_buf, FLASH_PAGE_SIZE);
//   // if the 2nd byte is unwritten, that means we should write the default
//   // settings to flash
//   if (setDefault && (page_buf[1] == 0xFF)) {
//     setDefaultConfig();
//     tripleBlink();
//     loadConfig(); // call yourself again, and default to read + apply
//   } else {
//     applyConfig(page_buf);
//   }
// }

// void applyConfig(uint8_t *conf) {
//   // take the config in a buffer and apply it to the device
//   // this means you could load from RAM or just go straight from sysex.
//   controller.currentBank = conf[0];
//   controller.enabledBanks = conf[1];
//   controller.rotated = conf[2];
//   controller.midiChannel = conf[3];

//   // load CCs from buffer
//   for (int i = 0; i < 32; i++) {
//     controller.ccs[i] = conf[i + 16];
//   }
// }

// void saveConfig(uint8_t *config) {
//   // erase the whole sector
//   flash_sector_erase(spi1, SPI_CS_PIN, target_addr);

//   // prepare the page buffer
//   for (int i = 0; i < FLASH_PAGE_SIZE; ++i) {
//     if (i < memoryMapLength) {
//       // set first configLength bytes to config
//       page_buf[i] = config[i];
//     } else {
//       // and set the rest to 255
//       page_buf[i] = 0xFF;
//     }
//   }

//   // write the page buffer
//   flash_page_program(spi1, SPI_CS_PIN, target_addr, page_buf);
// }

// void setDefaultConfig() { saveConfig(defaultMemoryMap); }
