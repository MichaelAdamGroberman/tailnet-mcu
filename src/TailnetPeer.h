#pragma once
#include "WgConfig.h"
#include "backends/wg_backend.h"

// OPTIONAL WireGuard tunnel, used only in WiFi mode. With no config supplied,
// the device simply does not start a tunnel (WiFi mode stays LAN-only).
class TailnetPeer {
public:
  enum State { OFF, STARTING, UP, FAILED };

  TailnetPeer();                       // uses the board's real backend
  explicit TailnetPeer(WgBackend* b);  // inject a backend (tests)

  bool begin(const char* wgQuickConfig);     // parse + start the tunnel
  bool loadConfigFromText(const char* s);    // runtime provisioning alias
  void tick();                               // call from loop(); polls handshake
  void stop();

  State       state() const        { return _state; }
  const char* tunnelIP() const     { return _tunIp; }
  const char* peerEndpoint() const { return _endpoint; }
  const char* lastError() const    { return _lastErr; }

private:
  WgBackend* _backend = nullptr;
  State _state = OFF;
  char  _tunIp[24]    = "(off)";
  char  _endpoint[80] = "";
  char  _lastErr[64]  = "";
};
