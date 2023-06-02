/*
 * 16next Header file
 *
*/

#define FIRMWARE_VERSION_MAJOR 3
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_POINT 0 

#define FADER_COUNT 16

#define FIRST_MUX_PIN 18
#define MUX_PIN_COUNT 4 
#define ADC_PIN 26
#define INTERNAL_LED_PIN 22

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

#define MIDI_INPUT_BUFFER 64

#define CONTROL_POLL_TIMEOUT 10 // ms

/*
 * Data structure containing all elements of controller config.
*/
struct ControllerConfig {
  bool powerLed;
  bool midiLed;
  bool rotated;
  bool i2cFollower;
  uint32_t faderMin;
  uint32_t faderMax;
  bool midiThru;
  uint8_t usbMidiChannels[16];
  uint8_t usbCCs[16];
  uint8_t trsMidiChannels[16];
  uint8_t trsCCs[16];
};

/*
 * Functions appearing in 16next.cpp
*/
void midi_read_task();
void processSysexBuffer();
void copySysexStreamToBuffer(uint8_t* inputBuffer, uint8_t streamLength);
void updateControls(bool force=false);
void loadConfig(bool setDefault=false);
void applyConfig(uint8_t* config);
void saveConfig(uint8_t* config);
void setDefaultConfig();
void sendByteArrayAsSysex(uint8_t messageId, uint8_t* byteArray, uint8_t byteArrayLength);
void sendCurrentConfig();
void updateConfig(uint8_t* incomingSysex, uint8_t incomingSysexLength);
