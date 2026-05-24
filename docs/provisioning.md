# tailnet-mcu — Provisioning

## Compile-time configuration (`secrets.h`)

All device configuration lives in `secrets.h` (git-ignored). Copy the
example and fill it in before building:

```bash
cp app/tailnet-sensor-node/secrets.example.h \
   app/tailnet-sensor-node/secrets.h
```

The file defines four items:

| Define | Required | Description |
|--------|----------|-------------|
| `WIFI_SSID` | Yes | WiFi network name |
| `WIFI_PASS` | Yes | WiFi passphrase |
| `SERVICE_TOKEN` | Yes | Shared token for TCP and BLE access. Generate with `openssl rand -hex 32` |
| `DEFAULT_MODE_WIFI` | Yes (one of two) | Define for WiFi boot; omit for BLE boot |
| `WG_CONFIG` | No | wg-quick config block. Omit to run WiFi LAN-only with no tunnel |

---

## Getting keys from the gateway script

Run `gateway/setup-subnet-router.sh` on any existing Tailscale Linux node
(must be `tailscale up` already). The script:

1. Generates gateway + device WireGuard keypairs (idempotent — skips if
   already present).
2. Writes `/etc/wireguard/wg0.conf` and brings up the interface.
3. Enables IP forwarding and adds a `MASQUERADE` rule toward `tailscale0`.
4. Calls `tailscale set --advertise-routes=<subnet>`.
5. Prints the ready-to-paste `WG_CONFIG` block for the device.

```bash
sudo WG_SUBNET=10.20.30.0/24 \
     GW_ADDR=10.20.30.1 \
     DEV_ADDR=10.20.30.5 \
  bash gateway/setup-subnet-router.sh
```

After running, approve the advertised route in the Tailscale admin console.
Paste the printed `WG_CONFIG` block into `secrets.h`.

---

## Endpoint scenarios

The `Endpoint` line in `WG_CONFIG` is the underlay address the MCU dials.
Three common scenarios:

### Scenario A: gateway on the same LAN (free, no public IP needed)

```
Endpoint = 192.168.1.10:51820   # gateway's LAN IP
```

The device is reachable from that LAN only. The tunnel still lets tailnet
members reach the device via the subnet router.

### Scenario B: gateway has a public IP or dynamic DNS + UDP port forward

```
Endpoint = my-home.dyndns.org:51820
# or
Endpoint = 203.0.113.45:51820
```

Route the external port 51820/UDP to the gateway's LAN IP in your router.
The device is reachable from anywhere.

### Scenario C: gateway behind NAT, no public endpoint

WireGuard on the MCU has no NAT traversal (no DERP, no hole-punching). A
gateway without a reachable UDP endpoint cannot be dialed. Use Scenario A
or B instead, or put the gateway on a cloud VPS.

---

## Runtime configuration

### Paste a WG config over serial

`TailnetPeer::loadConfigFromText()` lets you paste a new wg-quick block at
runtime without reflashing. Useful during development:

```cpp
// From your serial handler:
if (cmd.startsWith("wg-config ")) {
    tailnet.stop();
    tailnet.begin(cmd.substring(10).c_str());
}
```

### Switch radio mode at runtime

Send `mode wifi` or `mode bt` over whichever transport is currently active.
The demo app and `mode_switch` example wire this to `RadioManager::setMode()`.

---

## Enabling the optional Tailscale tunnel

By default (no `WG_CONFIG` defined) the device runs WiFi mode over plain LAN
TCP — no tunnel, no WireGuard dependency. To opt in:

1. Run the gateway script and get your `WG_CONFIG` block.
2. Approve the route in the Tailscale admin console.
3. Uncomment and fill in `WG_CONFIG` in `secrets.h`:

```cpp
#define WG_CONFIG \
"[Interface]\n" \
"PrivateKey = <base64-device-private-key>=\n" \
"Address = 10.20.30.5/32\n" \
"\n" \
"[Peer]\n" \
"PublicKey = <base64-gateway-public-key>=\n" \
"Endpoint = <gateway-underlay-ip-or-host>:51820\n" \
"AllowedIPs = 10.20.30.0/24\n"
```

4. In `startWifi()` inside your `RadioHooks` subclass (or in the demo app's
   `SensorRadioHooks`), start the peer:

```cpp
#ifdef WG_CONFIG
if (tailnet.begin(WG_CONFIG))
    Serial.println("Tunnel starting: " + String(tailnet.peerEndpoint()));
#endif
```

5. In `loop()`, call `tailnet.tick()` when `WG_CONFIG` is defined.
6. Reflash. The device will reach `TailnetPeer::UP` after the first WireGuard
   handshake and become reachable at `10.20.30.5` from any tailnet node.

### Verify the tunnel

```bash
# From any tailnet node:
ping 10.20.30.5

# From off-network — expect open|filtered with no WireGuard reply:
nmap -sU -p51820 <gateway-underlay-address>
```

See [gateway/harden.md](../gateway/harden.md) for firewall hardening and
flash-encryption steps.
