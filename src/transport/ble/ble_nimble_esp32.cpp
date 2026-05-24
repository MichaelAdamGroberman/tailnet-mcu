#if defined(ARDUINO_ARCH_ESP32)
#include "../ble_service_transport.h"
#include "../../transport/token_gate.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEService.h>
#include <NimBLECharacteristic.h>
#include <NimBLEAdvertising.h>

// Nordic UART Service (NUS) UUIDs
#define NUS_SERVICE_UUID  "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID       "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID       "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// ---- Context shared between the static callback and the transport instance ----
struct BleCtx {
  BleServiceTransport*        transport = nullptr;
  NimBLECharacteristic*       txChar    = nullptr;
  const char*                 token     = nullptr;
  bool*                       authed    = nullptr;
  String                      lineBuf;
};

static BleCtx s_ctx;

// ---- RX characteristic write callback ----
class RxCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    // Accumulate bytes until a newline
    std::string val = pChar->getValue();
    for (char ch : val) {
      if (ch == '\n' || ch == '\r') {
        if (s_ctx.lineBuf.length() == 0) continue;
        String line = s_ctx.lineBuf;
        s_ctx.lineBuf = "";
        line.trim();
        if (!(*s_ctx.authed)) {
          if (tokenEquals(line.c_str(), s_ctx.token)) {
            *s_ctx.authed = true;
          }
          // silent reject — no echo
        } else {
          if (s_ctx.transport && s_ctx.transport->hasHandler() && s_ctx.txChar) {
            String resp = s_ctx.transport->dispatch(line);
            resp += "\n";
            s_ctx.txChar->setValue(resp.c_str());
            s_ctx.txChar->notify();
          }
        }
      } else {
        s_ctx.lineBuf += ch;
      }
    }
  }
};

// ---- Server callbacks: reset auth on disconnect ----
class ServerCallback : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    if (s_ctx.authed) *s_ctx.authed = false;
    s_ctx.lineBuf = "";
    // Restart advertising so the device is discoverable again
    NimBLEDevice::startAdvertising();
  }
};

static RxCallback     s_rxCb;
static ServerCallback s_srvCb;

// ---- BleServiceTransport implementation ----

BleServiceTransport::BleServiceTransport(const char* name, const char* token)
  : _name(name), _token(token) {}

bool BleServiceTransport::begin() {
  NimBLEDevice::init(_name);

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&s_srvCb);

  NimBLEService* pSvc = pServer->createService(NUS_SERVICE_UUID);

  // TX characteristic — notify only
  NimBLECharacteristic* pTx = pSvc->createCharacteristic(
    NUS_TX_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  // RX characteristic — write / write-no-response
  NimBLECharacteristic* pRx = pSvc->createCharacteristic(
    NUS_RX_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pRx->setCallbacks(&s_rxCb);

  // Populate shared context
  s_ctx.transport = this;
  s_ctx.txChar    = pTx;
  s_ctx.token     = _token;
  s_ctx.authed    = &_authed;

  // In NimBLE 2.x services are started when the server starts; the explicit
  // start() call is a no-op and emits a deprecation warning — skip it.
  pServer->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(NUS_SERVICE_UUID);
  pAdv->enableScanResponse(true);
  NimBLEDevice::startAdvertising();

  return true;
}

void BleServiceTransport::tick() {
  // NimBLE is fully callback-driven; nothing to poll here.
}

#endif // ARDUINO_ARCH_ESP32
