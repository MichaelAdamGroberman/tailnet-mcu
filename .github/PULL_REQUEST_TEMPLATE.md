## What & why

Describe the change and the problem it solves.

## Boards / modes affected

- [ ] ESP32 (NimBLE / WireGuard)
- [ ] Raspberry Pi Pico W (WireGuard / BLE stub)
- [ ] Portable core (RadioManager / WgConfig / TailnetPeer / token gate)
- [ ] Gateway script / docs / CI

## Checklist

- [ ] `pio test -e native` passes (20/20) — run via `~/.platformio/penv/bin/pio` if local Python is 3.14+
- [ ] Affected board env compiles (`pio run -e <env> -d app/tailnet-sensor-node`)
- [ ] No secrets, private keys, or tokens committed
- [ ] Docs updated if behavior changed
- [ ] Commits are authored by me (no co-author trailers)
