#pragma once
#include "service_transport.h"

// Board-specific BLE UART (NUS) transport.
// One implementation compiles per board:
//   ESP32  -> NimBLE-Arduino 2.x  (src/transport/ble/ble_nimble_esp32.cpp)
//   Pico W -> BTstack             (added separately)
//
// The first line a connected client writes must equal the shared token
// (constant-time check via tokenEquals). After that, each line dispatches to
// the Handler and the returned String is sent back as a BLE notification.
// _authed resets on disconnect.
class BleServiceTransport : public ServiceTransport {
public:
  BleServiceTransport(const char* name, const char* token);
  bool begin() override;
  void tick() override; // no-op on NimBLE (callback-driven)

  // Called by board-specific BLE callbacks; not for end-user use.
  String dispatch(const String& line) {
    if (_handler) return _handler(line);
    return String();
  }
  bool hasHandler() const { return (bool)_handler; }

private:
  const char* _name;
  const char* _token;
  bool _authed = false;
};
