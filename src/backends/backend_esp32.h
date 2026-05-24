#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include "wg_backend.h"
#include <WireGuard-ESP32.h>

class Esp32WgBackend : public WgBackend {
public:
  bool begin(const WgConfig& c) override;
  void end() override;
  bool isUp() override;
private:
  WireGuard _wg;
  bool _begun = false;
};
#endif
