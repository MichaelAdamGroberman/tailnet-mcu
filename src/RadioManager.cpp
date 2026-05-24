#include "RadioManager.h"

bool RadioManager::setMode(Mode m) {
  if (m == _mode) return true;
  // XOR invariant: power down whatever is active BEFORE bringing up the next.
  if (_mode == WIFI) _hooks->stopWifi();
  else if (_mode == BT) _hooks->stopBt();
  _mode = OFF;
  if (m == WIFI) { if (!_hooks->startWifi()) return false; _mode = WIFI; }
  else if (m == BT) { if (!_hooks->startBt()) return false; _mode = BT; }
  return true;
}
