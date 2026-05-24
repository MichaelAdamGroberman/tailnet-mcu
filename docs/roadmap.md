# tailnet-mcu — Roadmap

## v1 known limitations

### Pico W BLE (BTstack app-forwarded-hook design)

**Status:** Compile-gated stub in v1.

BTstack (bundled with the Earle Philhower arduino-pico core) requires a
single global `gattWriteCallback` function in the final binary. A library
cannot own that symbol without colliding with any application that also
defines it. The current `ble_btstack_pico.cpp` compiles as a clearly-labeled
stub that logs a message when `begin()` is called.

**Planned fix (v2):** An app-forwarded-hook design where the application
defines the global `gattWriteCallback`, registers it with BTstack, and
forwards write events to the library via a registration function
(`BleServiceTransport::installWriteForwarder()`). This avoids the symbol
collision while keeping the BTstack integration inside the library.

**Workaround:** Use an ESP32-S3 for any BLE use case in v1. ESP32 with
NimBLE-Arduino is the fully supported and tested BLE path.

---

### Multi-endpoint roaming failover

**Status:** Out of scope in v1.

The MCU dials a single WireGuard peer endpoint at boot. If that endpoint
becomes unreachable (e.g. switching from LAN to mobile hotspot) the tunnel
drops and the device must be reflashed or serial-commanded with a new config.

**Planned (v2+):** A list of candidate endpoints in the WG config (or a
secondary `Endpoint` field), with `TailnetPeer::tick()` cycling through
them on handshake timeout. This covers on-LAN / public-IP failover without
reflashing.

---

## Completed in v1

- `RadioManager` WiFi XOR BT invariant (host-tested)
- `WgConfig` wg-quick parser (host-tested)
- `TailnetPeer` WireGuard state machine with FakeBackend (host-tested)
- ESP32 WireGuard backend (`ciniml/WireGuard-ESP32`)
- ESP32 NimBLE BLE transport (working)
- Pico W WireGuard backend (`jaszczurtd/arduino-wireguard-pico-w`, working)
- Pico W BLE transport (compile-gated stub — see above)
- `TcpServiceTransport` with constant-time token gate
- `ServiceTransport` abstraction (one handler, two radios)
- Gateway script (`setup-subnet-router.sh`), ACL example, hardening guide
- `minimal_tunnel` and `mode_switch` examples
- `tailnet-sensor-node` demo app
- 20/20 native Unity unit tests passing
