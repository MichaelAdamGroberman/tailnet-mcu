#include "TailnetPeer.h"
#include <string.h>
#include <stdio.h>

// Board-specific default backend. On the native test host there is no real
// backend, so the default ctor leaves _backend null; tests inject a
// FakeBackend via the other ctor.
#if defined(ARDUINO_ARCH_ESP32)
#include "backends/backend_esp32.h"
static WgBackend* makeDefaultBackend() { return new Esp32WgBackend(); }
#elif defined(ARDUINO_ARCH_RP2040)
#include "backends/backend_pico_w.h"
static WgBackend* makeDefaultBackend() { return new PicoWWgBackend(); }
#else
static WgBackend* makeDefaultBackend() { return nullptr; }
#endif

TailnetPeer::TailnetPeer() : _backend(makeDefaultBackend()) {}
TailnetPeer::TailnetPeer(WgBackend* b) : _backend(b) {}

bool TailnetPeer::begin(const char* wgQuickConfig) {
  WgConfig cfg;
  if (!wgConfigParse(wgQuickConfig, &cfg)) {
    snprintf(_lastErr, sizeof(_lastErr), "config parse failed (missing field)");
    _state = FAILED;
    return false;
  }
  snprintf(_endpoint, sizeof(_endpoint), "%s:%u", cfg.peer_host, cfg.peer_port);
  if (!_backend || !_backend->begin(cfg)) {
    snprintf(_lastErr, sizeof(_lastErr), "backend begin failed");
    _state = FAILED;
    return false;
  }
  strncpy(_tunIp, cfg.if_addr, sizeof(_tunIp) - 1);
  _tunIp[sizeof(_tunIp) - 1] = 0;
  _lastErr[0] = 0;
  _state = STARTING;
  return true;
}

bool TailnetPeer::loadConfigFromText(const char* s) { return begin(s); }

void TailnetPeer::tick() {
  if (_state != STARTING && _state != UP) return;
  if (_backend && _backend->isUp()) _state = UP;
}

void TailnetPeer::stop() {
  if (_backend) _backend->end();
  _state = OFF;
  strncpy(_tunIp, "(off)", sizeof(_tunIp));
}
