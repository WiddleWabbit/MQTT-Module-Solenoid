# Slave port reference

[Home](../home.md) · Guide: [Slave port](../slave-port.md)

## Purpose

One slave port, two adapters. The link enables TWI, commits an address,
and queues a reply. `AvrTwi0Slave` does that on TWI0.
`FakeModuleSlavePort` records the same calls.

## Classes

`IModuleSlavePort`:

| Method | Role |
| --- | --- |
| `enable(address7bit)` | Start listening at a 7-bit address |
| `disable()` | Stop acknowledging |
| `setAddress(address7bit)` | Commit a new address without leaving the bus |
| `setReply(frame, length)` | Copy the next reply. The pointer is valid only for this call |
| `setMasterWriteHandler(handler, context)` | One handler. A later call replaces it |

`AvrTwi0Slave` implements that on `Wire`. `FakeModuleSlavePort`
records `enable`, `disable`, `setAddress`, and `setReply`. `deliver`
invokes the registered handler.

## States

The driver starts disabled. `enable` selects this instance, clears the
reply length, calls `Wire.begin(address7bit)`, and registers
`onReceive` and `onRequest`. `disable` calls `Wire.end()` and clears
the enabled flag. `setAddress` writes `TWAR` only while enabled.

## Sequences

`setAddress` writes `TWAR` as `address7bit << 1`. Bit 0 stays clear,
so general call stays off. The write is safe from the stop callback.
It does not call `Wire.end()`.

`Wire.begin` enables the internal pull-ups on PC4 and PC5.
`disable` stops TWI0. The pins return to inputs with those pull-ups
off. The motherboard's 4.7 kΩ resistors hold the bus.

MiniCore delivers a master write from the STOP or repeated-start
status, after it has released the bus. The handler stores the reply
before it returns. The request callback runs when the read address
matches and clocks that copy out. A reply length of 0 sends nothing.

PC4 is SDA and PC5 is SCL. That is the TWI0 peripheral. MiniCore
numbers those pins 18 and 19. `Wire.begin()` connects that peripheral.
`SDA_PIN` and `SCL_PIN` in `main.cpp` name those pins. `Wire` takes no
pin arguments.

## Errors

`setReply` with a null frame stores a length of 0, so the next read
sends nothing. A length above `kMaxFrameBytes` is stored as 19 bytes.

`setAddress` while disabled does not write `TWAR`.

`_onReceive` passes the handler the number of bytes read from `Wire`.
When `Wire`'s count is larger than that, the count is the length
instead. Only the first `kMaxFrameBytes` are stored. The link then
applies its length check to that buffer.

The handler runs in the TWI interrupt and finishes inside
`kClockStretchMaxMs` (40 ms).

## Configuration

The driver has no product settings. The address comes from the link:
`kUnconfiguredAddress`, then the assigned address. Pin constants are
on the [architecture reference](architecture.md).

MiniCore's TWI buffer is 32 bytes. The host reads 19.

## Tests

There is no separate driver test. The module-link tests drive
`FakeModuleSlavePort`. The cases are on the
[module link reference](module-link.md). The interrupt path is not
executed on the desktop. `pio run -e Upload_ISP` compiles
`AvrTwi0Slave`.

## Source

- `lib/Interfaces/IModuleSlavePort.h`
- `lib/Drivers/AvrTwi0Slave.h`
- `lib/Drivers/AvrTwi0Slave.cpp`
- `test/test_desktop/fakes/FakeModuleSlavePort.h`
