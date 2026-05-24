#if defined(ARDUINO_ARCH_ESP32)
#include "backend_esp32.h"
#include <IPAddress.h>

bool Esp32WgBackend::begin(const WgConfig& c) {
  IPAddress local;
  if (!local.fromString(c.if_addr)) return false;
  _begun = _wg.begin(local, c.if_priv, c.peer_host, c.peer_pub, c.peer_port);
  return _begun;
}

void Esp32WgBackend::end() {
  if (_begun) { _wg.end(); _begun = false; }
}

bool Esp32WgBackend::isUp() {
  return _begun && _wg.is_initialized();
}
#endif
