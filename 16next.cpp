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
#include "midi_uart_lib.h"
#include "tusb.h"

#include "16next.h"
#include "lib/ResponsiveAnalogRead.h"
#include "lib/eeprom.h"

#define FIRMWARE_VERSION_MAJOR 3
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_POINT 0 

#define FIRST_MUX_PIN 18
#define MUX_PIN_COUNT 4 
#define ADC_PIN 26
#define INTERNAL_LED_PIN 22

// UART selection Pin mapping. You can move these for your design if you want to
// Make sure all these values are consistent with your choice of midi_uart
// The default is to use UART 1, but you are free to use UART 0 if you make
// the changes in the CMakeLists.txt file or in your environment. Note
// that if you use UART0, then serial port debug will not be enabled
#ifndef MIDI_UART_NUM
#define MIDI_UART_NUM 1
#endif
#ifndef MIDI_UART_TX_GPIO
#define MIDI_UART_TX_GPIO 4
#endif
#ifndef MIDI_UART_RX_GPIO
#define MIDI_UART_RX_GPIO 5
#endif

// 2 for now, we're being cheapskates
#define FADER_COUNT 2

EEPROM eeprom;

#define CONTROL_POLL_TIMEOUT 10 // ms

#define MIDI_BLINK_DURATION 5000 // us

const uint32_t target_addr = 0;
absolute_time_t updateControlsAt;
absolute_time_t midiActivityLightOffAt;
bool midiActivity = false;

bool shouldSendControlUpdate = false;
absolute_time_t sendForcedUpdateAt;

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
// | 16-31   | 1-16   | Channel for each control (USB)     |
// | 32-47   | 1-16   | Channel for each control (TRS)     |
// | 48-63   | 0-127  | CC for each control (USB)          |
// | 64-79   | 0-127  | CC for each control (TRS)          |
uint8_t memoryMapLength = 80;
uint8_t defaultMemoryMap[] = {
  0,1,0,0,0,0,0,0, // 0-7
  0,0,0,0,0,0,0,0, // 8-15
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 16-31
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 32-47
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, // 48-63
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 // 64-79
};

int faderLookup[] = {7,6,5,4,3,2,1,0,8,9,10,11,12,13,14,15}; // this faders to Mux positions, ie,
                                  // fader 6 is on mux input 0,
                                  // fader 4 is on mux input 1
int previousValues[16];
int muxMask;

ResponsiveAnalogRead *analog[FADER_COUNT];  // array of filters to smooth analog read.

static void *midi_uart_instance;

int main() {
  board_init();

  stdio_init_all();

  bi_decl(bi_1pin_with_name(INTERNAL_LED_PIN, "On-board LED"));
  bi_decl(bi_2pins_with_names(MIDI_UART_TX_GPIO, "MIDI UART TX", MIDI_UART_RX_GPIO, "MIDI UART RX"));


  // setup EEPROM on I2C0, on GPIO 11/12
  // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
  i2c_init(i2c0, 100 * 1000);
  gpio_set_function(8, GPIO_FUNC_I2C);
  gpio_set_function(9, GPIO_FUNC_I2C);
  gpio_pull_up(8);
  gpio_pull_up(9);
  // Make the I2C pins available to picotool
  bi_decl(bi_2pins_with_func(8, 9, GPIO_FUNC_I2C));

  eeprom.begin();

  loadConfig(true); // load config from eeprom; write default config TO eeprom if byte 1 is 0xFF

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

  // setup TRS MIDI
  midi_uart_instance = midi_uart_configure(MIDI_UART_NUM, MIDI_UART_TX_GPIO, MIDI_UART_RX_GPIO);


  // setup analog read buckets
  for (int i = 0; i < FADER_COUNT; i++) {
    analog[i] = new ResponsiveAnalogRead(0, true, .0001);
    analog[i]->setActivityThreshold(32);
    analog[i]->enableEdgeSnap();
  }

  tusb_init();

  gpio_put(INTERNAL_LED_PIN, 0);


  // end setup

  // begin infinite loop
  while (true) {
    // do the TinyUSB task and midi task - as fast as you can.
    tud_task();
    midi_read_task();

    if(controller.powerLed)   {
      gpio_put(INTERNAL_LED_PIN, true);
    } else {
      gpio_put(INTERNAL_LED_PIN, controller.midiLed && midiActivity);
    }

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
    sendCurrentConfig();
    break;
  case 0x0E:
    // 0x0E == c0nfig Edit
    updateConfig(sysexBuffer, 128);
    break;
  case 0x1A:
    // 0x1A == initi1Alize to factory defaults
    setDefaultConfig();
    loadConfig();
    break;
  }
}

void sendCurrentConfig() {
  // current Data length = memory + 3 bytes for firmware version + 1 byte for device ID
  uint8_t configDataLength = 4 + memoryMapLength;
  uint8_t currentConfigData[configDataLength];

  // read 80 bytes from external eeprom
  uint8_t buf[80];
  eeprom.read(0,buf,80);

  // build a message from the version number...
  currentConfigData[0] = 0x04; // 0x04 == 16next device id
  currentConfigData[1] = FIRMWARE_VERSION_MAJOR;
  currentConfigData[2] = FIRMWARE_VERSION_MINOR;
  currentConfigData[3] = FIRMWARE_VERSION_POINT;

  // ... and the first 80 bytes of the external data
  for (uint8_t i = 0; i < memoryMapLength; i++) {
    // TODO: remove default Memory Map, read from memory
    currentConfigData[i+4] = buf[i];
  }

  // send as sysex; 0x0F == c0nFig
  sendByteArrayAsSysex(0x0F, currentConfigData, configDataLength);

  // send the current state of the faders in 1s time.
  shouldSendControlUpdate = true;
  sendForcedUpdateAt = make_timeout_time_ms(100);
}

void updateConfig(uint8_t *incomingSysex, uint8_t incomingSysexLength) {
  // OK:
  uint8_t newMemoryMap[memoryMapLength];

  // 1) read the data that's just come in, and extract the 80 bytes of memory
  // to a variable we offset by five to strip: SYSEX_START,MFG0,MFG1,MFG2,MSG
  // and then also to strip
  // * device ID
  // * firmware MAJ/Min/POINT
  for (uint8_t i = 0; i < memoryMapLength; i++) {
    newMemoryMap[i] = incomingSysex[i + 9];
  }

  // 2) store that into memory...
  saveConfig(newMemoryMap);

  // 3) and now read that memory, loading it as data
  applyConfig(newMemoryMap);
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

        // Send CC on appropriate USB channel
        uint8_t cc[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex]-1), controller.usbCCs[controllerIndex],
                         outputValue};
        uint8_t cable_num = 0;
        tud_midi_stream_write(cable_num, cc, 3);

        // Send CC on appropriate TRS channel
        uint8_t trs_cc[3] = {(uint8_t)(0xB0 | controller.trsMidiChannels[controllerIndex]-1), controller.trsCCs[controllerIndex], outputValue};
        // tud_midi_stream_write(cable_num, cc, 3);
        midi_uart_write_tx_buffer(midi_uart_instance,trs_cc,3);

        midiActivity = true;
        midiActivityLightOffAt = make_timeout_time_us(MIDI_BLINK_DURATION);
      }
    }
  }
}

void loadConfig(bool setDefault) {
  // read 80 bytes from EEPROM
  uint8_t buf[memoryMapLength];
  eeprom.read(0,buf,memoryMapLength);
  // if the 2nd byte is unwritten, that means we should write the default
  // settings to flash
  if (setDefault && (buf[1] == 0xFF)) {
    setDefaultConfig();
    sleep_us(500);
    loadConfig(); // call yourself again, and default to read + apply
  } else {
    applyConfig(buf);
  }
}

void applyConfig(uint8_t *conf) {
  // take the config in a buffer and apply it to the device
  // this means you could load from RAM or just go straight from sysex.

  controller.powerLed = conf[0];
  controller.midiLed = conf[1];
  controller.rotated = conf[2];
  controller.i2cFollower = conf[3];
  controller.midiThru = conf[8];

  for (uint8_t i = 0; i < 16; i++)
  {
    controller.usbMidiChannels[i] = conf[16+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    controller.trsMidiChannels[i] = conf[32+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    controller.usbCCs[i] = conf[48+i];
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    controller.trsCCs[i] = conf[64+i];
  }
}

void saveConfig(uint8_t *config) {
  eeprom.write(0,config,memoryMapLength);
}

void setDefaultConfig() {
  saveConfig(defaultMemoryMap);
}

