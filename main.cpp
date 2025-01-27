/**
 * 16Next firmware
 *
 * See README.md for pinout / circuit configuration
 * Tom Armitage [tom@infovore.org]
 */

#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/i2c_slave.h"
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "midi_uart_lib.h"
#include "tusb.h"

#include "lib/ResponsiveAnalogRead.hpp"
#include "lib/config.h"
#include "lib/flash_onboard.h"
#include "lib/i2c_utils.h"
#include "lib/sysex.h"
#include "main.h"

absolute_time_t updateControlsAt;
absolute_time_t midiActivityLightOffAt;
bool midiActivity            = false;

bool shouldSendControlUpdate = false;
absolute_time_t sendForcedUpdateAt;

ControllerConfig controller; // struct to hold controller config

uint8_t sysexBuffer[128]; // 128 bytes to store incoming sysex in
bool isReadingSysex     = false;
uint8_t sysexOffset     = 0; // where in the buffer we start writing to.

// this maps faders to Mux positions, ie,
// fader 6 is on mux input 0,
// fader 4 is on mux input 1
const int faderLookup[] = {7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15};

uint16_t previousValues[16];
int i2cData[16];
int muxMask;

ResponsiveAnalogRead *analog[FADER_COUNT]; // array of filters to smooth analog read.

static void *midi_uart_instance;

// active input for I2C
int activeInput = 0;

int main() {
  board_init();

  stdio_init_all();

  bi_decl(bi_1pin_with_name(INTERNAL_LED_PIN, "On-board LED"));
  bi_decl(bi_2pins_with_names(MIDI_UART_TX_GPIO, "MIDI UART TX", MIDI_UART_RX_GPIO, "MIDI UART RX"));
  // for (uint8_t i = 0; i < MUX_PIN_COUNT; i++) {
  // bi_decl(bi_1pin_with_name(FIRST_MUX_PIN + i, "Mux Pin"));
  // }
  bi_decl(bi_4pins_with_names(FIRST_MUX_PIN, "Mux Address Pin 0", FIRST_MUX_PIN + 1, "Mux Address Pin 1", FIRST_MUX_PIN + 2, "Mux Address Pin 2", FIRST_MUX_PIN + 3, "Mux Address Pin 3"));

  loadConfig(&controller, true); // load config from flash; write default config TO flash if byte 1 is 0xFF

  if (controller.i2cLeader) {
    sleep_ms(BOOTDELAY);
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

  // setup TRS MIDI
  midi_uart_instance = midi_uart_configure(MIDI_UART_NUM, MIDI_UART_TX_GPIO, MIDI_UART_RX_GPIO);

  // setup analog read buckets
  for (int i = 0; i < FADER_COUNT; i++) {
    analog[i] = new ResponsiveAnalogRead(0, true, .05);
    analog[i]->setActivityThreshold(16);
    // analog[i]->enableEdgeSnap();
  }

  // set up I2C on jack
  // GPIO 10 = I2C1 SDA
  // GPIO 11 = I2C1 SCL
  gpio_init(I2C_SDA_PIN);
  gpio_init(I2C_SCL_PIN);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);
  // Make the I2C pins available to picotool
  bi_decl(bi_2pins_with_func(I2C_SDA_PIN, I2C_SCL_PIN, GPIO_FUNC_I2C));

  if (controller.i2cLeader) {
    i2c_init(i2c1, I2C_BAUDRATE);
    scanI2Cbus();
  } else {
    i2c_init(i2c1, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(i2c1, I2C_ADDRESS, &i2c_slave_handler);
  }

  // init TinyUSB
  tusb_init();

  // make sure LED is off.
  gpio_put(INTERNAL_LED_PIN, 0);
  // end setup

  // begin infinite loop
  while (true) {
    // do the TinyUSB task and midi task - as fast as you can.
    tud_task();
    midi_read_task();

    if (controller.powerLed) {
      gpio_put(INTERNAL_LED_PIN, true);
    } else {
      gpio_put(INTERNAL_LED_PIN, controller.midiLed && midiActivity);
    }

    if (absolute_time_diff_us(midiActivityLightOffAt, get_absolute_time()) > 0) {
      midiActivity = false;
    }

    if (shouldSendControlUpdate && absolute_time_diff_us(sendForcedUpdateAt, get_absolute_time()) > 0) {
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

    // drain the TX buffer to the TRS midi out - if you don't include this,
    // no data will ever get sent to the MIDI out.
    midi_uart_drain_tx_buffer(midi_uart_instance);
  }
  // end infinite loop
}

void midi_read_task() {
  uint8_t inputBuffer[MIDI_INPUT_BUFFER];
  uint8_t streamLength;
  while (tud_midi_available()) {
    streamLength = tud_midi_stream_read(inputBuffer, MIDI_INPUT_BUFFER);
    // if it's not clock...
    if (inputBuffer[0] != 0xF8) {
      midiActivity           = true;
      midiActivityLightOffAt = make_timeout_time_us(MIDI_BLINK_DURATION);
    }
  }

  if (isReadingSysex) {
    // keep doing sysex stuff
    bool sysexComplete = copySysexStreamToBuffer(sysexBuffer, inputBuffer, streamLength, sysexOffset);

    if (sysexComplete) {
      // we saw an 0xF7, sysex is over, time to process
      processSysexBuffer();
    } else {
      // we still haven't seen the end of message, glue it on the end
      // and process the next 64-byte chunk
      sysexOffset += streamLength;
    }
    return;
  }

  // BEGIN SYSEX HANDLER
  if (inputBuffer[0] == 0xF0 && inputBuffer[1] == 0x7D && inputBuffer[2] == 0x00 && inputBuffer[3] == 0x00) {
    // it's a sysex message and it's for us!

    // start the process of reading it
    isReadingSysex = true;
    sysexOffset    = 0;

    // blank the buffer;
    for (uint8_t i = 0; i < 128; i++) {
      sysexBuffer[i] = 0x00;
    }

    bool sysexComplete = copySysexStreamToBuffer(sysexBuffer, inputBuffer, streamLength, sysexOffset);

    if (sysexComplete) {
      // we saw an 0xF7, sysex is over, time to process
      processSysexBuffer();
    } else {
      // we still haven't seen the end of message, glue it on the end
      // and process the next 64-byte chunk
      sysexOffset += streamLength;
    }
    return;
  }
  // END SYSEX HANDLER

  // if it's not sysex, forward it thru to midi TRS if relevant.
  if (controller.midiThru) {
    midi_uart_write_tx_buffer(midi_uart_instance, inputBuffer, streamLength);
  }
}

void processSysexBuffer() {
  isReadingSysex = false;

  switch (sysexBuffer[4]) {
  case 0x1F:
    // 0x1F == tell me your 1nFo
    sendCurrentConfig();
    shouldSendControlUpdate = true;
    sendForcedUpdateAt      = make_timeout_time_ms(100);
    break;
  case 0x0E:
    // 0x0E == c0nfig Edit
    updateConfig(sysexBuffer, 128, &controller);
    break;
  case 0x1A:
    // 0x1A == initi1Alize to factory defaults
    setDefaultConfig();
    loadConfig(&controller);
    break;
  }
}

void updateControls(bool force) {
  if (force) {
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
#ifdef INVERT_ADC
    rawAdcValue = (1 << ADC_RESOLUTION) - 1 - rawAdcValue;
#endif
    analog[i]->update(rawAdcValue);

    uint8_t controllerIndex = i;

    if (controller.rotated) {
      controllerIndex = FADER_COUNT - 1 - i;
    }

    if (analog[i]->hasChanged() || force) {
      if (force) {
        // if we're being asked to update all our values, we _really_ would like a read, please.
        analog[i]->update(rawAdcValue);
      }

      // store the current value of the fader in this block
      // for i2c purposes
      // i2c resolution is 14-bit on 16n.
      // 16nx has a 12-bit max ADC, but we want compatibility with other
      // scripts. And so bit shift by 2 to scale up to 14-bit data.:
      i2cData[i] = analog[i]->getValue() << 2;
      if (controller.rotated) {
        i2cData[i] = ((1 << 14) - 1) - i2cData[i];
      }

      // test the scaled version against the previous CC.
      uint16_t usbOutputValue;
      uint16_t trsOutputValue;
      bool usbHighResolution = controller.rotated ? controller.usbHighResolution[controllerIndex] : controller.usbHighResolution[controllerIndex];
      bool trsHighResolution = controller.rotated ? controller.trsHighResolution[controllerIndex] : controller.trsHighResolution[controllerIndex];

      uint8_t usbOutputBits  = usbHighResolution ? 14 : 7;
      uint8_t trsOutputBits  = trsHighResolution ? 14 : 7;

      usbOutputValue         = usbHighResolution ? analog[i]->getValue() << 2 : analog[i]->getValue() >> 5;
      trsOutputValue         = trsHighResolution ? analog[i]->getValue() << 2 : analog[i]->getValue() >> 5;

      if ((usbOutputValue != previousValues[i]) || force) {
        previousValues[i] = usbOutputValue; // yes, I know USB is driving things.

        if (controller.rotated) {
          usbOutputValue = ((1 << usbOutputBits) - 1) - usbOutputValue;
          trsOutputValue = ((1 << trsOutputBits) - 1) - trsOutputValue;
        }

        // Send CC on appropriate USB channel
        uint8_t cable_num = 0;
        if (usbHighResolution) {
          uint8_t msb          = (usbOutputValue >> 7) & 0x7F;
          uint8_t lsb          = usbOutputValue & 0x7F;

          uint8_t msbCCData[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex] - 1), controller.usbCCs[controllerIndex], msb};
          uint8_t lsbCCData[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex] - 1), controller.usbCCs[controllerIndex] + 32, lsb};

          tud_midi_stream_write(cable_num, msbCCData, 3);
          tud_midi_stream_write(cable_num, lsbCCData, 3);
        } else {
          uint8_t ccData[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex] - 1), controller.usbCCs[controllerIndex],
                               usbOutputValue};
          tud_midi_stream_write(cable_num, ccData, 3);
        }

        // Send CC on appropiate TRS channel
        // TODO: if TRS high resolution
        if (trsHighResolution) {
          uint8_t msb              = (trsOutputValue >> 7) & 0x7F;
          uint8_t lsb              = trsOutputValue & 0x7F;

          uint8_t trs_msbCCData[3] = {(uint8_t)(0xB0 | controller.trsMidiChannels[controllerIndex] - 1), controller.trsCCs[controllerIndex], msb};
          uint8_t trs_lsbCCData[3] = {(uint8_t)(0xB0 | controller.trsMidiChannels[controllerIndex] - 1), controller.trsCCs[controllerIndex] + 32, lsb};
          // tud_midi_stream_write(cable_num, cc, 3);
          midi_uart_write_tx_buffer(midi_uart_instance, trs_msbCCData, 3);
          midi_uart_write_tx_buffer(midi_uart_instance, trs_lsbCCData, 3);
        } else {
          uint8_t ccData[3] = {(uint8_t)(0xB0 | controller.usbMidiChannels[controllerIndex] - 1), controller.usbCCs[controllerIndex],
                               trsOutputValue};
          midi_uart_write_tx_buffer(midi_uart_instance, ccData, 3);
        }

        midiActivity           = true;
        midiActivityLightOffAt = make_timeout_time_us(MIDI_BLINK_DURATION);
      }

      if (controller.i2cLeader) {
        sendToAllI2C(i, i2cData[i]);
      }
    }
  }
}

// Our handler is called from the I2C ISR, so it must complete quickly. Blocking calls /
// printing to stdio may interfere with interrupt handling.
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
  uint16_t shiftReady = 0;
  uint16_t outputValue;
  uint8_t incoming;

  switch (event) {
  case I2C_SLAVE_RECEIVE: // master has written some data
    // parse the response
    activeInput = i2c_read_byte_raw(i2c);
    if (activeInput < 0) {
      activeInput = 0;
    }
    if (activeInput > FADER_COUNT - 1) {
      activeInput = FADER_COUNT - 1;
    }

    break;
  case I2C_SLAVE_REQUEST: // master is requesting data
    // received an i2c read request

    // get the appropriate value
    shiftReady = i2cData[activeInput];

    // send the puppy as MSB/LSB
    i2c_write_byte_raw(i2c, shiftReady >> 8);
    i2c_write_byte_raw(i2c, shiftReady & 255);
    break;
  case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
    break;
  default:
    break;
  }
}
