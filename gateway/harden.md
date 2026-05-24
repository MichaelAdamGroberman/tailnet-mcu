# Hardening the gateway (and the device)

The `setup-subnet-router.sh` script gets you a working tunnel. This file makes
it safe to leave running.

## 1. Firewall — one open UDP port, nothing else

The only port that must face the network the device dials from is the
WireGuard listener (`UDP 51820`). Lock everything else down:

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 51820/udp           # WireGuard — the device dials this
sudo ufw enable
```

**Move SSH onto the tailnet** instead of exposing it publicly. Either use
`tailscale ssh`, or bind `sshd` to the node's `tailscale0` address
(`ListenAddress 100.x.y.z` in `/etc/ssh/sshd_config`) and do **not** open
`22/tcp` in the firewall. The device's WireGuard port is the only thing the
public internet ever sees.

## 2. Why one open UDP port is safe

WireGuard is **silent to unauthenticated peers**. A packet that is not signed
by a known peer's key is dropped with no response — no handshake, no banner,
no version string. To `nmap`/Shodan the port looks closed/filtered and offers
no unauthenticated code path to fuzz:

```bash
# From off-network — expect open|filtered with NO WireGuard reply:
nmap -sU -p51820 <gateway-public-addr>
```

## 3. Scoped AllowedIPs (anti-spoof)

The device's peer entry uses `AllowedIPs = <device-ip>/32`. WireGuard
cryptokey routing means the gateway will **only** accept packets from that
key claiming that one source IP — a cloned or stolen key cannot impersonate
other devices or source-spoof into the rest of the subnet.

## 4. Tailscale ACL + node tag (least privilege)

Apply `tailscale-acl.example.json` in the admin console and tag the gateway
`tag:iot-gateway`. The example allows tailnet members to reach the MCU subnet
and restricts the MCU to reaching only the gateway by default — widen the
second rule deliberately, only to the hosts the device actually needs.

## 5. Protect the device's private key at rest (ESP32 flash encryption)

The device's WireGuard private key lives in flash. Without flash encryption,
physical access plus a flash dump equals key extraction. The ESP32 supports
hardware flash encryption via eFuse:

- See Espressif's *Flash Encryption* guide for your chip.
- **This is one-way (eFuse burns are permanent) — read it fully before
  enabling, and test on a sacrificial board first.**
- Scope the device's access (steps 3–4) so even a compromised key reaches
  only what it needs.

## 6. Bring-up verification

1. `tailscale status` on the gateway shows the advertised route `10.20.30.0/24`
   (approve it in the admin console if pending).
2. Device boots in WiFi mode and `TailnetPeer` reaches `UP`.
3. From another tailnet node: `ping 10.20.30.5` (the device's tunnel IP).
4. From off-network: the `nmap` check in §2 returns no WireGuard reply.
