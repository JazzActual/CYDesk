#include "github_page.h"

#include <WiFi.h>

#include "count_text.h"
#include "fonts.h"
#include "github_service.h"
#include "layout.h"
#include "theme.h"

namespace {
constexpr int MARGIN = 14;

void centerMessage(TFT_eSPI& tft, const char* primary, const char* secondary, uint16_t color) {
  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, theme::BG);
  if (secondary && secondary[0]) {
    tft.drawString(primary, tft.width() / 2, tft.height() / 2 - 14);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(secondary, tft.width() / 2, tft.height() / 2 + 12);
  } else {
    tft.drawString(primary, tft.width() / 2, tft.height() / 2);
  }
}

String fitWidth(TFT_eSPI& tft, const String& text, int maxWidth) {
  if (tft.textWidth(text) <= maxWidth) return text;
  String fitted = text;
  while (fitted.length() > 1) {
    fitted.remove(fitted.length() - 1);
    if (tft.textWidth(fitted + "...") <= maxWidth) return fitted + "...";
  }
  return fitted;
}

String repoName(const String& slug) {
  const int slash = slug.indexOf('/');
  return slash < 0 ? slug : slug.substring(slash + 1);
}

String ownerName(const String& slug) {
  const int slash = slug.indexOf('/');
  return slash < 0 ? String() : slug.substring(0, slash);
}

uint16_t softAccent() {
  const uint32_t c = theme::accentRgb();
  return theme::rgb(((c >> 16) & 0xFF) / 2, ((c >> 8) & 0xFF) / 2, (c & 0xFF) / 2);
}

// A thin line of one point per day. Hidden until two days are saved.
void drawTrend(TFT_eSPI& tft, int repoIndex, int top) {
  const int n = github::historyCount(repoIndex);
  if (n < 2) return;
  int32_t lo = github::historyStars(repoIndex, 0);
  int32_t hi = lo;
  for (int i = 1; i < n; i++) {
    const int32_t value = github::historyStars(repoIndex, i);
    if (value < lo) lo = value;
    if (value > hi) hi = value;
  }

  const int width = layout::wide(tft) ? 280 : 200;
  const int height = 26;
  const int left = (tft.width() - width) / 2;
  const uint16_t color = softAccent();
  int lastX = left;
  int lastY = top + height / 2;
  for (int i = 0; i < n; i++) {
    const int x = left + i * (width - 1) / (n - 1);
    const int32_t value = github::historyStars(repoIndex, i);
    const int y = hi == lo ? top + height / 2
                           : top + (height - 1) - static_cast<int>((value - lo) * (height - 1) / (hi - lo));
    if (i > 0) tft.drawLine(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
  }
  tft.fillRect(lastX - 1, lastY - 1, 2, 2, theme::accent());
}

void drawOne(TFT_eSPI& tft, const github::Repo& repo) {
  const layout::HeroStats L = layout::heroStats(tft.height());
  if (repo.missing) {
    centerMessage(tft, "Repo not found", repo.slug.c_str(), theme::PINK);
    return;
  }
  if (!repo.ok) {
    if (github::lastError().length()) {
      centerMessage(tft, github::lastError().c_str(), nullptr, theme::PINK);
    } else if (WiFi.status() != WL_CONNECTED) {
      centerMessage(tft, "Waiting for Wi-Fi", nullptr, theme::MUTED);
    } else {
      centerMessage(tft, "Loading GitHub...", nullptr, theme::MUTED);
    }
    return;
  }

  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString(fitWidth(tft, repoName(repo.slug), tft.width() - 2 * MARGIN), MARGIN, L.nameY);

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextColor(theme::MUTED, theme::BG);
  String sub = ownerName(repo.slug);
  if (repo.stale) sub += sub.length() ? "  -  stale" : "stale";
  tft.drawString(sub, MARGIN, L.subY);

  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(compactCount(repo.stars), tft.width() / 2, L.heroY);
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("stars", tft.width() / 2, L.heroLabelY);
  drawTrend(tft, 0, L.trendY);

  const String url = "https://github.com/" + repo.slug;
  tft.setTextDatum(BC_DATUM);
  tft.drawString(fitWidth(tft, url, tft.width() - 16), tft.width() / 2, tft.height() - 6);
}

void drawList(TFT_eSPI& tft) {
  const int n = github::count();
  const int rowH = (tft.height() - 28) / n;
  for (int i = 0; i < n; i++) {
    const github::Repo& repo = github::at(i);
    const int y = 16 + i * rowH;
    String count = repo.ok ? compactCount(repo.stars) : (repo.missing ? "missing" : "...");
    if (repo.stale) count += "*";

    fonts::use(tft, fonts::Id::Small);
    const int countW = tft.textWidth(count);
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(theme::accent(), theme::BG);
    const int nameMax = tft.width() - 2 * MARGIN - countW - 10;
    tft.drawString(fitWidth(tft, repo.slug, nameMax), MARGIN, y + 8);

    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(repo.ok && !repo.stale ? theme::TEXT : theme::MUTED, theme::BG);
    tft.drawString(count, tft.width() - MARGIN, y + 14);
  }
}
}  // namespace

uint32_t GithubPage::signature() const {
  if (!settings_.githubRepos.length()) return 0;
  return 0x80000000u | github::generation();
}

void GithubPage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (!settings_.githubRepos.length() || github::count() == 0) {
    centerMessage(tft, "No repos yet", "Add them on the settings page", theme::TEXT);
    return;
  }
  drawOne(tft, github::at(0));
}

void GithubPage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}

uint32_t GithubListPage::signature() const {
  if (github::count() < 2) return settings_.githubRepos.length() ? 2 : 1;
  return 0x40000000u | github::generation();
}

void GithubListPage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (github::count() < 2) {
    if (!settings_.githubRepos.length()) {
      centerMessage(tft, "No repos yet", "Add them on the settings page", theme::TEXT);
    } else {
      centerMessage(tft, "One repo is on Stars", "Add another on the settings page", theme::TEXT);
    }
    return;
  }
  drawList(tft);
}

void GithubListPage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}
