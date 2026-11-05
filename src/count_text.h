// Short counts for the stats pages: 999, 1.2k, 12.5k, 1.25M.
// Under 1,000 the number is left as-is. Thousands and millions keep about
// three digits, and trailing zeros after the dot are dropped.
#pragma once
#include <Arduino.h>

String compactCount(int32_t value);
