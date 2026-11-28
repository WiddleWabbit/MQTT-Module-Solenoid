#pragma once

#include <stdint.h>

#include "ISolenoidPort.h"

/**
 * Hold rail and the channel, capacitor, and pulse switches on the
 * ATmega328PB. Each pin drives an NPN that turns on a high-side PMOS.
 * High is the switch on. begin() drives them low before they become
 * outputs. The sense pins and the ISP header are not this port.
 */
class AvrSolenoidPort : public ISolenoidPort
{
public:
  static constexpr uint8_t kChannelCount = 4;
  static constexpr uint8_t kCapacitorCount = 3;

  /**
   * Stores the pins. The levels are applied in begin().
   *
   * @param holdPins S_EN_1 through S_EN_4.
   * @param pulsePins P_EN_1 through P_EN_4.
   * @param capacitorPins C_S_1 through C_S_3.
   * @param pulseTiePin PULSE_T.
   * @param holdRailPin S_PWR_EN.
   */
  AvrSolenoidPort(const uint8_t holdPins[kChannelCount],
                  const uint8_t pulsePins[kChannelCount],
                  const uint8_t capacitorPins[kCapacitorCount],
                  uint8_t pulseTiePin, uint8_t holdRailPin);

  /**
   * Drives every switch low, then makes those pins outputs.
   *
   * @return Nothing.
   */
  void begin();

  /**
   * Drives the hold-rail enable.
   *
   * @param on True to drive S_PWR_EN high.
   * @return Nothing.
   */
  void setHoldRail(bool on) override;

  /**
   * Drives one channel hold switch.
   *
   * @param index Zero-based channel. Indexes past the four switches
   *        are ignored.
   * @param on True to drive that S_EN high.
   * @return Nothing.
   */
  void setChannelHold(uint8_t index, bool on) override;

  /**
   * Drives one channel pulse select.
   *
   * @param index Zero-based channel. Indexes past the four switches
   *        are ignored.
   * @param on True to drive that P_EN high.
   * @return Nothing.
   */
  void setChannelPulse(uint8_t index, bool on) override;

  /**
   * Drives one capacitor into the bank.
   *
   * @param index Zero-based capacitor. Indexes past the three
   *        switches are ignored.
   * @param on True to drive that C_S high.
   * @return Nothing.
   */
  void setCapacitor(uint8_t index, bool on) override;

  /**
   * Drives the bank onto the pulse net.
   *
   * @param on True to drive PULSE_T high.
   * @return Nothing.
   */
  void setPulseTie(bool on) override;

private:
  /**
   * Drives one pin low, then makes it an output, so the switch
   * cannot glitch on as the pin leaves its reset state.
   *
   * @param pin Arduino pin number from main.cpp.
   * @return Nothing.
   */
  void _outputLow(uint8_t pin);

  uint8_t _holdPins[kChannelCount];
  uint8_t _pulsePins[kChannelCount];
  uint8_t _capacitorPins[kCapacitorCount];
  uint8_t _pulseTiePin;
  uint8_t _holdRailPin;
};
