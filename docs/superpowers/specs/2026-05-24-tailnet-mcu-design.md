# tailnet-mcu — Design Spec

**Date:** 2026-05-24
**Status:** Approved — **Revision 2** (2026-05-24): added WiFi⊕BT radio modes;
Tailscale tunnel is now an *optional* WiFi-mode layer, not the centerpiece.
**Location:** `~/Projects/tailnet-mcu/`
**License:** MIT (preserving upstream BSD notices for `wireguard-lwip`)

---

## 1. Purpose

Give a microcontroller (ESP32 family, Raspberry Pi Pico W) a simple
connectivity layer with two **mutually exclusive** runtime transports —
**WiFi** or **Bluetooth LE** — and, *as an optional add-on in WiFi mode*,
a WireGuard tunnel that joins the device to the user's Tailscale network,
reachable from any tailnet node **without exposing any service to the
public internet**. That optional Tailscale tunnel is the novel piece:
today no turnkey, multi-board, security-first project does it; existing
examples are single-board WireGuard demos with no Tailscale story and no
hardening guidance.

The deliverable is two layers in one public repo:

1. A **reusable Arduino/PlatformIO library** that users drop into their
   own projects: a `RadioManager` (enforces WiFi⊕BT), a `ServiceTransport`
   abstraction with BLE and TCP implementations, and an *optional*
   `TailnetPeer` WireGuard tunnel — one API surface, two board backends
   for each radio.
2. A **demo app** built on that library — a *private IoT sensor node* —
   that serves the same reading over BLE (local) or TCP (LAN), and
   optionally tailnet-wide when the WireGuard tunnel is enabled.

## 2. Operating model — radio modes & transports (Revision 2)

The device runs in exactly **one** `RadioMode` at a time — `OFF`, `WIFI`,
or `BT` — and a `RadioManager` enforces that invariant: switching modes
**always powers down the active radio before bringing up the next**. This
is not tidiness; on a single-radio chip (e.g. ESP32-PICO-D4) WiFi + BLE
coexistence is a hard heap ceiling (Bluedroid + WiFi starved the WiFi
driver of RAM in the `claude-desktop-buddy` work). WiFi⊕BT means the
device only ever pays for one stack's heap.

A `ServiceTransport` interface exposes one command `Handler`
(`"read" → reading`). The user writes their service logic **once** and
binds it to whichever transport the active mode provides:

- `BleServiceTransport` — BLE GATT, NUS-style (write char in / notify char
  out). Local range, no IP, no tunnel. ESP32 = NimBLE; Pico W = BTstack.
- `TcpServiceTransport` — token-gated TCP. WiFi mode; reachable
  tailnet-wide **iff** the optional `TailnetPeer` tunnel is up, otherwise
  LAN-only.

```
                 ┌──────────── RadioManager (enforces WiFi⊕BT) ────────────┐
   boot/cmd ──▶  │   OFF ⇄ WIFI ⇄ BT     stopActive() before startNext()   │
                 └───────┬──────────────────────────────────┬──────────────┘
                         │ WIFI                              │ BT
                ┌────────▼──────────┐               ┌────────▼─────────┐
   one Handler  │ TcpServiceTransport│              │ BleServiceTransport│  same Handler
   (your logic) │  + OPTIONAL        │              │  (NimBLE / BTstack)│  (your logic)
                │    TailnetPeer     │              └───────────────────┘
                └───────────────────┘
```

**Mode selection:** boot mode from config (`DEFAULT_MODE`); runtime switch
via a reserved service command (`mode wifi` / `mode bt`) routed through
`RadioManager`; an optional hardware button is documented as
board-dependent.

**The Tailscale tunnel is opt-in.** `TailnetPeer` is started only in WiFi
mode and only when a WG config is supplied. With no config, WiFi mode
still serves over plain LAN TCP. §2A below describes that optional layer.

## 2A. WireGuard tunnel architecture — the optional layer (honest framing)

The MCU does **not** run Tailscale. It runs a vanilla WireGuard client
and joins the tailnet *through a subnet router* — an existing Tailscale
Linux node that also speaks plain WireGuard to the device. The README
leads with this so no one is misled.

```
[ESP32-S3 / Pico W]              [Existing Tailscale node]        [Your tailnet]
  TailnetPeer  ── WireGuard ──▶  wg0  +  tailscaled         ◀───▶ phone, laptop,
  (one UDP peer    (ChaCha20-     (advertise-routes               other nodes
   dialed by        Poly1305,      10.20.30.0/24;
   underlay addr)   scanner-       MASQUERADE → tailscale0)
                    silent)
```

- **Portable layer (shared across boards):** the
  `WG_OFF → WG_STARTING → WG_UP → WG_FAILED` state machine plus the
  `wg-quick` config parser. Generalized from the existing
  `claude-desktop-buddy` `net_wg.cpp`.
- **Backend layer (`#ifdef` by platform):**
  - ESP32 → `ciniml/WireGuard-ESP32` (wraps the ESP32 lwIP stack).
  - Pico W → `wireguard-lwip` on the arduino-pico (Earle Philhower)
    lwIP stack.
  - Both expose the same `begin(localIP, privKey, peerPub, peerHost,
    peerPort)` shape behind one public API.

### 2.1 Hard constraint — endpoint reachability

Plain WireGuard on the MCU cannot do NAT traversal (no DERP, no
hole-punching — that is Tailscale-client-only magic). It therefore needs
**exactly one peer with a reachable UDP endpoint** to dial.

Critically, the MCU must dial an **underlay** address (LAN IP or public
IP:port), **never** the gateway's Tailscale `100.x` address or MagicDNS
name — the device is not on the tailnet until *after* the tunnel is up
(chicken-and-egg). Tailscale Funnel/Serve cannot help: they are
HTTPS/TCP only and cannot carry WireGuard's UDP.

### 2.2 Gateway is any existing Tailscale node (node-agnostic)

The gateway is **not** a dedicated VPS by requirement — it is any Linux
box that is already a Tailscale node and has a reachable UDP endpoint.
Two documented paths, identical firmware, differing only in `Endpoint`:

| Existing node sits…                         | MCU dials             | Reachable from   |
|---------------------------------------------|-----------------------|------------------|
| On the same LAN as the MCU (free)           | node LAN IP `:51820`  | that LAN only    |
| Public (cloud VPS, or home box + UDP-forward + DDNS) | public addr `:51820` | anywhere |
| Behind NAT, no forward, no public IP        | —                     | cannot (no WG NAT-punch) |

## 3. Repository structure

```
tailnet-mcu/
  src/                          # the library
    RadioManager.{h,cpp}        # WiFi⊕BT mode state machine (portable; board radio hooks)
    WgConfig.{h,cpp}            # wg-quick parser (portable, host-tested)
    TailnetPeer.{h,cpp}         # OPTIONAL WireGuard tunnel state machine (WiFi mode)
    backends/wg_backend.h       # WgBackend interface
    backends/backend_esp32.cpp  # ciniml/WireGuard-ESP32        (#ifdef ESP32)
    backends/backend_pico_w.cpp # wireguard-lwip on arduino-pico (#ifdef RP2040)
    transport/service_transport.h         # ServiceTransport interface + Handler
    transport/tcp_service_transport.{h,cpp}  # token-gated TCP (WiFi mode)
    transport/ble_service_transport.h        # BLE transport interface decl
    transport/ble/ble_nimble_esp32.cpp       # NimBLE GATT      (#ifdef ESP32)
    transport/ble/ble_btstack_pico.cpp       # BTstack GATT     (#ifdef RP2040, spike)
  examples/
    minimal_tunnel/             # WiFi mode + optional tunnel; reach a tailnet host
    mode_switch/                # boot BT, switch to WiFi at runtime via command
  app/tailnet-sensor-node/      # demo app (layer 2); own platformio.ini
    src/main.cpp
    secrets.example.h
  gateway/
    setup-subnet-router.sh      # node-agnostic: detects tailscaled, adds WG peer,
                                #   advertises route, sets firewall + forwarding
    tailscale-acl.example.json  # least-privilege ACL + node tag (tag:iot-gateway)
    harden.md                   # VPS firewall, SSH-over-tailnet, flash-encryption notes
  docs/
    architecture.md
    provisioning.md
    security-model.md
    superpowers/specs/2026-05-24-tailnet-mcu-design.md   # this file
  library.json                  # PlatformIO registry metadata
  library.properties            # Arduino IDE metadata
  platformio.ini                # library CI envs: esp32-s3, pico-w, native (tests)
  README.md SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md CODEOWNERS LICENSE
  .github/                      # CI + issue/PR templates
```

## 4. Public API (sketch — finalized during planning)

```cpp
// Enforces WiFi⊕BT. Board radio start/stop behind a hook so the mode
// logic is host-testable with a fake.
class RadioManager {
public:
  enum Mode { OFF, WIFI, BT };
  bool setMode(Mode m);     // stops the active radio first; false on failure
  Mode mode() const;
};

// Same service logic over either radio.
class ServiceTransport {
public:
  using Handler = std::function<String(const String& line)>;
  virtual ~ServiceTransport() {}
  virtual bool begin() = 0;
  virtual void tick() = 0;          // call from loop()
  void onLine(Handler h);
};
// Implementations: TcpServiceTransport(port, token) [WiFi];
//                  BleServiceTransport(serviceName) [BT].

// OPTIONAL — only meaningful in WIFI mode.
class TailnetPeer {
public:
  enum State { OFF, STARTING, UP, FAILED };
  bool        begin(const char* wgQuickConfig);   // parse + start
  bool        loadConfigFromText(const char* s);  // runtime provisioning (serial paste)
  void        tick();                              // call from loop(); polls handshake
  void        stop();
  State       state() const;
  const char* tunnelIP() const;
  const char* peerEndpoint() const;
  const char* lastError() const;
};
```

`wg-quick` parser accepts a standard config (`[Interface]` PrivateKey /
Address, `[Peer]` PublicKey / Endpoint / AllowedIPs); strips `/cidr` from
Address (backends want a bare IP); splits `host:port`.

## 5. Provisioning

- **Demo app + examples:** compile-time `secrets.h` (gitignored) holding
  WiFi creds, the optional `wg-quick` text, the BLE service token, and a
  `DEFAULT_MODE` (`WIFI` or `BT`); `secrets.example.h` is committed.
- **Runtime (library feature):** `loadConfigFromText()` lets advanced
  users paste a WG config over serial; `mode wifi`/`mode bt` over the
  active transport switches radios at runtime.
- **The WG config is optional** — omit it and WiFi mode serves over plain
  LAN TCP with no tunnel.

## 6. Security model (first-class doc + enforced defaults)

- **Encrypted by construction.** WireGuard has no plaintext mode
  (ChaCha20-Poly1305 AEAD, Curve25519 ECDH, ~2-min rekey). Nothing to
  misconfigure into cleartext.
- **Scanner-silent.** The gateway's UDP 51820 drops any packet not signed
  by a known peer key, with no response — appears closed to nmap/Shodan,
  no banner, no unauthenticated code path to fuzz.
- **Nothing public behind the tunnel.** Device services (e.g. the demo's
  sensor endpoint) are reachable only from inside the tailnet.
- **BLE transport is local-range + token-gated.** BLE GATT has no IP
  exposure and only reaches devices in radio range. The same shared-token
  first-line check used by TCP gates the BLE write characteristic; LE
  Secure Connections pairing/bonding is documented as the recommended
  hardening on top.

Enforced defaults baked into scripts/docs:

1. **Scoped `AllowedIPs = <device-ip>/32`** on the gateway peer entry
   (WireGuard cryptokey routing prevents source-IP spoofing by a cloned
   key).
2. **Least-privilege Tailscale ACL + node tag** (`tag:iot-gateway`) so the
   routed device IP reaches only what it needs and only authorized users
   reach the device.
3. **VPS/host firewall:** inbound UDP 51820 only; SSH moved onto the
   tailnet (no public SSH); IP forwarding + `MASQUERADE` scoped to the
   one subnet.
4. **ESP32 flash encryption** documented (eFuse-based, one-way — warned
   loudly) to protect the at-rest WG private key in NVS.
5. **Strong device-side listener token**, listener bound to the tunnel
   interface, never `0.0.0.0`.

## 7. Testing strategy

1. **Host-native unit tests** (PlatformIO `native` env, no hardware) for
   the pure logic: the `wg-quick` parser, the `TailnetPeer` state machine
   (FakeBackend), the `RadioManager` WiFi⊕BT invariant (fake radio hooks),
   and `ServiceTransport` handler dispatch (fake transport).
2. **CI compile matrix** builds both board envs (esp32-s3, pico-w) on
   every push to catch WG + BLE backend breakage.
3. **Documented manual hardware bring-up checklist:** mode switch powers
   exactly one radio; BLE GATT read works in range; handshake reaches
   `UP`; device reachable from a tailnet node at its tunnel IP; `nmap`
   against the gateway confirms the WireGuard port is scanner-silent.

## 8. Definition of done

- Both examples + the demo app build in CI for ESP32-S3 and Pico W.
- Parser unit tests pass.
- `setup-subnet-router.sh` provisions a working subnet router on an
  existing Tailscale node (verified on-LAN and public).
- Full repo-protection set in place before declared public-ready:
  branch protection, security alerts, fork/star monitoring, SECURITY.md,
  CODEOWNERS, CONTRIBUTING.md, CODE_OF_CONDUCT.md, issue/PR templates,
  README badges, `.claude-plugin/marketplace.json`, LinkedIn contact.

## 9. Decisions log

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Gateway pattern | Subnet router via existing Tailscale node | MCU can't run full Tailscale; node-agnostic = flexible + reuses what user already runs |
| Gateway scope | Node-agnostic script; on-LAN (free) + public (anywhere) both documented | "Dedicated VPS" was just one instance of "reachable Tailscale node" |
| Boards | ESP32 (S3 = WiFi default) + Raspberry Pi Pico W | Two chips makers actually own; one Arduino/PlatformIO toolchain, one API, two backends |
| Deliverable | Library + demo app, two layers | Matches "incorporate into their projects" while showing a tangible showcase |
| Demo app | Private IoT sensor node | Makes the "tailnet-private, never public" security story concrete |
| **Radios (Rev 2)** | **WiFi⊕BT, mutually exclusive via RadioManager** | Single-radio chips can't run both stacks' heap; XOR sidesteps the buddy's coexistence fight |
| **Tailscale tunnel (Rev 2)** | **Optional WiFi-mode layer, not required** | Library is useful for BLE-only / plain-LAN users; tunnel is the marquee opt-in feature |
| **Transports (Rev 2)** | **`ServiceTransport` abstraction: BLE (NimBLE/BTstack) + TCP** | Write service logic once, serve over whichever radio is active |
| **BLE scope (Rev 2)** | **Both boards; Pico W BTstack as a spike, degrades to documented-stub** | ESP32 NimBLE proven; Pico W BLE is real integration risk, must not block v1 |
| Provisioning | Compile-time `secrets.h` (+ `DEFAULT_MODE`); runtime serial paste + `mode` command | Simple default; runtime mode switch for flexibility |
| License | MIT | Permissive; compatible with BSD `wireguard-lwip` (notices preserved) |
| Name / location | `tailnet-mcu` / `~/Projects/tailnet-mcu/` | Board-neutral; alongside mac-mcp / linux-mcp |

## 10. Out of scope (v1)

- BLE/softAP *provisioning portal* (buddy-specific). BLE is in scope as a
  runtime service transport, not as a config-provisioning UI.
- Simultaneous WiFi + BT (explicitly forbidden by the WiFi⊕BT invariant).
- Running BLE and the WireGuard tunnel at the same time (BLE is BT mode;
  the tunnel is WiFi mode — mutually exclusive by design).
- Roaming failover between multiple gateway endpoints (one reachable
  endpoint suffices; dial the public node when away).
- Boards beyond ESP32 family + Pico W.
- Embedded-Linux targets (Pi Zero/4) — those run real Tailscale natively.
