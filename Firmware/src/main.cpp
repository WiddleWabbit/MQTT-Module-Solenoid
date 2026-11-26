#include <Arduino.h>


// ========== Board pins ==========

// Motherboard bus is this chip's TWI0. PC4 is SDA and PC5 is SCL.
// Wire owns these pins in hardware. PE0 and PE1 are solenoid pins.
const uint8_t SDA_PIN = 18;
const uint8_t SCL_PIN = 19;
const uint8_t MOD_PIN = 5;
const uint8_t SNS_PIN = 6;

// S_EN drives the NPN that turns on that channel's high-side PMOS.
// High turns the channel on.
const uint8_t S_EN_1_PIN = 25;
const uint8_t S_EN_2_PIN = 1;
const uint8_t S_EN_3_PIN = 3;
const uint8_t S_EN_4_PIN = 23;

// P_EN is the same high-side switch for that channel's pulse path.
const uint8_t P_EN_1_PIN = 26;
const uint8_t P_EN_2_PIN = 2;
const uint8_t P_EN_3_PIN = 4;
const uint8_t P_EN_4_PIN = 24;

// S_PWR_EN high enables the TPS564257. A 10k pull-up to 3.3 V holds
// the rail on until this pin drives low. S_PWR_PG is open-drain.
const uint8_t S_PWR_EN_PIN = 20;
const uint8_t S_PWR_PG_PIN = 21;

// Monitoring-sheet sense into the ADC. The INA186 outputs land here.
const uint8_t SNS_1_PIN = 14;
const uint8_t SNS_2_PIN = 15;
const uint8_t SNS_3_PIN = 16;
const uint8_t SNS_4_PIN = 17;

// Pulse-bank sense into the ADC. PULSE_T is an input to this sheet.
const uint8_t C_S_1_PIN = 9;
const uint8_t C_S_2_PIN = 8;
const uint8_t C_S_3_PIN = 7;
const uint8_t PULSE_T_PIN = 10;

// ISP header. PC6 stays the reset pin. PD0 (Arduino pin 0) is
// unconnected: the controller sheet has a stub wire and no net.
const uint8_t MOSI_PIN = 11;
const uint8_t MISO_PIN = 12;
const uint8_t SCK_PIN = 13;

static_assert(SDA_PIN == 18, "PC4 is motherboard SDA.");
static_assert(SCL_PIN == 19, "PC5 is motherboard SCL.");
static_assert(MOD_PIN == 5, "PD5 is MOD.");
static_assert(SNS_PIN == 6, "PD6 is SNS.");
static_assert(S_EN_1_PIN == 25, "PE2 is S_EN_1.");
static_assert(S_EN_2_PIN == 1, "PD1 is S_EN_2.");
static_assert(S_EN_3_PIN == 3, "PD3 is S_EN_3.");
static_assert(S_EN_4_PIN == 23, "PE0 is S_EN_4.");
static_assert(P_EN_1_PIN == 26, "PE3 is P_EN_1.");
static_assert(P_EN_2_PIN == 2, "PD2 is P_EN_2.");
static_assert(P_EN_3_PIN == 4, "PD4 is P_EN_3.");
static_assert(P_EN_4_PIN == 24, "PE1 is P_EN_4.");
static_assert(S_PWR_EN_PIN == 20, "PB6 is S_PWR_EN.");
static_assert(S_PWR_PG_PIN == 21, "PB7 is S_PWR_PG.");
static_assert(SNS_1_PIN == 14, "PC0 is SNS_1.");
static_assert(SNS_2_PIN == 15, "PC1 is SNS_2.");
static_assert(SNS_3_PIN == 16, "PC2 is SNS_3.");
static_assert(SNS_4_PIN == 17, "PC3 is SNS_4.");
static_assert(C_S_1_PIN == 9, "PB1 is C_S_1.");
static_assert(C_S_2_PIN == 8, "PB0 is C_S_2.");
static_assert(C_S_3_PIN == 7, "PD7 is C_S_3.");
static_assert(PULSE_T_PIN == 10, "PB2 is PULSE_T.");
static_assert(MOSI_PIN == 11, "PB3 is MOSI.");
static_assert(MISO_PIN == 12, "PB4 is MISO.");
static_assert(SCK_PIN == 13, "PB5 is SCK.");


// ========== Application ==========

/**
 * Holds every solenoid path off, then tells the motherboard this
 * module is seated. Enables are driven low before SNS, so the host
 * does not see the board until the high-side switches and the
 * solenoid rail are off. MOD is an input with pull-up. Sense pins
 * are inputs. The ISP header and reset pin are left alone.
 *
 * @return Nothing.
 */
void setup()
{
  const uint8_t offPins[] = {
    S_EN_1_PIN, S_EN_2_PIN, S_EN_3_PIN, S_EN_4_PIN,
    P_EN_1_PIN, P_EN_2_PIN, P_EN_3_PIN, P_EN_4_PIN,
    S_PWR_EN_PIN
  };
  const uint8_t offPinCount = sizeof(offPins) / sizeof(offPins[0]);
  for (uint8_t index = 0; index < offPinCount; index++) {
    digitalWrite(offPins[index], LOW);
    pinMode(offPins[index], OUTPUT);
  }

  const uint8_t sensePins[] = {
    S_PWR_PG_PIN, PULSE_T_PIN,
    SNS_1_PIN, SNS_2_PIN, SNS_3_PIN, SNS_4_PIN,
    C_S_1_PIN, C_S_2_PIN, C_S_3_PIN
  };
  const uint8_t sensePinCount = sizeof(sensePins) / sizeof(sensePins[0]);
  for (uint8_t index = 0; index < sensePinCount; index++) {
    pinMode(sensePins[index], INPUT);
  }

  pinMode(SNS_PIN, OUTPUT);
  digitalWrite(SNS_PIN, LOW);
  pinMode(MOD_PIN, INPUT_PULLUP);
}

/**
 * Idle pass. The link and the solenoid modules are not in this tree
 * yet, so nothing reads MOD and nothing turns a channel on. This
 * loop does not block.
 *
 * @return Nothing.
 */
void loop()
{
}
