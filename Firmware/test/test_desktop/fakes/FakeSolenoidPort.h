#pragma once

#include <stdint.h>

#include "ISolenoidPort.h"

/**
 * Which switch a recorded change belongs to.
 */
enum class SolenoidChangeKind : uint8_t
{
  HoldRail,
  Hold,
  PulseSelect,
  Capacitor,
  PulseTie
};

/**
 * One switch edge. Repeated writes of the same level are not stored.
 */
struct SolenoidChange
{
  SolenoidChangeKind kind;
  uint8_t index;
  bool on;
};

/**
 * Records solenoid-port levels and the edges between them.
 */
class FakeSolenoidPort : public ISolenoidPort
{
public:
  static constexpr uint8_t kChannels = 4;
  static constexpr uint8_t kCaps = 3;
  static constexpr uint8_t kMaxChanges = 32;

  bool holdRail = false;
  bool hold[kChannels] = {};
  bool pulseSelect[kChannels] = {};
  bool capacitor[kCaps] = {};
  bool pulseTie = false;
  SolenoidChange changes[kMaxChanges] = {};
  uint8_t changeCount = 0;

  /**
   * Forgets recorded edges. Levels stay as they are.
   *
   * @return Nothing.
   */
  void clearChanges()
  {
    changeCount = 0;
  }

  /**
   * Records the hold rail when the level changes.
   *
   * @param on True to drive the enable high.
   * @return Nothing.
   */
  void setHoldRail(bool on) override
  {
    _write(SolenoidChangeKind::HoldRail, 0, on, holdRail);
  }

  /**
   * Records one channel hold when the level changes.
   *
   * @param index Zero-based channel.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  void setChannelHold(uint8_t index, bool on) override
  {
    if (index >= kChannels)
    {
      return;
    }
    _write(SolenoidChangeKind::Hold, index, on, hold[index]);
  }

  /**
   * Records one channel pulse select when the level changes.
   *
   * @param index Zero-based channel.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  void setChannelPulse(uint8_t index, bool on) override
  {
    if (index >= kChannels)
    {
      return;
    }
    _write(SolenoidChangeKind::PulseSelect, index, on, pulseSelect[index]);
  }

  /**
   * Records one capacitor switch when the level changes.
   *
   * @param index Zero-based capacitor.
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  void setCapacitor(uint8_t index, bool on) override
  {
    if (index >= kCaps)
    {
      return;
    }
    _write(SolenoidChangeKind::Capacitor, index, on, capacitor[index]);
  }

  /**
   * Records the pulse tie when the level changes.
   *
   * @param on True to drive the switch high.
   * @return Nothing.
   */
  void setPulseTie(bool on) override
  {
    _write(SolenoidChangeKind::PulseTie, 0, on, pulseTie);
  }

private:
  /**
   * Stores an edge when the level changes.
   *
   * @param kind Which switch moved.
   * @param index Channel or capacitor. 0 for the rail and the tie.
   * @param on New level.
   * @param level Stored level to compare and update.
   * @return Nothing.
   */
  void _write(SolenoidChangeKind kind, uint8_t index, bool on, bool& level)
  {
    if (level == on)
    {
      return;
    }
    level = on;
    if (changeCount >= kMaxChanges)
    {
      return;
    }
    changes[changeCount].kind = kind;
    changes[changeCount].index = index;
    changes[changeCount].on = on;
    changeCount = static_cast<uint8_t>(changeCount + 1);
  }
};
