#include "AvrTwi0Slave.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>
#include <Wire.h>
#include <avr/io.h>

AvrTwi0Slave* AvrTwi0Slave::_instance = nullptr;


// ========== Construction ==========

/**
 * Creates a disabled slave. The link registers the write handler
 * before MOD can go low.
 */
AvrTwi0Slave::AvrTwi0Slave()
  : _writeHandler(nullptr),
    _writeContext(nullptr),
    _replyLength(0),
    _enabled(false)
{
  for (uint8_t index = 0; index < module_protocol::kMaxFrameBytes; ++index)
  {
    _reply[index] = 0xFF;
  }
}


// ========== Public API ==========

/**
 * Starts Wire at address7bit and registers the receive and request
 * callbacks.
 *
 * @param address7bit 7-bit address to acknowledge.
 * @return Nothing.
 */
void AvrTwi0Slave::enable(uint8_t address7bit)
{
  _instance = this;
  _replyLength = 0;
  Wire.begin(address7bit);
  Wire.onReceive(AvrTwi0Slave::_onReceive);
  Wire.onRequest(AvrTwi0Slave::_onRequest);
  _enabled = true;
}

/**
 * Stops TWI0. The pins return to inputs with pull-ups off, so the
 * motherboard's 4.7 kΩ resistors hold the bus.
 *
 * @return Nothing.
 */
void AvrTwi0Slave::disable()
{
  Wire.end();
  _enabled = false;
}

/**
 * Writes TWAR. Safe from the stop callback.
 *
 * @param address7bit Assigned 7-bit address.
 * @return Nothing.
 */
void AvrTwi0Slave::setAddress(uint8_t address7bit)
{
  if (!_enabled)
  {
    return;
  }
  TWAR = static_cast<uint8_t>(address7bit << 1);
}

/**
 * Copies the reply sent on the next host read.
 *
 * @param frame Reply bytes, including 0xFF pad.
 * @param length Number of bytes in frame, at most 19.
 * @return Nothing.
 */
void AvrTwi0Slave::setReply(const uint8_t* frame, uint8_t length)
{
  if (frame == nullptr)
  {
    _replyLength = 0;
    return;
  }

  uint8_t count = length;
  if (count > module_protocol::kMaxFrameBytes)
  {
    count = module_protocol::kMaxFrameBytes;
  }
  for (uint8_t index = 0; index < count; ++index)
  {
    _reply[index] = frame[index];
  }
  _replyLength = count;
}

/**
 * Stores the single protocol handler. Wire's callbacks are plain
 * function pointers, so one driver instance is active.
 *
 * @param handler Function called with one master write.
 * @param context Pointer passed back to handler on each write.
 * @return Nothing.
 */
void AvrTwi0Slave::setMasterWriteHandler(MasterWriteFn handler,
                                         void* context)
{
  _writeHandler = handler;
  _writeContext = context;
  _instance = this;
}


// ========== Wire callbacks ==========

/**
 * Reads the write and calls the registered handler. Runs from the
 * TWI interrupt.
 *
 * @param count Bytes Wire reported.
 * @return Nothing.
 */
void AvrTwi0Slave::_onReceive(int count)
{
  if (_instance == nullptr || _instance->_writeHandler == nullptr)
  {
    return;
  }

  uint8_t buffer[module_protocol::kMaxFrameBytes] = {};
  size_t length = 0;
  if (count < 0)
  {
    count = 0;
  }

  while (Wire.available() > 0)
  {
    const int value = Wire.read();
    if (length < module_protocol::kMaxFrameBytes)
    {
      buffer[length] = static_cast<uint8_t>(value);
    }
    length++;
  }
  if (static_cast<size_t>(count) > length)
  {
    length = static_cast<size_t>(count);
  }
  _instance->_writeHandler(buffer, length, _instance->_writeContext);
}

/**
 * Clocks out the reply stored by setReply. Runs from the TWI
 * interrupt after a repeated start.
 *
 * @return Nothing.
 */
void AvrTwi0Slave::_onRequest()
{
  if (_instance == nullptr || _instance->_replyLength == 0)
  {
    return;
  }
  Wire.write(_instance->_reply, _instance->_replyLength);
}

#endif
