#include "transport/tcp_service_transport.h"
#include "transport/token_gate.h"

void TcpServiceTransport::tick() {
  WiFiClient c = _srv.available();
  if (!c) return;
  String first = c.readStringUntil('\n');
  first.trim();
  if (!tokenEquals(first.c_str(), _token.c_str())) { c.stop(); return; }  // silent drop
  while (c.connected()) {
    if (!c.available()) { delay(5); continue; }
    String line = c.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    if (_handler) c.println(_handler(line));
  }
  c.stop();
}
