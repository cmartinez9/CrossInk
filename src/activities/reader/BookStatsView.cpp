#include "BookStatsView.h"

#include <FreeInkUICore.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <array>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/TouchActionButtons.h"
#include "components/TouchHeaderBackButton.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/icons/listIcons.h"
#include "components/icons/reliefIcons.h"
#include "components/themes/relief/ReliefKit.h"
#include "fontIds.h"

namespace {
constexpr int kStatsButtonHintTopGap = 10;
constexpr int kStandaloneNoRtcMaxTopCardHeightDivisor = 2;
constexpr int kStandaloneNoRtcMaxVerticalOffset = 32;
constexpr int kPerBookRtcTopCardMaxExtra = 84;

struct StatsLayout {
  int headerHeight;
  int headerDrawHeight;
  int topGap;
  int cardGap;
  int topCardTitleH;
  int topCardH;
  int globalCardH;
  int sectionTitleH;
  int sectionTitleFontId;
  int chartLabelFontId;
  int chartLabelW;
  int barH;
  int barGap;
  int chartTopPadding;
  int chartBottomPadding;
};

constexpr StatsLayout kDefaultLayout = {
    .headerHeight = 78,
    .headerDrawHeight = 67,
    .topGap = 8,
    .cardGap = 26,
    .topCardTitleH = 36,
    .topCardH = 214,
    .globalCardH = 154,
    .sectionTitleH = 34,
    .sectionTitleFontId = UI_10_FONT_ID,
    .chartLabelFontId = UI_10_FONT_ID,
    .chartLabelW = 88,
    .barH = 22,
    .barGap = 12,
    .chartTopPadding = 14,
    .chartBottomPadding = 14,
};

constexpr StatsLayout kCompactLayout = {
    .headerHeight = 67,
    .headerDrawHeight = 67,
    .topGap = 6,
    .cardGap = 8,
    .topCardTitleH = 30,
    .topCardH = 156,
    .globalCardH = 110,
    .sectionTitleH = 30,
    .sectionTitleFontId = UI_10_FONT_ID,
    .chartLabelFontId = SMALL_FONT_ID,
    .chartLabelW = 78,
    .barH = 16,
    .barGap = 8,
    .chartTopPadding = 8,
    .chartBottomPadding = 8,
};

constexpr std::array<StrId, READING_TIME_BUCKET_COUNT> TIME_BUCKET_LABELS = {
    StrId::STR_STATS_MORNING, StrId::STR_STATS_AFTERNOON, StrId::STR_STATS_EVENING, StrId::STR_STATS_NIGHT};
constexpr std::array<StrId, READING_DAY_OF_WEEK_COUNT> DAY_LABELS = {
    StrId::STR_STATS_MON, StrId::STR_STATS_TUE, StrId::STR_STATS_WED, StrId::STR_STATS_THU,
    StrId::STR_STATS_FRI, StrId::STR_STATS_SAT, StrId::STR_STATS_SUN};

const char* dayCountText(const uint16_t days) { return days == 1 ? tr(STR_STATS_DAY) : tr(STR_STATS_DAYS); }

int sectionCardHeight(const StatsLayout& layout, const int rowCount) {
  if (rowCount <= 0) {
    return layout.sectionTitleH + layout.chartTopPadding + layout.chartBottomPadding;
  }
  const int rowStride = layout.barH + layout.barGap;
  return layout.sectionTitleH + layout.chartTopPadding + layout.chartBottomPadding + layout.barH +
         (rowCount - 1) * rowStride;
}

bool shouldShowRtcBasedStats() { return halClock.isAvailable(); }

int noRtcCardBaseHeight(const StatsLayout& layout) { return layout.globalCardH; }

int statsHeaderHeight(const ThemeMetrics& metrics, const StatsLayout& layout, const MappedInputManager* mappedInput) {
  if (mappedInput && mappedInput->hasTouchHardware()) {
    return CompactHeader::height(metrics);
  }
  return std::min(metrics.headerHeight, layout.headerHeight);
}

int statsContentHeight(const StatsLayout& layout, const int headerHeight, const bool globalPage,
                       const bool showRtcStats) {
  const int topCardH = globalPage ? layout.globalCardH : layout.topCardH;
  if (!showRtcStats) {
    return headerHeight + layout.topGap + topCardH;
  }
  const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
  const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
  return headerHeight + layout.topGap + topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
}

int noRtcCombinedContentHeight(const StatsLayout& layout, const int headerHeight, const bool showAllDevicesStats) {
  const int cardBaseH = noRtcCardBaseHeight(layout);
  return headerHeight + layout.topGap + cardBaseH + layout.cardGap + layout.globalCardH +
         (showAllDevicesStats ? layout.cardGap + layout.globalCardH : 0);
}

int statsBottomInset(const ThemeMetrics& metrics, const bool showButtonHints) {
  return metrics.verticalSpacing + (showButtonHints ? metrics.buttonHintsHeight + kStatsButtonHintTopGap : 0);
}

int perBookRtcTopCardHeight(const StatsLayout& layout, const int extraHeight) {
  return layout.topCardH + std::min(extraHeight, kPerBookRtcTopCardMaxExtra);
}

int globalRtcCardHeightForPerBookRowSpacing(const StatsLayout& layout, const int perBookExtraHeight) {
  constexpr int perBookDataRowCount = 3;
  constexpr int globalDataRowCount = 2;
  const int perBookDataRowH =
      (perBookRtcTopCardHeight(layout, perBookExtraHeight) - layout.topCardTitleH) / perBookDataRowCount;
  return std::max(layout.globalCardH, layout.topCardTitleH + perBookDataRowH * globalDataRowCount);
}

const StatsLayout& getStatsLayout(const GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const bool globalPage, const bool showButtonHints, const bool showRtcStats) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  const int defaultHeaderHeight = statsHeaderHeight(metrics, kDefaultLayout, mappedInput);
  const bool defaultFitsCurrentPage =
      statsContentHeight(kDefaultLayout, defaultHeaderHeight, globalPage, showRtcStats) <= availableHeight;
  const bool defaultMatchesPerBookCharts =
      !globalPage || !showRtcStats ||
      statsContentHeight(kDefaultLayout, defaultHeaderHeight, false, showRtcStats) <= availableHeight;
  if (defaultFitsCurrentPage && defaultMatchesPerBookCharts) {
    return kDefaultLayout;
  }
  return kCompactLayout;
}

const StatsLayout& getNoRtcCombinedLayout(const GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                          const bool showButtonHints, const bool showAllDevicesStats) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  if (noRtcCombinedContentHeight(kDefaultLayout, statsHeaderHeight(metrics, kDefaultLayout, mappedInput),
                                 showAllDevicesStats) <= availableHeight) {
    return kDefaultLayout;
  }
  return kCompactLayout;
}

bool fallbackEstimatedTimeLeft(const BookReadingStats& stats, const float progressPercent, uint32_t& seconds) {
  seconds = 0;
  if (progressPercent <= 0.0f || progressPercent >= 100.0f || stats.totalReadingSeconds < 120) {
    return false;
  }

  const float progress = progressPercent / 100.0f;
  const float estimate = (static_cast<float>(stats.totalReadingSeconds) * (1.0f - progress)) / progress;
  if (estimate <= 0.0f) {
    return false;
  }
  seconds = static_cast<uint32_t>(estimate + 0.5f);
  return seconds > 0;
}

bool cachedEstimatedTimeLeft(const BookReadingStats& stats, uint32_t& seconds) {
  seconds = stats.estimatedTimeLeftSeconds;
  return seconds > 0;
}

bool estimateFinishDateFromDailyPace(const BookReadingStats& stats, const ReadingStatsDateTime& today,
                                     const uint32_t estimatedReadingSeconds, ReadingStatsDate& outDate) {
  outDate = {};
  if (!today.isValid() || !stats.startDate.isValid() || estimatedReadingSeconds == 0 ||
      stats.totalReadingSeconds == 0) {
    return false;
  }

  const uint16_t elapsedDays = readingSpanDaysElapsed(stats.startDate, today.date);
  const uint16_t readingDays = std::max<uint16_t>(1, elapsedDays);

  // Convert remaining reading time into calendar time using the book's average reading seconds per calendar day.
  const uint64_t estimatedCalendarSeconds =
      (static_cast<uint64_t>(estimatedReadingSeconds) * static_cast<uint64_t>(readingDays) * 86400ULL +
       static_cast<uint64_t>(stats.totalReadingSeconds) / 2ULL) /
      static_cast<uint64_t>(stats.totalReadingSeconds);
  if (estimatedCalendarSeconds == 0) {
    return false;
  }

  ReadingStatsDateTime estimatedFinish = today;
  addSecondsToReadingStatsDateTime(estimatedFinish,
                                   static_cast<uint32_t>(std::min<uint64_t>(estimatedCalendarSeconds, UINT32_MAX)));
  outDate = estimatedFinish.date;
  return outDate.isValid();
}

float pagesPerMinute(const uint32_t totalPagesTurned, const uint32_t totalReadingSeconds) {
  if (totalReadingSeconds <= 60) {
    return 0.0f;
  }
  return static_cast<float>(totalPagesTurned) * 60.0f / static_cast<float>(totalReadingSeconds);
}

void drawCenteredLabel(const GfxRenderer& renderer, const int fontId, const int x, const int w, const int y,
                       const char* text, const bool bold = false) {
  const int textWidth = renderer.getTextWidth(fontId, text, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  renderer.drawText(fontId, x + (w - textWidth) / 2, y, text, true,
                    bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}

void drawStatCell(const GfxRenderer& renderer, const int x, const int w, const int y, const int h, const char* value,
                  const char* label) {
  const int valueLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int totalTextH = valueLineH + 4 + labelLineH;
  const int textY = y + (h - totalTextH) / 2;
  drawCenteredLabel(renderer, UI_12_FONT_ID, x, w, textY, value, true);
  drawCenteredLabel(renderer, SMALL_FONT_ID, x, w, textY + valueLineH + 4, label);
}

void drawSectionCard(const GfxRenderer& renderer, const int x, const int y, const int w, const int h, const char* title,
                     const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.sectionTitleH, x + w, y + layout.sectionTitleH);
  drawCenteredLabel(renderer, layout.sectionTitleFontId, x, w,
                    y + (layout.sectionTitleH - renderer.getLineHeight(layout.sectionTitleFontId)) / 2, title, true);
}

template <size_t N>
void drawHorizontalBars(GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                        const std::array<uint32_t, N>& values, const std::array<StrId, N>& labels,
                        const StatsLayout& layout) {
  constexpr int labelLeftPadding = 10;
  constexpr int labelRightPadding = 18;
  constexpr int barLeftGap = 8;
  constexpr int rightPadding = 18;
  const uint32_t maxValue = *std::max_element(values.begin(), values.end());
  const int labelLineH = renderer.getLineHeight(layout.chartLabelFontId);
  const int rowContentH = std::max(labelLineH, layout.barH);
  const int baseContentH = layout.sectionTitleH + layout.chartTopPadding + layout.chartBottomPadding + rowContentH +
                           (static_cast<int>(N) - 1) * (rowContentH + layout.barGap);
  const int extraHeight = std::max(0, h - baseContentH);
  const int spacingSlotCount = static_cast<int>(N) + 1;
  const int extraPerSlot = spacingSlotCount > 0 ? extraHeight / spacingSlotCount : 0;
  const int extraRemainder = spacingSlotCount > 0 ? extraHeight % spacingSlotCount : 0;
  const int topPadding = layout.chartTopPadding + extraPerSlot + (extraRemainder > 0 ? 1 : 0);
  const int rowGap = layout.barGap + extraPerSlot;
  const int contentTop = y + layout.sectionTitleH + topPadding;
  const int rowStride = rowContentH + rowGap;
  int maxLabelW = 0;
  for (size_t i = 0; i < N; ++i) {
    maxLabelW = std::max(maxLabelW, renderer.getTextWidth(layout.chartLabelFontId, I18N.get(labels[i])));
  }
  const int labelColumnW = std::max(layout.chartLabelW, labelLeftPadding + maxLabelW + labelRightPadding);
  const int barX = x + labelColumnW + barLeftGap;
  const int barW = std::max(0, w - labelColumnW - barLeftGap - rightPadding);
  for (size_t i = 0; i < N; ++i) {
    const int rowTop = contentTop + static_cast<int>(i) * rowStride;
    const int labelY = rowTop + (rowContentH - labelLineH) / 2;
    const int barY = rowTop + (rowContentH - layout.barH) / 2;
    renderer.drawText(layout.chartLabelFontId, x + labelLeftPadding, labelY, I18N.get(labels[i]));
    if (maxValue > 0 && values[i] > 0) {
      const int fillW = std::max(2, static_cast<int>((static_cast<uint64_t>(barW) * values[i]) / maxValue));
      renderer.fillRect(barX, barY, fillW, layout.barH, true);
    }
  }
}

void drawPerBookStatsCard(GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                          const std::string& bookTitle, const BookReadingStats& stats, const float progressPercent,
                          const bool hasEstimatedTimeLeft, const uint32_t estimatedTimeLeftSeconds,
                          const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.topCardTitleH, x + w, y + layout.topCardTitleH);
  const std::string visibleTitle =
      renderer.truncatedText(UI_10_FONT_ID, bookTitle.c_str(), w - 20, EpdFontFamily::BOLD);
  drawCenteredLabel(renderer, UI_10_FONT_ID, x, w,
                    y + (layout.topCardTitleH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, visibleTitle.c_str(), true);

  const bool showRtcStats = shouldShowRtcBasedStats();
  const int thirdW = w / 3;
  const int halfW = w / 2;
  const int rowCount = showRtcStats ? 3 : 2;
  const int rowH = (h - layout.topCardTitleH) / rowCount;
  char buf[40];

  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(stats.sessionCount));
  drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_SESSIONS_LBL));

  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_TIME_LBL));

  if (progressPercent >= 0.0f) {
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(progressPercent + 0.5f));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawStatCell(renderer, x + thirdW * 2, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_PROGRESS_LBL));

  const uint32_t avgSecs = stats.sessionCount > 0 ? stats.totalReadingSeconds / stats.sessionCount : 0;
  BookReadingStats::formatDuration(avgSecs, buf, sizeof(buf));
  drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));

  uint32_t fallbackEstimateSeconds = 0;
  uint32_t cachedEstimateSeconds = 0;
  const bool hasCachedEstimate = cachedEstimatedTimeLeft(stats, cachedEstimateSeconds);
  const bool hasFallbackEstimate = fallbackEstimatedTimeLeft(stats, progressPercent, fallbackEstimateSeconds);
  if (!stats.isCompleted && (hasEstimatedTimeLeft || hasCachedEstimate || hasFallbackEstimate)) {
    formatCompactReadingDuration(hasEstimatedTimeLeft ? estimatedTimeLeftSeconds
                                 : hasCachedEstimate  ? cachedEstimateSeconds
                                                      : fallbackEstimateSeconds,
                                 buf, sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_TIME_LEFT));

  snprintf(buf, sizeof(buf), "%.1f", pagesPerMinute(stats.totalPagesTurned, stats.totalReadingSeconds));
  drawStatCell(renderer, x + thirdW * 2, thirdW, y + layout.topCardTitleH + rowH, rowH, buf,
               tr(STR_STATS_PAGES_PER_MIN));

  if (!showRtcStats) {
    return;
  }

  ReadingStatsDateTime today;
  const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
  const ReadingStatsDate endDate = stats.isCompleted && stats.finishedDate.isValid()
                                       ? stats.finishedDate
                                       : (hasToday ? today.date : ReadingStatsDate{});
  const bool hasDaySpan = stats.startDate.isValid() && endDate.isValid();
  const uint16_t daysReading = hasDaySpan ? readingSpanDaysElapsed(stats.startDate, endDate) : 0;
  if (hasDaySpan) {
    snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(daysReading), dayCountText(daysReading));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  char startedLabel[32];
  char dateBuf[24];
  formatReadingStatsShortDate(stats.startDate, dateBuf, sizeof(dateBuf));
  snprintf(startedLabel, sizeof(startedLabel), "%s %s", tr(STR_STATS_STARTED), dateBuf);
  const int startedY = y + layout.topCardTitleH + rowH * 2;
  TouchRegistry::getInstance().add(Rect(x, startedY, halfW, rowH), BookStatsTouchTarget::StartedDaysStat,
                                   TouchRegistry::Item);
  drawStatCell(renderer, x, halfW, startedY, rowH, buf, startedLabel);

  ReadingStatsDate finishDisplayDate;
  bool finished = stats.isCompleted;
  if (finished) {
    finishDisplayDate = stats.finishedDate;
  } else if (hasToday && (hasEstimatedTimeLeft || hasCachedEstimate || hasFallbackEstimate)) {
    const uint32_t remainingReadingSeconds = hasEstimatedTimeLeft ? estimatedTimeLeftSeconds
                                             : hasCachedEstimate  ? cachedEstimateSeconds
                                                                  : fallbackEstimateSeconds;
    if (!estimateFinishDateFromDailyPace(stats, today, remainingReadingSeconds, finishDisplayDate)) {
      ReadingStatsDateTime estimatedFinish = today;
      addSecondsToReadingStatsDateTime(estimatedFinish, remainingReadingSeconds);
      finishDisplayDate = estimatedFinish.date;
    }
  }
  formatReadingStatsShortDate(finishDisplayDate, buf, sizeof(buf));
  drawStatCell(renderer, x + halfW, halfW, y + layout.topCardTitleH + rowH * 2, rowH, buf,
               finished ? tr(STR_STATS_FINISHED_DATE) : tr(STR_STATS_EST_FINISH_DATE));
}

void drawGlobalStatsCard(GfxRenderer& renderer, const int x, const int y, const int w, const int h, const char* title,
                         const GlobalReadingStats& stats, const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.topCardTitleH, x + w, y + layout.topCardTitleH);
  const bool showRtcStats = shouldShowRtcBasedStats();
  drawCenteredLabel(renderer, UI_10_FONT_ID, x, w,
                    y + (layout.topCardTitleH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, title, true);

  const int thirdW = w / 3;
  const int halfW = w / 2;
  const int rowH = (h - layout.topCardTitleH) / 2;
  char buf[40];

  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalSessions));
  drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_SESSIONS_LBL));

  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_TIME_LBL));

  snprintf(buf, sizeof(buf), "%.1f", pagesPerMinute(stats.totalPagesTurned, stats.totalReadingSeconds));
  drawStatCell(renderer, x + thirdW * 2, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_PAGES_PER_MIN));

  const uint32_t avgSecs = stats.totalSessions > 0 ? stats.totalReadingSeconds / stats.totalSessions : 0;
  BookReadingStats::formatDuration(avgSecs, buf, sizeof(buf));
  if (showRtcStats) {
    drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));
  } else {
    drawStatCell(renderer, x, halfW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));
  }

  if (showRtcStats) {
    ReadingStatsDateTime today;
    const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
    const uint16_t currentStreak = hasToday ? stats.currentReadingStreak(&today.date) : 0;
    if (currentStreak > 0) {
      snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(currentStreak), dayCountText(currentStreak));
    } else {
      snprintf(buf, sizeof(buf), "-");
    }
    drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH + rowH, rowH, buf,
                 tr(STR_STATS_READING_STREAK_LBL));
  }

  if (stats.completedBooks > 0) {
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.completedBooks));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawStatCell(renderer, showRtcStats ? x + thirdW * 2 : x + halfW, showRtcStats ? thirdW : halfW,
               y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_COMPLETED_LBL));
}

void drawDateField(const GfxRenderer& renderer, const int x, const int y, const int w, const char* text,
                   const bool selected, const int touchTarget = -1) {
  const int h = renderer.getLineHeight(UI_12_FONT_ID) + 10;
  if (touchTarget >= 0) {
    TouchRegistry::getInstance().add(Rect(x, y, w, h), touchTarget, TouchRegistry::Item);
  }
  renderer.fillRectDither(x, y, w, h, selected ? Color::LightGray : Color::White);
  renderer.drawRect(x, y, w, h, true);
  if (selected) {
    renderer.drawRect(x + 1, y + 1, w - 2, h - 2, true);
  }
  drawCenteredLabel(renderer, UI_12_FONT_ID, x, w, y + 5, text);
}

void drawDateAdjustButton(const GfxRenderer& renderer, const int x, const int y, const int size,
                          const freeink::Icon& icon, const int touchTarget) {
  TouchRegistry::getInstance().add(Rect(x, y, size, size), touchTarget, TouchRegistry::Item);
  renderer.drawRect(x, y, size, size, true);
  const freeink::ui::BitmapRef bitmap{icon.bits, icon.w, icon.h, freeink::ui::BitmapFormat::Mask1, true};
  freeink::ui::forEachBitmapPixel(
      freeink::ui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(size),
                        static_cast<int16_t>(size)},
      bitmap, freeink::ui::BitmapMode::Center,
      [&renderer](const int16_t px, const int16_t py) { renderer.drawPixel(px, py, true); });
}

// ---------------------------------------------------------------------------------------------------------------
// Relief layouts: the "Reading stats", "This device" and "Edit dates" boards of the Relief design (section 2).
// Vertical positions follow the 792 px X3 portrait design; widths follow the screen with 24 px side margins.

bool reliefStats() { return SETTINGS.uiTheme == CrossPointSettings::UI_THEME::RELIEF; }

constexpr int kReliefMargin = 24;
constexpr int kReliefRowH = 52;

// "H:MM" of a duration, for the clock and digits faces (digits and colon only).
void formatReliefHoursMinutes(const uint32_t seconds, char* buf, const size_t len) {
  snprintf(buf, len, "%lu:%02lu", static_cast<unsigned long>(seconds / 3600),
           static_cast<unsigned long>((seconds / 60) % 60));
}

void drawReliefStatsHeader(const GfxRenderer& renderer, const char* title, const char* subtitle) {
  const int W = renderer.getScreenWidth();
  std::string sub;
  if (subtitle && *subtitle) sub = renderer.truncatedText(SMALL_FONT_ID, subtitle, W - 2 * kReliefMargin - 120);
  GUI.drawHeader(renderer, Rect{0, 8, W, 80}, title, sub.empty() ? nullptr : sub.c_str());
}

void drawReliefChevron(const GfxRenderer& renderer, const int right, const int cy) {
  renderer.drawLine(right - 5, cy - 6, right, cy, 2, true);
  renderer.drawLine(right, cy, right - 5, cy + 6, 2, true);
}

struct ReliefStatsRow {
  const char* label;
  const char* value;
};

// One raised card of label / value rows with dotted rules between them; the focused row sinks.
void drawReliefRowsCard(const GfxRenderer& renderer, const int x, const int y, const int w, const ReliefStatsRow* rows,
                        const int count, const int rowH, const int focus, const bool chevrons) {
  using namespace relief;
  raised(renderer, x, y, w, 16 + count * rowH, 24);
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  for (int i = 0; i < count; ++i) {
    const int ry = y + 8 + i * rowH;
    const bool focused = i == focus;
    const int d = focused ? 1 : 0;
    if (focused) {
      pressed(renderer, x + 8, ry, w - 16, rowH - 4, 20);
    } else if (i < count - 1 && i + 1 != focus) {
      for (int dx = x + 22; dx < x + w - 22; dx += 4) renderer.drawPixel(dx, ry + rowH - 2, true);
    }
    const int ty = ry + (rowH - 4 - lh) / 2 + d;
    int right = x + w - 22 + d;
    if (chevrons) {
      drawReliefChevron(renderer, right, ty + lh / 2);
      right -= 16;
    }
    const std::string value = renderer.truncatedText(UI_10_FONT_ID, rows[i].value, w / 2, EpdFontFamily::BOLD);
    textRight(renderer, UI_10_FONT_ID, right, ty, value.c_str(), true, EpdFontFamily::BOLD);
    const int labelW =
        right - (x + 22 + d) - textWidth(renderer, UI_10_FONT_ID, value.c_str(), EpdFontFamily::BOLD) - 12;
    const auto labelStyle = focused ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, rows[i].label, std::max(0, labelW), labelStyle);
    text(renderer, UI_10_FONT_ID, x + 22 + d, ty, label.c_str(), true, labelStyle);
  }
}

// A small raised tile: a big value in the title face over a label.
void drawReliefStatTile(const GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                        const char* value, const char* label) {
  using namespace relief;
  raised(renderer, x, y, w, h, 24);
  const int font = titleFontFor(value);
  const auto style = font == kTitleFontId ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  const std::string v = renderer.truncatedText(font, value, w - 36, style);
  text(renderer, font, x + 18, y + 14, v.c_str(), true, style);
  const std::string l = renderer.truncatedText(SMALL_FONT_ID, label, w - 36);
  text(renderer, SMALL_FONT_ID, x + 18, y + h - 42, l.c_str());
}

// Time left for the book in the compact form ("2 h"), or "-" when there is no estimate or it is finished.
void formatReliefTimeLeft(const BookReadingStats& stats, const float progressPercent, const bool hasEstimatedTimeLeft,
                          const uint32_t estimatedTimeLeftSeconds, char* buf, const size_t len) {
  uint32_t cachedSeconds = 0;
  uint32_t fallbackSeconds = 0;
  const bool hasCached = cachedEstimatedTimeLeft(stats, cachedSeconds);
  const bool hasFallback = fallbackEstimatedTimeLeft(stats, progressPercent, fallbackSeconds);
  if (stats.isCompleted || !(hasEstimatedTimeLeft || hasCached || hasFallback)) {
    snprintf(buf, len, "-");
    return;
  }
  formatCompactReadingDuration(hasEstimatedTimeLeft ? estimatedTimeLeftSeconds
                               : hasCached          ? cachedSeconds
                                                    : fallbackSeconds,
                               buf, len);
}

// The top of the book page: hours in the book (clock face and book key), pace / sessions / time left tiles,
// and the book progress as ten tubes. The design's chapter tubes need chapter positions, which the stats
// screen does not have, so each tube is a tenth of the book instead.
void drawReliefBookSummary(const GfxRenderer& renderer, const BookReadingStats& stats, const float progressPercent,
                           const bool hasEstimatedTimeLeft, const uint32_t estimatedTimeLeftSeconds) {
  using namespace relief;
  const int W = renderer.getScreenWidth();
  const int x0 = kReliefMargin;
  const int cw = W - 2 * kReliefMargin;
  char buf[40];

  raised(renderer, x0, 112, cw, 170, 28);
  formatReliefHoursMinutes(stats.totalReadingSeconds, buf, sizeof(buf));
  text(renderer, kClockFontId, x0 + 20, 128, buf);
  text(renderer, SMALL_FONT_ID, x0 + 24, 236, tr(STR_RELIEF_HOURS_IN_BOOK));
  roundKey(renderer, x0 + cw - 64, 196, 72, &icon_book_open_32, true, false);

  constexpr int kTileGap = 18;
  const int tileW = (cw - 2 * kTileGap) / 3;
  const int pagesPerHour =
      static_cast<int>(pagesPerMinute(stats.totalPagesTurned, stats.totalReadingSeconds) * 60.0f + 0.5f);
  if (pagesPerHour > 0) {
    snprintf(buf, sizeof(buf), "%d", pagesPerHour);
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawReliefStatTile(renderer, x0, 306, tileW, 110, buf, tr(STR_RELIEF_PAGES_PER_HOUR));
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(stats.sessionCount));
  drawReliefStatTile(renderer, x0 + tileW + kTileGap, 306, tileW, 110, buf, tr(STR_RELIEF_SESSIONS));
  formatReliefTimeLeft(stats, progressPercent, hasEstimatedTimeLeft, estimatedTimeLeftSeconds, buf, sizeof(buf));
  drawReliefStatTile(renderer, x0 + 2 * (tileW + kTileGap), 306, cw - 2 * (tileW + kTileGap), 110, buf,
                     stats.isCompleted ? tr(STR_RELIEF_DONE) : tr(STR_RELIEF_LEFT));

  const float pct = progressPercent < 0.0f ? 0.0f : std::min(progressPercent, 100.0f);
  text(renderer, UI_10_FONT_ID, x0, 440, tr(STR_RELIEF_BOOK_PROGRESS), true, EpdFontFamily::BOLD);
  snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(pct + 0.5f));
  textRight(renderer, UI_10_FONT_ID, x0 + cw, 440, buf, true, EpdFontFamily::BOLD);
  constexpr int kTubes = 10;
  constexpr int kTubeGap = 6;
  const int tubeW = (cw - (kTubes - 1) * kTubeGap) / kTubes;
  for (int i = 0; i < kTubes; ++i) {
    const float level = std::clamp(pct / 10.0f - static_cast<float>(i), 0.0f, 1.0f);
    tube(renderer, x0 + i * (tubeW + kTubeGap), 480, tubeW, 10, level);
  }
}

void renderReliefBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                               const std::string& bookTitle, const BookReadingStats& stats, const float progressPercent,
                               const bool hasEstimatedTimeLeft, const uint32_t estimatedTimeLeftSeconds,
                               const bool showButtonHints, const bool showEditButton, const bool showNextButton) {
  renderer.clearScreen();
  const int W = renderer.getScreenWidth();
  drawReliefStatsHeader(renderer, tr(STR_READING_STATS), bookTitle.c_str());
  drawReliefBookSummary(renderer, stats, progressPercent, hasEstimatedTimeLeft, estimatedTimeLeftSeconds);

  // Dates card. The design's "Last read" is not tracked, so the middle row is the reading span instead.
  char started[24];
  char span[24];
  char finish[24];
  ReliefStatsRow rows[3];
  if (shouldShowRtcBasedStats()) {
    formatReadingStatsShortDate(stats.startDate, started, sizeof(started));
    ReadingStatsDateTime today;
    const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
    const ReadingStatsDate endDate = stats.isCompleted && stats.finishedDate.isValid()
                                         ? stats.finishedDate
                                         : (hasToday ? today.date : ReadingStatsDate{});
    if (stats.startDate.isValid() && endDate.isValid()) {
      const uint16_t days = readingSpanDaysElapsed(stats.startDate, endDate);
      snprintf(span, sizeof(span), "%u %s", static_cast<unsigned>(days), dayCountText(days));
    } else {
      snprintf(span, sizeof(span), "-");
    }
    ReadingStatsDate finishDate;
    if (stats.isCompleted) {
      finishDate = stats.finishedDate;
    } else if (hasToday) {
      uint32_t remaining = 0;
      if (hasEstimatedTimeLeft) {
        remaining = estimatedTimeLeftSeconds;
      } else if (!cachedEstimatedTimeLeft(stats, remaining)) {
        fallbackEstimatedTimeLeft(stats, progressPercent, remaining);
      }
      if (remaining > 0 && !estimateFinishDateFromDailyPace(stats, today, remaining, finishDate)) {
        ReadingStatsDateTime estimated = today;
        addSecondsToReadingStatsDateTime(estimated, remaining);
        finishDate = estimated.date;
      }
    }
    formatReadingStatsShortDate(finishDate, finish, sizeof(finish));
    rows[0] = {tr(STR_STATS_STARTED), started};
    rows[1] = {tr(STR_RELIEF_READING_FOR), span};
    rows[2] = {stats.isCompleted ? tr(STR_RELIEF_FINISHED) : tr(STR_RELIEF_EST_FINISH), finish};
  } else {
    // No clock: no dates, so the card holds the per-session figures instead.
    const uint32_t avg = stats.sessionCount > 0 ? stats.totalReadingSeconds / stats.sessionCount : 0;
    BookReadingStats::formatDuration(avg, started, sizeof(started));
    snprintf(span, sizeof(span), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
    BookReadingStats::formatDuration(stats.totalReadingSeconds, finish, sizeof(finish));
    rows[0] = {tr(STR_STATS_AVG_SESSION_LBL), started};
    rows[1] = {tr(STR_STATS_PAGES_LBL), span};
    rows[2] = {tr(STR_STATS_TIME_LBL), finish};
  }
  drawReliefRowsCard(renderer, kReliefMargin, 520, W - 2 * kReliefMargin, rows, 3, kReliefRowH, -1, false);

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(tr(STR_EXIT), showEditButton ? tr(STR_EDIT) : "", "",
                                               showNextButton ? tr(STR_RELIEF_NEXT) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderReliefDeviceStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                                 const GlobalReadingStats& stats, const bool showButtonHints,
                                 const bool showNextButton) {
  using namespace relief;
  renderer.clearScreen();
  const int W = renderer.getScreenWidth();
  const int x0 = kReliefMargin;
  const int cw = W - 2 * kReliefMargin;
  char buf[48];

  // The design's "This week" total needs a per-day minutes log the stats file does not keep; the
  // subtitle shows the current streak instead.
  ReadingStatsDateTime today;
  const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
  const uint16_t streak = hasToday ? stats.currentReadingStreak(&today.date) : 0;
  if (streak > 0) {
    snprintf(buf, sizeof(buf), tr(STR_STATS_DAY_STREAK_FORMAT), static_cast<unsigned>(streak));
  } else {
    snprintf(buf, sizeof(buf), "%s", tr(STR_STATS_NO_STREAK));
  }
  drawReliefStatsHeader(renderer, screenTitle, buf);

  raised(renderer, x0, 112, cw, 150, 28);
  formatReliefHoursMinutes(stats.totalReadingSeconds, buf, sizeof(buf));
  text(renderer, kClockFontId, x0 + 16, 124, buf);
  text(renderer, SMALL_FONT_ID, x0 + 20, 228, tr(STR_RELIEF_HOURS_IN_TOTAL));
  roundKey(renderer, x0 + cw - 58, 170, 64, &icon_flame_32, true, false);

  // Seven liquid columns. The design's "minutes per day" needs a 7-day log the firmware does not keep;
  // the columns show all-time reading per weekday, scaled to the busiest day. Today's label is bold.
  raised(renderer, x0, 286, cw, 270, 28);
  text(renderer, SMALL_FONT_ID, x0 + 20, 298, tr(STR_RELIEF_BY_WEEKDAY));
  const uint32_t maxDay = *std::max_element(stats.dayOfWeekSeconds.begin(), stats.dayOfWeekSeconds.end());
  const int todayIndex = hasToday ? readingStatsDayOfWeekIndex(today.date) : -1;
  constexpr int kColW = 52;
  const int colStep = (cw - 40 - kColW) / 6;
  for (int i = 0; i < static_cast<int>(DAY_LABELS.size()); ++i) {
    const int cx = x0 + 20 + i * colStep;
    const float level = maxDay > 0 ? static_cast<float>(stats.dayOfWeekSeconds[i]) / static_cast<float>(maxDay) : 0.0f;
    liquidColumn(renderer, cx, 326, kColW, 176, level, i);
    const auto style = i == todayIndex ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    textCentered(renderer, SMALL_FONT_ID, cx, kColW, 510, I18N.get(DAY_LABELS[i]), true, style);
  }

  // Books finished, and the busiest time of day. The design's "best hour" needs hourly totals; the
  // firmware keeps four time-of-day buckets, so the tile names the busiest bucket and draws the four.
  const int tileW = (cw - 24) / 2;
  raised(renderer, x0, 580, tileW, 140, 26);
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.completedBooks));
  text(renderer, kDigitsFontId, x0 + 20, 590, buf);
  text(renderer, SMALL_FONT_ID, x0 + 22, 676, tr(STR_RELIEF_BOOKS_FINISHED));

  const int tx = x0 + tileW + 24;
  raised(renderer, tx, 580, cw - tileW - 24, 140, 26);
  const auto best = std::max_element(stats.timeOfDaySeconds.begin(), stats.timeOfDaySeconds.end());
  const uint32_t maxBucket = *best;
  const int bestIndex = static_cast<int>(best - stats.timeOfDaySeconds.begin());
  const int innerW = cw - tileW - 24 - 40;
  const std::string name = renderer.truncatedText(
      UI_12_FONT_ID, maxBucket > 0 ? I18N.get(TIME_BUCKET_LABELS[bestIndex]) : tr(STR_RELIEF_NO_READING_YET), innerW,
      EpdFontFamily::BOLD);
  text(renderer, UI_12_FONT_ID, tx + 20, 594, name.c_str(), true, EpdFontFamily::BOLD);
  if (maxBucket > 0) {
    text(renderer, SMALL_FONT_ID, tx + 20, 628, tr(STR_RELIEF_BEST_TIME));
    constexpr int kBarW = 22;
    constexpr int kBarStep = 34;
    for (int i = 0; i < static_cast<int>(TIME_BUCKET_LABELS.size()); ++i) {
      const int barH = 6 + static_cast<int>(36.0f * stats.timeOfDaySeconds[i] / maxBucket);
      renderer.fillRoundedRect(tx + 20 + i * kBarStep, 704 - barH, kBarW, barH, kBarW / 2,
                               i == bestIndex ? Color::Black : Color::DarkGray);
    }
  }

  if (showButtonHints && mappedInput) {
    const auto labels =
        mappedInput->mapLabels(tr(STR_EXIT), "", tr(STR_RELIEF_PREV), showNextButton ? tr(STR_RELIEF_NEXT) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

// Without a clock there are no dates or weekday charts: one page with the book summary and the device totals.
void renderReliefNoRtcStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                const std::string& bookTitle, const BookReadingStats& bookStats,
                                const float progressPercent, const bool hasEstimatedTimeLeft,
                                const uint32_t estimatedTimeLeftSeconds, const GlobalReadingStats& deviceStats,
                                const GlobalReadingStats* allDevicesStats, const bool showButtonHints) {
  renderer.clearScreen();
  const int W = renderer.getScreenWidth();
  drawReliefStatsHeader(renderer, tr(STR_READING_STATS), bookTitle.c_str());
  drawReliefBookSummary(renderer, bookStats, progressPercent, hasEstimatedTimeLeft, estimatedTimeLeftSeconds);

  char hours[24];
  char books[16];
  char sessions[16];
  char allHours[24];
  formatReliefHoursMinutes(deviceStats.totalReadingSeconds, hours, sizeof(hours));
  snprintf(books, sizeof(books), "%lu", static_cast<unsigned long>(deviceStats.completedBooks));
  snprintf(sessions, sizeof(sessions), "%lu", static_cast<unsigned long>(deviceStats.totalSessions));
  ReliefStatsRow rows[4] = {{tr(STR_RELIEF_ROW_HOURS_READ), hours},
                            {tr(STR_RELIEF_ROW_BOOKS_FINISHED), books},
                            {tr(STR_STATS_SESSIONS_LBL), sessions},
                            {tr(STR_STATS_ALL_DEVICES_SCREEN), allHours}};
  int count = 3;
  if (allDevicesStats) {
    formatReliefHoursMinutes(allDevicesStats->totalReadingSeconds, allHours, sizeof(allHours));
    count = 4;
  }
  drawReliefRowsCard(renderer, kReliefMargin, 520, W - 2 * kReliefMargin, rows, count, count > 3 ? 44 : kReliefRowH, -1,
                     false);

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(tr(STR_EXIT), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

// Edit dates: the two dates as rows (the one being edited sinks) and a picker card for it. Columns follow
// the edit order (month, day, year) so the focus moves left to right; the design shows day first.
void renderReliefEditDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                               const std::string& bookTitle, const BookReadingStats& stats, const int selectedField,
                               const bool showButtonHints) {
  using namespace relief;
  renderer.clearScreen();
  const int W = renderer.getScreenWidth();
  const int x0 = kReliefMargin;
  const int cw = W - 2 * kReliefMargin;
  drawReliefStatsHeader(renderer, tr(STR_RELIEF_EDIT_DATES), bookTitle.c_str());

  const bool editingFinished = selectedField >= 3;
  const bool hasFinished = stats.isCompleted && stats.finishedDate.isValid();
  char started[24];
  char finished[24];
  formatReadingStatsShortDate(stats.startDate, started, sizeof(started));
  if (hasFinished) {
    formatReadingStatsShortDate(stats.finishedDate, finished, sizeof(finished));
  } else {
    snprintf(finished, sizeof(finished), "%s", tr(STR_RELIEF_NOT_YET));
  }
  const ReliefStatsRow rows[2] = {{tr(STR_STATS_STARTED), started}, {tr(STR_RELIEF_FINISHED), finished}};
  drawReliefRowsCard(renderer, x0, 112, cw, rows, 2, kReliefRowH, editingFinished ? 1 : 0, true);

  lifted(renderer, x0, 300, cw, 330, 28);
  textCentered(renderer, UI_12_FONT_ID, x0, cw, 318,
               editingFinished ? tr(STR_RELIEF_FINISHED_ON) : tr(STR_RELIEF_STARTED_ON), true, EpdFontFamily::BOLD);
  const ReadingStatsDate& date = editingFinished ? stats.finishedDate : stats.startDate;
  const bool showDate = editingFinished ? hasFinished : date.isValid();
  char month[8];
  char day[8];
  char year[8];
  formatReadingStatsMonthToken(showDate ? date : ReadingStatsDate{}, month, sizeof(month));
  snprintf(day, sizeof(day), "%s", "-");
  snprintf(year, sizeof(year), "%s", "-");
  if (showDate) {
    snprintf(day, sizeof(day), "%u", static_cast<unsigned>(date.day));
    snprintf(year, sizeof(year), "%u", static_cast<unsigned>(date.year));
  }
  const char* labels[3] = {tr(STR_RELIEF_MONTH), tr(STR_RELIEF_DAY), tr(STR_RELIEF_YEAR)};
  const char* values[3] = {month, day, year};
  constexpr int kColW = 128;
  const int colStep = (cw - 40 - kColW) / 2;
  const int focusColumn = selectedField % 3;
  for (int i = 0; i < 3; ++i) {
    const int cx = x0 + 20 + i * colStep;
    const bool focused = i == focusColumn;
    const int d = focused ? 1 : 0;
    textCentered(renderer, SMALL_FONT_ID, cx, kColW, 364, labels[i]);
    roundKey(renderer, cx + kColW / 2, 420, 44, &icon_chevron_up_24, false, false);
    surface(renderer, cx, 452, kColW, 64, 22, focused);
    textCentered(renderer, UI_12_FONT_ID, cx + d, kColW, 468 + d, values[i], true, EpdFontFamily::BOLD);
    roundKey(renderer, cx + kColW / 2, 556, 44, &icon_chevron_down_24, false, false);
  }

  if (showButtonHints && mappedInput) {
    sideNubs(renderer, true, true);
    const auto hints = mappedInput->mapLabels(tr(STR_SAVE), tr(STR_NEXT_FIELD), "-", "+");
    GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4, true);
  }
}

}  // namespace

void renderPerBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                            const BookReadingStats& stats, const float progressPercent, const bool hasEstimatedTimeLeft,
                            const uint32_t estimatedTimeLeftSeconds, const bool showButtonHints,
                            const bool showEditButton, const bool showMoreButton) {
  if (reliefStats()) {
    renderReliefBookStatsPage(renderer, mappedInput, bookTitle, stats, progressPercent, hasEstimatedTimeLeft,
                              estimatedTimeLeftSeconds, showButtonHints, showEditButton, showMoreButton);
    return;
  }
  renderer.clearScreen();
  const bool showRtcStats = shouldShowRtcBasedStats();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, mappedInput, false, showButtonHints, showRtcStats);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false, true);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS), true);
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  int topCardH = layout.topCardH;
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  int y = metrics.topPadding + headerHeight + layout.topGap;

  if (showRtcStats) {
    const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
    const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
    const int compactContentHeight =
        headerHeight + layout.topGap + layout.topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    const int extraTopCardHeight = std::min(extraHeight, kPerBookRtcTopCardMaxExtra);
    const int remainingExtraHeight = extraHeight - extraTopCardHeight;
    const int timeOfDayExtraHeight = (remainingExtraHeight * 4) / 11;
    const int dayOfWeekExtraHeight = remainingExtraHeight - timeOfDayExtraHeight;
    const int timeOfDayCardH = timeOfDayH + timeOfDayExtraHeight;
    const int dayOfWeekCardH = dayOfWeekH + dayOfWeekExtraHeight;
    topCardH += extraTopCardHeight;

    drawPerBookStatsCard(renderer, cardX, y, cardW, topCardH, bookTitle, stats, progressPercent, hasEstimatedTimeLeft,
                         estimatedTimeLeftSeconds, layout);
    y += topCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, timeOfDayCardH, tr(STR_STATS_TIME_OF_DAY), layout);
    drawHorizontalBars(renderer, cardX, y, cardW, timeOfDayCardH, stats.timeOfDaySeconds, TIME_BUCKET_LABELS, layout);
    y += timeOfDayCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, dayOfWeekCardH, tr(STR_STATS_DAY_OF_WEEK), layout);
    drawHorizontalBars(renderer, cardX, y, cardW, dayOfWeekCardH, stats.dayOfWeekSeconds, DAY_LABELS, layout);
  } else {
    const int compactContentHeight = headerHeight + layout.topGap + layout.topCardH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    if (showButtonHints) {
      topCardH += extraHeight;
    } else {
      // The sleep-screen variant has no footer controls, so on tall portrait displays the
      // single card can balloon and create huge internal gaps between the two stat rows.
      // Cap the card growth and spend the rest as outer margin instead.
      const int maxStandaloneCardHeight =
          std::max(layout.topCardH, renderer.getScreenHeight() / kStandaloneNoRtcMaxTopCardHeightDivisor);
      topCardH = std::min(layout.topCardH + extraHeight, maxStandaloneCardHeight);
      const int unusedExtraHeight = extraHeight - (topCardH - layout.topCardH);
      y += std::min(unusedExtraHeight / 3, kStandaloneNoRtcMaxVerticalOffset);
    }
    drawPerBookStatsCard(renderer, cardX, y, cardW, topCardH, bookTitle, stats, progressPercent, hasEstimatedTimeLeft,
                         estimatedTimeLeftSeconds, layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels =
        mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), showEditButton ? tr(STR_EDIT) : "", "",
                               showMoreButton ? tr(STR_MORE) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderGlobalStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                           const GlobalReadingStats& stats, const bool showButtonHints, const bool showMoreButton) {
  if (reliefStats()) {
    renderReliefDeviceStatsPage(renderer, mappedInput, screenTitle, stats, showButtonHints, showMoreButton);
    return;
  }
  renderer.clearScreen();
  const bool showRtcStats = shouldShowRtcBasedStats();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, mappedInput, true, showButtonHints, showRtcStats);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, screenTitle, false);
  } else {
    CompactHeader::drawTitle(renderer, screenTitle);
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  int globalCardH = layout.globalCardH;
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  int y = metrics.topPadding + headerHeight + layout.topGap;

  if (showRtcStats) {
    const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
    const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
    const int compactContentHeight =
        headerHeight + layout.topGap + layout.globalCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    const int perBookCompactContentHeight =
        headerHeight + layout.topGap + layout.topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int perBookExtraHeight = std::max(0, availableHeight - perBookCompactContentHeight);
    const int targetGlobalCardH = globalRtcCardHeightForPerBookRowSpacing(layout, perBookExtraHeight);
    const int extraTopCardHeight = std::min(extraHeight, std::max(0, targetGlobalCardH - layout.globalCardH));
    const int remainingExtraHeight = extraHeight - extraTopCardHeight;
    const int timeOfDayExtraHeight = (remainingExtraHeight * 4) / 11;
    const int dayOfWeekExtraHeight = remainingExtraHeight - timeOfDayExtraHeight;
    const int timeOfDayCardH = timeOfDayH + timeOfDayExtraHeight;
    const int dayOfWeekCardH = dayOfWeekH + dayOfWeekExtraHeight;
    globalCardH += extraTopCardHeight;

    drawGlobalStatsCard(renderer, cardX, y, cardW, globalCardH, tr(STR_STATS_ALL_TIME), stats, layout);
    y += globalCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, timeOfDayCardH, tr(STR_STATS_TIME_OF_DAY), layout);
    drawHorizontalBars(renderer, cardX, y, cardW, timeOfDayCardH, stats.timeOfDaySeconds, TIME_BUCKET_LABELS, layout);
    y += timeOfDayCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, dayOfWeekCardH, tr(STR_STATS_DAY_OF_WEEK), layout);
    drawHorizontalBars(renderer, cardX, y, cardW, dayOfWeekCardH, stats.dayOfWeekSeconds, DAY_LABELS, layout);
  } else {
    const int compactContentHeight = headerHeight + layout.topGap + layout.globalCardH;
    globalCardH += std::max(0, availableHeight - compactContentHeight);
    drawGlobalStatsCard(renderer, cardX, y, cardW, globalCardH, tr(STR_STATS_ALL_TIME), stats, layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels =
        mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_EXIT)), "", mappedInput->withBackArrow(tr(STR_BACK)),
                               showMoreButton ? tr(STR_MORE) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderNoRtcCombinedStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const std::string& bookTitle, const BookReadingStats& bookStats,
                                  const float progressPercent, const bool hasEstimatedTimeLeft,
                                  const uint32_t estimatedTimeLeftSeconds, const GlobalReadingStats& deviceStats,
                                  const GlobalReadingStats* allDevicesStats, const bool showButtonHints) {
  if (reliefStats()) {
    renderReliefNoRtcStatsPage(renderer, mappedInput, bookTitle, bookStats, progressPercent, hasEstimatedTimeLeft,
                               estimatedTimeLeftSeconds, deviceStats, allDevicesStats, showButtonHints);
    return;
  }
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getNoRtcCombinedLayout(renderer, mappedInput, showButtonHints, allDevicesStats != nullptr);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS));
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  const int compactContentHeight = noRtcCombinedContentHeight(layout, headerHeight, allDevicesStats != nullptr);
  const int extraHeight = std::max(0, availableHeight - compactContentHeight);
  const int visibleCardCount = allDevicesStats ? 3 : 2;
  const int extraPerCard = visibleCardCount > 0 ? extraHeight / visibleCardCount : 0;
  const int extraRemainder = visibleCardCount > 0 ? extraHeight % visibleCardCount : 0;
  const int perBookExtraHeight = extraPerCard + (extraRemainder > 0 ? 1 : 0);
  const int deviceExtraHeight = extraPerCard + (extraRemainder > 1 ? 1 : 0);
  const int allDevicesExtraHeight = allDevicesStats ? extraPerCard : 0;
  const int perBookCardH = noRtcCardBaseHeight(layout) + perBookExtraHeight;
  const int deviceCardH = layout.globalCardH + deviceExtraHeight;
  const int allDevicesCardH = layout.globalCardH + allDevicesExtraHeight;

  int y = metrics.topPadding + headerHeight + layout.topGap;
  drawPerBookStatsCard(renderer, cardX, y, cardW, perBookCardH, bookTitle, bookStats, progressPercent,
                       hasEstimatedTimeLeft, estimatedTimeLeftSeconds, layout);
  y += perBookCardH + layout.cardGap;

  drawGlobalStatsCard(renderer, cardX, y, cardW, deviceCardH, tr(STR_STATS_THIS_DEVICE_SCREEN), deviceStats, layout);
  y += deviceCardH;

  if (allDevicesStats) {
    y += layout.cardGap;
    drawGlobalStatsCard(renderer, cardX, y, cardW, allDevicesCardH, tr(STR_STATS_ALL_DEVICES_SCREEN), *allDevicesStats,
                        layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderEditBookDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                             const BookReadingStats& stats, const int selectedField, const bool showButtonHints) {
  if (reliefStats()) {
    renderReliefEditDatesPage(renderer, mappedInput, bookTitle, stats, selectedField, showButtonHints);
    return;
  }
  renderer.clearScreen();
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS));
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int cardW = pageWidth - 120;
  const int cardH = 250;
  const int cardX = (pageWidth - cardW) / 2;
  const int cardY = 138;

  const std::string visibleTitle =
      renderer.truncatedText(UI_12_FONT_ID, bookTitle.c_str(), pageWidth - 80, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_12_FONT_ID, 96, visibleTitle.c_str(), true, EpdFontFamily::BOLD);
  renderer.drawRect(cardX, cardY, cardW, cardH);

  const int sectionGap = 104;
  const int row1Y = cardY + 66;
  const int row2Y = row1Y + sectionGap;
  const int monthW = 52;
  const int dayW = 46;
  const int yearW = 68;
  const int gap = 14;
  const int totalFieldW = monthW + gap + dayW + gap + yearW;
#if CROSSINK_APP_CAP_TOUCH
  const bool showTouchControls = mappedInput && mappedInput->hasTouch();
  constexpr int adjustButtonSize = 60;
  constexpr int adjustButtonRightPadding = 34;
  constexpr int adjustButtonGap = 24;
  const int adjustButtonX = cardX + cardW - adjustButtonRightPadding - adjustButtonSize;
  const int fieldAreaW = showTouchControls ? adjustButtonX - cardX - adjustButtonGap : cardW;
#else
  const int fieldAreaW = cardW;
#endif
  const int fieldStartX = cardX + (std::max(totalFieldW, fieldAreaW) - totalFieldW) / 2;

  char monthBuf[8];
  char dayBuf[8];
  char yearBuf[8];

  drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, cardY + 24, tr(STR_STATS_START_DATE), true);
  formatReadingStatsMonthToken(stats.startDate, monthBuf, sizeof(monthBuf));
  snprintf(dayBuf, sizeof(dayBuf), "%s", stats.startDate.isValid() ? "" : "-");
  if (stats.startDate.isValid()) {
    snprintf(dayBuf, sizeof(dayBuf), "%02u", static_cast<unsigned>(stats.startDate.day));
    snprintf(yearBuf, sizeof(yearBuf), "%u", static_cast<unsigned>(stats.startDate.year));
  } else {
    snprintf(dayBuf, sizeof(dayBuf), "-");
    snprintf(yearBuf, sizeof(yearBuf), "-");
  }
  drawDateField(renderer, fieldStartX, row1Y, monthW, monthBuf, selectedField == 0, BookStatsTouchTarget::dateField(0));
  drawDateField(renderer, fieldStartX + monthW + gap, row1Y, dayW, dayBuf, selectedField == 1,
                BookStatsTouchTarget::dateField(1));
  drawDateField(renderer, fieldStartX + monthW + gap + dayW + gap, row1Y, yearW, yearBuf, selectedField == 2,
                BookStatsTouchTarget::dateField(2));

  drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, cardY + 24 + sectionGap, tr(STR_STATS_FINISHED_DATE), true);
  const bool showFinishedFields = stats.isCompleted && stats.finishedDate.isValid();
  formatReadingStatsMonthToken(showFinishedFields ? stats.finishedDate : ReadingStatsDate{}, monthBuf,
                               sizeof(monthBuf));
  if (showFinishedFields) {
    snprintf(dayBuf, sizeof(dayBuf), "%02u", static_cast<unsigned>(stats.finishedDate.day));
    snprintf(yearBuf, sizeof(yearBuf), "%u", static_cast<unsigned>(stats.finishedDate.year));
  } else {
    snprintf(dayBuf, sizeof(dayBuf), "-");
    snprintf(yearBuf, sizeof(yearBuf), "-");
  }
  drawDateField(renderer, fieldStartX, row2Y, monthW, monthBuf, selectedField == 3, BookStatsTouchTarget::dateField(3));
  drawDateField(renderer, fieldStartX + monthW + gap, row2Y, dayW, dayBuf, selectedField == 4,
                BookStatsTouchTarget::dateField(4));
  drawDateField(renderer, fieldStartX + monthW + gap + dayW + gap, row2Y, yearW, yearBuf, selectedField == 5,
                BookStatsTouchTarget::dateField(5));

#if CROSSINK_APP_CAP_TOUCH
  if (showTouchControls) {
    const int fieldH = renderer.getLineHeight(UI_12_FONT_ID) + 10;
    drawDateAdjustButton(renderer, adjustButtonX, row1Y + (fieldH - adjustButtonSize) / 2, adjustButtonSize,
                         icon_chevron_up_32, BookStatsTouchTarget::DateAdjustUp);
    drawDateAdjustButton(renderer, adjustButtonX, row2Y + (fieldH - adjustButtonSize) / 2, adjustButtonSize,
                         icon_chevron_down_32, BookStatsTouchTarget::DateAdjustDown);

    constexpr int actionHeight = TouchActionButtons::kDefaultHeight;
    constexpr int actionGap = TouchActionButtons::kDefaultGap;
    constexpr int actionTotalHeight = actionHeight * 2 + actionGap;
    const Rect safeArea = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
    const Rect actionArea{safeArea.x + metrics.contentSidePadding,
                          safeArea.y + safeArea.height - metrics.verticalSpacing - actionTotalHeight,
                          safeArea.width - metrics.contentSidePadding * 2, actionTotalHeight};
    const auto actions = TouchActionButtons::vertical(actionArea, 2);
    const char* labels[] = {tr(STR_SAVE), tr(STR_CANCEL)};
    TouchActionButtons::draw(renderer, actions, labels, 0, -1, UI_10_FONT_ID);
    TouchRegistry::getInstance().add(actions.buttons[0], BookStatsTouchTarget::DateSave, TouchRegistry::Item);
    TouchRegistry::getInstance().add(actions.buttons[1], BookStatsTouchTarget::DateCancel, TouchRegistry::Item);
  }
#endif

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), tr(STR_NEXT_FIELD),
                                               tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}
