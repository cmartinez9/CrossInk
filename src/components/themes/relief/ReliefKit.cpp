#include "ReliefKit.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <Icon.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace relief {
namespace {
struct ShadowShape {
  int16_t x, y, w, h, radius;
  uint8_t mist, tone, ink;  // band offsets; ink 0 = no ink edge (ink keys)
};
constexpr int kMaxShadowShapes = 32;
ShadowShape gShadowShapes[kMaxShadowShapes];
int gShadowCount = -1;  // -1: not recording

// Returns true when the shape was logged. A logged shape draws its bands solid black: the grey pass
// lightens marked black pixels (as it does for anti-aliased text), so solid bands become even greys.
bool recordShadow(int x, int y, int w, int h, int radius, uint8_t mist, uint8_t tone, uint8_t ink) {
  if (gShadowCount < 0 || gShadowCount >= kMaxShadowShapes) return false;
  gShadowShapes[gShadowCount++] = ShadowShape{static_cast<int16_t>(x),
                                              static_cast<int16_t>(y),
                                              static_cast<int16_t>(w),
                                              static_cast<int16_t>(h),
                                              static_cast<int16_t>(radius),
                                              mist,
                                              tone,
                                              ink};
  return true;
}

}  // namespace

void raised(const GfxRenderer& r, int x, int y, int w, int h, int radius, bool lifted) {
  const int m = lifted ? 10 : 7, t = lifted ? 6 : 4, k = lifted ? 2 : 1;
  const int rad = clampRadius(radius, w, h);
  const bool solid = recordShadow(x, y, w, h, rad, m, t, k);
  r.fillRoundedRect(x + m, y + m, w, h, rad, solid ? Color::Black : Color::LightGray);
  r.fillRoundedRect(x + t, y + t, w, h, rad, solid ? Color::Black : Color::DarkGray);
  r.fillRoundedRect(x + k, y + k, w, h, rad, Color::Black);
  r.fillRoundedRect(x, y, w, h, rad, Color::Black);
  r.fillRoundedRect(x + 1, y + 1, w - 2, h - 2, std::max(0, rad - 1), Color::White);
}

void pressed(const GfxRenderer& r, int x, int y, int w, int h, int radius) {
  // Inset shadow S \ (S + k) is approximated by stacking (S + k) ∩ S, which is the rounded rect
  // (x + k, y + k, w - k, h - k): the bands survive only along the top-left inside edge.
  const int rad = clampRadius(radius, w, h);
  r.fillRoundedRect(x, y, w, h, rad, Color::Black);
  r.fillRoundedRect(x + 3, y + 3, w - 3, h - 3, clampRadius(rad, w - 3, h - 3), Color::DarkGray);
  r.fillRoundedRect(x + 5, y + 5, w - 5, h - 5, clampRadius(rad, w - 5, h - 5), Color::LightGray);
  r.fillRoundedRect(x + 8, y + 8, w - 8, h - 8, clampRadius(rad, w - 8, h - 8), Color::White);
  r.drawRoundedRect(x, y, w, h, 1, rad, true);
}

void groove(const GfxRenderer& r, int x, int y, int w, int h) {
  const int rad = clampRadius(h / 2, w, h);
  r.fillRoundedRect(x, y, w, h, rad, Color::Black);
  r.fillRoundedRect(x + 2, y + 2, w - 2, h - 2, clampRadius(rad, w - 2, h - 2), Color::LightGray);
  r.fillRoundedRect(x + 4, y + 4, w - 4, h - 4, clampRadius(rad, w - 4, h - 4), Color::White);
  r.drawRoundedRect(x, y, w, h, 1, rad, true);
}

void tube(const GfxRenderer& r, int x, int y, int w, int h, float level, bool knob) {
  groove(r, x, y, w, h);
  const float lv = std::clamp(level, 0.0f, 1.0f);
  const int fill = std::max(h, static_cast<int>(w * lv + 0.5f));
  r.fillRoundedRect(x, y, fill, h, h / 2, Color::Black);
  if (knob) {
    const int k = h + 10;
    raised(r, x + fill - k / 2 - 2, y - 5, k, k, k / 2);
  }
}

void liquidColumn(const GfxRenderer& r, int x, int y, int w, int h, float level, int phase) {
  // A groove whose ink rises from the bottom with a sine crest (amplitude 5 px, wavelength 46 px).
  const int rad = w / 2;
  r.fillRoundedRect(x, y, w, h, rad, Color::Black);
  r.fillRoundedRect(x + 2, y + 2, w - 2, h - 2, clampRadius(rad, w - 2, h - 2), Color::LightGray);
  r.fillRoundedRect(x + 4, y + 4, w - 4, h - 4, clampRadius(rad, w - 4, h - 4), Color::White);
  for (int i = 1; i < w - 1; ++i) {
    const int top = std::max(y, liquidSurfaceY(y, h, level, i, phase));
    if (top < y + h) r.fillRect(x + i, top, 1, y + h - top, true);
  }
  r.maskRoundedRectOutsideCorners(x, y, w, h, rad, Color::White);
  r.drawRoundedRect(x, y, w, h, 1, rad, true);
}

void liquidPill(const GfxRenderer& r, int x, int y, int w, int h, float level, const char* title, const char* big,
                const char* small, bool focused, int phase) {
  surface(r, x, y, w, h, w / 2, focused);
  textCentered(r, SMALL_FONT_ID, x, w, y + 12, title, true, EpdFontFamily::REGULAR);
  const int inset = 10;
  const int wx = x + inset, wy = y + inset + 26, ww = w - 2 * inset, wh = h - 2 * inset - 26;
  liquidColumn(r, wx, wy, ww, wh, level, phase);
  const float lv = std::clamp(level, 0.0f, 1.0f);
  const int surf = wy + static_cast<int>(wh * (1.0f - lv));
  // Big number in the title face; "100%" falls back to Inter 12 Bold so it stays inside the well.
  const bool fits = textWidth(r, kTitleFontId, big) <= ww - 10;
  const int bigFont = fits ? kTitleFontId : UI_12_FONT_ID;
  const auto bigStyle = fits ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  const int bigH = r.getLineHeight(bigFont);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  if (lv < 0.62f) {
    // Number floats above the liquid; a low level leaves no room for a label inside the ink.
    const int by = std::min(wy + wh / 3, surf - bigH - 8);
    textCentered(r, bigFont, wx, ww, std::max(wy + 8, by), big, true, bigStyle);
    if (lv < 0.22f)
      textCentered(r, SMALL_FONT_ID, wx, ww, std::max(wy + 8, by) + bigH + 2, small, true);
    else
      textCentered(r, SMALL_FONT_ID, wx, ww, wy + wh - smallH - 12, small, false);
  } else {
    textCentered(r, bigFont, wx, ww, surf + 12, big, false, bigStyle);
    textCentered(r, SMALL_FONT_ID, wx, ww, wy + wh - smallH - 12, small, false);
  }
}

void roundKey(const GfxRenderer& r, int cx, int cy, int d, const freeink::Icon* icon, bool ink, bool focused,
              char fallbackLetter) {
  const int x = cx - d / 2, y = cy - d / 2;
  const auto glyph = [&](bool black) {
    if (icon != nullptr) {
      drawLucideIcon(r, *icon, cx - static_cast<int>(icon->w) / 2, cy - static_cast<int>(icon->h) / 2, black);
    } else {
      const char s[2] = {fallbackLetter, 0};
      const int lh = r.getLineHeight(UI_12_FONT_ID);
      textCentered(r, UI_12_FONT_ID, x, d, cy - lh / 2, s, black, EpdFontFamily::BOLD);
    }
  };
  if (focused) {
    pressed(r, x, y, d, d, d / 2);
    if (ink) {
      const int dd = d - 18;
      r.fillRoundedRect(x + 9, y + 9, dd, dd, dd / 2, Color::Black);
      glyph(false);
    } else {
      glyph(true);
    }
    return;
  }
  if (ink) {
    const bool solid = recordShadow(x, y, d, d, d / 2, 6, 3, 0);
    r.fillRoundedRect(x + 6, y + 6, d, d, d / 2, solid ? Color::Black : Color::LightGray);
    r.fillRoundedRect(x + 3, y + 3, d, d, d / 2, solid ? Color::Black : Color::DarkGray);
    r.fillRoundedRect(x, y, d, d, d / 2, Color::Black);
    glyph(false);
  } else {
    raised(r, x, y, d, d, d / 2);
    glyph(true);
  }
}

int textWidth(const GfxRenderer& r, int fontId, const char* text, EpdFontFamily::Style style) {
  return text ? r.getTextWidth(fontId, text, style) : 0;
}

void text(const GfxRenderer& r, int fontId, int x, int y, const char* text, bool black, EpdFontFamily::Style style) {
  if (text && *text) r.drawText(fontId, x, y, text, black, style);
}

void textCentered(const GfxRenderer& r, int fontId, int x, int w, int y, const char* text, bool black,
                  EpdFontFamily::Style style) {
  if (!text || !*text) return;
  const int tw = r.getTextWidth(fontId, text, style);
  r.drawText(fontId, x + (w - tw) / 2, y, text, black, style);
}

void textRight(const GfxRenderer& r, int fontId, int right, int y, const char* text, bool black,
               EpdFontFamily::Style style) {
  if (!text || !*text) return;
  r.drawText(fontId, right - r.getTextWidth(fontId, text, style), y, text, black, style);
}

void greyText(const GfxRenderer& r, int fontId, int x, int y, const char* text, EpdFontFamily::Style style) {
  if (!text || !*text) return;
  r.drawText(fontId, x, y, text, true, style);
  // Knock out every other pixel of the text box: black glyph pixels become the DarkGray checker,
  // and the paper around them stays white. Only valid on a white background.
  const int tw = r.getTextWidth(fontId, text, style);
  const int th = r.getLineHeight(fontId);
  for (int yy = y; yy < y + th; ++yy) {
    for (int xx = x + ((x + yy) & 1); xx < x + tw + 2; xx += 2) r.drawPixel(xx, yy, false);
  }
}

int titleFontFor(const char* text) {
  if (text == nullptr) return kTitleFontId;
  for (const char* p = text; *p; ++p) {
    if (static_cast<unsigned char>(*p) >= 0x80) return UI_12_FONT_ID;
  }
  return kTitleFontId;
}

void rockerHints(const GfxRenderer& r, const char* back, const char* confirm, const char* left, const char* right,
                 const bool confirmHeld) {
  const int H = r.getScreenHeight();
  const int W = r.getScreenWidth();
  constexpr int kH = 36, kW = 212;
  const int y = H - kH - 10;
  r.fillRect(0, y - 4, W, H - y + 4, false);
  const int lx = W / 2 - kW - 18;
  const int rx = W / 2 + 18;
  const int lh = r.getLineHeight(UI_10_FONT_ID);
  const auto fit = [&](const char* s) {
    return r.truncatedText(UI_10_FONT_ID, s ? s : "", kW / 2 - 10, EpdFontFamily::BOLD);
  };
  const auto rocker = [&](int x, const char* a0, const char* b0, bool primary) {
    // A rocker with no actions is left off rather than drawn as an empty key.
    if ((!a0 || !*a0) && (!b0 || !*b0)) return;
    const std::string as = fit(a0), bs = fit(b0);
    const char* a = as.c_str();
    const char* b = bs.c_str();
    raised(r, x, y, kW, kH, kH / 2);
    if (primary && b && *b) {
      if (confirmHeld) {
        fillShade(r, x + kW / 2, y, kW / 2, kH, kH / 2);
      } else {
        r.fillRoundedRect(x + kW / 2, y, kW / 2, kH, kH / 2, false, true, false, true, Color::Black);
      }
    }
    r.fillRect(x + kW / 2, y + 8, 1, kH - 16, true);
    textCentered(r, UI_10_FONT_ID, x, kW / 2, y + (kH - lh) / 2, a, true, EpdFontFamily::BOLD);
    textCentered(r, UI_10_FONT_ID, x + kW / 2, kW / 2, y + (kH - lh) / 2, b, !(primary && b && *b),
                 EpdFontFamily::BOLD);
  };
  rocker(lx, back, confirm, true);
  rocker(rx, left, right, false);
}

void sideNubs(const GfxRenderer& r, bool top, bool bottom) {
  constexpr int kY = 170, kH = 50;
  const int W = r.getScreenWidth();
  if (top) {
    raised(r, -12, kY, 20, kH, 10);
    r.drawLine(3, kY + kH / 2 + 3, 5, kY + kH / 2 - 3, 2, true);
  }
  if (bottom) {
    raised(r, W - 8, kY, 20, kH, 10);
    r.drawLine(W - 4, kY + kH / 2 - 3, W - 2, kY + kH / 2 + 3, 2, true);
  }
}

void drawCoverBmp(const GfxRenderer& r, const char* path, int x, int y, int w, int h, int radius) {
  FsFile file;
  if (path && *path && Storage.openFileForRead("RELIEF", path, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      r.drawBitmap(bitmap, x, y, w, h);
      r.maskRoundedRectOutsideCorners(x, y, w, h, radius, Color::White);
      file.close();
      r.drawRoundedRect(x, y, w, h, 1, radius, true);
      return;
    }
    file.close();
  }
  // No cover: a quiet ink placeholder.
  r.fillRoundedRect(x, y, w, h, radius, Color::DarkGray);
  r.drawRoundedRect(x, y, w, h, 1, radius, true);
}

void pressedFrame(const GfxRenderer& r, int x, int y, int w, int h, int radius) {
  const int rad = clampRadius(radius, w, h);
  r.drawRoundedRect(x, y, w, h, 1, rad, true);
  const int span = std::max(0, w - 2 * rad);
  const int vspan = std::max(0, h - 2 * rad);
  r.fillRect(x + rad, y + 1, span, 2, true);
  r.fillRectDither(x + rad, y + 3, span, 2, Color::DarkGray);
  r.fillRectDither(x + rad, y + 5, span, 3, Color::LightGray);
  r.fillRect(x + 1, y + rad, 2, vspan, true);
  r.fillRectDither(x + 3, y + rad, 2, vspan, Color::DarkGray);
  r.fillRectDither(x + 5, y + rad, 3, vspan, Color::LightGray);
}

void liquidBar(const GfxRenderer& r, int x, int y, int w, int h, float level, int phase) {
  groove(r, x, y, w, h);
  const float lv = std::clamp(level, 0.0f, 1.0f);
  const int fill = static_cast<int>(w * lv);
  if (fill <= 0) return;
  constexpr float kTwoPi = 6.2831853f;
  for (int j = 1; j < h - 1; ++j) {
    const int end = std::min(w, static_cast<int>(fill + 4 * std::sin(kTwoPi * j / 22.0f + phase * 0.9f)));
    if (end > 0) r.fillRect(x, y + j, end, 1, true);
  }
  r.maskRoundedRectOutsideCorners(x, y, w, h, h / 2, Color::White);
  r.drawRoundedRect(x, y, w, h, 1, h / 2, true);
}

void fillShade(const GfxRenderer& r, int x, int y, int w, int h, int radius) {
  const int rad = clampRadius(radius, w, h);
  r.fillRoundedRect(x, y, w, h, rad, Color::Black);
  // Knock out the LightGray lattice inside the shape (corners excluded by a distance test).
  for (int yy = y + (y & 1); yy < y + h; yy += 2) {
    for (int xx = x + (x & 1); xx < x + w; xx += 2) {
      const int dx = xx < x + rad ? x + rad - xx : (xx >= x + w - rad ? xx - (x + w - rad - 1) : 0);
      const int dy = yy < y + rad ? y + rad - yy : (yy >= y + h - rad ? yy - (y + h - rad - 1) : 0);
      if (dx * dx + dy * dy <= rad * rad) r.drawPixel(xx, yy, false);
    }
  }
}

void toggle(const GfxRenderer& r, int x, int y, bool on) {
  constexpr int kW = 52, kH = 28;
  if (on) {
    r.fillRoundedRect(x, y, kW, kH, kH / 2, Color::Black);
    r.fillRoundedRect(x + kW - kH + 3, y + 3, kH - 6, kH - 6, (kH - 6) / 2, Color::White);
  } else {
    groove(r, x, y, kW, kH);
    raised(r, x + 3, y + 3, kH - 6, kH - 6, (kH - 6) / 2);
  }
}

void key(const GfxRenderer& r, int x, int y, int w, int h, const char* label, bool focused, bool primary) {
  const int lh = r.getLineHeight(UI_10_FONT_ID);
  if (focused) {
    pressed(r, x, y, w, h, h / 2);
    if (primary) r.fillRoundedRect(x + 6, y + 6, w - 12, h - 12, (h - 12) / 2, Color::Black);
    textCentered(r, UI_10_FONT_ID, x + 1, w, y + (h - lh) / 2 + 1, label, !primary, EpdFontFamily::BOLD);
    return;
  }
  if (primary) {
    r.fillRoundedRect(x + 6, y + 6, w, h, h / 2, Color::LightGray);
    r.fillRoundedRect(x + 3, y + 3, w, h, h / 2, Color::DarkGray);
    r.fillRoundedRect(x, y, w, h, h / 2, Color::Black);
  } else {
    raised(r, x, y, w, h, h / 2);
  }
  textCentered(r, UI_10_FONT_ID, x, w, y + (h - lh) / 2, label, !primary, EpdFontFamily::BOLD);
}

void sheetEdge(const GfxRenderer& r, int y, int bottom) {
  const int W = r.getScreenWidth();
  r.fillRoundedRect(0, y - 2, W, 60, 30, true, true, false, false, Color::Black);
  r.fillRoundedRect(0, y, W, 60, 30, true, true, false, false, Color::White);
  r.fillRect(0, y + 30, W, std::max(0, bottom - y - 30), false);
  groove(r, W / 2 - 26, y + 10, 52, 8);
}

void beginShadowRecording() { gShadowCount = 0; }
void endShadowRecording() { gShadowCount = -1; }

void drawRecordedShadowPlane(const GfxRenderer& r, const bool msbPlane) {
  // Plane bits: dark grey sets both planes, light grey sets only MSB. In a grey plane a set bit (drawn
  // as White, like the reader's anti-aliasing pass) marks a grey pixel. Shapes replay in drawing order,
  // as the black-and-white frame was painted: each marks its bands, then clears its face and ink edge
  // (drawn as Black), which also clears any earlier band the face covers.
  for (int i = 0; i < std::max(0, gShadowCount); ++i) {
    const auto& s = gShadowShapes[i];
    if (msbPlane) r.fillRoundedRect(s.x + s.mist, s.y + s.mist, s.w, s.h, s.radius, Color::White);
    r.fillRoundedRect(s.x + s.tone, s.y + s.tone, s.w, s.h, s.radius, Color::White);
    r.fillRoundedRect(s.x, s.y, s.w, s.h, s.radius, Color::Black);
    if (s.ink) r.fillRoundedRect(s.x + s.ink, s.y + s.ink, s.w, s.h, s.radius, Color::Black);
  }
  if (!msbPlane) gShadowCount = -1;  // the LSB pass is the last user of this frame's log
}

}  // namespace relief
