<!-- Badges resolve once the repo is published on GitHub and CI has run -->
![CI](https://github.com/MichaelAdamGroberman/tailnet-mcu/actions/workflows/ci.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![PlatformIO Registry](https://badges.registry.platformio.org/packages/michaeladamgroberman/library/tailnet-mcu.svg)

# tailnet-mcu

Arduino/PlatformIO library giving an **ESP32 or Raspberry Pi Pico W** two
mutually-exclusive radio modes — **WiFi or Bluetooth LE** — and, optionally
in WiFi mode, a WireGuard tunnel that joins the device to a Tailscale network.

> **Important:** This library does **NOT** run Tailscale on the MCU. It runs
> a standard WireGuard client and joins your tailnet via a **subnet router** —
> any existing Tailscale Linux node that advertises the device's WireGuard
> subnet. The MCU never dials a `100.x` address; it dials an underlay LAN or
> public IP. See [docs/architecture.md](docs/architecture.md) for why.

---

## What it provides

### WiFi or Bluetooth LE — never both

A `RadioManager` guarantees exactly one radio stack is ever running. On
single-radio chips (e.g. ESP32-PICO-D4) WiFi + BLE heaps cannot coexist.
`RadioManager` always powers down the active radio before bringing up the
next, so you pay for one stack's heap at a time.

Modes switch at boot via `DEFAULT_MODE_WIFI` / the absent macro, or at
runtime via a `"mode wifi"` / `"mode bt"` command through whichever transport
is active.

### One service handler, two transports

Write your service logic once (e.g. `"read" → sensor JSON`). Bind it to
whichever `ServiceTransport` the active mode provides:

- **`TcpServiceTransport`** — token-gated TCP, WiFi mode. Reachable
  tailnet-wide if the optional tunnel is up; LAN-only otherwise.
- **`BleServiceTransport`** — BLE GATT (NUS-style), BT mode. Local range,
  no IP, no tunnel.

### Optional WireGuard tunnel (WiFi mode only)

`TailnetPeer` is started only when a `wg-quick` config is supplied and only
while in WiFi mode. Omit the config and WiFi mode still works — the device
serves over plain LAN TCP with no tunnel required.

```
[ESP32-S3 / Pico W]              [Existing Tailscale node]        [Your tailnet]
  TailnetPeer  ── WireGuard ──>  wg0  +  tailscaled         <---> phone, laptop,
  (one UDP peer    (ChaCha20-     (advertise-routes               other nodes
   dialed by        Poly1305,      10.20.30.0/24;
   underlay addr)   scanner-       MASQUERADE -> tailscale0)
                    silent)

                    ┌──────────── RadioManager (WiFi XOR BT) ────────────┐
  boot/cmd ──>      │   OFF <-> WIFI <-> BT   stopActive() before next   │
                    └───────┬──────────────────────────────┬─────────────┘
                            │ WIFI                          │ BT
                   ┌────────▼──────────┐           ┌────────▼─────────┐
  one Handler      │ TcpServiceTransport│           │BleServiceTransport│  same Handler
  (your logic)     │ + OPTIONAL         │           │ (NimBLE/BTstack) │  (your logic)
                   │   TailnetPeer      │           └──────────────────┘
                   └────────────────────┘
```

---

## Board support

| Board | WiFi | BLE | WireGuard tunnel |
|-------|------|-----|-----------------|
| **ESP32-S3** (recommended) | Yes | Yes (NimBLE) | Yes — full, verified |
| ESP32 family | Yes | Yes (NimBLE) | Yes |
| Raspberry Pi Pico W | Yes | **Experimental — compile-gated stub** (see note) | Yes |

**Pico W BLE note:** BTstack requires a single global `gattWriteCallback`
that a library cannot own without colliding with the application. The BLE
transport on Pico W compiles as a stub that logs clearly. A v2 design
(app-forwarded-hook) is tracked in [docs/roadmap.md](docs/roadmap.md).
ESP32 with NimBLE is the recommended path for BLE use cases.

---

## Quick start

### 1. Provision the gateway

Run on any existing Tailscale Linux node (on-LAN is free; add a public IP or
DDNS + UDP-forward if you want the device reachable from anywhere):

```bash
sudo WG_SUBNET=10.20.30.0/24 GW_ADDR=10.20.30.1 DEV_ADDR=10.20.30.5 \
  bash gateway/setup-subnet-router.sh
```

The script prints a ready-to-paste `wg-quick` config block for the device.
Then in the Tailscale admin console, approve the advertised route
(`10.20.30.0/24`).

### 2. Configure `secrets.h`

```bash
cp app/tailnet-sensor-node/secrets.example.h app/tailnet-sensor-node/secrets.h
# Edit secrets.h — set WIFI_SSID, WIFI_PASS, SERVICE_TOKEN
# Paste the WG_CONFIG block printed by the gateway script (optional)
```

### 3. Flash

```bash
# Using PlatformIO bundled pio (required for ESP32 — see Python note below)
~/.platformio/penv/bin/pio run -e esp32-s3 -d app/tailnet-sensor-node --target upload
# or for Pico W:
~/.platformio/penv/bin/pio run -e pico-w -d app/tailnet-sensor-node --target upload
```

---

## Library usage

```cpp
#include <RadioManager.h>
#include <TailnetPeer.h>
#include <transport/tcp_service_transport.h>
#include <transport/ble_service_transport.h>

// 1. Implement RadioHooks with your board's real WiFi/BLE bring-up code
class MyHooks : public RadioHooks {
public:
  bool startWifi() override { /* WiFi.begin(...); tcpTransport.begin(); */ return true; }
  void stopWifi()  override { /* WiFi.disconnect(true); WiFi.mode(WIFI_OFF); */ }
  bool startBt()   override { /* bleTransport.begin(); */ return true; }
  void stopBt()    override { /* NimBLEDevice::deinit(true); */ }
};

MyHooks hooks;
RadioManager radio(&hooks);

TcpServiceTransport tcpTransport(4242, SERVICE_TOKEN);
BleServiceTransport bleTransport("my-device", SERVICE_TOKEN);
ServiceTransport* activeTransport = nullptr;

// 2. Write your service handler once
ServiceTransport::Handler handler = [](const String& cmd) -> String {
  if (cmd == "read") return "{\"value\":42}";
  if (cmd == "mode wifi") { radio.setMode(RadioManager::WIFI); return "ok"; }
  if (cmd == "mode bt")   { radio.setMode(RadioManager::BT);   return "ok"; }
  return "unknown";
};

void setup() {
  radio.setMode(RadioManager::WIFI); // or BT
  if (activeTransport) activeTransport->onLine(handler);
}

void loop() {
  if (activeTransport) activeTransport->tick();
  delay(5);
}
```

### Enabling the optional Tailscale tunnel

See [docs/provisioning.md — "Enabling the optional Tailscale tunnel"](docs/provisioning.md#enabling-the-optional-tailscale-tunnel).
In short: define `WG_CONFIG` in `secrets.h` with the config block from the
gateway script, then start `TailnetPeer` inside `startWifi()`:

```cpp
#ifdef WG_CONFIG
TailnetPeer tailnet;
// inside startWifi():
if (tailnet.begin(WG_CONFIG))
  Serial.println("Tunnel starting: " + String(tailnet.peerEndpoint()));
// in loop():
tailnet.tick();
#endif
```

---

## Build and test

### Python requirement for ESP32

The ESP32 environment is pinned to `pioarduino/platform-espressif32`
v55.03.35 (arduino-esp32 3.x). The espressif32 platform **requires
Python ≤ 3.13**. If `python3 --version` reports 3.14+ (e.g. from Homebrew),
use the PlatformIO bundled interpreter instead:

```bash
# Use the bundled pio — always has the right Python
~/.platformio/penv/bin/pio run -e esp32-s3 -d app/tailnet-sensor-node
```

CI pins Python 3.12 for both jobs.

### Run host-native unit tests (no hardware)

```bash
~/.platformio/penv/bin/pio test -e native
# Expected: 20/20 tests pass
# Covers: WgConfig parser, RadioManager WiFi-XOR-BT invariant,
#         TailnetPeer state machine (FakeBackend), token gate constant-time compare
```

### Build demo for each board (compile check)

```bash
~/.platformio/penv/bin/pio run -e esp32-s3 -d app/tailnet-sensor-node
~/.platformio/penv/bin/pio run -e pico-w   -d app/tailnet-sensor-node
```

---

## Security

WireGuard is encrypted by construction (ChaCha20-Poly1305). The gateway
port is silent to unauthenticated scanners. Nothing behind the tunnel is
reachable from the public internet. BLE and TCP are both token-gated.

See [docs/security-model.md](docs/security-model.md) for the full security
model and [gateway/harden.md](gateway/harden.md) for gateway-specific
hardening steps.

---

## Examples

- [`examples/minimal_tunnel/`](examples/minimal_tunnel/) — WiFi mode + optional WireGuard tunnel; reach a host inside your tailnet.
- [`examples/mode_switch/`](examples/mode_switch/) — Boot in BT mode, switch to WiFi at runtime via a serial command.

---

## License

MIT. Portions derive from the BSD-licensed `wireguard-lwip`; its license is
preserved in vendored sources.
