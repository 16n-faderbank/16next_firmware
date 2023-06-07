/*
 * TELEX Eurorack Modules
 * (c) 2016,2017 Brendon Cassidy
 * MIT License
 */

#include <hardware/i2c.h>
#ifndef TxHelper_h
#define TxHelper_h

#include <stdint.h>
#include <stdio.h>

struct TxResponse
{
  uint8_t Command;
  uint8_t Output;
  int Value;
};

struct TxIO
{
  short Port;
  short Mode;
};

class TxHelper
{
public:
  static TxResponse Parse(size_t len);
  static TxIO DecodeIO(int io);

  static void SetPorts(int ports);
  static void SetModes(int modes);
  static void UseI2CPort1(bool use);

protected:
  static int Ports;
  static int Modes;
  static bool i2cPort0;

private:
};

#endif
