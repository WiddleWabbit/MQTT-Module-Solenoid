#pragma once

#include <stdint.h>

#include "IModuleSlavePort.h"
#include "ModuleProtocol.h"

/**
 * TWI0 slave on PC4 (SDA) and PC5 (SCL).
 *
 * Those are the silicon TWI0 pins. MiniCore numbers them 18 and 19.
 * This PCB wires them to the motherboard header. Wire enables that
 * peripheral and takes no pin arguments.
 * setAddress writes TWAR directly so the new address is committed
 * from the stop callback without resetting the bus. General call stays
 * off because bit 0 of TWAR stays clear.
 */
class AvrTwi0Slave : public IModuleSlavePort
{
public:
  /**
   * Creates a disabled slave. The link registers the write handler
   * before MOD can go low.
   */
  AvrTwi0Slave();

  /**
   * Starts Wire at address7bit and registers the receive and request
   * callbacks.
   *
   * @param address7bit 7-bit address to acknowledge.
   * @return Nothing.
   */
  void enable(uint8_t address7bit) override;

  /**
   * Stops TWI0. The pins return to inputs with pull-ups off, so the
   * motherboard's 4.7 kΩ resistors hold the bus.
   *
   * @return Nothing.
   */
  void disable() override;

  /**
   * Writes TWAR. Safe from the stop callback.
   *
   * @param address7bit Assigned 7-bit address.
   * @return Nothing.
   */
  void setAddress(uint8_t address7bit) override;

  /**
   * Copies the reply sent on the next host read.
   *
   * @param frame Reply bytes, including 0xFF pad.
   * @param length Number of bytes in frame, at most 19.
   * @return Nothing.
   */
  void setReply(const uint8_t* frame, uint8_t length) override;

  /**
   * Stores the single protocol handler. Wire's callbacks are plain
   * function pointers, so one driver instance is active.
   *
   * @param handler Function called with one master write.
   * @param context Pointer passed back to handler on each write.
   * @return Nothing.
   */
  void setMasterWriteHandler(MasterWriteFn handler, void* context) override;

private:
  /**
   * Reads the write and calls the registered handler. Runs from the
   * TWI interrupt.
   *
   * @param count Bytes Wire reported.
   * @return Nothing.
   */
  static void _onReceive(int count);

  /**
   * Clocks out the reply stored by setReply. Runs from the TWI
   * interrupt after a repeated start.
   *
   * @return Nothing.
   */
  static void _onRequest();

  static AvrTwi0Slave* _instance;
  MasterWriteFn _writeHandler;
  void* _writeContext;
  uint8_t _reply[module_protocol::kMaxFrameBytes];
  uint8_t _replyLength;
  bool _enabled;
};
