#pragma once

// Copy this file to secrets.h and fill in your real values.
// secrets.h is git-ignored; never commit real keys.

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSPHRASE"

// Long random token — used for both TCP (WiFi mode) and BLE (BT mode).
// Generate with: openssl rand -hex 32
#define SERVICE_TOKEN "change-me-long-random-token"

// Boot into WiFi by default. Comment out to boot into BT (BLE) mode.
#define DEFAULT_MODE_WIFI

// ---- WireGuard tunnel (OPTIONAL) --------------------------------------
// Uncomment and fill in to enable a WireGuard tunnel in WiFi mode.
// Leave commented out to run in WiFi mode LAN-only (no tunnel needed).
//
// #define WG_CONFIG \
// "[Interface]\n" \
// "PrivateKey = REPLACE_WITH_BASE64_PRIVATE_KEY=\n" \
// "Address = 100.64.0.2/32\n" \
// "\n" \
// "[Peer]\n" \
// "PublicKey = REPLACE_WITH_BASE64_PUBLIC_KEY=\n" \
// "Endpoint = YOUR_TAILNET_RELAY:51820\n" \
// "AllowedIPs = 100.64.0.0/24\n"
// -----------------------------------------------------------------------
