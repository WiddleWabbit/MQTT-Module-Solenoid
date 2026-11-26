# Slave port

[Home](home.md) · Reference: [Slave port](reference/slave-port.md)

## Summary

`IModuleSlavePort` is the seam between the link and the bus.
`AvrTwi0Slave` is the TWI0 adapter. Desktop tests use
`FakeModuleSlavePort`. The fake's `deliver` calls the same handler the
driver's receive callback calls.

## Where it sits

`main.cpp` constructs `AvrTwi0Slave` and passes it to `ModuleLink`. The
link calls `enable`, `disable`, `setAddress`, and `setReply`. The
driver calls the link back on a master write. Tests construct the fake
in place of the driver.

## What it creates

```text
IModuleSlavePort
  AvrTwi0Slave          Wire on TWI0
  FakeModuleSlavePort   records calls
```

Wire's callbacks are plain function pointers, so one driver instance
is the active slave. That instance attaches one protocol handler.

## One update

The sketch does not call the driver. One transaction is a master
write, then the repeated-start read.

```mermaid
flowchart TD
  rx["onReceive"] --> hand["write handler"]
  hand --> reply["setReply copies the frame"]
  reply --> tx["onRequest clocks the copy out"]
```

`setAddress` writes `TWAR` from the stop callback and leaves the bus
up. `disable` calls `Wire.end()`.

## What is stored, and who reads it

The driver keeps the handler, the reply bytes, and whether TWI0 is
enabled. The host clocks the reply out on the next read. `TWAR` is
the address the peripheral acknowledges. The fake keeps the same calls
so a test can read them.

`SDA_PIN` is PC4 and `SCL_PIN` is PC5. `Wire` takes no pin argument,
because TWI0's pins are fixed.

## See also

- [Slave port reference](reference/slave-port.md) — `TWAR`, pin names, what the fake records.
- [Module link](module-link.md)
- [Architecture](architecture.md)
