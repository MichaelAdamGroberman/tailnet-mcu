# tailnet-mcu — Architecture

## Overview

The library is split into a **portable core** and two sets of **board
backends** — one per radio/chip combination. The portable core contains all
the interesting logic and is fully host-testable with no hardware. Board
backends are compile-gated by `#ifdef` and covered by the CI compile matrix.

---

## Portable core (host-testable)

These files compile on any POSIX host under the `native` PlatformIO env and
are exercised by the Unity unit-test suite (20/20 passing).

### `RadioManager` — WiFi XOR BT invariant

`RadioManager` holds the current `Mode` (`OFF`, `WIFI`, or `BT`) and a
pointer to a `RadioHooks` interface. On every `setMode()` call it:

1. Calls the stop hook for the currently active radio (if any).
2. Calls the start hook for the new radio.
3. Updates the stored mode only on success.

The invariant — exactly one radio stack is ever running — is enforced here,
not in the application. On single-radio chips (e.g. ESP32-PICO-D4) running
both WiFi and BLE simultaneously starves one stack of heap. The XOR design
makes the failure mode structurally impossible.

`RadioHooks` is a pure-virtual interface (`startWifi`, `stopWifi`, `startBt`,
`stopBt`). Applications subclass it with their real board bring-up code;
tests inject a `FakeHooks` that records calls.

### `WgConfig` / `wgConfigParse` — wg-quick config parser

Parses a standard `wg-quick` text block (`[Interface]` PrivateKey / Address,
`[Peer]` PublicKey / Endpoint / AllowedIPs) into a fixed-buffer `WgConfig`
struct (no heap). Key behaviors:

- Strips the CIDR suffix from the Interface `Address` (backends want a bare IP).
- Splits `host:port` from the Peer `Endpoint`.
- Sets `valid = true` only when all four required fields are present.
- Ignores section headers, comments (`#`), and unknown keys.

### `TailnetPeer` — WireGuard tunnel state machine

Manages the lifecycle `WG_OFF → WG_STARTING → WG_UP → WG_FAILED` for an
optional WireGuard tunnel. Responsibilities:

- Calls `wgConfigParse` on the supplied text.
- Delegates the actual WireGuard handshake to a `WgBackend` instance.
- Polls `backend.isUp()` in `tick()` to detect the first handshake.
- Exposes `tunnelIP()`, `peerEndpoint()`, `state()`, and `lastError()`.

`TailnetPeer` is **optional and only meaningful in WiFi mode**. Without a
`WG_CONFIG`, the application simply does not instantiate or start it; WiFi
mode works over plain LAN TCP.

### `ServiceTransport` — transport abstraction

A pure-virtual base with:
```cpp
virtual bool begin() = 0;
virtual void tick()  = 0;
void onLine(Handler h);  // concrete — stores the handler
```

`Handler` is `std::function<String(const String& line)>`. Applications write
one handler and bind it to whichever transport the active `RadioMode` selects.
Because the handler is registered via `onLine()` after mode transition, the
same function serves both transports without the application knowing which is
active.

### `token_gate` — constant-time token comparison

Used by both `TcpServiceTransport` and `BleServiceTransport`. The comparison
is timing-safe (constant-time over the full token length) to prevent
timing-oracle attacks on the shared token.

---

## Board backends

### ESP32 — WireGuard backend (`backend_esp32.cpp`)

Wraps `ciniml/WireGuard-ESP32` (lwIP-based). Implements `WgBackend::begin()`
(configure keys + peer), `end()`, and `isUp()` (polls the library's
handshake-received flag). Active only when `ARDUINO_ARCH_ESP32` is defined.

### ESP32 — BLE transport (`ble_nimble_esp32.cpp`)

Implements `BleServiceTransport` using NimBLE-Arduino (`h2zero/NimBLE-Arduino`
v2.x). Exposes a NUS-style GATT service with one write characteristic (command
in) and one notify characteristic (response out). Token gate applied on each
write. Active only when `ARDUINO_ARCH_ESP32` is defined.

### Pico W — WireGuard backend (`backend_pico_w.cpp`)

Wraps `jaszczurtd/arduino-wireguard-pico-w` (lwIP-based, Earle Philhower
arduino-pico core). Same `WgBackend` shape as the ESP32 backend. Active only
when `ARDUINO_ARCH_RP2040` is defined.

### Pico W — BLE transport (`ble_btstack_pico.cpp`) — STUB, experimental

The BTstack library (bundled with arduino-pico) requires a **single global
`gattWriteCallback`** function. A reusable library cannot own that callback
without colliding with the application. The current implementation compiles
as a clearly-labeled stub that prints a log message. This is a known
limitation tracked in [docs/roadmap.md](roadmap.md).

---

## Endpoint reachability constraint

The MCU dials a plain WireGuard UDP handshake with no NAT traversal (no DERP,
no hole-punching — those are Tailscale-client-only features). It therefore
needs **exactly one peer with a reachable UDP endpoint** at boot:

- The MCU must dial an **underlay** address: LAN IP or public IP:port.
- It must **never** dial a `100.x` Tailscale address or a MagicDNS name.
  The device is not on the tailnet until after the tunnel is up — dialing
  `100.x` is a chicken-and-egg deadlock.
- Tailscale Funnel and Serve cannot help: they are HTTPS/TCP only and cannot
  carry WireGuard's UDP.

| Gateway location | MCU dials | Reachable from |
|---|---|---|
| Same LAN as MCU | gateway LAN IP `:51820` | that LAN only |
| Public (cloud VPS or home + DDNS + UDP-forward) | public addr `:51820` | anywhere |
| Behind NAT, no public endpoint | — | cannot (no WG NAT traversal) |

---

## Node-agnostic gateway

The gateway is **not** a dedicated host — it is any Linux machine already
running `tailscaled`. `gateway/setup-subnet-router.sh` detects `tailscaled`,
generates WireGuard keypairs, writes `/etc/wireguard/wg0.conf`, enables IP
forwarding, adds `MASQUERADE` toward `tailscale0`, and starts the WireGuard
interface. It is idempotent and prints the device's config block on success.

---

## Host-test seam

The native test env (`pio test -e native`) compiles only the portable sources:
`WgConfig.cpp`, `RadioManager.cpp`, `TailnetPeer.cpp`,
`transport/token_gate.cpp`. Board-specific backends and Arduino headers are
excluded. Tests inject `FakeHooks` (for `RadioManager`) and `FakeBackend`
(for `TailnetPeer`) to run the full state-machine logic without hardware.

Board-specific code (both WireGuard backends, both BLE transports, TCP
transport, examples, demo app) is covered only by the CI compile matrix
(`esp32-s3`, `pico-w` envs).
