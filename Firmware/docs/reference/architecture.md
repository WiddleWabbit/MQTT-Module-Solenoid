# Architecture reference

[Home](../home.md) · Guide: [Architecture](../architecture.md)

## Purpose

`src/main.cpp` wires this daughterboard. It owns the pins and the
product settings, constructs the adapters, the bank, and the link, and
calls the bank and the link once per pass.

## Classes

`setup` drives the solenoid paths off, drives `SNS_PIN`, and configures
`MOD_PIN`. `loop` steps the bank with interrupts off, then reads
`MOD_PIN` and calls `ModuleLink::update`.

The objects are `moduleSlave` (`AvrTwi0Slave`), `solenoidPort`
(`AvrSolenoidPort`), `solenoidBank` (`SolenoidBank`), and `moduleLink`
(`ModuleLink`).

## States

The sketch has no mode of its own. Pulse, hold, and silence are the
bank's states, on the [solenoid bank reference](solenoid-bank.md).
Listening, assignment, and replies are the link's states, on the
[module link reference](module-link.md).

## Sequences

1. Construction registers the master-write handler. The slave stays off. The bank does not touch the pins.
2. `setup` calls `solenoidPort.begin()`. That drives `S_EN_1_PIN` through `S_EN_4_PIN`, `P_EN_1_PIN` through `P_EN_4_PIN`, `C_S_1_PIN` through `C_S_3_PIN`, `PULSE_T_PIN`, and `S_PWR_EN_PIN` low, then sets each as an output. Those go low before SNS.
3. `setup` sets `S_PWR_PG_PIN` and `SNS_1_PIN` through `SNS_4_PIN` as inputs. The ISP header and the reset pin are left alone.
4. `setup` sets `SNS_PIN` as an output driven low and `MOD_PIN` as an input with pull-up.
5. Each `loop` calls `solenoidBank.update(millis())` with interrupts off, then reads `MOD_PIN` and calls the link. The calls return without waiting. The first update turns the hold rail on. A channel turns on only after `SET_SOLENOID`.

The host holds MOD low for `kModGateMaxMs` (5 ms). Because `loop` does
not block, the unconfigured address is enabled on the pass that sees
MOD low.

## Errors

Protocol status bytes come from the link. A handler that runs in the
TWI interrupt finishes inside `kClockStretchMaxMs` (40 ms). The pulse
runs in `update()`, outside that handler. Both limits are in
`ModuleProtocol.h`.

## Configuration

Declared in `src/main.cpp`. The pulse uses these times and counts. The
bank page names the fields and points here for the values.

| Constant | Value | Port | Role |
| --- | --- | --- | --- |
| `SDA_PIN` | 18 | PC4 | Motherboard SDA, TWI0. `Wire` takes no pin argument |
| `SCL_PIN` | 19 | PC5 | Motherboard SCL, TWI0. `Wire` takes no pin argument |
| `MOD_PIN` | 5 | PD5 | Enumeration select, input pull-up |
| `SNS_PIN` | 6 | PD6 | Seated detect, driven low |
| `S_EN_1_PIN` | 25 | PE2 | Channel 1 hold. Driven low in `begin()`, then by the bank |
| `S_EN_2_PIN` | 1 | PD1 | Channel 2 hold. Driven low in `begin()`, then by the bank |
| `S_EN_3_PIN` | 3 | PD3 | Channel 3 hold. Driven low in `begin()`, then by the bank |
| `S_EN_4_PIN` | 23 | PE0 | Channel 4 hold. Driven low in `begin()`, then by the bank |
| `P_EN_1_PIN` | 26 | PE3 | Channel 1 pulse select. Driven low in `begin()`, then by the bank |
| `P_EN_2_PIN` | 2 | PD2 | Channel 2 pulse select. Driven low in `begin()`, then by the bank |
| `P_EN_3_PIN` | 4 | PD4 | Channel 3 pulse select. Driven low in `begin()`, then by the bank |
| `P_EN_4_PIN` | 24 | PE1 | Channel 4 pulse select. Driven low in `begin()`, then by the bank |
| `S_PWR_EN_PIN` | 20 | PB6 | Hold-rail enable. Driven low in `begin()`. The first loop turns it on. A 10 kΩ pull-up holds the rail on until this pin is low |
| `S_PWR_PG_PIN` | 21 | PB7 | Hold-rail power-good. Input, unused |
| `SNS_1_PIN` | 14 | PC0 | Channel 1 current sense. Input, unused |
| `SNS_2_PIN` | 15 | PC1 | Channel 2 current sense. Input, unused |
| `SNS_3_PIN` | 16 | PC2 | Channel 3 current sense. Input, unused |
| `SNS_4_PIN` | 17 | PC3 | Channel 4 current sense. Input, unused |
| `C_S_1_PIN` | 9 | PB1 | Capacitor 1 into the bank. Driven low in `begin()`, then by the bank |
| `C_S_2_PIN` | 8 | PB0 | Capacitor 2 into the bank. Driven low in `begin()`, then by the bank |
| `C_S_3_PIN` | 7 | PD7 | Capacitor 3 into the bank. Driven low in `begin()`. Stays off at the fitted count |
| `PULSE_T_PIN` | 10 | PB2 | Bank tie to the pulse net. Driven low in `begin()`, then by the bank |
| `MOSI_PIN` | 11 | PB3 | ISP. Left alone |
| `MISO_PIN` | 12 | PB4 | ISP. Left alone |
| `SCK_PIN` | 13 | PB5 | ISP. Left alone |
| `MODULE_TYPE` | `kTypeSolenoidModule` | | Reported by `GET_IDENTITY` |
| `BOARD_REVISION` | 1 | | High byte of the `GET_IDENTITY` firmware word |
| `FIRMWARE_REVISION` | 2 | | Low byte of the `GET_IDENTITY` firmware word |
| `FIRMWARE_VERSION` | `0x0102` | | `BOARD_REVISION`, then `FIRMWARE_REVISION` |
| `SOLENOID_COUNT` | 4 | | Channels reported to the host |
| `CAPACITOR_COUNT` | 2 | | `C_S_1` and `C_S_2`. Zero would close the hold and skip the pulse |
| `CAP_CHARGE_MS` | 500 | | Idle time before the next pulse |
| `PULSE_MS` | 100 | | How long `PULSE_T` stays on |
| `COMMAND_SILENCE_MS` | 15 minutes | | `15UL * 60UL * 1000UL`. Off after the last accepted set |

`kTypeSolenoidModule` is `0x0100` in `ModuleProtocol.h`.

The image is an ATmega328PB at 8 MHz from the internal oscillator.
`Upload_ISP` programs it with stk500v1 at 19200 on COM3.
`fuses_bootloader` sets no bootloader, brown-out 2.7 V, and EEPROM
save. The avrdude lines are in the firmware `README.md`.

## Tests

Desktop cases and the command that runs them are on the
[solenoid bank reference](solenoid-bank.md) and the
[module link reference](module-link.md).

The image build is:

```text
pio run -e Upload_ISP
```

That environment does not run the desktop tests. The TWI interrupt
path is not executed on the desktop.

## Source

- `src/main.cpp`
- `platformio.ini`

Sources in a library root are compiled when one of its headers is
included. `platformio.ini` does not list `lib_deps` for that reason.
