#include "fonts.h"

// The font data lives in flash. Include each header in exactly one .cpp file.
#include "fonts/rajdhani_14.h"
#include "fonts/rajdhani_16.h"
#include "fonts/rajdhani_24.h"
#include "fonts/rajdhani_40.h"
#include "fonts/rajdhani_56.h"

namespace fonts {
namespace {
Id loaded = Id::BuiltIn;

const uint8_t* dataFor(Id id) {
  switch (id) {
    case Id::Tiny:   return rajdhani_14;
    case Id::Small:  return rajdhani_16;
    case Id::Medium: return rajdhani_24;
    case Id::Temp:   return rajdhani_40;
    case Id::Clock:  return rajdhani_56;
    default:         return nullptr;
  }
}
}  // namespace

void use(TFT_eSPI& tft, Id id) {
  if (id == loaded) return;
  if (loaded != Id::BuiltIn) tft.unloadFont();
  const uint8_t* data = dataFor(id);
  if (data) tft.loadFont(data);
  loaded = id;
}

}  // namespace fonts
