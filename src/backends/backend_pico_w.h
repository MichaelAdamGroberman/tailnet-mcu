#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include "wg_backend.h"
#include <arduino-wireguard-pico-w.h>  // jaszczurtd/arduino-wireguard-pico-w

// REAL implementation — uses jaszczurtd/arduino-wireguard-pico-w 0.1.x
// API mirrors WireGuard-ESP32: begin(IP, privKey, host, pubKey, port) /
// is_initialized() / end().  peerUp() is used for handshake detection.
class PicoWWgBackend : public WgBackend {
public:
  bool begin(const WgConfig& c) override;
  void end() override;
  bool isUp() override;
private:
  WireGuard _wg;
  bool _begun = false;
};
#endif // ARDUINO_ARCH_RP2040
