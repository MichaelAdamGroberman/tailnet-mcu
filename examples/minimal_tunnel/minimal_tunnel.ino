// minimal_tunnel — connect to WiFi, bring up a WireGuard tunnel, and attempt
// a TCP reach-test to a host inside your tailnet. Edit the consts below.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <TailnetPeer.h>

// ---- Edit these -------------------------------------------------------
static const char* WIFI_SSID = "YOUR_SSID";
static const char* WIFI_PASS = "YOUR_PASS";

// wg-quick format block (add real keys / endpoint before flashing)
static const char* WG_CONFIG = R"(
[Interface]
PrivateKey = REPLACE_WITH_BASE64_PRIVATE_KEY=
Address = 100.64.0.2/32

[Peer]
PublicKey = REPLACE_WITH_BASE64_PUBLIC_KEY=
Endpoint = 1.2.3.4:51820
AllowedIPs = 100.64.0.0/24
)";

static const char* TAILNET_TEST_HOST = "100.64.0.1"; // a tailnet device to ping
static const uint16_t TAILNET_TEST_PORT = 22;         // any open port on that host
// -----------------------------------------------------------------------

TailnetPeer peer;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[minimal_tunnel] starting…");

  // 1. Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500); Serial.print('.');
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed — halting");
    while (true) delay(1000);
  }
  Serial.println("\nWiFi OK  IP=" + WiFi.localIP().toString());

  // 2. Start the WireGuard tunnel
  if (!peer.begin(WG_CONFIG)) {
    Serial.println("TailnetPeer begin failed: " + String(peer.lastError()));
    return;
  }
  Serial.println("Tunnel starting…  endpoint=" + String(peer.peerEndpoint()));

  // 3. Wait up to 10 s for the first handshake
  t0 = millis();
  while (peer.state() != TailnetPeer::UP && millis() - t0 < 10000) {
    peer.tick();
    delay(200);
  }
  if (peer.state() != TailnetPeer::UP) {
    Serial.println("Tunnel did not reach UP in 10 s (state=" + String(peer.state()) + ")");
    Serial.println("Probe may still work if the ESP32 WireGuard lib is just slow to report handshake.");
  } else {
    Serial.println("Tunnel UP  tunIP=" + String(peer.tunnelIP()));
  }

  // 4. TCP reach-test
  WiFiClient c;
  Serial.printf("Connecting to %s:%u … ", TAILNET_TEST_HOST, TAILNET_TEST_PORT);
  if (c.connect(TAILNET_TEST_HOST, TAILNET_TEST_PORT, 4000)) {
    Serial.println("REACHABLE");
    c.stop();
  } else {
    Serial.println("UNREACHABLE (tunnel may not be up yet, or host/port wrong)");
  }
}

void loop() {
  peer.tick();
  delay(1000);
}
