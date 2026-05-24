#if defined(ARDUINO_ARCH_RP2040)
#include "backend_pico_w.h"
#include <IPAddress.h>

// Spike result (2026-05-24):
//   `pio pkg search wireguard` found jaszczurtd/arduino-wireguard-pico-w 0.1.8 —
//   a direct port of WireGuard-ESP32 to RP2040/Pico W (earlephilhower core + lwIP).
//   API is identical to WireGuard-ESP32: WireGuard::begin(localIP, privKey, host,
//   pubKey, port) / end() / is_initialized() / peerUp().
//   This is a REAL integration; no stub required.

bool PicoWWgBackend::begin(const WgConfig& c) {
  IPAddress local;
  if (!local.fromString(c.if_addr)) {
    Serial.println("[wg] Pico W: invalid interface address");
    return false;
  }
  _begun = _wg.begin(local, c.if_priv, c.peer_host, c.peer_pub, c.peer_port);
  if (!_begun) {
    Serial.println("[wg] Pico W: WireGuard::begin() failed");
  }
  return _begun;
}

void PicoWWgBackend::end() {
  if (_begun) {
    _wg.end();
    _begun = false;
  }
}

bool PicoWWgBackend::isUp() {
  // peerUp() returns true when at least one handshake has completed.
  return _begun && _wg.peerUp();
}
#endif // ARDUINO_ARCH_RP2040
