# Hardware bring-up checklist

The host tests (`pio test -e native`, 20/20) and the CI compile matrix prove
the logic and that the firmware builds. They cannot prove on-device behavior.
Run this checklist once on each physical board (ESP32-S3, Pico W).

Flash the demo:

```bash
# Use the bundled pio (Python <= 3.13) — see README for why.
~/.platformio/penv/bin/pio run -e esp32-s3 -d app/tailnet-sensor-node --target upload --target monitor
# or:  -e pico-w
```

## 1. Radio mutual exclusion (WiFi XOR BT)

- [ ] Boot in `DEFAULT_MODE_WIFI`: serial shows `WiFi OK <ip>`, no BLE advertising.
- [ ] Send `mode bt` over the active transport: serial shows `WiFi OFF` **then**
      `BLE advertising as: tailnet-sensor` — in that order, never overlapping.
- [ ] Send `mode wifi`: serial shows `BLE OFF` **then** `WiFi OK`.
- [ ] A BLE scanner sees the device **only** while in BT mode; it disappears
      after `mode wifi`.

### Free heap is reclaimed across a switch (the heap guarantee)

The XOR design exists to avoid running both radio stacks' heap at once. Confirm
the stop hook actually *frees* the stack, not just disconnects — otherwise the
guarantee is defeated on RAM-constrained chips (e.g. ESP32-PICO-D4).

- [ ] Add a temporary log of free heap around a switch and confirm it returns
      to roughly its pre-switch level (no monotonic downward drift over repeated
      `mode wifi` / `mode bt` cycles):

  ```cpp
  // ESP32 — log before/after each setMode in the mode command handler:
  Serial.printf("free heap before switch: %u\n", ESP.getFreeHeap());
  radio.setMode(RadioManager::BT);
  Serial.printf("free heap after switch:  %u\n", ESP.getFreeHeap());
  // (Pico W: rp2040.getFreeHeap())
  ```

- [ ] Cycle WiFi -> BT -> WiFi -> BT ~10 times; free heap after each full cycle
      should be stable (within a small constant), not steadily shrinking. A
      steady decline means a stop hook is leaking — verify it fully deinits
      (`WiFi.mode(WIFI_OFF)`, `NimBLEDevice::deinit(true)`).

## 2. BLE transport (BT mode) — ESP32 only

> Pico W BLE ships as a stub (see [roadmap.md](roadmap.md)); skip on Pico W.

- [ ] Connect with a BLE client (e.g. nRF Connect) to the NUS service.
- [ ] Write the token line first, then `read` — expect a `{"temp_c":..,"uptime_s":..}`
      notification. Writing a wrong token first yields no data (silent drop).

## 3. Optional WireGuard tunnel (WiFi mode)

- [ ] With `WG_CONFIG` defined, serial shows `WG tunnel starting… endpoint=<addr>`.
- [ ] On the gateway, the advertised route is approved in the Tailscale admin
      console.
- [ ] From another tailnet node: `ping <device tunnel IP>` (e.g. `10.20.30.5`)
      succeeds once the handshake completes.
- [ ] From off-network: `nmap -sU -p51820 <gateway>` returns no WireGuard reply
      (scanner-silent). See [../gateway/harden.md](../gateway/harden.md).

## 4. Token-gated TCP (WiFi mode)

- [ ] `printf 'TOKEN\nread\n' | nc <device-ip-or-tunnel-ip> 4242` returns the
      sensor JSON. A wrong token closes the connection with no data.
