#include "transport/token_gate.h"
#include <string.h>

bool tokenEquals(const char* got, const char* expected) {
  if (!got || !expected) return false;
  size_t lg = strlen(got), le = strlen(expected);
  unsigned char diff = (unsigned char)((lg ^ le) != 0);
  for (size_t i = 0; i < le; i++) {
    unsigned char g = (i < lg) ? (unsigned char)got[i] : 0;
    diff |= (unsigned char)(g ^ (unsigned char)expected[i]);
  }
  return diff == 0;
}
