#pragma once
// Board radio control behind an interface so the WiFi-XOR-BT invariant is
// host-testable with a fake. Real hooks live in the demo/examples and call
// WiFi.mode(WIFI_OFF)/NimBLEDevice::deinit (ESP32) or the CYW43/BTstack
// equivalents (Pico W).
class RadioHooks {
public:
  virtual ~RadioHooks() {}
  virtual bool startWifi() = 0;
  virtual void stopWifi()  = 0;
  virtual bool startBt()   = 0;
  virtual void stopBt()    = 0;
};

class RadioManager {
public:
  enum Mode { OFF, WIFI, BT };
  explicit RadioManager(RadioHooks* hooks) : _hooks(hooks) {}
  bool setMode(Mode m);   // ALWAYS stops the active radio before starting next
  Mode mode() const { return _mode; }
private:
  RadioHooks* _hooks;
  Mode _mode = OFF;
};
