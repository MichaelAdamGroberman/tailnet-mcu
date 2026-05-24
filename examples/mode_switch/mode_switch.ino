// mode_switch — demonstrate WiFi (TCP) ⇄ BT (BLE) switching via RadioManager.
// Boot mode is set by DEFAULT_MODE below.  Send "mode wifi" or "mode bt" over
// whichever transport is active to switch at runtime.
#include <Arduino.h>
#include <WiFi.h>
#include <RadioManager.h>
#include <transport/tcp_service_transport.h>
#include <transport/ble_service_transport.h>

// ---- Edit these -------------------------------------------------------
static const char* WIFI_SSID      = "YOUR_SSID";
static const char* WIFI_PASS      = "YOUR_PASS";
static const char* SERVICE_TOKEN  = "change-me-long-random";
static const uint16_t TCP_PORT    = 4242;
static const char*    BLE_NAME    = "tailnet-mcu";
// Boot into WIFI or BT
static RadioManager::Mode DEFAULT_MODE = RadioManager::WIFI;
// -----------------------------------------------------------------------

// ---- Transport instances ----------------------------------------------
TcpServiceTransport  tcpTransport(TCP_PORT, SERVICE_TOKEN);
BleServiceTransport  bleTransport(BLE_NAME, SERVICE_TOKEN);

ServiceTransport* active = nullptr;

// ---- Real ESP32 RadioHooks --------------------------------------------
class Esp32RadioHooks : public RadioHooks {
public:
  bool startWifi() override {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi connecting");
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
      delay(500); Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) { Serial.println(" FAIL"); return false; }
    Serial.println("\nWiFi OK  " + WiFi.localIP().toString());
    // Start the TCP transport when WiFi comes up
    tcpTransport.begin();
    active = &tcpTransport;
    return true;
  }
  void stopWifi() override {
    active = nullptr;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi OFF");
  }
  bool startBt() override {
    Serial.println("BLE starting…");
    bleTransport.begin();
    active = &bleTransport;
    Serial.println("BLE advertising as: " BLE_NAME);
    return true;
  }
  void stopBt() override {
    active = nullptr;
    NimBLEDevice::deinit(true);
    Serial.println("BLE OFF");
  }
};

Esp32RadioHooks hooks;
RadioManager    radio(&hooks);

// ---- Shared service handler -------------------------------------------
// Wire this to whichever transport just became active.
ServiceTransport::Handler makeHandler() {
  return [](const String& line) -> String {
    if (line == "read") return "hello from tailnet-mcu";
    if (line == "mode wifi") {
      radio.setMode(RadioManager::WIFI);
      // After mode change active pointer updated inside hooks; rebind handler
      if (active) active->onLine(makeHandler());
      return "switching to WiFi";
    }
    if (line == "mode bt") {
      radio.setMode(RadioManager::BT);
      if (active) active->onLine(makeHandler());
      return "switching to BT";
    }
    return "unknown: " + line;
  };
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[mode_switch] starting…");

  radio.setMode(DEFAULT_MODE);
  if (active) active->onLine(makeHandler());
}

void loop() {
  if (active) active->tick();
  delay(5);
}
