#include "ReliefTheme.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "CrossPointSettings.h"
#include "I18n.h"
#include "QuickActions.h"
#include "ReliefKit.h"
#include "components/UiAppHelpers.h"
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"
#include "fontIds.h"

void ReliefTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                  const char* btn4, bool /*allowInvertedText*/) const {
  if (!btn1 && !btn2 && !btn3 && !btn4) return;
  relief::rockerHints(renderer, btn1 ? btn1 : "", btn2 ? btn2 : "", btn3 ? btn3 : "", btn4 ? btn4 : "", holdFeedback());
}

void ReliefTheme::drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const {
  relief::sideNubs(renderer, topBtn && *topBtn, bottomBtn && *bottomBtn);
}

void ReliefTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle,
                             bool /*readerContext*/, bool showStatus) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  const int pad = 24;
  int right = rect.x + rect.width - pad;
  if (showStatus) {
    // Battery as a small liquid tube in a groove.
    const uint16_t pct = powerManager.getBatteryPercentage();
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", static_cast<unsigned>(pct));
    const int tw = relief::textWidth(renderer, SMALL_FONT_ID, buf);
    const int ty = rect.y + 20;
    relief::text(renderer, SMALL_FONT_ID, right - tw, ty, buf);
    relief::tube(renderer, right - tw - 8 - 38, ty + 5, 38, 10, pct / 100.0f);
    right -= tw + 8 + 38 + 12;
  }
  if (title && *title) {
    const int font = relief::titleFontFor(title);
    const auto style = font == relief::kTitleFontId ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
    const int lh = renderer.getLineHeight(font);
    const std::string t = renderer.truncatedText(font, title, right - rect.x - pad, style);
    const int ty = subtitle && *subtitle ? rect.y + 10 : rect.y + (rect.height - lh) / 2 + 4;
    relief::text(renderer, font, rect.x + pad, ty, t.c_str(), true, style);
    if (subtitle && *subtitle) {
      relief::text(renderer, SMALL_FONT_ID, rect.x + pad + 2, ty + lh + 2, subtitle);
    }
  }
}

void ReliefTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                                const char* rightLabel) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int y = rect.y + (rect.height - lh) / 2;
  if (label) relief::text(renderer, UI_10_FONT_ID, rect.x + 26, y, label, true, EpdFontFamily::BOLD);
  if (rightLabel) relief::textRight(renderer, UI_10_FONT_ID, rect.x + rect.width - 26, y, rightLabel);
}

namespace {
// Glyphs with no Lucide asset in the firmware are drawn: Dark (half-filled disc) and Sleep (crescent).
void drawQuickGlyph(const GfxRenderer& r, uint8_t action, int cx, int cy, bool black) {
  using S = CrossPointSettings::SHORT_PWRBTN;
  const freeink::Icon* icon = nullptr;
  switch (action) {
    case S::TOGGLE_BOOKMARK:
      icon = &icon_bookmark_32;
      break;
    case S::FORCE_REFRESH:
      icon = &icon_history_32;
      break;
    case S::FILE_TRANSFER:
      icon = &icon_lyra_transfer_32;
      break;
    case S::TOGGLE_FONT:
      icon = &icon_case_sensitive_32;
      break;
    case S::FILE_BROWSER:
      icon = &icon_folder_32;
      break;
    case S::LIBRARY:
      icon = &icon_landmark_32;
      break;
    case S::READING_STATS:
      icon = &icon_reading_stats_32;
      break;
    case S::TOGGLE_DARK_MODE:
      r.drawRoundedRect(cx - 13, cy - 13, 26, 26, 3, 13, black);
      r.fillRoundedRect(cx, cy - 13, 13, 26, 13, false, true, false, true, black ? Color::Black : Color::White);
      return;
    case S::SLEEP:
    case S::SLEEP_ONLY:
      r.fillRoundedRect(cx - 13, cy - 13, 26, 26, 13, black ? Color::Black : Color::White);
      r.fillRoundedRect(cx - 5, cy - 18, 24, 24, 12, black ? Color::White : Color::Black);
      return;
    default:
      break;
  }
  if (icon != nullptr) {
    drawLucideIcon(r, *icon, cx - icon->w / 2, cy - icon->h / 2, black);
  } else {
    r.fillRoundedRect(cx - 5, cy - 5, 10, 10, 5, black ? Color::Black : Color::White);
  }
}

uint8_t quickActionForLabel(const std::string& label) {
  for (uint8_t a = 1; a < CrossPointSettings::QUICK_ACTION_SLOT_ACTION_COUNT; ++a) {
    const char* l = I18N.get(QuickActions::actionLabel(a));
    if (l != nullptr && label == l) return a;
  }
  return 0;
}
}  // namespace

void ReliefTheme::drawOptionPopup(const GfxRenderer& renderer, const char* title,
                                  const std::vector<std::string>& options, int selectedIndex,
                                  bool showConfirmationFooter, const char* cancelLabel, const char* saveLabel,
                                  bool saveFocused, int primaryOptionIndex, const char* noteLabel, const char* noteBody,
                                  const std::vector<bool>& disabledOptions, int firstOptionIndex) const {
  using namespace relief;
  if (noteBody && *noteBody) {
    // Long-form notes keep the Lyra layout, which wraps and paginates the note body.
    LyraCarouselTheme::drawOptionPopup(renderer, title, options, selectedIndex, showConfirmationFooter, cancelLabel,
                                       saveLabel, saveFocused, primaryOptionIndex, noteLabel, noteBody, disabledOptions,
                                       firstOptionIndex);
    return;
  }
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int rail = 50;  // the rocker hints clear and redraw everything below this line
  const bool quick = title != nullptr && std::string(title) == tr(STR_QUICK_ACTIONS);
  if (quick && !showConfirmationFooter) {
    // A paper sheet risen over the current screen, holding one round key per action.
    const int y0 = 300;
    sheetEdge(renderer, y0, H - rail);
    text(renderer, kTitleFontId, 24, y0 + 26, tr(STR_QUICK_ACTIONS));
    const int n = static_cast<int>(options.size());
    for (int i = 0; i < n && i < 8; ++i) {
      const int cx = 72 + (i % 4) * 128;
      const int cy = y0 + 136 + (i / 4) * 150;
      constexpr int d = 80;
      const uint8_t action = quickActionForLabel(options[i]);
      const bool on = action == CrossPointSettings::SHORT_PWRBTN::TOGGLE_DARK_MODE && SETTINGS.screenInverted;
      const bool f = i == selectedIndex;
      const int x = cx - d / 2, y = cy - d / 2;
      if (f && this->pressEcho()) {
        // Press echo: the key sinks deeper (SHADE) for one frame before the action runs.
        pressed(renderer, x, y, d, d, d / 2);
        fillShade(renderer, x + 8, y + 8, d - 16, d - 16, (d - 16) / 2);
        drawQuickGlyph(renderer, action, cx + 1, cy + 1, false);
      } else if (f) {
        pressed(renderer, x, y, d, d, d / 2);
        if (on) renderer.fillRoundedRect(x + 10, y + 10, d - 20, d - 20, (d - 20) / 2, Color::Black);
        drawQuickGlyph(renderer, action, cx + 1, cy + 1, !on);
      } else if (on) {
        renderer.fillRoundedRect(x + 6, y + 6, d, d, d / 2, Color::LightGray);
        renderer.fillRoundedRect(x + 3, y + 3, d, d, d / 2, Color::DarkGray);
        renderer.fillRoundedRect(x, y, d, d, d / 2, Color::Black);
        drawQuickGlyph(renderer, action, cx, cy, false);
      } else {
        raised(renderer, x, y, d, d, d / 2);
        drawQuickGlyph(renderer, action, cx, cy, true);
      }
      const auto style = f ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
      const std::string label = renderer.truncatedText(SMALL_FONT_ID, options[i].c_str(), 120, style);
      textCentered(renderer, SMALL_FONT_ID, cx - 62, 124, cy + d / 2 + 12, label.c_str(), true, style);
    }
    return;
  }
  // Option list (and confirmation dialog): a lifted paper card; the focused option sinks; the current
  // value gets an ink dot. Long lists scroll inside the card with a thin position track.
  constexpr int kStep = 50, kRowH = 42, kMaxRows = 9, kFooterH = 72;
  const int count = static_cast<int>(options.size());
  const int maxRows = showConfirmationFooter ? kMaxRows - 2 : kMaxRows;
  const int visible = std::min(maxRows, count);
  int first = std::max(0, firstOptionIndex);
  if (selectedIndex >= 0 && !(showConfirmationFooter && saveFocused)) {
    if (selectedIndex < first) first = selectedIndex;
    if (selectedIndex >= first + visible) first = selectedIndex - visible + 1;
  }
  first = std::clamp(first, 0, std::max(0, count - visible));
  const int cw = W - 96;
  const int ch = 66 + visible * kStep + 8 + (showConfirmationFooter ? kFooterH : 0);
  const int cx = 48, cy = std::max(56, (H - rail - ch) / 2);
  lifted(renderer, cx, cy, cw, ch, 28);
  if (title && *title) {
    const std::string t = renderer.truncatedText(UI_12_FONT_ID, title, cw - 40, EpdFontFamily::BOLD);
    textCentered(renderer, UI_12_FONT_ID, cx, cw, cy + 18, t.c_str(), true, EpdFontFamily::BOLD);
  }
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  for (int i = 0; i < visible; ++i) {
    const int idx = first + i;
    const int ry = cy + 62 + i * kStep;
    const bool f = idx == selectedIndex && !(showConfirmationFooter && saveFocused);
    const bool disabled = idx < static_cast<int>(disabledOptions.size()) && disabledOptions[idx];
    const int d = f ? 1 : 0;
    if (f) pressed(renderer, cx + 12, ry, cw - 24, kRowH, kRowH / 2);
    const auto style = f ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, options[idx].c_str(), cw - 90, style);
    text(renderer, UI_10_FONT_ID, cx + 32 + d, ry + (kRowH - lh) / 2 + d, label.c_str(), true, style);
    if (disabled) {
      // Unavailable: a dashed strike instead of grey text (thin grey text breaks up on the panel).
      for (int dx = cx + 32; dx < cx + 32 + textWidth(renderer, UI_10_FONT_ID, label.c_str()); dx += 6) {
        renderer.fillRect(dx, ry + kRowH / 2, 3, 1, true);
      }
    }
    if (idx == primaryOptionIndex) {
      renderer.fillRoundedRect(cx + cw - 44 + d, ry + kRowH / 2 - 5 + d, 10, 10, 5, Color::Black);
    }
  }
  if (count > visible) {
    const int trackY = cy + 62, trackH = visible * kStep - 8;
    renderer.fillRectDither(cx + cw - 16, trackY, 4, trackH, Color::LightGray);
    const int thumbH = std::max(16, trackH * visible / count);
    const int thumbY = trackY + (trackH - thumbH) * first / std::max(1, count - visible);
    renderer.fillRoundedRect(cx + cw - 16, thumbY, 4, thumbH, 2, Color::Black);
  }
  if (showConfirmationFooter) {
    const int fy = cy + ch - kFooterH + 6;
    const int kw = (cw - 72) / 2;
    key(renderer, cx + 24, fy, kw, 44, cancelLabel ? cancelLabel : tr(STR_CANCEL), false, false);
    key(renderer, cx + 48 + kw, fy, kw, 44, saveLabel ? saveLabel : tr(STR_SAVE), saveFocused, true);
  }
}

Rect ReliefTheme::drawPopup(const GfxRenderer& renderer, const char* message, const bool preserveBackdrop) const {
  using namespace relief;
  // A lifted card: the message, plus a band below it where fillPopupProgress pours a liquid bar.
  const int W = renderer.getScreenWidth();
  const int textW = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::BOLD);
  const int lh = renderer.getLineHeight(UI_12_FONT_ID);
  const int w = std::min(W - 64, std::max(260, textW + 64));
  const int h = lh + 70;
  const int x = (W - w) / 2;
  const int y = static_cast<int>(renderer.getScreenHeight() * 0.38f);
  constexpr int kShadow = 12;
  std::unique_ptr<uint8_t[]> backdrop;
  if (preserveBackdrop) {
    const size_t bytes = renderer.getRegionByteSize(x - 2, y - 2, w + kShadow, h + kShadow);
    if (bytes != 0) backdrop = makeUniqueNoThrow<uint8_t[]>(bytes);
    if (!backdrop || !renderer.copyRegionToBuffer(x - 2, y - 2, w + kShadow, h + kShadow, backdrop.get(), bytes)) {
      LOG_ERR("GUI", "Unable to preserve loading popup backdrop");
      return Rect{x, y, w, h};
    }
  }
  lifted(renderer, x, y, w, h, 26);
  const std::string t = renderer.truncatedText(UI_12_FONT_ID, message, w - 40, EpdFontFamily::BOLD);
  textCentered(renderer, UI_12_FONT_ID, x, w, y + 16, t.c_str(), true, EpdFontFamily::BOLD);
  renderer.displayBuffer();
  if (backdrop) {
    renderer.copyBufferToRegion(x - 2, y - 2, w + kShadow, h + kShadow, backdrop.get(),
                                renderer.getRegionByteSize(x - 2, y - 2, w + kShadow, h + kShadow));
  }
  return Rect{x, y, w, h};
}

void ReliefTheme::fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const {
  // The liquid loader: the bar fills and its wave end advances with every step.
  const int bx = layout.x + 28, bw = layout.width - 56, by = layout.y + layout.height - 32;
  relief::liquidBar(renderer, bx, by, bw, 16, std::clamp(progress, 0, 100) / 100.0f, progress / 10);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void ReliefTheme::drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth, bool cursorMode,
                                int contentStartX, int contentWidth) const {
  // A pressed well around the entry text (drawn as an edge only, so the text already on screen stays).
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int w = contentWidth > 0 ? contentWidth + 32 : textWidth + 48;
  const int x = contentWidth > 0 ? rect.x + contentStartX - 16 : rect.x + (rect.width - w) / 2;
  const int h = std::max(rect.height, lineHeight) + 24;
  relief::pressedFrame(renderer, x, rect.y - 12, w, h, std::min(24, h / 2));
  if (cursorMode) renderer.fillRect(x + 12, rect.y + h - 20, w - 24, 2, true);
}

void ReliefTheme::drawReaderProgress(const GfxRenderer& renderer, const int x, const int y, const int width,
                                     const int height, const float percent, const bool foregroundBlack) const {
  if (!foregroundBlack || height < 2) {
    BaseTheme::drawReaderProgress(renderer, x, y, width, height, percent, foregroundBlack);
    return;
  }
  // A groove whose ink fill is the progress; it changes only where the fill crosses a pixel column.
  const int h = std::max(8, height + 4);
  const int gx = x + 12, gw = std::max(0, width - 24);
  relief::tube(renderer, gx, y + height - h, gw, h, std::clamp(percent, 0.0f, 100.0f) / 100.0f);
}
