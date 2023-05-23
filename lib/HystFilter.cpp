#include "HystFilter.h"

/**
     * @brief   Create an exponential moving average filter with hysteresis,
     *          with a pole at a given location, and with a set number of bits
     *          to be reduced in hysteresis.
     * 
     * @param   pole
     *          The pole of the filter (@f$1-\alpha@f$).  
     *          Should be a value in the range 
     *          @f$ \left[0,1\right) @f$.  
     *          Zero means no filtering, and closer to one means more filtering.
     * 
     * @param   hyst_bits
     *          The number of bits to reduce the input by within a hysteresis
     *          function. For instance, to convert 12-bit (0-4096) input to
     *          7-bit (0-128), set this to 5.
     */
HystFilter::HystFilter(float pole, int hyst_bits) {
  this->alpha = 1 - pole;
  this->hyst_bits = hyst_bits;
  this->margin = (1UL << hyst_bits) - 1;
  this->offset = 1UL << (hyst_bits - 1);
  this->max_out = static_cast<uint8_t>(max_in >> hyst_bits);
}

/**
 * @brief   Update the hysteresis output with a new input value.
 *
 * @param   rawInput
 *          The raw Input to feed into filter, then hysteresis
 * @retval  true
 *          The output level has changed.
 * @retval  false
 *          The output level is still the same.
 */
bool HystFilter::update(uint16_t rawInput) {
  uint16_t inputLevel = (uint16_t)updateFiltered(rawInput);

  uint16_t prevLevelFull = (uint16_t(this->prevLevel) << this->hyst_bits) | this->offset;
  uint16_t lowerbound = this->prevLevel > 0 ? prevLevelFull - this->margin : 0;
  uint16_t upperbound = this->prevLevel < this->max_out ? prevLevelFull + this->margin : this->max_in;
  if (inputLevel < lowerbound || inputLevel > upperbound) {
      setValue(inputLevel);
      return true;
  }
  return false;
}

/**
 * @brief   Filter the input: Given @f$ x[n] @f$, calculate @f$ y[n] @f$.
 *
 * @param   value
 *          The new raw input value.
 * @return  The new filtered output value.
     */
float HystFilter::updateFiltered(float value) {
  this->filtered += (value-this->filtered) * this->alpha;
  return this->filtered;
}

/**
 * @brief   Get the current output level.
 *
 * @return  The output level.
 */
uint8_t HystFilter::getValue() { return this->prevLevel; }

/** 
 * @brief   Forcefully update the internal state to the given level.
 */
void HystFilter::setValue(uint16_t inputLevel) { this->prevLevel = inputLevel >> this->hyst_bits; }
