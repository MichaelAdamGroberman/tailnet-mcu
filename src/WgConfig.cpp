#include "WgConfig.h"
#include <string.h>
#include <strings.h>   // strncasecmp
#include <stdlib.h>

static const char* skipWs(const char* s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

// If `line` is "Key = value", copy value into out (trimmed). Returns true on match.
static bool getKV(const char* line, const char* key, char* out, size_t outsz) {
  size_t klen = strlen(key);
  const char* s = skipWs(line);
  if (strncasecmp(s, key, klen) != 0) return false;
  s += klen;
  s = skipWs(s);
  if (*s != '=') return false;
  s = skipWs(s + 1);
  size_t n = 0;
  while (*s && *s != '\r' && *s != '\n' && n < outsz - 1) out[n++] = *s++;
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
  out[n] = 0;
  return n > 0;
}

bool wgConfigParse(const char* text, WgConfig* out) {
  memset(out, 0, sizeof(*out));
  out->peer_port = 51820;

  char endpoint[80] = "";
  const char* p = text;
  char line[160];
  while (*p) {
    size_t n = 0;
    while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
    line[n] = 0;
    if (*p == '\n') p++;
    if (line[0] == '[' || line[0] == '#' || line[0] == 0) continue;

    char val[160];
    if (getKV(line, "PrivateKey", val, sizeof(val))) {
      strncpy(out->if_priv, val, sizeof(out->if_priv) - 1);
    } else if (getKV(line, "Address", val, sizeof(val))) {
      char* slash = strchr(val, '/');
      if (slash) *slash = 0;
      strncpy(out->if_addr, val, sizeof(out->if_addr) - 1);
    } else if (getKV(line, "PublicKey", val, sizeof(val))) {
      strncpy(out->peer_pub, val, sizeof(out->peer_pub) - 1);
    } else if (getKV(line, "Endpoint", val, sizeof(val))) {
      strncpy(endpoint, val, sizeof(endpoint) - 1);
    }
  }

  if (endpoint[0]) {
    const char* colon = strrchr(endpoint, ':');
    if (colon) {
      size_t hl = (size_t)(colon - endpoint);
      if (hl >= sizeof(out->peer_host)) hl = sizeof(out->peer_host) - 1;
      memcpy(out->peer_host, endpoint, hl);
      out->peer_host[hl] = 0;
      int port = atoi(colon + 1);
      if (port > 0 && port <= 65535) out->peer_port = (uint16_t)port;
    } else {
      strncpy(out->peer_host, endpoint, sizeof(out->peer_host) - 1);
    }
  }

  out->valid = out->if_priv[0] && out->if_addr[0] && out->peer_pub[0] && out->peer_host[0];
  return out->valid;
}
