# Solenoid bank reference

[Home](../home.md) · Guide: [Solenoid bank](../solenoid-bank.md)

## Purpose

Turn sprinkler channels on with a capacitor pulse and hold them from
the buck. Turn them off by dropping the hold. One pulse runs at a
time. The hold rail stays on after the first `update()`.

`count()`, `state()`, and `setDesired()` may run from the TWI
interrupt. `update()` runs from the main loop with interrupts off.

## Classes

`SolenoidBank(ISolenoidPort& port, const SolenoidConfig& config)`
copies the settings and does not touch the pins. Copy and assignment
are deleted.

`ISolenoidPort` is the seam. `AvrSolenoidPort` drives the board.
`FakeSolenoidPort` records levels and edges. High is the switch on.

| Method | Net in `main.cpp` |
| --- | --- |
| `setHoldRail` | `S_PWR_EN` |
| `setChannelHold` | `S_EN_1` .. `S_EN_4` |
| `setChannelPulse` | `P_EN_1` .. `P_EN_4` |
| `setCapacitor` | `C_S_1` .. `C_S_3` |
| `setPulseTie` | `PULSE_T` |

Wire index 0 is channel 1. The count command returns `channelCount`.
A reported state is `off` (0) or `on` (1). `disconnected` is not used.

## States

```text
off        desired clear. The hold is off after the next update.
waiting    desired on, charge time still missing. Pulse switches off.
pulsing    capacitors, P_EN, PULSE_T, and S_EN on for that channel.
holding    desired on, pulse switches off, S_EN left on.
```

`state()` and `setDesired()`:

| Condition | Access | Wire state |
| --- | --- | --- |
| index outside the count, or level other than 0 or 1 | `Rejected` | the link replies `BadLength` |
| desired off | `Ok` | `off` |
| desired on, not yet holding | `Busy` | the link replies `Busy` |
| holding | `Ok` | `on` |

`Busy` is returned as soon as the on is stored, including while
`S_EN` is already on and the pulse switches are still closing. The
host retries `Busy` only a few times, so the desired bit is kept and
the pulse still finishes. A later `GET_SOLENOID_STATE` reports the
result.

`SET` off is `Ok` and `off` before the pins move. The next `update()`
drops them.

## Sequences

A turn-on uses `C_S_1` upward for `capacitorCount`. Zero capacitors
closes `S_EN` on the next `update()` and does not pulse.

```text
wait until chargeMs has elapsed since boot or the last release
C_S_1 .. C_S_N on
P_EN of that channel on
PULSE_T on and S_EN on
wait pulseMs
PULSE_T off, P_EN off, capacitors off, S_EN stays on
```

There is no extra wait after the capacitors are selected. They charge
while they are off the bank.

The lowest waiting index goes first. The next channel waits until
this release, then for whatever charge time is still missing. `SET`
on for a channel that is already holding does not pulse again.

`SET` off of the pulsing channel releases `PULSE_T`, `P_EN`, the
capacitors, and `S_EN`, and stamps the release. `SET` off of a
different channel drops only that `S_EN`.

An accepted `SET` restarts the silence window, including one that
replies `Busy`, and including on for a channel that is already
holding. `GET`, `PING`, and `GET_IDENTITY` do not. A rejected index
or level does not. The window also starts on the first `update()`.
When it elapses, every desired bit clears, which aborts a pulse the
same way `SET` off does.

The motherboard is expected to send `SET_SOLENOID` about once a
minute while any output should stay on. This firmware does not send
that repeat.

## Errors

`Rejected` is a bad index or a level other than off or on. The link
maps it to `BadLength`. The desired bits and the silence window stay
as they were.

`Busy` is not a failure. The command was stored.

Silence is not reported. The holds go off, and the next read says
`off`.

## Configuration

The sketch owns the values. They are listed on the
[architecture reference](architecture.md).

| Field | Constant in `main.cpp` | Role |
| --- | --- | --- |
| `channelCount` | `SOLENOID_COUNT` | Count reported to the host |
| `capacitorCount` | `CAPACITOR_COUNT` | `C_S_1` upward. 0 means hold only |
| `chargeMs` | `CAP_CHARGE_MS` | Idle time before the next pulse |
| `pulseMs` | `PULSE_MS` | How long `PULSE_T` stays on |
| `commandSilenceMs` | `COMMAND_SILENCE_MS` | Off after the last accepted set |

`kCapacitorSlots` is 3, shared with `AvrSolenoidPort::kCapacitorCount`.

## Tests

`test/test_desktop/test_solenoid.cpp` drives `FakeSolenoidPort`.
`test/test_desktop/test_module_link.cpp` checks the framed replies.
`runSolenoidTests()` is called from the link file's `main()`.

| Test | Path |
| --- | --- |
| `testTurnOnPulsesThenHolds` | Charge wait with the bank off, then caps, `P_EN`, `PULSE_T` with `S_EN`, the width, then cleanup with `S_EN` left on. `Busy` while pulsing, then `Ok` `on` |
| `testSecondChannelWaitsForItsOwnCharge` | The second channel starts only after the first release plus its own charge time |
| `testChargeAlreadyElapsedFiresWithoutAnotherWait` | A late command selects the capacitors on that step |
| `testSetOnWhileHoldingDoesNotPulse` | A repeat on leaves the pulse switches off |
| `testSetOffMidPulseReleasesAndStamps` | Off replies `Ok` `off`, then releases the bank and the hold. The next channel waits from that stamp |
| `testSetOffOfHoldingChannelDuringAnotherPulse` | Off drops only that hold |
| `testNoCapacitorsCloseHoldOnly` | Capacitor count 0 closes `S_EN` and leaves the pulse switches off |
| `testThirdCapacitorStaysOff` | Two capacitors never drive `C_S_3` |
| `testRejectedSetDoesNotRestartSilence` | Level 2 and a bad index leave the deadline where it was |
| `testReadDoesNotRestartSilence` | `state()` does not move the deadline |
| `testAcceptedSetRestartsSilence` | On for a channel that is already holding moves the deadline |
| `testSilenceAbortsAPulse` | The deadline releases the bank and the hold |
| `testTimesWrap` | Charge and silence across the millisecond wrap |
| `testOffOfTheActiveChannelLetsTheNextRun` | Off for the pulsing channel lets the queued channel run, and the first stays off |
| `testBootSilenceLeavesHoldsOff` | No command: the rail comes on, the channels stay off |
| `testQuietChannelReportsItsHoldDuringAPulse` | A holding channel stays `on` while another is pulsing. A quiet channel stays `off` |
| `testSolenoidFramesAndRejects` | Count, off, `Busy`, and `BadLength` for a bad length, index, or level. A rejected set leaves the channel `Busy` |
| `testSolenoidCountWhilePulsing` | Count stays 4 during a pulse. A quiet channel is `off`. After cleanup the channel is `on`. Set off replies `off` before the pin drops |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/SolenoidBank.h`
- `lib/Logic/SolenoidBank.cpp`
- `lib/Interfaces/ISolenoidPort.h`
- `lib/Drivers/AvrSolenoidPort.h`
- `lib/Drivers/AvrSolenoidPort.cpp`
- `test/test_desktop/test_solenoid.cpp`
- `test/test_desktop/fakes/FakeSolenoidPort.h`
