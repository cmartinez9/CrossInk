#pragma once

#include "components/themes/lyra/LyraCarouselTheme.h"

// The "Relief" soft-UI theme. Inherits Lyra Carousel so the carousel
// cache machinery and every screen it does not restyle keep working.
namespace ReliefMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraCarouselMetrics::values;
  v.homeRecentBooksCount = 4;  // Now reading + three Up next
  v.homeCoverHeight = 180;
  v.listRowHeight = 44;
  v.listWithSubtitleRowHeight = 66;
  v.listRowGap = 8;
  v.listRowRadius = 18;
  v.listInset = 20;
  v.listSidePadding = 14;
  v.headerUnderlineSize = 0;
  v.buttonHintsHeight = 58;
  v.popupCornerRadius = 22;
  return v;
}
constexpr ThemeMetrics values = makeValues();
}  // namespace ReliefMetrics

class ReliefTheme : public LyraCarouselTheme {
 public:
  static const freeink::Icon* icon(UIIcon which, uint32_t size) { return iconForName(which, size); }
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3, const char* btn4,
                       bool allowInvertedText = false) const override;
  void drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const override;
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool readerContext = false, bool showStatus = true) const override;
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const override;
  // Quick actions become a sheet of round keys; plain option lists become a lifted card.
  void drawOptionPopup(const GfxRenderer& renderer, const char* title, const std::vector<std::string>& options,
                       int selectedIndex, bool showConfirmationFooter = false, const char* cancelLabel = nullptr,
                       const char* saveLabel = nullptr, bool saveFocused = false, int primaryOptionIndex = -1,
                       const char* noteLabel = nullptr, const char* noteBody = nullptr,
                       const std::vector<bool>& disabledOptions = {}, int firstOptionIndex = -1) const override;
  Rect drawPopup(const GfxRenderer& renderer, const char* message, bool preserveBackdrop = false) const override;
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, int progress) const override;
  void drawTextField(const GfxRenderer& renderer, Rect rect, int textWidth, bool cursorMode = false,
                     int contentStartX = 0, int contentWidth = 0) const override;
  void drawReaderProgress(const GfxRenderer& renderer, int x, int y, int width, int height, float percent,
                          bool foregroundBlack) const override;
};
