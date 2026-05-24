# tailnet-mcu — Design Spec

**Date:** 2026-05-24
**Status:** Approved (design); pending implementation plan
**Location:** `~/Projects/tailnet-mcu/`
**License:** MIT (preserving upstream BSD notices for `wireguard-lwip`)

---

## 1. Purpose

Let a microcontroller (ESP32 family, Raspberry Pi Pico W) join a user's
Tailscale network and be reachable from any tailnet node — **without
exposing any service to the public internet**. Today no turnkey,
multi-board, security-first project does this; existing examples are
single-board WireGuard demos with no Tailscale story and no hardening
guidance.

The deliverable is two layers in one public repo:

1. A **reusable Arduino/PlatformIO library** (`TailnetPeer`) that users
   drop into their own projects — one API, two board backends.
2. A **demo app** built on that library — a *private IoT sensor node* —
   that makes the "reachable on my tailnet, invisible to the public"
   value concrete.

## 2. Core architecture & honest framing

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
    TailnetPeer.{h,cpp}         # state machine + wg-quick parser (portable)
    backends/backend_esp32.cpp  # ciniml/WireGuard-ESP32
    backends/backend_pico_w.cpp # wireguard-lwip on arduino-pico lwIP
  examples/
    minimal_tunnel/             # connect; confirm handshake UP; reach a tailnet host
    reachable_service/          # token-gated TCP listener bound to the tunnel iface
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
class TailnetPeer {
public:
  enum State { OFF, STARTING, UP, FAILED };
  bool        begin(const char* wgQuickConfig);   // parse + start
  bool        loadConfigFromText(const char* s);  // runtime provisioning (serial paste)
  void        tick();                              // call from loop(); polls handshake
  void        stop();
  State       state() const;
  const char* tunnelIP() const;                    // our address on the tunnel
  const char* peerEndpoint() const;                // host:port we dial
  const char* lastError() const;
};
```

`wg-quick` parser accepts a standard config (`[Interface]` PrivateKey /
Address, `[Peer]` PublicKey / Endpoint / AllowedIPs); strips `/cidr` from
Address (backends want a bare IP); splits `host:port`.

## 5. Provisioning

- **Demo app + examples:** compile-time `secrets.h` (gitignored) holding
  the `wg-quick` text; `secrets.example.h` is committed. Dead simple, no
  BLE.
- **Runtime (library feature):** `loadConfigFromText()` lets advanced
  users paste a config over serial.
- **Explicitly out of scope:** the buddy's BLE/NimBLE provisioning — it
  is app-specific and drags in the Bluedroid-vs-NimBLE heap fight.

## 6. Security model (first-class doc + enforced defaults)

- **Encrypted by construction.** WireGuard has no plaintext mode
  (ChaCha20-Poly1305 AEAD, Curve25519 ECDH, ~2-min rekey). Nothing to
  misconfigure into cleartext.
- **Scanner-silent.** The gateway's UDP 51820 drops any packet not signed
  by a known peer key, with no response — appears closed to nmap/Shodan,
  no banner, no unauthenticated code path to fuzz.
- **Nothing public behind the tunnel.** Device services (e.g. the demo's
  sensor endpoint) are reachable only from inside the tailnet.

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

1. **Host-native unit tests** for the `wg-quick` parser (pure string
   logic; runs in PlatformIO `native` env, no hardware).
2. **CI compile matrix** builds both board envs (esp32-s3, pico-w) on
   every push to catch backend breakage.
3. **Documented manual hardware bring-up checklist:** handshake reaches
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
| Provisioning | Compile-time `secrets.h`; runtime serial paste as library feature | Simple default; no BLE/NimBLE complexity |
| License | MIT | Permissive; compatible with BSD `wireguard-lwip` (notices preserved) |
| Name / location | `tailnet-mcu` / `~/Projects/tailnet-mcu/` | Board-neutral; alongside mac-mcp / linux-mcp |

## 10. Out of scope (v1)

- BLE/serial-portal provisioning (buddy-specific).
- Roaming failover between multiple gateway endpoints (one reachable
  endpoint suffices; dial the public node when away).
- Boards beyond ESP32 family + Pico W.
- Embedded-Linux targets (Pi Zero/4) — those run real Tailscale natively.
