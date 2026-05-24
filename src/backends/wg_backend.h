#pragma once
#include "../WgConfig.h"

// One implementation compiles per board (selected by preprocessor);
// FakeBackend (in tests) lets the state machine run host-native.
class WgBackend {
public:
  virtual ~WgBackend() {}
  virtual bool begin(const WgConfig& cfg) = 0; // bind keys/peer; false = hard fail
  virtual void end() = 0;
  virtual bool isUp() = 0;                      // true once first handshake completes
};
