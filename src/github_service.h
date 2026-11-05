// GitHub star counts for the Stars page.
//
// One read-only request at a time:
//   GET https://api.github.com/repos/<owner>/<repo>
// Public repos need no token. GitHub allows about 60 of these an hour
// without one, so a few repos every 15 minutes fits. The board never
// uploads anything. A failed check keeps the last count.
//
// One repo also keeps a daily star count, up to 21 days, and the page draws
// that as a thin line once two days are saved. The big number still updates
// every 15 minutes. A new point is stored only once per local day.
#pragma once
#include "settings.h"

namespace github {

constexpr int MAX_REPOS = 4;
constexpr uint32_t REFRESH_MS = 15 * 60 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;

struct Repo {
  String slug;  // "owner/repo"
  int32_t stars = 0;
  bool ok = false;
  bool missing = false;
  bool stale = false;
  uint32_t lastAttempt = 0;
};

// Turns a typed list into at most four "owner/repo" lines.
String normalizeRepos(const String& raw);

void prepare(const Settings& settings);
void update(const Settings& settings);

uint32_t generation();
int count();
const Repo& at(int index);
bool loading();
const String& lastError();

// Daily points for the sparkline, oldest first. Empty until two local days
// have a saved count.
int historyCount(int index);
int32_t historyStars(int index, int point);

}  // namespace github
