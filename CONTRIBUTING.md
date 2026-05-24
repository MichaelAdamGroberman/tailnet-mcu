# Contributing to tailnet-mcu

Thank you for your interest in contributing. This is a small focused library —
pull requests that are well-scoped, tested, and clearly described are the best
way to contribute.

---

## Development setup

### Prerequisites

- [PlatformIO](https://platformio.org/install/cli) — install via the bundled
  `pip` inside `~/.platformio/penv/`, not your system pip.
- A C++17-capable host compiler (GCC or Clang) for native tests.
- `shellcheck` for linting gateway scripts.

### Python version — ESP32 builds

The ESP32 env is pinned to `pioarduino/platform-espressif32` v55.03.35
(arduino-esp32 3.x). This platform **rejects Python 3.14+**. Use either:

- The PlatformIO bundled interpreter: `~/.platformio/penv/bin/pio`
- Or ensure `python3 --version` reports 3.13 or earlier before invoking `pio`.

CI pins Python 3.12. If in doubt, use the bundled `pio`.

### Run native unit tests (no hardware required)

```bash
~/.platformio/penv/bin/pio test -e native
```

Expected: **20/20 tests pass**. These tests cover:

- `WgConfig` wg-quick parser (edge cases, missing fields, bad Endpoint)
- `RadioManager` WiFi-XOR-BT invariant (fake radio hooks, mode transitions)
- `TailnetPeer` state machine (FakeBackend, OFF/STARTING/UP/FAILED)
- `token_gate` constant-time compare

All portable-core changes must keep the test suite at 20/20 before a PR is
opened.

### Build the demo app (compile check, no flash needed)

```bash
# ESP32-S3
~/.platformio/penv/bin/pio run -e esp32-s3 -d app/tailnet-sensor-node

# Raspberry Pi Pico W
~/.platformio/penv/bin/pio run -e pico-w -d app/tailnet-sensor-node
```

You need a `secrets.h` in `app/tailnet-sensor-node/` for the build to find
its headers. Copy the example:

```bash
cp app/tailnet-sensor-node/secrets.example.h \
   app/tailnet-sensor-node/secrets.h
```

The example values are valid placeholders — the binary will not connect to
anything real, which is fine for a compile check.

### Lint the gateway script

```bash
shellcheck gateway/setup-subnet-router.sh
```

---

## Pull request expectations

1. **Tests first for portable-core changes.** If you modify `RadioManager`,
   `TailnetPeer`, `WgConfig`, or `token_gate`, update or add Unity tests
   under `test/` and confirm 20/20 still passes.
2. **Board code: at minimum a clean compile.** Backend and transport changes
   must compile for both `esp32-s3` and `pico-w` environments.
3. **Honest documentation.** If a feature is a stub or experimental (e.g.
   Pico W BLE), say so — do not mark it as working.
4. **No secrets in commits.** `secrets.h` is git-ignored. Verify with
   `git status` before pushing.
5. **One concern per PR.** Smaller, focused PRs are reviewed faster.

---

## Commit style

This project uses
[Conventional Commits](https://www.conventionalcommits.org/):

```
feat(transport): add BLE bonding support for NimBLE
fix(wgconfig): handle AllowedIPs without CIDR suffix
docs(provisioning): clarify endpoint reachability scenarios
test(radiomanager): add coverage for BT -> OFF -> WIFI path
```

Types: `feat`, `fix`, `docs`, `test`, `chore`, `refactor`.

---

## DCO sign-off

By contributing you certify that you have the right to submit the code
under the MIT license. Add a `Signed-off-by` trailer to each commit:

```
git commit -s -m "feat(...): ..."
```

---

## Reporting bugs

File a GitHub issue using the Bug Report template. Include the board, radio
mode (WiFi/BT), whether the tunnel is enabled, and any relevant Serial output.
