#pragma once
#include "transport/service_transport.h"
#include <WiFi.h>

// Token-gated TCP transport for WiFi mode. Reachable tailnet-wide IFF the
// optional TailnetPeer tunnel is up; otherwise LAN-only. The first line a
// client sends must equal the shared token (constant-time check) or the
// connection is dropped silently.
class TcpServiceTransport : public ServiceTransport {
public:
  TcpServiceTransport(uint16_t port, const char* token) : _srv(port), _token(token) {}
  bool begin() override { _srv.begin(); return true; }
  void tick() override;
private:
  WiFiServer _srv;
  String _token;
};
