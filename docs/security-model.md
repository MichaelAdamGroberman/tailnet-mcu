# tailnet-mcu — Security Model

## Threat model summary

The device is an MCU (ESP32 or Pico W) deployed on a network the operator
does not fully control (home LAN, remote site). Goals:

- No service reachable from the public internet.
- Encrypted channel between device and the rest of the tailnet.
- Minimal unauthenticated attack surface at rest and over the air.
- Stolen or cloned device cannot impersonate another device or expand its own access.

---

## WireGuard: encrypted by construction

WireGuard has **no plaintext mode**. Every packet is
ChaCha20-Poly1305 AEAD-authenticated with keys derived via Curve25519 ECDH
and BLAKE2s. Key rotation happens automatically every ~2 minutes (`Rekey After
Time`). There is nothing to misconfigure into cleartext; the tunnel either
authenticates or drops.

## Scanner-silent gateway port

The WireGuard listener (`UDP 51820`) on the gateway node is **silent to
unauthenticated peers**. A packet not signed by a known peer key is dropped
with no response — no handshake, no banner, no version string. To nmap and
Shodan the port appears closed/filtered and offers no unauthenticated code
path to fuzz or fingerprint.

Verification:
```bash
# From off-network — expect open|filtered with NO WireGuard reply:
nmap -sU -p51820 <gateway-underlay-address>
```

## Nothing public behind the tunnel

Device services (sensor endpoint, command interface) are bound to the tunnel
interface, never to `0.0.0.0`. With no `TailnetPeer` tunnel, the TCP
transport is LAN-only and never reachable from the internet. With a tunnel,
the device is reachable from inside the tailnet via the subnet router — still
not the public internet.

## Scoped `AllowedIPs` (anti-spoof)

The gateway peer entry for the device uses `AllowedIPs = <device-ip>/32`
(e.g. `10.20.30.5/32`). WireGuard cryptokey routing means the gateway will
**only** accept packets from that key claiming that one source IP. A cloned
or stolen device key cannot impersonate other devices or source-spoof into
the rest of the subnet.

## Least-privilege Tailscale ACL + node tag

Apply `gateway/tailscale-acl.example.json` and tag the gateway
`tag:iot-gateway`. The example ACL:

- Allows tailnet members to reach the MCU subnet (`10.20.30.0/24`).
- Restricts the MCU to reaching only the gateway by default.

Widen the second rule deliberately, only to the hosts the device actually
needs to reach. See the Tailscale admin console to apply the JSON.

## BLE transport: local-range + token-gated

BLE GATT has no IP exposure and only reaches devices within radio range
(typically 10–30 m). The BLE write characteristic applies the same
constant-time token gate used by the TCP transport before dispatching any
command. LE Secure Connections (LESC) pairing and bonding are documented
as the recommended additional hardening layer on top of the token gate —
NimBLE on ESP32 supports LESC; see the NimBLE-Arduino documentation for
pairing configuration.

## TCP transport: token-gated, constant-time

The TCP listener gate uses a constant-time `memcmp`-equivalent comparison
over the full token length to prevent timing-oracle attacks. Bind the listener
to the tunnel interface (`TailnetPeer::tunnelIP()`) rather than `0.0.0.0`
when the tunnel is active.

## ESP32 flash encryption (at-rest key protection)

The device's WireGuard private key lives in flash (in the `secrets.h`-compiled
image or in NVS). Without flash encryption, physical access plus a flash dump
equals key extraction. The ESP32 supports hardware flash encryption via eFuse.

**Warning:** flash encryption is one-way (eFuse burns are permanent). Read
Espressif's *Flash Encryption* guide fully before enabling, and test on a
sacrificial board first. See [gateway/harden.md](../gateway/harden.md) §5
for pointers.

## Gateway hardening

For firewall rules, SSH-over-tailnet setup, subnet scoping, and verification
steps, see [gateway/harden.md](../gateway/harden.md).

---

## Enforced defaults checklist

| Control | Where enforced |
|---|---|
| WireGuard encrypted-by-construction | WireGuard protocol |
| Scanner-silent UDP 51820 | WireGuard protocol + `gateway/harden.md` §1–2 |
| Scoped `AllowedIPs = <device-ip>/32` | `setup-subnet-router.sh` default + harden.md §3 |
| Least-privilege Tailscale ACL + `tag:iot-gateway` | `tailscale-acl.example.json` + harden.md §4 |
| TCP listener bound to tunnel interface | demo app `TcpServiceTransport` init |
| Token gate on both TCP and BLE | `token_gate.cpp` constant-time compare |
| ESP32 flash encryption (at-rest) | Documented in harden.md §5 — user must enable |
| SSH moved onto tailnet (public SSH port closed) | harden.md §1 |
