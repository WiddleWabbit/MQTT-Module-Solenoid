#include "SolenoidBank.h"


// ========== Construction ==========

/**
 * Binds the port and copies the settings. The constructor does not
 * touch the pins, because globals run before the chip is ready.
 *
 * @param port Hold rail and the channel, capacitor, and pulse switches.
 * @param config Counts and times. Copied. Counts above the board's
 *        switches are clamped.
 */
SolenoidBank::SolenoidBank(ISolenoidPort& port, const SolenoidConfig& config)
  : _port(port),
    _channelCount(config.channelCount),
    _capacitorCount(config.capacitorCount),
    _chargeMs(config.chargeMs),
    _pulseMs(config.pulseMs),
    _silenceMs(config.commandSilenceMs),
    _active(0),
    _pulseStartedMs(0),
    _lastReleaseMs(0),
    _silenceAnchor(0),
    _phase(Phase::Idle),
    _booted(false),
    _silenceRestarted(false)
{
  if (_channelCount > module_protocol::kMaxSolenoidsPerModule)
  {
    _channelCount = module_protocol::kMaxSolenoidsPerModule;
  }
  if (_capacitorCount > kCapacitorSlots)
  {
    _capacitorCount = kCapacitorSlots;
  }
  for (uint8_t index = 0; index < module_protocol::kMaxSolenoidsPerModule;
       ++index)
  {
    _desired[index] = false;
    _holding[index] = false;
  }
}


// ========== Public API ==========

/**
 * Turns the hold rail on, drops holds that are no longer wanted,
 * and advances at most one pulse. The first call starts the charge
 * clock and the silence window.
 *
 * @param nowMs Milliseconds from the same clock as the last call.
 *        Unsigned wrap is valid.
 * @return Nothing.
 */
void SolenoidBank::update(uint32_t nowMs)
{
  _openClock(nowMs);
  _port.setHoldRail(true);
  _dropReleasedHolds();
  _advance(nowMs);
}

/**
 * Reports the channel count from the settings.
 *
 * @return Channel count, 0..kMaxSolenoidsPerModule.
 */
uint8_t SolenoidBank::count() const
{
  return _channelCount;
}

/**
 * Reports one channel. Off when it is not wanted. On when the hold
 * is committed. Busy when it is wanted and the pulse has not
 * finished. Rejected when the index is outside the count.
 *
 * @param index Zero-based channel.
 * @return Access and, when Ok, the wire state.
 */
SolenoidView SolenoidBank::state(uint8_t index) const
{
  return _view(index);
}

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
SolenoidView SolenoidBank::setDesired(uint8_t index, uint8_t level)
{
  SolenoidView rejected;
  rejected.access = SolenoidAccess::Rejected;
  rejected.state = module_protocol::kSolenoidStateOff;
  if (index >= _channelCount)
  {
    return rejected;
  }
  if (level != module_protocol::kSolenoidStateOff &&
      level != module_protocol::kSolenoidStateOn)
  {
    return rejected;
  }
  _desired[index] = level == module_protocol::kSolenoidStateOn;
  _silenceRestarted = true;
  return _view(index);
}


// ========== Clock and holds ==========

/**
 * Starts the clocks on the first call. An accepted set since the
 * previous call restarts silence. Otherwise a finished window
 * clears every desired bit.
 *
 * @param nowMs Milliseconds from update.
 * @return Nothing.
 */
void SolenoidBank::_openClock(uint32_t nowMs)
{
  if (!_booted)
  {
    _booted = true;
    _lastReleaseMs = nowMs;
    _silenceAnchor = nowMs;
  }
  if (_silenceRestarted)
  {
    _silenceAnchor = nowMs;
    _silenceRestarted = false;
    return;
  }
  if (static_cast<uint32_t>(nowMs - _silenceAnchor) < _silenceMs)
  {
    return;
  }
  for (uint8_t index = 0; index < _channelCount; ++index)
  {
    _desired[index] = false;
  }
}

/**
 * Turns off holds whose desired bit is clear.
 *
 * @return Nothing.
 */
void SolenoidBank::_dropReleasedHolds()
{
  for (uint8_t index = 0; index < _channelCount; ++index)
  {
    if (_holding[index] && !_desired[index])
    {
      _holding[index] = false;
      _port.setChannelHold(index, false);
    }
  }
}

/**
 * Reports whether the bank has been idle long enough to pulse.
 *
 * @param nowMs Milliseconds from update.
 * @return True when no charge wait remains, or when no capacitors
 *         are in use.
 */
bool SolenoidBank::_chargeReady(uint32_t nowMs) const
{
  if (_capacitorCount == 0)
  {
    return true;
  }
  return static_cast<uint32_t>(nowMs - _lastReleaseMs) >= _chargeMs;
}

/**
 * Finds the lowest channel that is wanted and not holding.
 *
 * @return Channel index, or -1.
 */
int8_t SolenoidBank::_nextChannel() const
{
  for (uint8_t index = 0; index < _channelCount; ++index)
  {
    if (_desired[index] && !_holding[index])
    {
      return static_cast<int8_t>(index);
    }
  }
  return -1;
}


// ========== Pulse ==========

/**
 * Ends the pulse that is running, or starts the lowest channel
 * that is wanted and not yet holding.
 *
 * @param nowMs Milliseconds from update.
 * @return Nothing.
 */
void SolenoidBank::_advance(uint32_t nowMs)
{
  if (_phase == Phase::Pulsing)
  {
    const uint32_t pulsed = static_cast<uint32_t>(nowMs - _pulseStartedMs);
    if (!_desired[_active])
    {
      _endPulse(nowMs, false);
    }
    else if (pulsed >= _pulseMs)
    {
      _endPulse(nowMs, true);
    }
    else
    {
      return;
    }
  }

  const int8_t next = _nextChannel();
  if (next < 0 || !_chargeReady(nowMs))
  {
    return;
  }
  _beginPulse(static_cast<uint8_t>(next), nowMs);
}

/**
 * Closes the hold, or connects the capacitors and fires the pulse.
 *
 * @param index Channel to turn on.
 * @param nowMs Milliseconds from update.
 * @return Nothing.
 */
void SolenoidBank::_beginPulse(uint8_t index, uint32_t nowMs)
{
  if (_capacitorCount == 0)
  {
    _port.setChannelHold(index, true);
    _holding[index] = true;
    return;
  }

  // The capacitors are already charging while they sit off the bank.
  // Selecting them does not add another wait.
  for (uint8_t cap = 0; cap < _capacitorCount; ++cap)
  {
    _port.setCapacitor(cap, true);
  }
  _port.setChannelPulse(index, true);
  _port.setPulseTie(true);
  _port.setChannelHold(index, true);
  _active = index;
  _pulseStartedMs = nowMs;
  _phase = Phase::Pulsing;
}

/**
 * Turns the pulse switches off and stamps the release. keepHold
 * leaves the channel hold on when that channel is still wanted.
 *
 * @param nowMs Milliseconds from update. Becomes the release time.
 * @param keepHold True to leave the hold switch on.
 * @return Nothing.
 */
void SolenoidBank::_endPulse(uint32_t nowMs, bool keepHold)
{
  _port.setPulseTie(false);
  _port.setChannelPulse(_active, false);
  for (uint8_t cap = 0; cap < _capacitorCount; ++cap)
  {
    _port.setCapacitor(cap, false);
  }
  if (keepHold && _desired[_active])
  {
    _holding[_active] = true;
  }
  else
  {
    _port.setChannelHold(_active, false);
    _holding[_active] = false;
  }
  _lastReleaseMs = nowMs;
  _phase = Phase::Idle;
}

/**
 * Builds the view for one in-range or rejected index.
 *
 * @param index Zero-based channel.
 * @return Access and, when Ok, the wire state.
 */
SolenoidView SolenoidBank::_view(uint8_t index) const
{
  SolenoidView view;
  view.access = SolenoidAccess::Rejected;
  view.state = module_protocol::kSolenoidStateOff;
  if (index >= _channelCount)
  {
    return view;
  }
  if (!_desired[index])
  {
    view.access = SolenoidAccess::Ok;
    return view;
  }
  if (_holding[index])
  {
    view.access = SolenoidAccess::Ok;
    view.state = module_protocol::kSolenoidStateOn;
    return view;
  }
  view.access = SolenoidAccess::Busy;
  return view;
}
