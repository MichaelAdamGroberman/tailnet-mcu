#pragma once
// Constant-time token comparison: folds length + every byte into one diff
// accumulator so compare time does not depend on the matching prefix length.
bool tokenEquals(const char* got, const char* expected);
