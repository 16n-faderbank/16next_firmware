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
    sleep_ms(settings.pageWriteTime_ms);
  // }

  i2c_read_blocking(settings.i2cPort, settings.deviceAddress,readbuf, 1, false);
  return readbuf[0];
}

// kludgy way of reading from EEPROM. Try not to use this.
void EEPROM::readArray(uint32_t eepromLocation, uint8_t buffer[], uint16_t length) {
  for (int i = 0; i < length; i++) {
    buffer[i] = read(eepromLocation+i);
  }
}

// kludgy way of writing in bulk to EEPROM. Try not to use this.
void EEPROM::writeArray(uint32_t eepromLocation, uint8_t buffer[], uint16_t length) {
  for (int i = 0; i < length; i++) {
    write(eepromLocation+i, buffer[i]);
  }
}

// Bulk read from EEPROM
// Handles breaking up read amt into 32 byte chunks.
//
void EEPROM::read(uint32_t eepromLocation, uint8_t *buff, uint16_t bufferSize) {
  uint8_t received = 0;
  uint8_t amtToRead = 0;
  uint8_t tempBuf[settings.pageSize_bytes];

  while(received < bufferSize) {
  uint8_t amtToRead = bufferSize - received;
    if(amtToRead > settings.pageSize_bytes) {
      amtToRead = settings.pageSize_bytes;
    }

    uint32_t runningLocation = eepromLocation + received;

    uint8_t memaddr[] = {runningLocation>> 8, runningLocation & 0xFF};

    while(i2c_write_blocking(settings.i2cPort, settings.deviceAddress, memaddr, 2, true) < 1) {
      i2c_write_blocking(settings.i2cPort, settings.deviceAddress, memaddr, 2, true);
    }
    sleep_ms(settings.pageWriteTime_ms);

    i2c_read_blocking(settings.i2cPort, settings.deviceAddress,tempBuf, amtToRead, false);

    for(uint8_t i = 0; i < amtToRead; i++) {
      uint8_t index = received + i;
      buff[index] = tempBuf[i];
    }

    received += amtToRead;
  }
}

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
  sleep_ms(settings.pageWriteTime_ms);
}

//Write large bulk amounts
//Limits writes to the I2C buffer size (default is 32 bytes)
void EEPROM::write(uint32_t eepromLocation, const uint8_t *dataToWrite, uint16_t bufferSize)
{
  //Error check
  if (eepromLocation + bufferSize >= settings.memorySize_bytes)
    bufferSize = settings.memorySize_bytes - eepromLocation;

  int16_t maxWriteSize = settings.pageSize_bytes;

  //Break the buffer into page sized chunks
  uint16_t recorded = 0;
  while (recorded < bufferSize)
  {
    //Limit the amount to write to either the page size or the Arduino limit of 30
    int amtToWrite = bufferSize - recorded;
    if (amtToWrite > maxWriteSize)
      amtToWrite = maxWriteSize;

    if (amtToWrite > 1)
    {
      //Check for crossing of a page line. Writes cannot cross a page line.
      uint16_t pageNumber1 = (eepromLocation + recorded) / settings.pageSize_bytes;
      uint16_t pageNumber2 = (eepromLocation + recorded + amtToWrite - 1) / settings.pageSize_bytes;
      if (pageNumber2 > pageNumber1)
        amtToWrite = ((pageNumber1+1) * settings.pageSize_bytes) - (eepromLocation + recorded); //Limit the write amt to go right up to edge of page barrier
    }

    uint8_t i2cAddress = settings.deviceAddress;

    uint32_t runningLocation = eepromLocation + recorded;

    uint8_t memaddr[] = {runningLocation>> 8, runningLocation & 0xFF};

    while(i2c_write_blocking(settings.i2cPort, i2cAddress, memaddr, 2, true) < 1) {
      i2c_write_blocking(settings.i2cPort, i2cAddress, memaddr, 2, true);
    }
    sleep_ms(settings.pageWriteTime_ms);

    uint8_t writeBuffer[amtToWrite];
    for(int i = 0; i < amtToWrite; i++) {
      writeBuffer[i] = dataToWrite[recorded+i];
    }

    i2c_write_blocking(settings.i2cPort,i2cAddress, writeBuffer,amtToWrite,false);
    recorded += amtToWrite;

    sleep_ms(settings.pageWriteTime_ms); //Delay the amount of time to record a page
  }
}
