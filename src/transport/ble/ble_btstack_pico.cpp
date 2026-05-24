#if defined(ARDUINO_ARCH_RP2040)
// ---------------------------------------------------------------------------
// Pico W BLE transport — BTstack (earlephilhower arduino-pico core)
//
// Spike result (2026-05-24):
//   Searched PlatformIO for BTstack / BLE-peripheral libs compatible with the
//   earlephilhower RP2040 core.  Found:
//     • iot-gamer/pico-ble-notify 1.0.1 — TX/notify helper over BTstackLib.h
//     • kuba2k2/BTstack 1.6.2-pkg.1 — raw BTstack (no Arduino wrapper).
//   BTstackLib.h (bundled with earlephilhower arduino-pico) provides a thin
//   Arduino-style wrapper: BTstack global, addGATTService(),
//   addGATTCharacteristicDynamic(), setGATTCharacteristicWrite(), etc.
//
//   CRITICAL DESIGN CONSTRAINT:
//   BTstack requires the application to define a *single* global
//   gattWriteCallback() function.  Since this file is a library component, we
//   cannot define that symbol — doing so would collide with any write callback
//   the application might register via BTstack.setGATTCharacteristicWrite().
//   The BLENotify library itself expects the app's gattWriteCallback to forward
//   CCC descriptor writes to BLENotify.handleSubscriptionChange().
//
//   Given this collision risk, implementing a fully self-contained NUS GATT
//   server inside a library translation unit is not safe without a coordinating
//   shim that the app must opt into (similar to how NimBLE-Arduino requires
//   explicit NimBLEDevice::init()).
//
//   DECISION: Ship a clean STUB for this session.  The stub compiles, logs its
//   limitation clearly, and leaves the door open for a v2 integration that
//   provides a user-visible "bleWriteHook" the app routes its gattWriteCallback
//   into.  See docs/roadmap for the recommended v2 design.
//
//   What was attempted:
//     1. iot-gamer/pico-ble-notify — good TX notify; no solution for write-callback
//        conflict without app-side plumbing.
//     2. Raw BTstackLib.h — same constraint; single global write-callback slot.
//     3. Considered weak-symbol override of gattWriteCallback — unsafe, may
//        silently shadow app-registered callbacks on linker versions that fold
//        COMDAT; rejected.
// ---------------------------------------------------------------------------

#include "../ble_service_transport.h"

BleServiceTransport::BleServiceTransport(const char* name, const char* token)
  : _name(name), _token(token) {}

bool BleServiceTransport::begin() {
  Serial.println("[ble] Pico W BTstack BLE not yet supported — see docs/roadmap");
  Serial.println("[ble] Reason: BTstack requires a single global gattWriteCallback();");
  Serial.println("[ble]         a library-internal definition would collide with any");
  Serial.println("[ble]         app-level callback.  v2 will expose a bleWriteHook.");
  return false;
}

void BleServiceTransport::tick() {
  // No-op: BTstack not initialised.
}

#endif // ARDUINO_ARCH_RP2040
