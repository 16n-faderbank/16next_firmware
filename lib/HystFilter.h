#pragma once

#include <stdint.h>

/**
 * EMA Filter with Hysteresis
 * 
 * Based on https://github.com/tttapa/Arduino-Filters
 */

class HystFilter {
  public: 
    HystFilter(float pole, int hyst_bits);
    bool update(uint16_t);
    float updateFiltered(float);

    uint8_t getValue();

    void setValue(uint16_t);

  private:
    float alpha;
    uint8_t hyst_bits;
    float filtered = 0;

    uint16_t prevLevel = 0;
    uint16_t margin;
    uint16_t offset;
    uint16_t max_in = -1;
    uint8_t max_out;
};
