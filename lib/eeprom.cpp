/*
  This is a library to read/write to external I2C EEPROMs.
  It uses the same template system found in the Arduino 
  EEPROM library so you can use the same get() and put() functions.

  https://github.com/sparkfun/SparkFun_External_EEPROM_Arduino_Library
  Best used with the Qwiic EEPROM: https://www.sparkfun.com/products/14764

  Various external EEPROMs have various interface specs
  (overall size, page size, write times, etc). This library works with
  all types and allows the various settings to be set at runtime.

  All read and write restrictions associated with pages are taken care of.
  You can access the external memory as if it was contiguous.

  Development environment specifics:
  Arduino IDE 1.8.x

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
*/

#include "eeprom.h"

bool EEPROM::begin(uint8_t deviceAddress, i2c_inst_t *i2cPort)
{
  settings.i2cPort = i2cPort; //Grab which port the user wants us to use
  settings.deviceAddress = deviceAddress;

  if (isConnected() == false)
  {
    return false;
  }

  return true;
}

//Erase entire EEPROM
void EEPROM::erase(uint8_t toWrite)
{
  uint8_t tempBuffer[settings.pageSize_bytes];
  for (uint32_t x = 0; x < settings.pageSize_bytes; x++)
    tempBuffer[x] = toWrite;

  for (uint32_t addr = 0; addr < length(); addr += settings.pageSize_bytes)
    write(addr, tempBuffer, settings.pageSize_bytes);
}

uint32_t EEPROM::length()
{
  return settings.memorySize_bytes;
}

//Returns true if device is detected
bool EEPROM::isConnected(uint8_t i2cAddress)
{
  if (i2cAddress == 255)
    i2cAddress = settings.deviceAddress; //We can't set the default to settings.deviceAddress so we use 255 instead

  // settings.i2cPort->beginTransmission((uint8_t)i2cAddress);
  uint8_t buf;
  if(i2c_read_blocking(settings.i2cPort,i2cAddress,&buf,0, false) >= 0) {
    return (true);
  }
  return (false);
}

//Returns true if device is not answering (currently writing)
//Caller can pass in an I2C address. This is helpful for larger EEPROMs that have two addresses (see block bit 2).
bool EEPROM::isBusy(uint8_t i2cAddress)
{
  if (i2cAddress == 255)
    i2cAddress = settings.deviceAddress; //We can't set the default to settings.deviceAddress so we use 255 instead

  if (isConnected(i2cAddress))
    return (false);
  return (true);
}

//void EEPROM::settings(struct_memorySettings newSettings)
//{
//  settings.deviceAddress = newSettings.deviceAddress;
//}
void EEPROM::setMemorySize(uint32_t memSize)
{
  settings.memorySize_bytes = memSize;
}
uint32_t EEPROM::getMemorySize()
{
  return settings.memorySize_bytes;
}
void EEPROM::setPageSize(uint16_t pageSize)
{
  settings.pageSize_bytes = pageSize;
}
uint16_t EEPROM::getPageSize()
{
  return settings.pageSize_bytes;
}
void EEPROM::setPageWriteTime(uint8_t writeTimeMS)
{
  settings.pageWriteTime_ms = writeTimeMS;
}
uint8_t EEPROM::getPageWriteTime()
{
  return settings.pageWriteTime_ms;
}
void EEPROM::enablePollForWriteComplete()
{
  settings.pollForWriteComplete = true;
}
void EEPROM::disablePollForWriteComplete()
{
  settings.pollForWriteComplete = false;
}
constexpr uint16_t EEPROM::getI2CBufferSize()
{
  return I2C_BUFFER_LENGTH_TX;
}
//Read a byte from a given location
uint8_t EEPROM::read(uint32_t eepromLocation)
{
  uint8_t tempByte;
  // read(eepromLocation, &tempByte, 1);
  uint8_t memaddr[] = {eepromLocation >> 8, eepromLocation & 0xFF};
  uint8_t readbuf[1];

  // while(i2c_write_blocking(settings.i2cPort, settings.deviceAddress, memaddr, 2, true) < 1) {
    i2c_write_blocking(settings.i2cPort, settings.deviceAddress, memaddr, 2, true);
    sleep_ms(5);
  // }

  i2c_read_blocking(settings.i2cPort, settings.deviceAddress,readbuf, 1, false);
  return readbuf[0];
}

void EEPROM::readArray(uint32_t eepromLocation, uint8_t buffer[], uint16_t length) {
  for (int i = 0; i < length; i++) {
    buffer[i] = read(eepromLocation+i);
  }
}

void EEPROM::writeArray(uint32_t eepromLocation, uint8_t buffer[], uint16_t length) {
  for (int i = 0; i < length; i++) {
    write(eepromLocation+i, buffer[i]);
  }
}

//Bulk read from EEPROM
//Handles breaking up read amt into 32 byte chunks (can be overriden with setI2CBufferSize)
//Handles a read that straddles the 512kbit barrier
// void EEPROM::read(uint32_t eepromLocation, uint8_t *buff, uint16_t bufferSize)
// {
//   uint16_t received = 0;

//   while (received < bufferSize)
//   {
//     //Limit the amount to write to a page size
//     uint16_t amtToRead = bufferSize - received;
//     if (amtToRead > I2C_BUFFER_LENGTH_RX) //Arduino I2C buffer size limit
//       amtToRead = I2C_BUFFER_LENGTH_RX;

//     //Check if we are dealing with large (>512kbit) EEPROMs
//     uint8_t i2cAddress = settings.deviceAddress;
//     if (settings.memorySize_bytes > 0xFFFF)
//     {
//       //Figure out if we are going to cross the barrier with this read
//       if (eepromLocation + received < 0xFFFF)
//       {
//         if (0xFFFF - (eepromLocation + received) < amtToRead) //0xFFFF - 0xFFFA < 32
//           amtToRead = 0xFFFF - (eepromLocation + received);   //Limit the read amt to go right up to edge of barrier
//       }

//       //Figure out if we are accessing the lower half or the upper half
//       if (eepromLocation + received > 0xFFFF)
//         i2cAddress |= 0b100; //Set the block bit to 1
//     }

//     //See if EEPROM is available or still writing a previous request
//     while (settings.pollForWriteComplete && isBusy(i2cAddress) == true) //Poll device
//       sleep_us(100);          //This shortens the amount of time waiting between writes but hammers the I2C bus

//     if(getMemorySize() > 2048)
//       i2c_write_blocking(settings.i2cPort, i2cAddress,(eepromLocation+received) >> 8, 1, true); // MSB
//     i2c_write_blocking(settings.i2cPort, i2cAddress,(eepromLocation+received) & 0xFF, 1, true); // LSB

//     i2c_read_blocking(settings.i2cPort, i2cAddress,buff,1,false)


//     i2c_read_blocking(settings.i2cPort,i2cAddress,buff,amtToRead)
//     received += amtToRead;
//   }
// }

//Write a byte to a given location
void EEPROM::write(uint32_t eepromLocation, uint8_t dataToWrite)
{
  // update only if data is new
  if (read(eepromLocation) == dataToWrite) {
    return;
  }

  uint8_t tempByte;
  // read(eepromLocation, &tempByte, 1);

  uint8_t memaddr[] = {eepromLocation >> 8, eepromLocation & 0xFF};
  uint8_t writebuf[] = {memaddr[0],memaddr[1],dataToWrite};

  // while(i2c_write_blocking(settings.i2cPort, settings.deviceAddress, writebuf, 3, false) < 1) {
    i2c_write_blocking(settings.i2cPort, settings.deviceAddress, writebuf, 3, false);
  // }
  sleep_ms(5);
}

//Write large bulk amounts
//Limits writes to the I2C buffer size (default is 32 bytes)
// void EEPROM::write(uint32_t eepromLocation, const uint8_t *dataToWrite, uint16_t bufferSize)
// {
//   //Error check
//   if (eepromLocation + bufferSize >= settings.memorySize_bytes)
//     bufferSize = settings.memorySize_bytes - eepromLocation;

//   int16_t maxWriteSize = settings.pageSize_bytes;
//   if (maxWriteSize > I2C_BUFFER_LENGTH_TX - 2)
//     maxWriteSize = I2C_BUFFER_LENGTH_TX - 2; //Arduino has 32 byte limit. We loose two to the EEPROM address

//   //Break the buffer into page sized chunks
//   uint16_t recorded = 0;
//   while (recorded < bufferSize)
//   {
//     //Limit the amount to write to either the page size or the Arduino limit of 30
//     int amtToWrite = bufferSize - recorded;
//     if (amtToWrite > maxWriteSize)
//       amtToWrite = maxWriteSize;

//     if (amtToWrite > 1)
//     {
//       //Check for crossing of a page line. Writes cannot cross a page line.
//       uint16_t pageNumber1 = (eepromLocation + recorded) / settings.pageSize_bytes;
//       uint16_t pageNumber2 = (eepromLocation + recorded + amtToWrite - 1) / settings.pageSize_bytes;
//       if (pageNumber2 > pageNumber1)
//         amtToWrite = ((pageNumber1+1) * settings.pageSize_bytes) - (eepromLocation + recorded); //Limit the write amt to go right up to edge of page barrier
//     }

//     uint8_t i2cAddress = settings.deviceAddress;
//     //Check if we are dealing with large (>512kbit) EEPROMs
//     if (settings.memorySize_bytes > 0xFFFF)
//     {
//       //Figure out if we are accessing the lower half or the upper half
//       if (eepromLocation + recorded > 0xFFFF)
//         i2cAddress |= 0b100; //Set the block bit to 1
//     }

//     //See if EEPROM is available or still writing a previous request
//     while (settings.pollForWriteComplete && isBusy(i2cAddress) == true) //Poll device
//       sleep_us(100);          //This shortens the amount of time waiting between writes but hammers the I2C bus

//     if(getMemorySize() > 2048)
//       i2c_write_byte_raw(settings.i2cPort,i2cAddress,(eepromLocation + recorded) >> 8);   // MSB
//     i2c_write_byte_raw(settings.i2cPort,i2cAddress,(eepromLocation + recorded) & 0xFF);   // LSB

//     i2c_write_blocking(settings.i2cPort,i2cAddress)
//     for (uint8_t x = 0; x < amtToWrite; x++)

//       settings.i2cPort->write(dataToWrite[recorded + x]);

//     recorded += amtToWrite;

//     if (settings.pollForWriteComplete == false)
//       sleep_ms(settings.pageWriteTime_ms); //Delay the amount of time to record a page
//   }
// }
