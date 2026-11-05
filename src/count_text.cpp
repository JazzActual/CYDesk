#include "count_text.h"

namespace {
int decimalsFor(int64_t n, int64_t unit) {
  const int64_t whole = n / unit;
  if (whole >= 100) return 0;
  if (whole >= 10) return 1;
  return 2;
}

int64_t roundTo(int64_t n, int64_t unit, int decimals) {
  int64_t factor = 1;
  for (int i = 0; i < decimals; i++) factor *= 10;
  return (n * factor + unit / 2) / unit;
}

// 9.996 rounds up to 10.00, which is one less decimal place.
void carry(int64_t& rounded, int& decimals) {
  while (decimals > 0 && rounded >= 1000) {
    rounded = (rounded + 5) / 10;
    decimals--;
  }
}

String digits(int64_t rounded, int decimals) {
  while (decimals > 0 && rounded % 10 == 0) {
    rounded /= 10;
    decimals--;
  }
  if (decimals == 0) return String(static_cast<long>(rounded));

  int64_t scale = 1;
  for (int i = 0; i < decimals; i++) scale *= 10;
  char frac[8];
  snprintf(frac, sizeof(frac), "%0*d", decimals, static_cast<int>(rounded % scale));
  return String(static_cast<long>(rounded / scale)) + "." + frac;
}
}  // namespace

String compactCount(int32_t value) {
  if (value < 0) value = 0;
  if (value < 1000) return String(static_cast<long>(value));

  const int64_t n = value;
  int64_t unit = n >= 1000000 ? 1000000 : 1000;
  char suffix = n >= 1000000 ? 'M' : 'k';
  int decimals = decimalsFor(n, unit);
  int64_t rounded = roundTo(n, unit, decimals);
  carry(rounded, decimals);

  // 999,500 networks is 1,000k, which is just 1M.
  if (suffix == 'k' && decimals == 0 && rounded >= 1000) {
    suffix = 'M';
    unit = 1000000;
    decimals = decimalsFor(n, unit);
    rounded = roundTo(n, unit, decimals);
    carry(rounded, decimals);
  }

  String text = digits(rounded, decimals);
  text += suffix;
  return text;
}
