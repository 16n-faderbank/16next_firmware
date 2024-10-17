/*
 * 16next Header file
 *
 */

#include "pico/i2c_slave.h"

#define FIRMWARE_VERSION_MAJOR 3
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_POINT 1

#define FADER_COUNT            16

// define the board type here:
// SIXTEEN_RX = 16rx
// SIXTEEN_NX = 16nx
// SIXTEEN_NEXT_DEV_BOARD = 16next dev board
#define SIXTEEN_NX             1

#ifdef SIXTEEN_NEXT_DEV_BOARD
#define FIRST_MUX_PIN    18
#define MUX_PIN_COUNT    4
#define ADC_PIN          26
#define INTERNAL_LED_PIN 22
#define I2C_SDA_PIN      10
#define I2C_SCL_PIN      11
#define DEVICE_INDEX     5
#elif SIXTEEN_RX
#define FIRST_MUX_PIN    18
#define MUX_PIN_COUNT    4
#define ADC_PIN          26
#define INTERNAL_LED_PIN 2
#define I2C_SDA_PIN      10
#define I2C_SCL_PIN      11
#define DEVICE_INDEX     4
#elif SIXTEEN_NX
#define FIRST_MUX_PIN    18
#define MUX_PIN_COUNT    4
#define ADC_PIN          26
#define INTERNAL_LED_PIN 2
#define I2C_SDA_PIN      10
#define I2C_SCL_PIN      11
#define DEVICE_INDEX     5
// #define INVERT_ADC 1// pots appear to be wired in reverse?
#endif

// I2C Address for Faderbank. 0x34 unless you ABSOLUTELY know what you are doing.
#define I2C_ADDRESS         0x34
#define I2C_BAUDRATE        400000

// define startup delay in milliseconds for i2c Leader devices
// this gives follower devices time to boot up.
#define BOOTDELAY           10000

#define MIDI_BLINK_DURATION 5000 // us

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

#define MIDI_INPUT_BUFFER    64

#define CONTROL_POLL_TIMEOUT 10 // ms

/*
 * Functions appearing in 16next.cpp
 */

void midi_read_task();
void updateControls(bool force = false);
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void processSysexBuffer();
