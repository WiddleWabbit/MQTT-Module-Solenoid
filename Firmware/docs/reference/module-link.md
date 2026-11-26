# Module link reference

[Home](../home.md) · Guide: [Module link](../module-link.md)

## Purpose

Slave side of the module protocol for one solenoid module. It accepts
an address while MOD is low, then identifies as the type and firmware
version it was given, with protocol version 1.

Frame layout, CRC-8/SMBus, and command bytes are in
`lib/Interfaces/ModuleProtocol.h`. If this page and that header
disagree, the header wins.

## Classes

`ModuleLink(IModuleSlavePort& port, uint16_t typeId, uint16_t firmwareVersion)`
binds the port and registers the master-write handler. The slave stays
off. Copy and assignment are deleted.

`update(bool modIsLow)` is the call `loop` makes.

The port methods the link uses are `enable`, `disable`, `setAddress`,
`setReply`, and `setMasterWriteHandler`. `setReply` copies the bytes
before it returns.

## States

`kUnconfiguredAddress` is `0x0A`.

```text
boot, MOD high          slave off; stored address is 0x0A
MOD low                 enable 0x0A once
MOD low again           no second enable
MOD high, no address    disable once
MOD low after that      enable 0x0A again
SET_ADDRESS accepted    setAddress(new); stay enabled; assigned
MOD high after that     stay enabled at the new address
```

## Sequences

A master write is checked in this order:

1. A null buffer, or fewer than `kMinFrameBytes` (3), replies `BadLength`.
2. The length byte must be from 2 through `2 + kMaxPayload`, and the buffer size must be that length plus one. Otherwise `BadLength`.
3. CRC-8/SMBus over the bytes the length covers must match the CRC byte. Otherwise `BadCrc`, and the address stays as it is.
4. The command runs.

A reply is `kMaxFrameBytes` (19) long. The real frame comes first. The
rest is `0xFF`. `SET_ADDRESS`, accepted or refused as an address, queues
no reply.

On the AVR the handler runs from the TWI interrupt. On the desktop,
`FakeModuleSlavePort::deliver` calls it. It finishes inside
`kClockStretchMaxMs`.

## Commands

| Command | When | Result |
| --- | --- | --- |
| `PING` | empty payload | status `Ok`, empty payload. Worked reply starts `02 00 2A` |
| `PING` | any payload | `BadLength` |
| `GET_IDENTITY` | empty payload | type, protocol 1, firmware version |
| `GET_IDENTITY` | any payload | `BadLength` |
| `SET_ADDRESS` | enabled, not yet assigned, one payload byte, address `0x10`–`0x6F` | `setAddress`, no reply. Slot 1 is `03 03 10 F2` |
| `SET_ADDRESS` | payload length is not 1 | `BadLength`, no commit |
| `SET_ADDRESS` | disabled, already assigned, or address outside `0x10`–`0x6F` | no reply, no commit |
| any other command | includes `ECHO`, `GET_FAULT`, `GET_SOLENOID_COUNT`, `GET_SOLENOID_STATE`, and `SET_SOLENOID` | `UnknownCmd` |

Identity payload, big-endian: type high, type low, `kProtocolVersion`,
firmware high, firmware low. `kIdentityPayloadLen` is 5. The sketch
passes board revision 1 and firmware revision 1, packed as `0x0101`.
For `kTypeSolenoidModule` and protocol 1 the worked frame is
`07 00 01 00 01 01 01 08`. The desktop identity test uses firmware
`0x0001`, frame `07 00 01 00 01 00 01 1D`.

`SET_ADDRESS` commits from the stop callback, before the motherboard
releases MOD. A later `SET_ADDRESS` is ignored. `0x00` is not an
assignable address. `isAssignableAddress` accepts `kMinAssignedAddress`
through `kMaxAssignedAddress`.

## Errors

| Status | Constant | When |
| --- | --- | --- |
| `Ok` | `kStatusOk` (`0x00`) | PING or identity |
| `BadCrc` | `kStatusBadCrc` (`0x01`) | CRC byte does not match |
| `UnknownCmd` | `kStatusUnknownCmd` (`0x02`) | Command this type does not implement |
| `BadLength` | `kStatusBadLength` (`0x03`) | Short frame, length mismatch, or a payload length the command rejects |

A refused `SET_ADDRESS` leaves the address in place and queues no
status. The header also defines `Busy` and `Unsupported`. This link
does not send them.

## Configuration

`typeId` and `firmwareVersion` are constructor arguments. The sketch
passes `MODULE_TYPE` and `FIRMWARE_VERSION`, listed on the
[architecture reference](architecture.md). `GET_IDENTITY` reports the
values it was given.

## Tests

`test/test_desktop/test_module_link.cpp` drives `FakeModuleSlavePort`.

| Test | Path |
| --- | --- |
| `testCrc8SmbusCheckValue` | ASCII `123456789` returns `0xF4` |
| `testBootWithModHighLeavesSlaveDisabled` | MOD high at boot leaves the slave off |
| `testModLowEnablesUnconfiguredAddressAndModHighDisables` | `0x0A` follows MOD until assignment. A second call at the same level does not enable or disable again. MOD low after a disable enables `0x0A` again |
| `testPingReplyMatchesWorkedFrame` | `02 00 2A` and 19-byte `0xFF` pad |
| `testIdentityReportsSolenoidModule` | Type `0x0100`, protocol 1, firmware 1, CRC `0x1D` |
| `testSuppliedFirmwareVersion` | Identity uses the constructor values (type `0x00AB`, firmware `0x1234`) |
| `testSetAddressCommitsAndSurvivesModHigh` | Request `03 03 10 F2`. A write while off does not commit. Then commit `0x10`, ignore `0x11`, stay on after MOD rises, and still answer PING |
| `testSetAddressAcceptsTopOfRange` | `0x6F` commits and queues no reply |
| `testSetAddressRejectsBadCrcAndIllegalAddresses` | Bad CRC, `0x00`, `0x0A`, and `0x70` stay at `0x0A` |
| `testUnknownCommandAndMalformedFrames` | `ECHO`, `GET_FAULT`, `GET_SOLENOID_COUNT`, `GET_SOLENOID_STATE`, `SET_SOLENOID`, a null buffer, a truncated frame, an extra byte, a bad CRC, PING with a payload, empty `SET_ADDRESS` |
| `testEnumerationSequence` | PING, assign `0x10`, MOD high, PING, identity |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/ModuleLink.h`
- `lib/Logic/ModuleLink.cpp`
- `test/test_desktop/test_module_link.cpp`
- `test/test_desktop/fakes/FakeModuleSlavePort.h`
