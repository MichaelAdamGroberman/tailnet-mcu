// tailnet-sensor-node — main.cpp
// Demonstrates WiFi (TCP) ⇄ BT (BLE) switching with an optional WireGuard
// tunnel.  Exposes a "read" command that returns temperature + uptime JSON.
//
// Copy secrets.example.h -> secrets.h and configure before building.
#include <Arduino.h>
#include "../secrets.h"

#include <RadioManager.h>
#include <TailnetPeer.h>
#include <transport/tcp_service_transport.h>
#include <transport/ble_service_transport.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <NimBLEDevice.h>
#elif defined(ARDUINO_ARCH_RP2040)
  // TODO: add CYW43/BTstack includes when Pico W BLE backend is implemented
  #include <WiFi.h>
#endif

// ---- Transport config -------------------------------------------------
static const uint16_t  TCP_PORT  = 4242;
static const char*     BLE_NAME  = "tailnet-sensor";

TcpServiceTransport  tcpTransport(TCP_PORT, SERVICE_TOKEN);
BleServiceTransport  bleTransport(BLE_NAME, SERVICE_TOKEN);
ServiceTransport*    activeTransport = nullptr;

// ---- Optional WireGuard tunnel ----------------------------------------
#ifdef WG_CONFIG
TailnetPeer tailnet;
static bool tunnelStarted = false;
#endif

// ---- Forward declarations ---------------------------------------------
ServiceTransport::Handler makeHandler();

// ---- Real RadioHooks --------------------------------------------------
class SensorRadioHooks : public RadioHooks {
public:
  bool startWifi() override {
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi connecting");
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) {
      delay(500); Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) { Serial.println(" FAIL"); return false; }
    Serial.println("\nWiFi OK  " + WiFi.localIP().toString());
#endif
    tcpTransport.begin();
    activeTransport = &tcpTransport;
    activeTransport->onLine(makeHandler());

#ifdef WG_CONFIG
    if (!tunnelStarted) {
      if (tailnet.begin(WG_CONFIG)) {
        tunnelStarted = true;
        Serial.println("WG tunnel starting…  endpoint=" + String(tailnet.peerEndpoint()));
      } else {
        Serial.println("WG tunnel begin failed: " + String(tailnet.lastError()));
      }
    }
#endif
    return true;
  }

  void stopWifi() override {
    activeTransport = nullptr;
#if defined(ARDUINO_ARCH_ESP32)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
    Serial.println("WiFi OFF");
  }

  bool startBt() override {
    bleTransport.begin();
    activeTransport = &bleTransport;
    activeTransport->onLine(makeHandler());
    Serial.println(String("BLE advertising as: ") + BLE_NAME);
    return true;
  }

  void stopBt() override {
    activeTransport = nullptr;
#if defined(ARDUINO_ARCH_ESP32)
    NimBLEDevice::deinit(true);
#endif
    Serial.println("BLE OFF");
  }
};

SensorRadioHooks hooks;
RadioManager     radio(&hooks);

// ---- Sensor handler ---------------------------------------------------
ServiceTransport::Handler makeHandler() {
  return [](const String& cmd) -> String {
    String line = cmd;
    line.trim();

    if (line == "read") {
#if defined(ARDUINO_ARCH_ESP32)
      float t = temperatureRead();
#else
      float t = 0.0f; // placeholder for Pico W (use onboard sensor when backend added)
#endif
      unsigned long up = millis() / 1000UL;
      char buf[64];
      snprintf(buf, sizeof(buf), "{\"temp_c\":%.1f,\"uptime_s\":%lu}", t, up);
      return String(buf);
    }

    if (line == "mode wifi") {
      radio.setMode(RadioManager::WIFI);
      return "switching to WiFi";
    }
    if (line == "mode bt") {
      radio.setMode(RadioManager::BT);
      return "switching to BT";
    }

    return "unknown: " + line;
  };
}

// ---- Arduino setup / loop --------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[tailnet-sensor-node] starting…");

#ifdef DEFAULT_MODE_WIFI
  radio.setMode(RadioManager::WIFI);
#else
  radio.setMode(RadioManager::BT);
#endif
}

void loop() {
  if (activeTransport) activeTransport->tick();
#ifdef WG_CONFIG
  if (tunnelStarted) tailnet.tick();
#endif
  delay(5);
}
