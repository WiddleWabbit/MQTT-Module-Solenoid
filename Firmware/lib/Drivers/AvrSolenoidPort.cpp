#include "AvrSolenoidPort.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>


// ========== Construction ==========

/**
 * Stores the pins. The levels are applied in begin().
 *
 * @param holdPins S_EN_1 through S_EN_4.
 * @param pulsePins P_EN_1 through P_EN_4.
 * @param capacitorPins C_S_1 through C_S_3.
 * @param pulseTiePin PULSE_T.
 * @param holdRailPin S_PWR_EN.
 */
AvrSolenoidPort::AvrSolenoidPort(const uint8_t holdPins[kChannelCount],
                                 const uint8_t pulsePins[kChannelCount],
                                 const uint8_t capacitorPins[kCapacitorCount],
                                 uint8_t pulseTiePin, uint8_t holdRailPin)
  : _pulseTiePin(pulseTiePin),
    _holdRailPin(holdRailPin)
{
  for (uint8_t index = 0; index < kChannelCount; ++index)
  {
    _holdPins[index] = holdPins[index];
    _pulsePins[index] = pulsePins[index];
  }
  for (uint8_t index = 0; index < kCapacitorCount; ++index)
  {
    _capacitorPins[index] = capacitorPins[index];
  }
}


// ========== Public API ==========

/**
 * Drives every switch low, then makes those pins outputs.
 *
 * @return Nothing.
 */
void AvrSolenoidPort::begin()
{
  for (uint8_t index = 0; index < kChannelCount; ++index)
  {
    _outputLow(_holdPins[index]);
    _outputLow(_pulsePins[index]);
  }
  for (uint8_t index = 0; index < kCapacitorCount; ++index)
  {
    _outputLow(_capacitorPins[index]);
  }
  _outputLow(_pulseTiePin);
  _outputLow(_holdRailPin);
}

/**
 * Drives the hold-rail enable.
 *
 * @param on True to drive S_PWR_EN high.
 * @return Nothing.
 */
void AvrSolenoidPort::setHoldRail(bool on)
{
  digitalWrite(_holdRailPin, on ? HIGH : LOW);
}

/**
 * Drives one channel hold switch.
 *
 * @param index Zero-based channel. Indexes past the four switches
 *        are ignored.
 * @param on True to drive that S_EN high.
 * @return Nothing.
 */
void AvrSolenoidPort::setChannelHold(uint8_t index, bool on)
{
  if (index >= kChannelCount)
  {
    return;
  }
  digitalWrite(_holdPins[index], on ? HIGH : LOW);
}

/**
 * Drives one channel pulse select.
 *
 * @param index Zero-based channel. Indexes past the four switches
 *        are ignored.
 * @param on True to drive that P_EN high.
 * @return Nothing.
 */
void AvrSolenoidPort::setChannelPulse(uint8_t index, bool on)
{
  if (index >= kChannelCount)
  {
    return;
  }
  digitalWrite(_pulsePins[index], on ? HIGH : LOW);
}

/**
 * Drives one capacitor into the bank.
 *
 * @param index Zero-based capacitor. Indexes past the three
 *        switches are ignored.
 * @param on True to drive that C_S high.
 * @return Nothing.
 */
void AvrSolenoidPort::setCapacitor(uint8_t index, bool on)
{
  if (index >= kCapacitorCount)
  {
    return;
  }
  digitalWrite(_capacitorPins[index], on ? HIGH : LOW);
}

/**
 * Drives the bank onto the pulse net.
 *
 * @param on True to drive PULSE_T high.
 * @return Nothing.
 */
void AvrSolenoidPort::setPulseTie(bool on)
{
  digitalWrite(_pulseTiePin, on ? HIGH : LOW);
}


// ========== Pins ==========

/**
 * Drives one pin low, then makes it an output, so the switch
 * cannot glitch on as the pin leaves its reset state.
 *
 * @param pin Arduino pin number from main.cpp.
 * @return Nothing.
 */
void AvrSolenoidPort::_outputLow(uint8_t pin)
{
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

#endif
