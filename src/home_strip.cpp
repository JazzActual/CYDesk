#include "home_strip.h"

#include "count_text.h"
#include "theme.h"
#include "wardrift_service.h"
#include "wdgwars_service.h"
#include "wigle_service.h"

namespace home_strip {
namespace {

// WDGWars field order matches the settings dropdown.
enum WdgField : uint8_t {
  WdgTotal = 0,
  WdgToday,
  WdgWeek,
  WdgWifi,
  WdgBle,
  WdgAircraft,
  WdgMesh,
  WdgBadges,
  WdgCount
};

enum WigleField : uint8_t {
  WigleMonthRank = 0,
  WigleRank,
  WigleWifi,
  WigleCell,
  WigleBt,
  WigleMonthCount,
  WigleCount
};

enum WardriftField : uint8_t {
  DriftLevel = 0,
  DriftNetworks,
  DriftRoutes,
  DriftThreats,
  DriftStreak,
  DriftCurrency,
  DriftExpToNext,
  DriftCount
};

const char* WDG_LABELS[] = {"total", "today", "week", "WiFi", "BLE", "Air", "Mesh", "badges"};
const char* WDG_TITLES[] = {"Total networks", "Today", "This week", "Wi-Fi", "BLE",
                            "Aircraft", "Mesh", "Badges"};

const char* WIGLE_LABELS[] = {"mo #", "rank", "WiFi", "Cell", "BT", "month"};
const char* WIGLE_TITLES[] = {"Monthly rank", "All-time rank", "Wi-Fi discoveries", "Cell",
                              "Bluetooth", "This month's count"};

const char* DRIFT_LABELS[] = {"lvl", "nets", "routes", "threats", "streak", "coin", "next"};
const char* DRIFT_TITLES[] = {"Level", "Networks", "Routes", "Threats", "Day streak",
                              "Currency", "XP to next level"};

bool isRankField(HomeStripSource source, uint8_t field) {
  return source == HomeStripSource::Wigle &&
         (field == WigleMonthRank || field == WigleRank);
}

}  // namespace

uint8_t fieldCount(HomeStripSource source) {
  switch (source) {
    case HomeStripSource::Wdg:
      return WdgCount;
    case HomeStripSource::Wigle:
      return WigleCount;
    case HomeStripSource::Wardrift:
      return DriftCount;
    case HomeStripSource::Off:
    default:
      return 0;
  }
}

const char* fieldLabel(HomeStripSource source, uint8_t field) {
  switch (source) {
    case HomeStripSource::Wdg:
      return field < WdgCount ? WDG_LABELS[field] : "?";
    case HomeStripSource::Wigle:
      return field < WigleCount ? WIGLE_LABELS[field] : "?";
    case HomeStripSource::Wardrift:
      return field < DriftCount ? DRIFT_LABELS[field] : "?";
    default:
      return "";
  }
}

const char* fieldTitle(HomeStripSource source, uint8_t field) {
  switch (source) {
    case HomeStripSource::Wdg:
      return field < WdgCount ? WDG_TITLES[field] : "?";
    case HomeStripSource::Wigle:
      return field < WigleCount ? WIGLE_TITLES[field] : "?";
    case HomeStripSource::Wardrift:
      return field < DriftCount ? DRIFT_TITLES[field] : "?";
    default:
      return "";
  }
}

String formatValue(HomeStripSource source, uint8_t field) {
  switch (source) {
    case HomeStripSource::Wdg: {
      const wdg::Stats& s = wdg::current();
      if (!s.ok) return "—";
      switch (field) {
        case WdgTotal:
          return compactCount(s.total);
        case WdgToday:
          return s.haveRecent ? compactCount(s.today) : "—";
        case WdgWeek:
          return s.haveRecent ? compactCount(s.week) : "—";
        case WdgWifi:
          return compactCount(s.wifi);
        case WdgBle:
          return compactCount(s.ble);
        case WdgAircraft:
          return compactCount(s.aircraft);
        case WdgMesh:
          return compactCount(s.mesh);
        case WdgBadges:
          return String(s.badges);
        default:
          return "—";
      }
    }
    case HomeStripSource::Wigle: {
      const wigle::Stats& s = wigle::current();
      if (!s.ok) return "—";
      switch (field) {
        case WigleMonthRank:
          return "#" + compactCount(s.monthRank);
        case WigleRank:
          return "#" + compactCount(s.rank);
        case WigleWifi:
          return compactCount(s.wifi);
        case WigleCell:
          return compactCount(s.cell);
        case WigleBt:
          return compactCount(s.bt);
        case WigleMonthCount:
          return compactCount(s.monthCount);
        default:
          return "—";
      }
    }
    case HomeStripSource::Wardrift: {
      const wardrift::Stats& s = wardrift::current();
      if (!s.ok) return "—";
      switch (field) {
        case DriftLevel:
          return "L" + String(s.level);
        case DriftNetworks:
          return compactCount(s.networks);
        case DriftRoutes:
          return compactCount(s.routes);
        case DriftThreats:
          return compactCount(s.threats);
        case DriftStreak:
          return String(s.streakDays);
        case DriftCurrency:
          return compactCount(s.currency);
        case DriftExpToNext:
          return compactCount(s.expToNext);
        default:
          return "—";
      }
    }
    default:
      return "—";
  }
}

uint16_t valueColour(HomeStripSource source, uint8_t field, bool haveValue) {
  if (!haveValue) return theme::MUTED;
  if (isRankField(source, field)) return theme::PINK;
  return theme::TEXT;
}

int pageIndex(HomeStripSource source) {
  switch (source) {
    case HomeStripSource::Wdg:
      return ScreenBadge;
    case HomeStripSource::Wigle:
      return ScreenWigle;
    case HomeStripSource::Wardrift:
      return ScreenWardrift;
    default:
      return -1;
  }
}

void clamp(Settings& settings) {
  if (settings.homeStripSource > static_cast<uint8_t>(HomeStripSource::Wardrift)) {
    settings.homeStripSource = static_cast<uint8_t>(HomeStripSource::Off);
  }
  const auto source = static_cast<HomeStripSource>(settings.homeStripSource);
  const uint8_t n = fieldCount(source);
  for (int i = 0; i < LINES; i++) {
    if (n == 0) {
      settings.homeStripField[i] = 0;
      continue;
    }
    if (settings.homeStripField[i] >= n) {
      settings.homeStripField[i] = static_cast<uint8_t>(i < n ? i : 0);
    }
  }
}

}  // namespace home_strip
