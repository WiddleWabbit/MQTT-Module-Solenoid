#pragma once

#include <stdint.h>

/**
 * Hold rail, channel hold, channel pulse select, capacitor bank,
 * and the pulse tie. High is the switch on. The AVR driver is the
 * board. Desktop tests use a fake. No Arduino types.
 */
class ISolenoidPort
{
public:
  virtual ~ISolenoidPort() = default;

  /**
   * Drives the hold-rail enable. S_PWR_EN in main.cpp. High turns
   * the buck on.
   *
   * @param on True to drive the enable high.
   * @return Nothing.
   */
  virtual void setHoldRail(bool on) = 0;

  /**
   * Drives one channel's hold switch. S_EN in main.cpp. High
   * connects the hold rail to that channel.
   *
   * @param index Zero-based channel.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  virtual void setChannelHold(uint8_t index, bool on) = 0;

  /**
   * Drives one channel's pulse select. P_EN in main.cpp. High
   * connects the pulse net to that channel.
   *
   * @param index Zero-based channel.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  virtual void setChannelPulse(uint8_t index, bool on) = 0;

  /**
   * Drives one capacitor into the bank. C_S in main.cpp. High
   * connects that capacitor.
   *
   * @param index Zero-based capacitor.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  virtual void setCapacitor(uint8_t index, bool on) = 0;

  /**
   * Drives the bank onto the pulse net. PULSE_T in main.cpp. High
   * connects the bank.
   *
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  virtual void setPulseTie(bool on) = 0;
};
