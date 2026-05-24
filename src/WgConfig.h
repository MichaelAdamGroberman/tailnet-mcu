#pragma once
#include <stdint.h>
#include <stddef.h>

// Parsed wg-quick config. Fixed buffers so it lives on the stack with
// no heap on the MCU. Strings are NUL-terminated.
struct WgConfig {
  char     if_addr[24];   // "10.20.30.5"  (CIDR suffix stripped)
  char     if_priv[48];   // base64 interface private key
  char     peer_pub[48];  // base64 peer public key
  char     peer_host[64]; // underlay host or IP we dial (never a 100.x addr)
  uint16_t peer_port;     // defaults to 51820
  bool     valid;         // true iff PrivateKey, Address, PublicKey, Endpoint all present
};

// Parse wg-quick text. Returns out->valid. Section headers ([Interface],
// [Peer]), comments (#), and unknown keys are ignored.
bool wgConfigParse(const char* text, WgConfig* out);
