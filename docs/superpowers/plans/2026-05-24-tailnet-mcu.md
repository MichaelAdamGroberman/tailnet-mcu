# tailnet-mcu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a multi-board (ESP32 + Raspberry Pi Pico W) WireGuard-to-Tailscale integration: a reusable `TailnetPeer` library, a private-sensor demo app, a node-agnostic subnet-router gateway script, and security-first docs.

**Architecture:** A portable core (`WgConfig` parser + `TailnetPeer` state machine over a `WgBackend` interface) with two compile-time backends — ESP32 (`ciniml/WireGuard-ESP32`) and Pico W (`wireguard-lwip` on arduino-pico's lwIP). The MCU dials one reachable underlay endpoint of an existing Tailscale node that advertises the device's WG subnet as a route. The parser and state machine are unit-tested host-native; backends are compile-gated + hardware-verified.

**Tech Stack:** C++17, PlatformIO (Arduino framework + `native` test env), Unity test framework, WireGuard, Tailscale, bash + shellcheck, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-05-24-tailnet-mcu-design.md`

> **⚠ REVISION 2 (2026-05-24):** The project gained **WiFi⊕BT mutually-exclusive
> radio modes** and the **Tailscale tunnel is now OPTIONAL** (a WiFi-mode layer).
> New components — `RadioManager`, `ServiceTransport` (BLE + TCP), BLE backends —
> are defined in the **"Revision 2 Addendum"** at the bottom of this file, with the
> revised task order. Tasks 7, 9, 10 below are amended there. Read the addendum
> first; where it conflicts with Tasks 1–12, the addendum wins.

---

## File structure

```
tailnet-mcu/
  platformio.ini                 # envs: native (tests), esp32-s3, pico-w
  library.json                   # PlatformIO registry metadata
  library.properties             # Arduino IDE metadata
  .gitignore
  LICENSE                        # MIT
  src/
    WgConfig.h  WgConfig.cpp      # pure wg-quick parser (no Arduino deps)
    backends/wg_backend.h        # WgBackend interface
    backends/backend_esp32.h  backends/backend_esp32.cpp   # #ifdef ESP32
    backends/backend_pico_w.h backends/backend_pico_w.cpp  # #ifdef RP2040
    TailnetPeer.h  TailnetPeer.cpp   # state machine + orchestration
    TcpTokenListener.h TcpTokenListener.cpp  # token-gated listener helper
  test/
    test_wgconfig/test_wgconfig.cpp     # parser unit tests (native)
    test_tailnetpeer/test_tailnetpeer.cpp # state-machine tests w/ FakeBackend (native)
  examples/
    minimal_tunnel/minimal_tunnel.ino
    reachable_service/reachable_service.ino
  app/tailnet-sensor-node/
    platformio.ini  src/main.cpp  secrets.example.h
  gateway/
    setup-subnet-router.sh  tailscale-acl.example.json  harden.md
  docs/
    architecture.md  provisioning.md  security-model.md
    superpowers/{specs,plans}/...
  .github/workflows/ci.yml
  README.md SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md CODEOWNERS
  .github/ISSUE_TEMPLATE/... PULL_REQUEST_TEMPLATE.md
```

---

### Task 1: Project scaffolding & PlatformIO config

**Files:**
- Create: `platformio.ini`, `library.json`, `library.properties`, `.gitignore`, `LICENSE`

- [ ] **Step 1: Create `platformio.ini`**

```ini
; tailnet-mcu — library CI envs
[platformio]
default_envs = native

[env:native]              ; host-native unit tests (no hardware)
platform = native
test_framework = unity
build_flags = -std=gnu++17

[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags = -std=gnu++17
lib_deps = ciniml/WireGuard-ESP32 @ ^0.1.5

[env:pico-w]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipicow
framework = arduino
board_build.core = earlephilhower
build_flags = -std=gnu++17
; lib_deps for the WireGuard implementation is confirmed in Task 5 (spike).
```

- [ ] **Step 2: Create `library.json`**

```json
{
  "name": "tailnet-mcu",
  "version": "0.1.0",
  "description": "Join an ESP32 or Raspberry Pi Pico W to your Tailscale network over WireGuard, reachable from your whole tailnet but never the public internet.",
  "keywords": "tailscale, wireguard, esp32, pico-w, vpn, iot, security",
  "repository": { "type": "git", "url": "https://github.com/MichaelAdamGroberman/tailnet-mcu.git" },
  "license": "MIT",
  "frameworks": "arduino",
  "platforms": ["espressif32", "raspberrypi"],
  "export": { "include": ["src", "examples", "library.json", "library.properties", "LICENSE", "README.md"] }
}
```

- [ ] **Step 3: Create `library.properties`** (Arduino IDE discovery)

```ini
name=tailnet-mcu
version=0.1.0
author=Michael Groberman
maintainer=Michael Groberman
sentence=Join an ESP32 or Pico W to your Tailscale network over WireGuard.
paragraph=Reusable TailnetPeer class with ESP32 and RP2040 backends, plus a node-agnostic subnet-router gateway and security-first defaults.
category=Communication
url=https://github.com/MichaelAdamGroberman/tailnet-mcu
architectures=esp32,rp2040
```

- [ ] **Step 4: Create `.gitignore`**

```gitignore
.pio/
.vscode/
**/secrets.h
*.bin
```

- [ ] **Step 5: Create `LICENSE`** — standard MIT text, copyright `2026 Michael Groberman`. Add a note line: "Portions derive from the BSD-licensed wireguard-lwip; its license is preserved in vendored sources."

- [ ] **Step 6: Verify PlatformIO sees the env**

Run: `sudo -u michaelgroberman pio project config`
Expected: lists `native`, `esp32-s3`, `pico-w` envs without error.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini library.json library.properties .gitignore LICENSE
git commit -m "chore: scaffold tailnet-mcu PlatformIO library"
```

---

### Task 2: `WgConfig` wg-quick parser (TDD, host-native)

**Files:**
- Create: `src/WgConfig.h`, `src/WgConfig.cpp`
- Test: `test/test_wgconfig/test_wgconfig.cpp`

- [ ] **Step 1: Write the failing tests**

`test/test_wgconfig/test_wgconfig.cpp`:
```cpp
#include <unity.h>
#include "WgConfig.h"

static const char* FULL =
  "[Interface]\n"
  "PrivateKey = aGVsbG9wcml2YXRla2V5MDAwMDAwMDAwMDAwMDA9\n"
  "Address = 10.20.30.5/24\n"
  "[Peer]\n"
  "PublicKey = cGVlcnB1YmxpY2tleTAwMDAwMDAwMDAwMDAwMDA9\n"
  "Endpoint = gw.example.com:51820\n"
  "AllowedIPs = 0.0.0.0/0\n";

void setUp(){} void tearDown(){}

void test_full_config_parses() {
  WgConfig c;
  TEST_ASSERT_TRUE(wgConfigParse(FULL, &c));
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL_STRING("10.20.30.5", c.if_addr);   // cidr stripped
  TEST_ASSERT_EQUAL_STRING("gw.example.com", c.peer_host);
  TEST_ASSERT_EQUAL_UINT16(51820, c.peer_port);
}
void test_default_port_when_no_colon() {
  WgConfig c;
  const char* t = "PrivateKey=k\nAddress=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_UINT16(51820, c.peer_port);
  TEST_ASSERT_EQUAL_STRING("1.2.3.4", c.peer_host);
}
void test_custom_port_parsed() {
  WgConfig c;
  const char* t = "PrivateKey=k\nAddress=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4:12345\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_UINT16(12345, c.peer_port);
}
void test_missing_private_key_invalid() {
  WgConfig c;
  const char* t = "Address=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4\n";
  TEST_ASSERT_FALSE(wgConfigParse(t, &c));
  TEST_ASSERT_FALSE(c.valid);
}
void test_comments_and_whitespace() {
  WgConfig c;
  const char* t = "# comment\n  PrivateKey   =   k  \nAddress=10.0.0.2/32\nPublicKey=p\nEndpoint=h:9\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_STRING("k", c.if_priv);
  TEST_ASSERT_EQUAL_STRING("10.0.0.2", c.if_addr);
  TEST_ASSERT_EQUAL_UINT16(9, c.peer_port);
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_full_config_parses);
  RUN_TEST(test_default_port_when_no_colon);
  RUN_TEST(test_custom_port_parsed);
  RUN_TEST(test_missing_private_key_invalid);
  RUN_TEST(test_comments_and_whitespace);
  return UNITY_END();
}
```

- [ ] **Step 2: Create the header so the test compiles** — `src/WgConfig.h`:

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

// Parsed wg-quick config. Fixed buffers so it lives on the stack with
// no heap on the MCU. Strings are NUL-terminated.
struct WgConfig {
  char     if_addr[24];   // "10.20.30.5"  (CIDR suffix stripped)
  char     if_priv[48];   // base64 interface private key
  char     peer_pub[48];  // base64 peer public key
  char     peer_host[64]; // underlay host or IP we dial (never a 100.x addr)
  uint16_t peer_port;     // defaults to 51820
  bool     valid;         // true iff PrivateKey, Address, PublicKey, Endpoint all present
};

// Parse wg-quick text. Returns out->valid. Section headers ([Interface],
// [Peer]), comments (#), and unknown keys are ignored.
bool wgConfigParse(const char* text, WgConfig* out);
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `sudo -u michaelgroberman pio test -e native`
Expected: FAIL — undefined reference to `wgConfigParse`.

- [ ] **Step 4: Implement `src/WgConfig.cpp`**

```cpp
#include "WgConfig.h"
#include <string.h>
#include <strings.h>   // strncasecmp
#include <stdlib.h>

static const char* skipWs(const char* s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

// If `line` is "Key = value", copy value into out (trimmed). Returns true on match.
static bool getKV(const char* line, const char* key, char* out, size_t outsz) {
  size_t klen = strlen(key);
  const char* s = skipWs(line);
  if (strncasecmp(s, key, klen) != 0) return false;
  s += klen;
  s = skipWs(s);
  if (*s != '=') return false;
  s = skipWs(s + 1);
  size_t n = 0;
  while (*s && *s != '\r' && *s != '\n' && n < outsz - 1) out[n++] = *s++;
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
  out[n] = 0;
  return n > 0;
}

bool wgConfigParse(const char* text, WgConfig* out) {
  memset(out, 0, sizeof(*out));
  out->peer_port = 51820;

  char endpoint[80] = "";
  const char* p = text;
  char line[160];
  while (*p) {
    size_t n = 0;
    while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
    line[n] = 0;
    if (*p == '\n') p++;
    if (line[0] == '[' || line[0] == '#' || line[0] == 0) continue;

    char val[160];
    if (getKV(line, "PrivateKey", val, sizeof(val))) {
      strncpy(out->if_priv, val, sizeof(out->if_priv) - 1);
    } else if (getKV(line, "Address", val, sizeof(val))) {
      char* slash = strchr(val, '/');
      if (slash) *slash = 0;                 // backends want a bare IP
      strncpy(out->if_addr, val, sizeof(out->if_addr) - 1);
    } else if (getKV(line, "PublicKey", val, sizeof(val))) {
      strncpy(out->peer_pub, val, sizeof(out->peer_pub) - 1);
    } else if (getKV(line, "Endpoint", val, sizeof(val))) {
      strncpy(endpoint, val, sizeof(endpoint) - 1);
    }
  }

  if (endpoint[0]) {
    const char* colon = strrchr(endpoint, ':');
    if (colon) {
      size_t hl = (size_t)(colon - endpoint);
      if (hl >= sizeof(out->peer_host)) hl = sizeof(out->peer_host) - 1;
      memcpy(out->peer_host, endpoint, hl);
      out->peer_host[hl] = 0;
      int port = atoi(colon + 1);
      if (port > 0 && port <= 65535) out->peer_port = (uint16_t)port;
    } else {
      strncpy(out->peer_host, endpoint, sizeof(out->peer_host) - 1);
    }
  }

  out->valid = out->if_priv[0] && out->if_addr[0] && out->peer_pub[0] && out->peer_host[0];
  return out->valid;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `sudo -u michaelgroberman pio test -e native`
Expected: PASS — 5 tests, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add src/WgConfig.h src/WgConfig.cpp test/test_wgconfig/test_wgconfig.cpp
git commit -m "feat: wg-quick config parser with host-native tests"
```

---

### Task 3: `WgBackend` interface + `TailnetPeer` state machine (TDD with FakeBackend)

**Files:**
- Create: `src/backends/wg_backend.h`, `src/TailnetPeer.h`, `src/TailnetPeer.cpp`
- Test: `test/test_tailnetpeer/test_tailnetpeer.cpp`

- [ ] **Step 1: Create the backend interface** — `src/backends/wg_backend.h`:

```cpp
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
```

- [ ] **Step 2: Write the failing state-machine tests** — `test/test_tailnetpeer/test_tailnetpeer.cpp`:

```cpp
#include <unity.h>
#include "TailnetPeer.h"
#include "backends/wg_backend.h"

static const char* FULL =
  "[Interface]\nPrivateKey=k\nAddress=10.20.30.5/24\n"
  "[Peer]\nPublicKey=p\nEndpoint=gw.example.com:51820\n";

class FakeBackend : public WgBackend {
public:
  bool begin_ret = true, up = false, begun = false;
  WgConfig last{};
  bool begin(const WgConfig& c) override { begun = true; last = c; return begin_ret; }
  void end() override { begun = false; }
  bool isUp() override { return up; }
};

void setUp(){} void tearDown(){}

void test_begin_parses_and_starts() {
  FakeBackend fb; TailnetPeer p(&fb);
  TEST_ASSERT_TRUE(p.begin(FULL));
  TEST_ASSERT_EQUAL(TailnetPeer::STARTING, p.state());
  TEST_ASSERT_TRUE(fb.begun);
  TEST_ASSERT_EQUAL_STRING("10.20.30.5", p.tunnelIP());
  TEST_ASSERT_EQUAL_STRING("gw.example.com:51820", p.peerEndpoint());
}
void test_tick_promotes_to_up() {
  FakeBackend fb; TailnetPeer p(&fb);
  p.begin(FULL);
  p.tick(); TEST_ASSERT_EQUAL(TailnetPeer::STARTING, p.state());
  fb.up = true; p.tick();
  TEST_ASSERT_EQUAL(TailnetPeer::UP, p.state());
}
void test_bad_config_fails() {
  FakeBackend fb; TailnetPeer p(&fb);
  TEST_ASSERT_FALSE(p.begin("garbage"));
  TEST_ASSERT_EQUAL(TailnetPeer::FAILED, p.state());
}
void test_backend_begin_failure() {
  FakeBackend fb; fb.begin_ret = false; TailnetPeer p(&fb);
  TEST_ASSERT_FALSE(p.begin(FULL));
  TEST_ASSERT_EQUAL(TailnetPeer::FAILED, p.state());
}
void test_stop_returns_to_off() {
  FakeBackend fb; TailnetPeer p(&fb);
  p.begin(FULL); p.stop();
  TEST_ASSERT_EQUAL(TailnetPeer::OFF, p.state());
  TEST_ASSERT_FALSE(fb.begun);
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_parses_and_starts);
  RUN_TEST(test_tick_promotes_to_up);
  RUN_TEST(test_bad_config_fails);
  RUN_TEST(test_backend_begin_failure);
  RUN_TEST(test_stop_returns_to_off);
  return UNITY_END();
}
```

- [ ] **Step 3: Create `src/TailnetPeer.h`**

```cpp
#pragma once
#include "WgConfig.h"
#include "backends/wg_backend.h"

class TailnetPeer {
public:
  enum State { OFF, STARTING, UP, FAILED };

  TailnetPeer();                       // uses the board's real backend
  explicit TailnetPeer(WgBackend* b);  // inject a backend (tests)

  bool begin(const char* wgQuickConfig);     // parse + start the tunnel
  bool loadConfigFromText(const char* s);    // runtime provisioning alias
  void tick();                               // call from loop(); polls handshake
  void stop();

  State       state() const        { return _state; }
  const char* tunnelIP() const     { return _tunIp; }
  const char* peerEndpoint() const { return _endpoint; }
  const char* lastError() const    { return _lastErr; }

private:
  WgBackend* _backend = nullptr;
  State _state = OFF;
  char  _tunIp[24]    = "(off)";
  char  _endpoint[80] = "";
  char  _lastErr[64]  = "";
};
```

- [ ] **Step 4: Run tests to verify they fail**

Run: `sudo -u michaelgroberman pio test -e native -f test_tailnetpeer`
Expected: FAIL — undefined reference to `TailnetPeer::begin` etc.

- [ ] **Step 5: Implement `src/TailnetPeer.cpp`**

```cpp
#include "TailnetPeer.h"
#include <string.h>
#include <stdio.h>

// Board-specific default backend. On the native test host there is no
// real backend, so the default ctor leaves _backend null; tests inject
// a FakeBackend via the other ctor.
#if defined(ARDUINO_ARCH_ESP32)
#include "backends/backend_esp32.h"
static WgBackend* makeDefaultBackend() { return new Esp32WgBackend(); }
#elif defined(ARDUINO_ARCH_RP2040)
#include "backends/backend_pico_w.h"
static WgBackend* makeDefaultBackend() { return new PicoWWgBackend(); }
#else
static WgBackend* makeDefaultBackend() { return nullptr; }
#endif

TailnetPeer::TailnetPeer() : _backend(makeDefaultBackend()) {}
TailnetPeer::TailnetPeer(WgBackend* b) : _backend(b) {}

bool TailnetPeer::begin(const char* wgQuickConfig) {
  WgConfig cfg;
  if (!wgConfigParse(wgQuickConfig, &cfg)) {
    snprintf(_lastErr, sizeof(_lastErr), "config parse failed (missing field)");
    _state = FAILED;
    return false;
  }
  snprintf(_endpoint, sizeof(_endpoint), "%s:%u", cfg.peer_host, cfg.peer_port);
  if (!_backend || !_backend->begin(cfg)) {
    snprintf(_lastErr, sizeof(_lastErr), "backend begin failed");
    _state = FAILED;
    return false;
  }
  strncpy(_tunIp, cfg.if_addr, sizeof(_tunIp) - 1);
  _tunIp[sizeof(_tunIp) - 1] = 0;
  _lastErr[0] = 0;
  _state = STARTING;
  return true;
}

bool TailnetPeer::loadConfigFromText(const char* s) { return begin(s); }

void TailnetPeer::tick() {
  if (_state != STARTING && _state != UP) return;
  if (_backend && _backend->isUp()) _state = UP;
}

void TailnetPeer::stop() {
  if (_backend) _backend->end();
  _state = OFF;
  strncpy(_tunIp, "(off)", sizeof(_tunIp));
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `sudo -u michaelgroberman pio test -e native`
Expected: PASS — all parser + state-machine tests (10 total).

- [ ] **Step 7: Commit**

```bash
git add src/backends/wg_backend.h src/TailnetPeer.h src/TailnetPeer.cpp test/test_tailnetpeer/
git commit -m "feat: TailnetPeer state machine over WgBackend interface (TDD)"
```

---

### Task 4: ESP32 backend (`ciniml/WireGuard-ESP32`)

**Files:**
- Create: `src/backends/backend_esp32.h`, `src/backends/backend_esp32.cpp`

> No host test — guarded by `ARDUINO_ARCH_ESP32`. Verified by CI compile (Task 11) + hardware checklist (Task 12). API confirmed from the existing buddy `net_wg.cpp`.

- [ ] **Step 1: Create `src/backends/backend_esp32.h`**

```cpp
#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include "wg_backend.h"
#include <WireGuard-ESP32.h>

class Esp32WgBackend : public WgBackend {
public:
  bool begin(const WgConfig& c) override;
  void end() override;
  bool isUp() override;
private:
  WireGuard _wg;
  bool _begun = false;
};
#endif
```

- [ ] **Step 2: Create `src/backends/backend_esp32.cpp`**

```cpp
#if defined(ARDUINO_ARCH_ESP32)
#include "backend_esp32.h"
#include <IPAddress.h>

bool Esp32WgBackend::begin(const WgConfig& c) {
  IPAddress local;
  if (!local.fromString(c.if_addr)) return false;
  // ciniml signature: begin(localIP, privKey, peerHost, peerPubKey, peerPort)
  _begun = _wg.begin(local, c.if_priv, c.peer_host, c.peer_pub, c.peer_port);
  return _begun;
}
void Esp32WgBackend::end()  { if (_begun) { _wg.end(); _begun = false; } }
bool Esp32WgBackend::isUp() { return _begun && _wg.is_initialized(); }
#endif
```

- [ ] **Step 3: Compile-verify for ESP32-S3**

Run: `sudo -u michaelgroberman pio run -e esp32-s3`
Expected: SUCCESS (links `WireGuard-ESP32`). Note: the env has no `main` yet — if PlatformIO reports "nothing to build," proceed; the example in Task 6 provides the entry point. To compile in isolation now, temporarily build the `minimal_tunnel` example after Task 6.

- [ ] **Step 4: Commit**

```bash
git add src/backends/backend_esp32.h src/backends/backend_esp32.cpp
git commit -m "feat: ESP32 WireGuard backend (ciniml/WireGuard-ESP32)"
```

---

### Task 5: Pico W backend (`wireguard-lwip`) — **spike first, this carries integration risk**

**Files:**
- Create: `src/backends/backend_pico_w.h`, `src/backends/backend_pico_w.cpp`
- Modify: `platformio.ini` (`[env:pico-w]` `lib_deps`)

> This is the least turnkey part: unlike ESP32, RP2040 has no drop-in Arduino WireGuard wrapper. arduino-pico exposes lwIP, and `wireguard-lwip` (smartalock, BSD) provides a `wireguardif` netif. The spike confirms the exact library + API before writing the backend.

- [ ] **Step 1: SPIKE — confirm the WireGuard-for-lwIP source**

Investigate, in order of preference, and record the choice in a commit message:
1. A PlatformIO/Arduino library wrapping `wireguard-lwip` for rp2040 (search `pio pkg search wireguard`).
2. Vendoring `smartalock/wireguard-lwip` sources (`wireguardif.c/.h`, `wireguard.c/.h`, `crypto/*`) under `src/backends/wireguard_lwip/` with a preserved BSD `LICENSE`.

Run: `sudo -u michaelgroberman pio pkg search wireguard`
Decision point: set `[env:pico-w] lib_deps` to the confirmed library, OR add the vendored path. Confirm `wireguardif.h` exposes `wireguardif_init`, `wireguardif_add_peer`, `wireguardif_connect`, `wireguardif_peer_init`, `wireguardif_peer_is_up`. If the netif API differs in the installed version, adjust Step 3 to match its actual signatures (this is expected and acceptable — match the header, do not invent calls).

- [ ] **Step 2: Create `src/backends/backend_pico_w.h`**

```cpp
#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include "wg_backend.h"
extern "C" {
  #include "lwip/netif.h"
  #include "wireguardif.h"
}

class PicoWWgBackend : public WgBackend {
public:
  bool begin(const WgConfig& c) override;
  void end() override;
  bool isUp() override;
private:
  struct netif _wgNetif{};
  uint8_t _peerIndex = WIREGUARDIF_INVALID_INDEX;
  bool _begun = false;
};
#endif
```

- [ ] **Step 3: Create `src/backends/backend_pico_w.cpp`**

```cpp
#if defined(ARDUINO_ARCH_RP2040)
#include "backend_pico_w.h"
#include <string.h>
extern "C" {
  #include "lwip/ip_addr.h"
  #include "lwip/netif.h"
  #include "wireguardif.h"
}

bool PicoWWgBackend::begin(const WgConfig& c) {
  ip_addr_t ipaddr, netmask, gateway;
  if (!ipaddr_aton(c.if_addr, &ipaddr)) return false;
  IP4_ADDR(ip_2_ip4(&netmask), 255, 255, 255, 0);
  ip_addr_set_zero(&gateway);

  struct wireguardif_init_data wg{};
  wg.private_key = c.if_priv;
  wg.listen_port = 51820;          // local listen port; outbound still works behind NAT

  if (!netif_add(&_wgNetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask),
                 ip_2_ip4(&gateway), &wg, &wireguardif_init, &ip_input)) {
    return false;
  }
  netif_set_up(&_wgNetif);

  struct wireguardif_peer peer{};
  wireguardif_peer_init(&peer);
  peer.public_key = c.peer_pub;
  peer.endpoint_port = c.peer_port;
  // Route the whole tailnet through the tunnel.
  ip_addr_t allowed, allowed_mask;
  ipaddr_aton("0.0.0.0", &allowed);
  ipaddr_aton("0.0.0.0", &allowed_mask);
  peer.allowed_ip = *ip_2_ip4(&allowed);
  peer.allowed_mask = *ip_2_ip4(&allowed_mask);
  // Resolve and set the peer endpoint (underlay address).
  ip_addr_t ep;
  if (!ipaddr_aton(c.peer_host, &ep)) {
    // Hostname: resolve via DNS before calling begin(), or pass an IP.
    return false;
  }
  peer.endpoint_ip = ep;

  if (wireguardif_add_peer(&_wgNetif, &peer, &_peerIndex) != ERR_OK ||
      _peerIndex == WIREGUARDIF_INVALID_INDEX) {
    return false;
  }
  if (wireguardif_connect(&_wgNetif, _peerIndex) != ERR_OK) return false;
  _begun = true;
  return true;
}

void PicoWWgBackend::end() {
  if (_begun) {
    wireguardif_disconnect(&_wgNetif, _peerIndex);
    netif_remove(&_wgNetif);
    _begun = false;
  }
}

bool PicoWWgBackend::isUp() {
  if (!_begun) return false;
  bool up = false;
  wireguardif_peer_is_up(&_wgNetif, _peerIndex, NULL, NULL) == ERR_OK ? (up = true) : (up = false);
  return up;
}
#endif
```

> Note: `peer.endpoint_ip` requires an IP. The example/demo resolves the gateway hostname to an IP with `WiFi.hostByName()` before calling `begin()` and passes the IP in the `Endpoint`. Document this in `provisioning.md` (Task 10): "Pico W: use an IP in `Endpoint`, or pre-resolve the hostname."

- [ ] **Step 4: Compile-verify for Pico W** (after Task 6 provides an entry point)

Run: `sudo -u michaelgroberman pio run -e pico-w`
Expected: SUCCESS. If `wireguardif` symbols are unresolved, revisit the Step 1 decision (vendoring vs. library) and the include paths.

- [ ] **Step 5: Commit**

```bash
git add src/backends/backend_pico_w.h src/backends/backend_pico_w.cpp platformio.ini
git commit -m "feat: Pico W WireGuard backend (wireguard-lwip)"
```

---

### Task 6: `minimal_tunnel` example

**Files:**
- Create: `examples/minimal_tunnel/minimal_tunnel.ino`

- [ ] **Step 1: Write the example**

```cpp
// minimal_tunnel — bring up WiFi, dial the gateway over WireGuard, and
// prove tailnet reachability by TCP-connecting to a tailnet host.
#include <TailnetPeer.h>
#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#elif defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
#endif

// ---- EDIT THESE (or move to a secrets.h) -------------------------------
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-pass";
// Paste the device's wg-quick config (Endpoint must be a reachable
// underlay addr — LAN or public IP:port — NOT a 100.x tailnet address).
const char* WG_CONFIG =
  "[Interface]\n"
  "PrivateKey = <device-private-key>\n"
  "Address    = 10.20.30.5/24\n"
  "[Peer]\n"
  "PublicKey  = <gateway-public-key>\n"
  "Endpoint   = 203.0.113.10:51820\n"
  "AllowedIPs = 0.0.0.0/0\n";
const char* TAILNET_TEST_HOST = "100.100.100.100"; // a node on your tailnet
const uint16_t TAILNET_TEST_PORT = 22;
// ------------------------------------------------------------------------

TailnetPeer tailnet;

void setup() {
  Serial.begin(115200);
  delay(300);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[wifi] connecting");
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(400); }
  Serial.printf("\n[wifi] online: %s\n", WiFi.localIP().toString().c_str());

  if (!tailnet.begin(WG_CONFIG)) {
    Serial.printf("[wg] begin failed: %s\n", tailnet.lastError());
    return;
  }
  Serial.printf("[wg] dialing %s ...\n", tailnet.peerEndpoint());
}

void loop() {
  tailnet.tick();
  static TailnetPeer::State last = TailnetPeer::OFF;
  if (tailnet.state() != last) {
    last = tailnet.state();
    if (last == TailnetPeer::UP) {
      Serial.printf("[wg] tunnel UP as %s\n", tailnet.tunnelIP());
      WiFiClient c;
      Serial.printf("[test] connecting to tailnet %s:%u ...\n",
                    TAILNET_TEST_HOST, TAILNET_TEST_PORT);
      Serial.println(c.connect(TAILNET_TEST_HOST, TAILNET_TEST_PORT)
                       ? "[test] reachable — tailnet routing works"
                       : "[test] not reachable (check route advertisement/ACL)");
      c.stop();
    }
  }
  delay(50);
}
```

- [ ] **Step 2: Compile-verify both boards**

Run: `sudo -u michaelgroberman pio ci examples/minimal_tunnel/minimal_tunnel.ino -l . -e esp32-s3`
Then: `sudo -u michaelgroberman pio ci examples/minimal_tunnel/minimal_tunnel.ino -l . -e pico-w`
Expected: both SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add examples/minimal_tunnel/minimal_tunnel.ino
git commit -m "feat: minimal_tunnel example"
```

---

### Task 7: `TcpTokenListener` helper + `reachable_service` example

**Files:**
- Create: `src/TcpTokenListener.h`, `src/TcpTokenListener.cpp`, `examples/reachable_service/reachable_service.ino`

> A small, board-portable helper: a TCP listener that requires a shared token as the first line before serving. Bound to the tunnel so only tailnet clients reach it.

- [ ] **Step 1: Create `src/TcpTokenListener.h`**

```cpp
#pragma once
#include <WiFi.h>
#include <functional>

// Token-gated single-client TCP listener. The client must send the
// token followed by '\n' as its first line; otherwise the connection is
// dropped. On success, `handler(line)` is called per subsequent line and
// its return string is written back.
class TcpTokenListener {
public:
  using Handler = std::function<String(const String& line)>;
  TcpTokenListener(uint16_t port, const char* token) : _srv(port), _token(token) {}
  void begin() { _srv.begin(); }
  void tick();                       // call from loop()
  void onLine(Handler h) { _handler = h; }
private:
  WiFiServer _srv;
  String     _token;
  Handler    _handler;
};
```

- [ ] **Step 2: Create `src/TcpTokenListener.cpp`**

```cpp
#include "TcpTokenListener.h"

void TcpTokenListener::tick() {
  WiFiClient c = _srv.available();
  if (!c) return;
  // First line must be the token.
  String first = c.readStringUntil('\n');
  first.trim();
  if (first != _token) { c.stop(); return; }
  while (c.connected()) {
    if (!c.available()) { delay(5); continue; }
    String line = c.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    if (_handler) c.println(_handler(line));
  }
  c.stop();
}
```

- [ ] **Step 3: Write `examples/reachable_service/reachable_service.ino`**

```cpp
// reachable_service — expose a token-gated TCP service over the tunnel.
#include <TailnetPeer.h>
#include <TcpTokenListener.h>
#include <WiFi.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-pass";
const char* WG_CONFIG = "...";              // as in minimal_tunnel
const char* SERVICE_TOKEN = "change-me-to-a-long-random-token";

TailnetPeer tailnet;
TcpTokenListener listener(6400, SERVICE_TOKEN);

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(400);
  tailnet.begin(WG_CONFIG);
  listener.onLine([](const String& cmd) -> String {
    if (cmd == "ping") return "pong";
    if (cmd == "uptime") return String(millis() / 1000) + "s";
    return "unknown";
  });
}

void loop() {
  tailnet.tick();
  if (tailnet.state() == TailnetPeer::UP) {
    static bool started = false;
    if (!started) { listener.begin(); started = true; }
    listener.tick();
  }
  delay(20);
}
```

- [ ] **Step 4: Compile-verify both boards**

Run: `sudo -u michaelgroberman pio ci examples/reachable_service/reachable_service.ino -l . -e esp32-s3`
Then the same with `-e pico-w`. Expected: both SUCCESS.

- [ ] **Step 5: Commit**

```bash
git add src/TcpTokenListener.h src/TcpTokenListener.cpp examples/reachable_service/
git commit -m "feat: TcpTokenListener helper + reachable_service example"
```

---

### Task 8: Gateway — node-agnostic subnet-router script, ACL, hardening doc

**Files:**
- Create: `gateway/setup-subnet-router.sh`, `gateway/tailscale-acl.example.json`, `gateway/harden.md`

- [ ] **Step 1: Write `gateway/setup-subnet-router.sh`**

```bash
#!/usr/bin/env bash
# Turn ANY existing Tailscale Linux node into a WireGuard subnet router
# for a tailnet-mcu device. Idempotent. Run with sudo on the gateway node.
set -euo pipefail

WG_IF="${WG_IF:-wg0}"
WG_PORT="${WG_PORT:-51820}"
WG_SUBNET="${WG_SUBNET:-10.20.30.0/24}"
GW_ADDR="${GW_ADDR:-10.20.30.1}"
DEV_ADDR="${DEV_ADDR:-10.20.30.5}"
TS_IF="${TS_IF:-tailscale0}"

command -v tailscale >/dev/null || { echo "tailscale not installed"; exit 1; }
tailscale status >/dev/null   || { echo "this node is not on a tailnet (run 'tailscale up')"; exit 1; }
command -v wg >/dev/null      || { echo "install wireguard-tools first"; exit 1; }

umask 077
mkdir -p /etc/wireguard
[ -f /etc/wireguard/gw_private.key ] || wg genkey > /etc/wireguard/gw_private.key
GW_PRIV="$(cat /etc/wireguard/gw_private.key)"
GW_PUB="$(printf '%s' "$GW_PRIV" | wg pubkey)"

# Device keypair (generated here for convenience; move to the device, then
# you may delete dev_private.key from the gateway).
[ -f /etc/wireguard/dev_private.key ] || wg genkey > /etc/wireguard/dev_private.key
DEV_PRIV="$(cat /etc/wireguard/dev_private.key)"
DEV_PUB="$(printf '%s' "$DEV_PRIV" | wg pubkey)"

cat > "/etc/wireguard/${WG_IF}.conf" <<EOF
[Interface]
Address = ${GW_ADDR}/24
ListenPort = ${WG_PORT}
PrivateKey = ${GW_PRIV}

[Peer]
# tailnet-mcu device — AllowedIPs scoped to its /32 (anti-spoof)
PublicKey = ${DEV_PUB}
AllowedIPs = ${DEV_ADDR}/32
EOF

# Forwarding + MASQUERADE from the WG subnet onto the tailnet.
sysctl -w net.ipv4.ip_forward=1
echo 'net.ipv4.ip_forward=1' > /etc/sysctl.d/99-tailnet-mcu.conf
iptables -t nat -C POSTROUTING -s "${WG_SUBNET}" -o "${TS_IF}" -j MASQUERADE 2>/dev/null \
  || iptables -t nat -A POSTROUTING -s "${WG_SUBNET}" -o "${TS_IF}" -j MASQUERADE

systemctl enable --now "wg-quick@${WG_IF}"
tailscale up --advertise-routes="${WG_SUBNET}" --reset

echo "=================================================================="
echo "Gateway public key : ${GW_PUB}"
echo "Approve the route ${WG_SUBNET} in the Tailscale admin console."
echo
echo "Paste this into the device's secrets.h (set Endpoint to THIS node's"
echo "reachable underlay address — LAN IP on-LAN, or public IP if roaming):"
echo "------------------------------------------------------------------"
cat <<EOF
[Interface]
PrivateKey = ${DEV_PRIV}
Address    = ${DEV_ADDR}/24
[Peer]
PublicKey  = ${GW_PUB}
Endpoint   = <THIS_NODE_REACHABLE_ADDR>:${WG_PORT}
AllowedIPs = 0.0.0.0/0
EOF
echo "------------------------------------------------------------------"
echo "Then delete /etc/wireguard/dev_private.key from this gateway."
```

- [ ] **Step 2: Lint the script**

Run: `sudo -u michaelgroberman shellcheck gateway/setup-subnet-router.sh`
Expected: no errors (warnings about `$(cat)` are acceptable; fix any SC2086/quoting errors).

- [ ] **Step 3: Write `gateway/tailscale-acl.example.json`**

```json
{
  "tagOwners": { "tag:iot-gateway": ["autogroup:admin"] },
  "acls": [
    { "action": "accept",
      "src": ["autogroup:member"],
      "dst": ["10.20.30.0/24:*"],
      "comment": "tailnet members may reach the MCU subnet" },
    { "action": "accept",
      "src": ["10.20.30.0/24"],
      "dst": ["tag:iot-gateway:*"],
      "comment": "MCU may reach only the gateway by default — widen deliberately" }
  ]
}
```

- [ ] **Step 4: Write `gateway/harden.md`** with these concrete sections:
  - **Firewall:** allow inbound `udp/51820` only; example `ufw` commands; move SSH onto the tailnet (`ufw deny 22/tcp`, rely on `tailscale ssh` or bind sshd to the tailscale0 address).
  - **Why one open UDP port is safe:** WireGuard is silent to unauthenticated peers (no response, appears filtered to nmap/Shodan).
  - **Scoped AllowedIPs:** the device peer is `/32` — explain cryptokey routing anti-spoof.
  - **Tailscale ACL:** apply `tailscale-acl.example.json`; tag the gateway `tag:iot-gateway`.
  - **ESP32 flash encryption:** link to Espressif flash-encryption docs; warn it is one-way (eFuse) and protects the at-rest WG private key in NVS.
  - **Verification:** from off-network, `nmap -sU -p51820 <gateway>` shows open|filtered with no WG response; from a tailnet node, `ping 10.20.30.5` once the device is up.

- [ ] **Step 5: Commit**

```bash
git add gateway/
git commit -m "feat: node-agnostic subnet-router gateway script, ACL, hardening doc"
```

---

### Task 9: Demo app — `tailnet-sensor-node`

**Files:**
- Create: `app/tailnet-sensor-node/platformio.ini`, `app/tailnet-sensor-node/src/main.cpp`, `app/tailnet-sensor-node/secrets.example.h`

- [ ] **Step 1: Create `app/tailnet-sensor-node/platformio.ini`** (depends on the library by relative path)

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags = -std=gnu++17
lib_deps =
    ciniml/WireGuard-ESP32 @ ^0.1.5
    file://../../          ; the tailnet-mcu library

[env:pico-w]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipicow
framework = arduino
board_build.core = earlephilhower
build_flags = -std=gnu++17
lib_deps = file://../../
```

- [ ] **Step 2: Create `app/tailnet-sensor-node/secrets.example.h`**

```cpp
#pragma once
// Copy to secrets.h (gitignored) and fill in.
#define WIFI_SSID  "your-ssid"
#define WIFI_PASS  "your-pass"
#define SERVICE_TOKEN "change-me-to-a-long-random-token"
// Endpoint must be the gateway's reachable underlay addr (LAN or public IP):
#define WG_CONFIG \
  "[Interface]\n" \
  "PrivateKey = <device-private-key>\n" \
  "Address    = 10.20.30.5/24\n" \
  "[Peer]\n" \
  "PublicKey  = <gateway-public-key>\n" \
  "Endpoint   = 203.0.113.10:51820\n" \
  "AllowedIPs = 0.0.0.0/0\n"
```

- [ ] **Step 3: Create `app/tailnet-sensor-node/src/main.cpp`**

```cpp
// tailnet-sensor-node — serves a sensor reading over the tunnel, to the
// tailnet only. Never exposed publicly. Send "<token>\n" then "read\n".
#include <TailnetPeer.h>
#include <TcpTokenListener.h>
#include <WiFi.h>
#include "../secrets.h"

TailnetPeer tailnet;
TcpTokenListener listener(6400, SERVICE_TOKEN);

static float readSensor() {
#if defined(ARDUINO_ARCH_ESP32)
  return temperatureRead();         // on-die temp (°C) — placeholder sensor
#else
  return (float)analogReadTemp();   // arduino-pico on-die temp (°C)
#endif
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(400);
  if (!tailnet.begin(WG_CONFIG))
    Serial.printf("[wg] %s\n", tailnet.lastError());
  listener.onLine([](const String& cmd) -> String {
    if (cmd == "read")
      return String("{\"temp_c\":") + String(readSensor(), 1) +
             ",\"uptime_s\":" + String(millis() / 1000) + "}";
    return "{\"error\":\"unknown command\"}";
  });
}

void loop() {
  tailnet.tick();
  static bool up = false;
  if (tailnet.state() == TailnetPeer::UP && !up) {
    Serial.printf("[sensor] serving on tunnel %s:6400 (tailnet-only)\n",
                  tailnet.tunnelIP());
    listener.begin();
    up = true;
  }
  if (up) listener.tick();
  delay(20);
}
```

- [ ] **Step 4: Compile-verify both boards**

Run (after copying `secrets.example.h` → `secrets.h`):
`cd app/tailnet-sensor-node && sudo -u michaelgroberman pio run -e esp32-s3 && sudo -u michaelgroberman pio run -e pico-w`
Expected: both SUCCESS. (Use `git -C` style or a subshell to avoid leaving the cwd changed.)

- [ ] **Step 5: Commit**

```bash
git add app/tailnet-sensor-node/platformio.ini app/tailnet-sensor-node/src/main.cpp app/tailnet-sensor-node/secrets.example.h
git commit -m "feat: tailnet-sensor-node demo app"
```

---

### Task 10: Documentation

**Files:**
- Create: `README.md`, `docs/architecture.md`, `docs/provisioning.md`, `docs/security-model.md`

- [ ] **Step 1: Write `README.md`** containing, in order:
  1. **One-line pitch** + a **bold honest disclaimer**: "The MCU does not run Tailscale — it runs WireGuard and joins your tailnet through a subnet router (an existing Tailscale node)."
  2. The ASCII architecture diagram from the spec (§2).
  3. **Quick start** (numbered): run `gateway/setup-subnet-router.sh` on an existing Tailscale node → approve the route → copy the printed config into `secrets.h` → flash an example.
  4. **Supported boards** table: ESP32-S3 (recommended WiFi default), ESP32 family, Raspberry Pi Pico W. Note the Pico W `Endpoint`-must-be-IP caveat.
  5. **Library usage** snippet (the `TailnetPeer` 6-line happy path).
  6. **Security** section linking `docs/security-model.md`; one sentence on scanner-silence.
  7. Badges placeholder line (CI, license) — wired in Task 11/12.

- [ ] **Step 2: Write `docs/architecture.md`** — portable-core-vs-backends split; the `WgBackend` interface; why the state machine polls (`isUp()`); the endpoint-reachability constraint and the no-`100.x` rule with the chicken-and-egg explanation; the node-agnostic gateway table from spec §2.2.

- [ ] **Step 3: Write `docs/provisioning.md`** — compile-time `secrets.h` vs runtime `loadConfigFromText()` over serial; how to obtain keys from `setup-subnet-router.sh`; the three Endpoint scenarios (on-LAN / home+DDNS / public); **Pico W: Endpoint must be an IP (or pre-resolve the hostname with `WiFi.hostByName`)**.

- [ ] **Step 4: Write `docs/security-model.md`** — the spec §6 content in full: encrypted-by-construction; scanner-silence; nothing public behind the tunnel; the 5 enforced defaults (scoped AllowedIPs, ACL+tag, firewall, flash encryption, listener token); the verification steps (nmap silence, tailnet ping).

- [ ] **Step 5: Commit**

```bash
git add README.md docs/architecture.md docs/provisioning.md docs/security-model.md
git commit -m "docs: README + architecture, provisioning, security-model"
```

---

### Task 11: CI — compile matrix + native tests + shellcheck

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Write `.github/workflows/ci.yml`**

```yaml
name: CI
on: [push, pull_request]
jobs:
  test-native:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.12' }
      - run: pip install platformio
      - run: pio test -e native
  build-boards:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        env: [esp32-s3, pico-w]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.12' }
      - run: pip install platformio
      - run: pio ci examples/minimal_tunnel/minimal_tunnel.ino -l . -e ${{ matrix.env }}
      - run: pio ci examples/reachable_service/reachable_service.ino -l . -e ${{ matrix.env }}
  shellcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y shellcheck
      - run: shellcheck gateway/setup-subnet-router.sh
```

- [ ] **Step 2: Validate YAML locally**

Run: `sudo -u michaelgroberman python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml')); print('ok')"`
Expected: `ok`.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: native tests + board compile matrix + shellcheck"
```

---

### Task 12: Repo-protection set & publish (GATED — confirm with user before any GitHub/network action)

> Per the standing repo-protection rule, these must exist before the repo is declared public-ready. Steps 1–2 are local file commits. Step 3 is an **outward-facing publish action** — do NOT run it without explicit user go-ahead.

- [ ] **Step 1: Create community-health files** (real content, not stubs):
  - `SECURITY.md` — how to report a vuln (private), supported versions, the LinkedIn/contact line.
  - `CONTRIBUTING.md` — build/test commands (`pio test -e native`, `pio ci ...`), PR expectations, DCO/sign-off note.
  - `CODE_OF_CONDUCT.md` — Contributor Covenant.
  - `CODEOWNERS` — `* @MichaelAdamGroberman`.
  - `.github/ISSUE_TEMPLATE/bug_report.md`, `.github/ISSUE_TEMPLATE/feature_request.md`, `.github/PULL_REQUEST_TEMPLATE.md`.

- [ ] **Step 2: Commit the protection set**

```bash
git add SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md CODEOWNERS .github/
git commit -m "chore: community health files + repo-protection set"
```

- [ ] **Step 3: Publish (ONLY after explicit user confirmation)**

```bash
# Confirm GitHub login defaults to MichaelAdamGroberman.
sudo -u michaelgroberman gh repo create tailnet-mcu --public --source=. --remote=origin --push
```
Then enable branch protection on `main`, Dependabot/secret-scanning alerts, and add CI + license README badges. Add the repo to `.claude-plugin/marketplace.json` if applicable. Set up fork/star monitoring per the standing rule.

---

## Self-Review

**Spec coverage:**
- §2 architecture (portable core + 2 backends) → Tasks 2,3,4,5 ✓
- §2.1 endpoint reachability / no-100.x → documented in Tasks 6 (example comment), 10 (architecture+provisioning) ✓
- §2.2 node-agnostic gateway → Task 8 ✓
- §3 repo structure → Tasks 1–12 collectively ✓
- §4 TailnetPeer API → Task 3 ✓
- §5 provisioning (secrets.h + runtime) → Tasks 6,9 + `loadConfigFromText` in Task 3; documented Task 10 ✓
- §6 security defaults → Task 8 (scoped AllowedIPs, ACL, firewall, flash-enc) + Task 7/9 (token) + Task 10 (security-model.md) ✓
- §7 testing (native unit + CI matrix + manual hw) → Tasks 2,3 (unit), 11 (CI), 8/12 (manual verification steps) ✓
- §8 definition of done → Tasks 11 (CI green), 12 (repo-protection) ✓

**Placeholder scan:** No "TBD/TODO" in code steps. The one genuine unknown — the Pico W WireGuard library name — is handled as an explicit spike (Task 5 Step 1) with a leading candidate and a "match the actual header" instruction, not a fabricated API. Doc tasks (10) specify exact required sections + facts rather than "write docs."

**Type consistency:** `WgConfig` fields (`if_addr`, `if_priv`, `peer_pub`, `peer_host`, `peer_port`, `valid`) used identically in Tasks 2,3,4,5. `WgBackend` methods (`begin`/`end`/`isUp`) consistent across the interface (Task 3) and both backends (Tasks 4,5). `TailnetPeer` API (`begin`/`tick`/`stop`/`state`/`tunnelIP`/`peerEndpoint`/`lastError`/`State::{OFF,STARTING,UP,FAILED}`) consistent across Tasks 3,6,7,9. `TcpTokenListener(port, token)` + `onLine`/`begin`/`tick` consistent across Tasks 7,9.

---

# Revision 2 Addendum — radio modes & transports (2026-05-24)

**What changed:** WiFi and BT are now mutually-exclusive runtime transports; a
`RadioManager` enforces WiFi⊕BT. A `ServiceTransport` abstraction serves the
same user `Handler` over BLE (BT mode) or token-gated TCP (WiFi mode). The
`TailnetPeer` WireGuard tunnel is now **optional** — started only in WiFi mode
when a config is supplied. BLE: ESP32 NimBLE (proven) + Pico W BTstack (spike,
may degrade to documented-stub).

**Revised task order:** 1 ✓ → 2 (parser) → **2A RadioManager** → 3 (TailnetPeer,
now optional) → **2B ServiceTransport + token gate + TCP** → **2C BLE transports**
→ 4,5 (WG backends) → 6 (minimal_tunnel) → 7 (→ **superseded by 2B/2C + new
`mode_switch` example**) → 8 (gateway) → 9 (demo, **amended**) → 10 (docs,
**amended**) → 11 (CI, add BLE) → 12.

---

### Task 2A: `RadioManager` — WiFi⊕BT invariant (TDD, host-native)

**Files:** Create `src/RadioManager.h`, `src/RadioManager.cpp`; Test `test/test_radiomanager/test_radiomanager.cpp`

- [ ] **Step 1: Write failing tests** — `test/test_radiomanager/test_radiomanager.cpp`:
```cpp
#include <unity.h>
#include "RadioManager.h"

// Fake radio hooks: track on/off state and assert WiFi & BT are NEVER both on.
struct FakeHooks : RadioHooks {
  bool wifi=false, bt=false; bool everBoth=false; bool failNext=false;
  bool startWifi() override { if(failNext){failNext=false;return false;} wifi=true; check(); return true; }
  void stopWifi()  override { wifi=false; }
  bool startBt()   override { if(failNext){failNext=false;return false;} bt=true; check(); return true; }
  void stopBt()    override { bt=false; }
  void check(){ if(wifi&&bt) everBoth=true; }
};
void setUp(){} void tearDown(){}

void test_starts_off(){ FakeHooks h; RadioManager r(&h); TEST_ASSERT_EQUAL(RadioManager::OFF, r.mode()); }
void test_off_to_wifi(){ FakeHooks h; RadioManager r(&h);
  TEST_ASSERT_TRUE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::WIFI, r.mode()); TEST_ASSERT_TRUE(h.wifi); TEST_ASSERT_FALSE(h.bt); }
void test_wifi_to_bt_is_exclusive(){ FakeHooks h; RadioManager r(&h);
  r.setMode(RadioManager::WIFI); TEST_ASSERT_TRUE(r.setMode(RadioManager::BT));
  TEST_ASSERT_EQUAL(RadioManager::BT, r.mode());
  TEST_ASSERT_FALSE(h.wifi); TEST_ASSERT_TRUE(h.bt);
  TEST_ASSERT_FALSE(h.everBoth); }      // never both on at once
void test_same_mode_noop(){ FakeHooks h; RadioManager r(&h);
  r.setMode(RadioManager::WIFI); TEST_ASSERT_TRUE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::WIFI, r.mode()); }
void test_start_failure_leaves_off(){ FakeHooks h; RadioManager r(&h);
  h.failNext=true; TEST_ASSERT_FALSE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::OFF, r.mode()); TEST_ASSERT_FALSE(h.wifi); }
int main(int,char**){ UNITY_BEGIN();
  RUN_TEST(test_starts_off); RUN_TEST(test_off_to_wifi);
  RUN_TEST(test_wifi_to_bt_is_exclusive); RUN_TEST(test_same_mode_noop);
  RUN_TEST(test_start_failure_leaves_off); return UNITY_END(); }
```

- [ ] **Step 2: Create `src/RadioManager.h`**
```cpp
#pragma once
// Board radio control behind an interface so the WiFi⊕BT invariant is
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
```

- [ ] **Step 3: run native test → FAIL.** `sudo -u michaelgroberman pio test -e native -d <repo> -f test_radiomanager`

- [ ] **Step 4: Create `src/RadioManager.cpp`**
```cpp
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
```

- [ ] **Step 5: native test → PASS.** **Step 6: commit** `feat: RadioManager enforces WiFi-XOR-BT (TDD)`.

---

### Task 2B: `ServiceTransport` + constant-time token gate + TCP transport

> Supersedes Task 7's `TcpTokenListener`. The token gate is host-tested (security-relevant); the Arduino transport is compile-gated.

**Files:** Create `src/transport/token_gate.{h,cpp}`, `src/transport/service_transport.h`, `src/transport/tcp_service_transport.{h,cpp}`; Test `test/test_token_gate/test_token_gate.cpp`

- [ ] **Step 1: Write failing token-gate tests** — `test/test_token_gate/test_token_gate.cpp`:
```cpp
#include <unity.h>
#include "transport/token_gate.h"
void setUp(){} void tearDown(){}
void test_equal_true(){ TEST_ASSERT_TRUE(tokenEquals("s3cret-token","s3cret-token")); }
void test_diff_false(){ TEST_ASSERT_FALSE(tokenEquals("s3cret-token","wrong-token!!")); }
void test_prefix_false(){ TEST_ASSERT_FALSE(tokenEquals("s3cret","s3cret-token")); }
void test_empty_inputs(){ TEST_ASSERT_FALSE(tokenEquals("", "x")); TEST_ASSERT_FALSE(tokenEquals(0,"x")); }
int main(int,char**){ UNITY_BEGIN();
  RUN_TEST(test_equal_true); RUN_TEST(test_diff_false);
  RUN_TEST(test_prefix_false); RUN_TEST(test_empty_inputs); return UNITY_END(); }
```

- [ ] **Step 2: `src/transport/token_gate.h`**
```cpp
#pragma once
// Constant-time token comparison: folds length + every byte into one diff
// accumulator so compare time does not depend on the matching prefix length.
bool tokenEquals(const char* got, const char* expected);
```

- [ ] **Step 3: native test → FAIL.**

- [ ] **Step 4: `src/transport/token_gate.cpp`**
```cpp
#include "transport/token_gate.h"
#include <string.h>
bool tokenEquals(const char* got, const char* expected) {
  if (!got || !expected) return false;
  size_t lg = strlen(got), le = strlen(expected);
  unsigned char diff = (unsigned char)((lg ^ le) != 0);
  for (size_t i = 0; i < le; i++) {
    unsigned char g = (i < lg) ? (unsigned char)got[i] : 0;
    diff |= (unsigned char)(g ^ (unsigned char)expected[i]);
  }
  return diff == 0;
}
```
> Native include path: tests reference `transport/token_gate.h`; ensure the `native` env build flags add `-Isrc` (add `build_src_flags = -Isrc` or `-I src` to `[env:native]` if includes don't resolve).

- [ ] **Step 5: native test → PASS. Commit** `feat: constant-time token gate (TDD)`.

- [ ] **Step 6: `src/transport/service_transport.h`** (Arduino; compile-gated by use in board envs)
```cpp
#pragma once
#include <Arduino.h>
#include <functional>
// One Handler, served over whichever radio is active.
class ServiceTransport {
public:
  using Handler = std::function<String(const String& line)>;
  virtual ~ServiceTransport() {}
  virtual bool begin() = 0;   // start listening/advertising
  virtual void tick()  = 0;   // call from loop()
  void onLine(Handler h) { _handler = h; }
protected:
  Handler _handler;
};
```

- [ ] **Step 7: `src/transport/tcp_service_transport.{h,cpp}`**
```cpp
// tcp_service_transport.h
#pragma once
#include "service_transport.h"
#include <WiFi.h>
class TcpServiceTransport : public ServiceTransport {
public:
  TcpServiceTransport(uint16_t port, const char* token) : _srv(port), _token(token) {}
  bool begin() override { _srv.begin(); return true; }
  void tick() override;
private:
  WiFiServer _srv;
  String _token;
};
```
```cpp
// tcp_service_transport.cpp
#include "transport/tcp_service_transport.h"
#include "transport/token_gate.h"
void TcpServiceTransport::tick() {
  WiFiClient c = _srv.available();
  if (!c) return;
  String first = c.readStringUntil('\n'); first.trim();
  if (!tokenEquals(first.c_str(), _token.c_str())) { c.stop(); return; }  // silent drop
  while (c.connected()) {
    if (!c.available()) { delay(5); continue; }
    String line = c.readStringUntil('\n'); line.trim();
    if (line.length() == 0) break;
    if (_handler) c.println(_handler(line));
  }
  c.stop();
}
```

- [ ] **Step 8: compile-verify on esp32-s3 (via an example or the demo). Commit** `feat: ServiceTransport interface + token-gated TCP transport`.

---

### Task 2C: BLE transports — NimBLE (ESP32) + BTstack (Pico W spike)

**Files:** Create `src/transport/ble_service_transport.h`, `src/transport/ble/ble_nimble_esp32.cpp`, `src/transport/ble/ble_btstack_pico.cpp`; Modify `platformio.ini` (NimBLE dep on esp32-s3; BTstack note on pico-w).

**GATT contract (both backends implement identically):**
- Advertise as the given service name. NUS-style service.
- **RX characteristic** (write / write-no-response): client writes one command line. The **first** line on a fresh connection must equal the token (`tokenEquals`); otherwise ignore further writes / disconnect.
- **TX characteristic** (notify): the `Handler`'s return string is sent as a notification.

- [ ] **Step 1: `src/transport/ble_service_transport.h`**
```cpp
#pragma once
#include "service_transport.h"
// Compile-gated backend: ESP32 -> NimBLE; RP2040 -> BTstack (spike).
class BleServiceTransport : public ServiceTransport {
public:
  BleServiceTransport(const char* name, const char* token);
  bool begin() override;
  void tick() override;
private:
  const char* _name;
  const char* _token;
  bool _authed = false;   // first valid token seen this connection
};
```

- [ ] **Step 2: ESP32 NimBLE backend** — `src/transport/ble/ble_nimble_esp32.cpp`, guarded `#if defined(ARDUINO_ARCH_ESP32)`. Use `h2zero/NimBLE-Arduino @ ^2.2.0` (proven in the buddy). Implement: `NimBLEDevice::init(_name)`, create a server + NUS service (RX `6e400002-...`, TX `6e400003-...`, service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`), an `onWrite` callback that buffers to newline, gates the first line with `tokenEquals`, calls `_handler`, and notifies the TX char with the response. `begin()` starts advertising; `tick()` is a no-op (NimBLE is callback-driven). Add `lib_deps += h2zero/NimBLE-Arduino @ ^2.2.0` to `[env:esp32-s3]`.

- [ ] **Step 3: Pico W BTstack backend — SPIKE** — `src/transport/ble/ble_btstack_pico.cpp`, guarded `#if defined(ARDUINO_ARCH_RP2040)`. Confirm the arduino-pico BTstack BLE peripheral API (the core ships BTstack; check `pio pkg show` / arduino-pico docs for the `BTstackLib`/`BLEServer`-style peripheral API). Implement the same NUS GATT contract. **If BTstack peripheral GATT proves too costly to integrate cleanly in this session, degrade to:** a compile-gated stub whose `begin()` returns `false` and logs "BLE not yet supported on Pico W — see docs/roadmap", and document the limitation in README + `docs/architecture.md`. Record the decision in the commit message. Do NOT block the rest of the plan on this.

- [ ] **Step 4: compile-verify esp32-s3 (BLE must link). pico-w must at least compile (real backend or stub). Commit** `feat: BLE service transport — NimBLE (ESP32) + Pico W BTstack/stub`.

---

### Amendments to existing tasks

- **Task 7 (TcpTokenListener + reachable_service):** SUPERSEDED. The listener becomes `TcpServiceTransport` (Task 2B). Replace the `reachable_service` example with a **`mode_switch` example**: boots in `DEFAULT_MODE`, serves a `Handler` over the active transport, and switches WiFi⇄BT on the reserved `mode wifi`/`mode bt` command via `RadioManager`. Compile both boards.

- **Task 9 (demo app):** AMENDED. `tailnet-sensor-node` now:
  1. Builds a `RadioManager` with real `RadioHooks` (ESP32: `WiFi.mode`/NimBLE deinit; Pico W: CYW43/BTstack).
  2. Boots into `DEFAULT_MODE` (from `secrets.h`).
  3. Binds the sensor `Handler` to the active transport (`BleServiceTransport` in BT, `TcpServiceTransport` in WiFi).
  4. In WiFi mode, **if `WG_CONFIG` is defined**, brings up `TailnetPeer` (optional); otherwise serves LAN-only.
  5. Handles `mode wifi`/`mode bt` to switch radios.
  `secrets.example.h` adds `DEFAULT_MODE`, `SERVICE_TOKEN`, and makes `WG_CONFIG` clearly optional (commented how to omit).

- **Task 10 (docs):** AMENDED. README + `docs/architecture.md` must cover the WiFi⊕BT model and `RadioManager`. Add **`docs/provisioning.md` section "Enabling the optional Tailscale tunnel"** — step-by-step opt-in: run the gateway script, get the config, define `WG_CONFIG`; explain that omitting it keeps WiFi mode LAN-only and BT mode unaffected. `docs/security-model.md` adds the BLE-transport bullet (local-range, token-gated, LE Secure Connections recommended).

- **Task 11 (CI):** add NimBLE to the esp32-s3 example builds; ensure native test job runs `test_radiomanager` and `test_token_gate` too (it runs all `test/` dirs by default).

### Revision 2 self-review

- Spec §2 (radio modes/transports) → Tasks 2A, 2B, 2C ✓
- Spec §2 (Tailscale optional) → Task 9 amendment (conditional `TailnetPeer`) ✓
- Spec §4 (RadioManager/ServiceTransport API) → Tasks 2A, 2B ✓
- Spec §6 (BLE local-range + token, constant-time) → Tasks 2B (token gate), 2C (BLE), 10 ✓
- Spec §7 (RadioManager + token-gate host tests) → Tasks 2A, 2B ✓
- **Type consistency (Rev 2):** `RadioManager::Mode::{OFF,WIFI,BT}` + `setMode`/`mode` consistent (2A, 9, mode_switch). `RadioHooks::{startWifi,stopWifi,startBt,stopBt}` consistent (2A, 9). `ServiceTransport::{begin,tick,onLine,Handler}` consistent (2B, 2C, 7→mode_switch, 9). `tokenEquals(got, expected)` consistent (2B, TCP + BLE). `TailnetPeer` unchanged.
