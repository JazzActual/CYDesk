// Fetches the badge image in the background and keeps the last good JPEG.
//
// The desk screen can show a picture, not a web page. The saved badge is an
// image address (taken from an embed on the settings page). This asks wsrv.nl
// to fit that image inside the screen as one baseline JPEG, every 10 minutes.
// Pages draw whatever is already here, so tapping to the badge page is instant.
#pragma once
#include <TFT_eSPI.h>

#include "settings.h"

namespace badge {

constexpr uint32_t REFRESH_MS = 10 * 60 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;

// Call when settings are saved, before the screen redraws. Drops the old
// picture immediately if the address changed.
void prepare(const Settings& settings);

// Call every loop(), after the weather fetch. Downloads when one is due
// (blocks for a second or two while it does). Skipped while Wi-Fi is down.
void update(const Settings& settings);

// Bumps when the picture, the error, or the saved address changes.
uint32_t generation();

bool hasImage();
// True when a later fetch failed and the screen is still showing the last good picture.
bool stale();
const String& lastError();

// Draws the JPEG centered. The caller fills the background first.
// Returns false when there is no picture yet.
bool paint(TFT_eSPI& tft, int& x, int& y, int& w, int& h);

}  // namespace badge
