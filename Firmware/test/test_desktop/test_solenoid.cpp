#include <unity.h>

#include "ModuleProtocol.h"
#include "SolenoidBank.h"
#include "fakes/FakeSolenoidPort.h"

namespace
{

const uint8_t kOn = module_protocol::kSolenoidStateOn;
const uint8_t kOff = module_protocol::kSolenoidStateOff;

/**
 * One bank and the fake port it drives. Boot is the first update.
 */
struct Rig
{
  FakeSolenoidPort port;
  SolenoidConfig config;
  SolenoidBank bank;

  /**
   * Builds a four-channel bank. capacitors 0 closes the hold only.
   *
   * @param capacitors How many capacitors the pulse uses.
   * @param chargeMs Idle time required before a pulse.
   * @param pulseMs How long the pulse tie stays on.
   * @param silenceMs Window after the last accepted set.
   */
  explicit Rig(uint8_t capacitors = 2, uint32_t chargeMs = 500,
               uint32_t pulseMs = 100, uint32_t silenceMs = 60000)
    : config{4, capacitors, chargeMs, pulseMs, silenceMs},
      bank(port, config)
  {
  }
};

/**
 * Checks one recorded edge.
 *
 * @param port Fake that recorded the edge.
 * @param at Position in the change list.
 * @param kind Which switch moved.
 * @param index Channel or capacitor. 0 for the rail and the tie.
 * @param on New level.
 * @return Nothing.
 */
void expectChange(const FakeSolenoidPort& port, uint8_t at,
                  SolenoidChangeKind kind, uint8_t index, bool on)
{
  TEST_ASSERT_TRUE(at < port.changeCount);
  TEST_ASSERT_TRUE(port.changes[at].kind == kind);
  TEST_ASSERT_EQUAL_UINT8(index, port.changes[at].index);
  TEST_ASSERT_TRUE(port.changes[at].on == on);
}

/**
 * Checks that the pulse switches are off.
 *
 * @param port Fake to read.
 * @return Nothing.
 */
void expectPulseIdle(const FakeSolenoidPort& port)
{
  TEST_ASSERT_FALSE(port.pulseTie);
  TEST_ASSERT_FALSE(port.capacitor[0]);
  TEST_ASSERT_FALSE(port.capacitor[1]);
  TEST_ASSERT_FALSE(port.capacitor[2]);
  for (uint8_t index = 0; index < FakeSolenoidPort::kChannels; ++index)
  {
    TEST_ASSERT_FALSE(port.pulseSelect[index]);
  }
}

/**
 * Checks an Ok view.
 *
 * @param view Result from the bank.
 * @param state Expected wire state.
 * @return Nothing.
 */
void expectOk(SolenoidView view, uint8_t state)
{
  TEST_ASSERT_TRUE(view.access == SolenoidAccess::Ok);
  TEST_ASSERT_EQUAL_UINT8(state, view.state);
}

/**
 * Turns one channel on from a fresh rig and returns the time at
 * which the hold is committed. The charge clock starts at 0.
 *
 * @param rig Bank under test.
 * @param index Channel to hold.
 * @return Milliseconds when the pulse has cleaned up.
 */
uint32_t holdChannel(Rig& rig, uint8_t index)
{
  rig.bank.update(0);
  TEST_ASSERT_TRUE(rig.bank.setDesired(index, kOn).access ==
                   SolenoidAccess::Busy);
  rig.bank.update(0);
  rig.bank.update(rig.config.chargeMs);
  const uint32_t held = rig.config.chargeMs + rig.config.pulseMs;
  rig.bank.update(held);
  expectOk(rig.bank.state(index), kOn);
  return held;
}

}

/**
 * A turn-on waits out the charge time with the capacitors off, then
 * selects them, selects the channel, and fires. The tie drops after
 * the pulse width. The hold stays on. The channel is Busy while the
 * hold pin is already on, and Ok once the pulse switches are off.
 *
 * @return Nothing.
 */
void testTurnOnPulsesThenHolds()
{
  Rig rig;
  rig.bank.update(0);
  rig.port.clearChanges();
  TEST_ASSERT_TRUE(rig.bank.setDesired(0, kOn).access == SolenoidAccess::Busy);

  rig.bank.update(499);
  TEST_ASSERT_EQUAL_UINT8(0, rig.port.changeCount);
  expectPulseIdle(rig.port);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
  TEST_ASSERT_TRUE(rig.bank.state(0).access == SolenoidAccess::Busy);

  rig.bank.update(500);
  TEST_ASSERT_EQUAL_UINT8(5, rig.port.changeCount);
  expectChange(rig.port, 0, SolenoidChangeKind::Capacitor, 0, true);
  expectChange(rig.port, 1, SolenoidChangeKind::Capacitor, 1, true);
  expectChange(rig.port, 2, SolenoidChangeKind::PulseSelect, 0, true);
  expectChange(rig.port, 3, SolenoidChangeKind::PulseTie, 0, true);
  expectChange(rig.port, 4, SolenoidChangeKind::Hold, 0, true);
  TEST_ASSERT_TRUE(rig.bank.state(0).access == SolenoidAccess::Busy);
  TEST_ASSERT_EQUAL_UINT8(4, rig.bank.count());

  rig.port.clearChanges();
  rig.bank.update(599);
  TEST_ASSERT_EQUAL_UINT8(0, rig.port.changeCount);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  TEST_ASSERT_TRUE(rig.port.hold[0]);

  rig.bank.update(600);
  TEST_ASSERT_EQUAL_UINT8(4, rig.port.changeCount);
  expectChange(rig.port, 0, SolenoidChangeKind::PulseTie, 0, false);
  expectChange(rig.port, 1, SolenoidChangeKind::PulseSelect, 0, false);
  expectChange(rig.port, 2, SolenoidChangeKind::Capacitor, 0, false);
  expectChange(rig.port, 3, SolenoidChangeKind::Capacitor, 1, false);
  TEST_ASSERT_TRUE(rig.port.hold[0]);
  expectOk(rig.bank.state(0), kOn);
  TEST_ASSERT_TRUE(rig.port.holdRail);
}

/**
 * The second channel waits until the first pulse releases the bank,
 * then waits out its own charge time.
 *
 * @return Nothing.
 */
void testSecondChannelWaitsForItsOwnCharge()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.setDesired(0, kOn);
  rig.bank.setDesired(1, kOn);
  rig.bank.update(500);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[0]);
  TEST_ASSERT_FALSE(rig.port.pulseSelect[1]);
  TEST_ASSERT_TRUE(rig.bank.state(1).access == SolenoidAccess::Busy);

  rig.bank.update(600);
  expectOk(rig.bank.state(0), kOn);
  TEST_ASSERT_TRUE(rig.port.hold[0]);
  expectPulseIdle(rig.port);

  rig.port.clearChanges();
  rig.bank.update(1099);
  TEST_ASSERT_EQUAL_UINT8(0, rig.port.changeCount);
  TEST_ASSERT_FALSE(rig.port.hold[1]);

  rig.bank.update(1100);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[1]);
  TEST_ASSERT_TRUE(rig.port.hold[1]);
  TEST_ASSERT_FALSE(rig.port.pulseSelect[0]);
  TEST_ASSERT_TRUE(rig.port.hold[0]);

  rig.bank.update(1200);
  expectOk(rig.bank.state(1), kOn);
  expectPulseIdle(rig.port);
}

/**
 * A command that arrives after the bank has already been charging
 * selects the capacitors on that step.
 *
 * @return Nothing.
 */
void testChargeAlreadyElapsedFiresWithoutAnotherWait()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.update(500);
  rig.port.clearChanges();
  rig.bank.setDesired(0, kOn);
  rig.bank.update(500);
  expectChange(rig.port, 0, SolenoidChangeKind::Capacitor, 0, true);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  TEST_ASSERT_TRUE(rig.port.hold[0]);
}

/**
 * A later on for a channel that is already holding does not pulse.
 *
 * @return Nothing.
 */
void testSetOnWhileHoldingDoesNotPulse()
{
  Rig rig;
  holdChannel(rig, 0);
  rig.port.clearChanges();
  expectOk(rig.bank.setDesired(0, kOn), kOn);
  rig.bank.update(700);
  TEST_ASSERT_EQUAL_UINT8(0, rig.port.changeCount);
  expectPulseIdle(rig.port);
  TEST_ASSERT_TRUE(rig.port.hold[0]);
}

/**
 * Off during a pulse replies off, then the next update releases the
 * bank and the hold and stamps that release. The next turn-on waits
 * the charge time from the stamp.
 *
 * @return Nothing.
 */
void testSetOffMidPulseReleasesAndStamps()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.setDesired(0, kOn);
  rig.bank.update(500);
  TEST_ASSERT_TRUE(rig.port.pulseTie);

  expectOk(rig.bank.setDesired(0, kOff), kOff);
  expectOk(rig.bank.state(0), kOff);
  TEST_ASSERT_TRUE(rig.port.pulseTie);

  rig.port.clearChanges();
  rig.bank.update(500);
  TEST_ASSERT_EQUAL_UINT8(5, rig.port.changeCount);
  expectChange(rig.port, 0, SolenoidChangeKind::PulseTie, 0, false);
  expectChange(rig.port, 1, SolenoidChangeKind::PulseSelect, 0, false);
  expectChange(rig.port, 2, SolenoidChangeKind::Capacitor, 0, false);
  expectChange(rig.port, 3, SolenoidChangeKind::Capacitor, 1, false);
  expectChange(rig.port, 4, SolenoidChangeKind::Hold, 0, false);

  rig.bank.setDesired(1, kOn);
  rig.port.clearChanges();
  rig.bank.update(999);
  TEST_ASSERT_EQUAL_UINT8(0, rig.port.changeCount);
  rig.bank.update(1000);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[1]);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * Off for a channel that is holding, while another channel is
 * pulsing, drops only that hold.
 *
 * @return Nothing.
 */
void testSetOffOfHoldingChannelDuringAnotherPulse()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.setDesired(0, kOn);
  rig.bank.update(500);
  rig.bank.update(600);
  expectOk(rig.bank.state(0), kOn);

  rig.bank.setDesired(1, kOn);
  rig.bank.update(1100);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[1]);
  TEST_ASSERT_TRUE(rig.port.hold[0]);

  expectOk(rig.bank.setDesired(0, kOff), kOff);
  rig.port.clearChanges();
  rig.bank.update(1100);
  TEST_ASSERT_EQUAL_UINT8(1, rig.port.changeCount);
  expectChange(rig.port, 0, SolenoidChangeKind::Hold, 0, false);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  TEST_ASSERT_TRUE(rig.port.hold[1]);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[1]);

  rig.bank.update(1200);
  expectOk(rig.bank.state(1), kOn);
  expectOk(rig.bank.state(0), kOff);
  expectPulseIdle(rig.port);
}

/**
 * No capacitors closes the hold on the next update and leaves the
 * pulse switches off.
 *
 * @return Nothing.
 */
void testNoCapacitorsCloseHoldOnly()
{
  Rig rig(0);
  rig.bank.update(0);
  rig.port.clearChanges();
  TEST_ASSERT_TRUE(rig.bank.setDesired(0, kOn).access == SolenoidAccess::Busy);
  rig.bank.update(0);
  TEST_ASSERT_EQUAL_UINT8(1, rig.port.changeCount);
  expectChange(rig.port, 0, SolenoidChangeKind::Hold, 0, true);
  expectPulseIdle(rig.port);
  expectOk(rig.bank.state(0), kOn);
}

/**
 * Two capacitors never drive the third switch.
 *
 * @return Nothing.
 */
void testThirdCapacitorStaysOff()
{
  Rig rig;
  holdChannel(rig, 0);
  TEST_ASSERT_FALSE(rig.port.capacitor[2]);
  for (uint8_t index = 0; index < rig.port.changeCount; ++index)
  {
    if (rig.port.changes[index].kind == SolenoidChangeKind::Capacitor)
    {
      TEST_ASSERT_TRUE(rig.port.changes[index].index != 2);
    }
  }
}

/**
 * A rejected set does not move the silence deadline.
 *
 * @return Nothing.
 */
void testRejectedSetDoesNotRestartSilence()
{
  Rig rig(2, 500, 100, 1000);
  holdChannel(rig, 0);
  TEST_ASSERT_TRUE(rig.bank.setDesired(0, 2).access == SolenoidAccess::Rejected);
  TEST_ASSERT_TRUE(rig.bank.setDesired(4, kOn).access == SolenoidAccess::Rejected);
  rig.bank.update(999);
  expectOk(rig.bank.state(0), kOn);
  rig.bank.update(1000);
  expectOk(rig.bank.state(0), kOff);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * A state read does not move the silence deadline.
 *
 * @return Nothing.
 */
void testReadDoesNotRestartSilence()
{
  Rig rig(2, 500, 100, 1000);
  holdChannel(rig, 0);
  expectOk(rig.bank.state(0), kOn);
  rig.bank.update(999);
  expectOk(rig.bank.state(0), kOn);
  rig.bank.update(1000);
  expectOk(rig.bank.state(0), kOff);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * An accepted set moves the silence deadline, including on for a
 * channel that is already holding.
 *
 * @return Nothing.
 */
void testAcceptedSetRestartsSilence()
{
  Rig rig(2, 500, 100, 1000);
  holdChannel(rig, 0);
  expectOk(rig.bank.setDesired(0, kOn), kOn);
  rig.bank.update(700);
  rig.bank.update(1000);
  expectOk(rig.bank.state(0), kOn);
  TEST_ASSERT_TRUE(rig.port.hold[0]);
  rig.bank.update(1699);
  expectOk(rig.bank.state(0), kOn);
  rig.bank.update(1700);
  expectOk(rig.bank.state(0), kOff);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * Silence during a pulse releases the bank and the hold.
 *
 * @return Nothing.
 */
void testSilenceAbortsAPulse()
{
  Rig rig(2, 500, 5000, 800);
  rig.bank.update(0);
  rig.bank.setDesired(0, kOn);
  rig.bank.update(0);
  rig.bank.update(500);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  TEST_ASSERT_TRUE(rig.port.hold[0]);

  rig.bank.update(800);
  expectPulseIdle(rig.port);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
  expectOk(rig.bank.state(0), kOff);
}

/**
 * The charge wait and the silence window both use unsigned time.
 *
 * @return Nothing.
 */
void testTimesWrap()
{
  const uint32_t boot = 0xFFFFFE00UL;
  Rig rig(2, 0x200, 0x10, 0x300);
  rig.bank.update(boot);
  rig.bank.setDesired(0, kOn);
  rig.bank.update(boot);
  rig.bank.update(0xFFFFFFFFUL);
  expectPulseIdle(rig.port);
  TEST_ASSERT_FALSE(rig.port.hold[0]);

  rig.bank.update(0);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  TEST_ASSERT_TRUE(rig.port.hold[0]);

  rig.bank.update(0x0F);
  TEST_ASSERT_TRUE(rig.port.pulseTie);
  rig.bank.update(0x10);
  expectOk(rig.bank.state(0), kOn);
  expectPulseIdle(rig.port);

  rig.bank.update(0xFF);
  expectOk(rig.bank.state(0), kOn);
  rig.bank.update(0x100);
  expectOk(rig.bank.state(0), kOff);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * Off for the channel that is pulsing lets the queued channel run
 * after the charge time, and the first channel stays off.
 *
 * @return Nothing.
 */
void testOffOfTheActiveChannelLetsTheNextRun()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.setDesired(0, kOn);
  rig.bank.setDesired(1, kOn);
  rig.bank.update(500);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[0]);

  expectOk(rig.bank.setDesired(0, kOff), kOff);
  rig.bank.update(500);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
  expectPulseIdle(rig.port);
  TEST_ASSERT_TRUE(rig.bank.state(1).access == SolenoidAccess::Busy);

  rig.bank.update(999);
  TEST_ASSERT_FALSE(rig.port.pulseSelect[1]);
  rig.bank.update(1000);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[1]);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
  rig.bank.update(1100);
  expectOk(rig.bank.state(1), kOn);
  expectOk(rig.bank.state(0), kOff);
}

/**
 * With no command, the rail comes on and every channel stays off
 * across the silence window.
 *
 * @return Nothing.
 */
void testBootSilenceLeavesHoldsOff()
{
  Rig rig(2, 500, 100, 1000);
  rig.bank.update(0);
  TEST_ASSERT_TRUE(rig.port.holdRail);
  expectPulseIdle(rig.port);
  for (uint8_t index = 0; index < 4; ++index)
  {
    TEST_ASSERT_FALSE(rig.port.hold[index]);
    expectOk(rig.bank.state(index), kOff);
  }
  rig.bank.update(1000);
  TEST_ASSERT_TRUE(rig.port.holdRail);
  TEST_ASSERT_FALSE(rig.port.hold[0]);
}

/**
 * A channel that is already holding keeps reporting on while a
 * later channel is still pulsing.
 *
 * @return Nothing.
 */
void testQuietChannelReportsItsHoldDuringAPulse()
{
  Rig rig;
  rig.bank.update(0);
  rig.bank.setDesired(1, kOn);
  rig.bank.update(500);
  rig.bank.update(600);
  expectOk(rig.bank.state(1), kOn);

  rig.bank.setDesired(0, kOn);
  rig.bank.update(1100);
  TEST_ASSERT_TRUE(rig.port.pulseSelect[0]);
  TEST_ASSERT_TRUE(rig.bank.state(0).access == SolenoidAccess::Busy);
  expectOk(rig.bank.state(1), kOn);
  expectOk(rig.bank.state(2), kOff);
  TEST_ASSERT_TRUE(rig.port.hold[1]);
  TEST_ASSERT_EQUAL_UINT8(4, rig.bank.count());
}

/**
 * Runs the solenoid-bank cases.
 *
 * @return Nothing.
 */
void runSolenoidTests()
{
  RUN_TEST(testTurnOnPulsesThenHolds);
  RUN_TEST(testSecondChannelWaitsForItsOwnCharge);
  RUN_TEST(testChargeAlreadyElapsedFiresWithoutAnotherWait);
  RUN_TEST(testSetOnWhileHoldingDoesNotPulse);
  RUN_TEST(testSetOffMidPulseReleasesAndStamps);
  RUN_TEST(testSetOffOfHoldingChannelDuringAnotherPulse);
  RUN_TEST(testNoCapacitorsCloseHoldOnly);
  RUN_TEST(testThirdCapacitorStaysOff);
  RUN_TEST(testRejectedSetDoesNotRestartSilence);
  RUN_TEST(testReadDoesNotRestartSilence);
  RUN_TEST(testAcceptedSetRestartsSilence);
  RUN_TEST(testSilenceAbortsAPulse);
  RUN_TEST(testTimesWrap);
  RUN_TEST(testOffOfTheActiveChannelLetsTheNextRun);
  RUN_TEST(testBootSilenceLeavesHoldsOff);
  RUN_TEST(testQuietChannelReportsItsHoldDuringAPulse);
}
