#pragma once

#include <stdint.h>

#include "ISolenoidPort.h"
#include "ModuleProtocol.h"

/**
 * Product settings for the solenoid bank. The sketch owns the values.
 * channelCount is the count reported to the host. capacitorCount is
 * how many of C_S_1 upward are used. 0 closes the hold switch and
 * does not pulse. chargeMs is how long the bank must sit after boot
 * or after a release before the next pulse. The capacitors charge
 * while they are off the bank. pulseMs is how long the pulse tie
 * stays on. commandSilenceMs is how long the holds stay up after
 * the last accepted set. The motherboard resends that set about
 * once a minute while an output should stay on.
 */
struct SolenoidConfig
{
  uint8_t channelCount;
  uint8_t capacitorCount;
  uint32_t chargeMs;
  uint32_t pulseMs;
  uint32_t commandSilenceMs;
};

/**
 * Result of a state read or a set. state is a wire state byte when
 * access is Ok.
 */
enum class SolenoidAccess : uint8_t
{
  Ok,
  Busy,
  Rejected
};

/**
 * One solenoid command result.
 */
struct SolenoidView
{
  SolenoidAccess access;
  uint8_t state;
};

/**
 * Four sprinkler channels. A turn-on waits out the charge time, dumps
 * the selected capacitors into one channel, then leaves the hold
 * switch on. Turn-off drops that hold. One pulse runs at a time.
 * The hold rail turns on at the first update and stays on.
 *
 * count, state, and setDesired may run from the TWI interrupt.
 * update runs from the main loop with interrupts off. No Arduino
 * types.
 */
class SolenoidBank
{
public:
  static constexpr uint8_t kCapacitorSlots = 3;

  /**
   * Binds the port and copies the settings. The constructor does
   * not touch the pins, because globals run before the chip is ready.
   *
   * @param port Hold rail and the channel, capacitor, and pulse switches.
   * @param config Counts and times. Copied. Counts above the board's
   *        switches are clamped.
   */
  SolenoidBank(ISolenoidPort& port, const SolenoidConfig& config);

  /**
   * Copying would share the port without a second owner.
   *
   * @param other Source bank. Unused.
   */
  SolenoidBank(const SolenoidBank& other) = delete;

  /**
   * Assignment would share the port without a second owner.
   *
   * @param other Source bank. Unused.
   * @return Nothing.
   */
  SolenoidBank& operator=(const SolenoidBank& other) = delete;

  /**
   * Turns the hold rail on, drops holds that are no longer wanted,
   * and advances at most one pulse. The first call starts the charge
   * clock and the silence window.
   *
   * @param nowMs Milliseconds from the same clock as the last call.
   *        Unsigned wrap is valid.
   * @return Nothing.
   */
  void update(uint32_t nowMs);

  /**
   * Reports the channel count from the settings.
   *
   * @return Channel count, 0..kMaxSolenoidsPerModule.
   */
  uint8_t count() const;

  /**
   * Reports one channel. Off when it is not wanted. On when the hold
   * is committed. Busy when it is wanted and the pulse has not
   * finished. Rejected when the index is outside the count.
   *
   * @param index Zero-based channel.
   * @return Access and, when Ok, the wire state.
   */
  SolenoidView state(uint8_t index) const;

  /**
   * Stores on or off for one channel and restarts the silence window.
   * Off is Ok immediately. On is Busy until the pulse has cleaned up,
   * or Ok when that channel is already holding. A bad index or a
   * level other than off or on is Rejected and does not restart the
   * window.
   *
   * @param index Zero-based channel.
   * @param level kSolenoidStateOff or kSolenoidStateOn.
   * @return Access and, when Ok, the wire state.
   */
  SolenoidView setDesired(uint8_t index, uint8_t level);

private:
  enum class Phase : uint8_t
  {
    Idle,
    Pulsing
  };

  /**
   * Starts the clocks on the first call. An accepted set since the
   * previous call restarts silence. Otherwise a finished window
   * clears every desired bit.
   *
   * @param nowMs Milliseconds from update.
   * @return Nothing.
   */
  void _openClock(uint32_t nowMs);

  /**
   * Turns off holds whose desired bit is clear.
   *
   * @return Nothing.
   */
  void _dropReleasedHolds();

  /**
   * Ends the pulse that is running, or starts the lowest channel
   * that is wanted and not yet holding.
   *
   * @param nowMs Milliseconds from update.
   * @return Nothing.
   */
  void _advance(uint32_t nowMs);

  /**
   * Reports whether the bank has been idle long enough to pulse.
   *
   * @param nowMs Milliseconds from update.
   * @return True when no charge wait remains, or when no capacitors
   *         are in use.
   */
  bool _chargeReady(uint32_t nowMs) const;

  /**
   * Finds the lowest channel that is wanted and not holding.
   *
   * @return Channel index, or -1.
   */
  int8_t _nextChannel() const;

  /**
   * Closes the hold, or connects the capacitors and fires the pulse.
   *
   * @param index Channel to turn on.
   * @param nowMs Milliseconds from update.
   * @return Nothing.
   */
  void _beginPulse(uint8_t index, uint32_t nowMs);

  /**
   * Turns the pulse switches off and stamps the release. keepHold
   * leaves the channel hold on when that channel is still wanted.
   *
   * @param nowMs Milliseconds from update. Becomes the release time.
   * @param keepHold True to leave the hold switch on.
   * @return Nothing.
   */
  void _endPulse(uint32_t nowMs, bool keepHold);

  /**
   * Builds the view for one in-range or rejected index.
   *
   * @param index Zero-based channel.
   * @return Access and, when Ok, the wire state.
   */
  SolenoidView _view(uint8_t index) const;

  ISolenoidPort& _port;
  uint8_t _channelCount;
  uint8_t _capacitorCount;
  uint32_t _chargeMs;
  uint32_t _pulseMs;
  uint32_t _silenceMs;
  uint8_t _active;
  uint32_t _pulseStartedMs;
  uint32_t _lastReleaseMs;
  uint32_t _silenceAnchor;
  Phase _phase;
  bool _booted;
  bool _silenceRestarted;
  bool _desired[module_protocol::kMaxSolenoidsPerModule];
  bool _holding[module_protocol::kMaxSolenoidsPerModule];
};
