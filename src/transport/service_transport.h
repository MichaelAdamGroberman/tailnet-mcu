#pragma once
#include <Arduino.h>
#include <functional>

// One Handler, served over whichever radio is active. The user writes their
// service logic once (e.g. "read" -> sensor JSON) and binds it to whichever
// transport the active RadioMode provides (TCP in WiFi, BLE in BT).
class ServiceTransport {
public:
  using Handler = std::function<String(const String& line)>;
  virtual ~ServiceTransport() {}
  virtual bool begin() = 0;   // start listening / advertising
  virtual void tick()  = 0;   // call from loop()
  void onLine(Handler h) { _handler = h; }
protected:
  Handler _handler;
};
