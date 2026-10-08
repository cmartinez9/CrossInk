#pragma once

#include <EpdFontFamily.h>
#include <GfxRenderer.h>

#include "ReliefGeometry.h"
#include "fontIds.h"

namespace freeink {
struct Icon;
}

// Drawing primitives for the "Relief" soft-UI theme.
// Light comes from the top-left. A raised shape is stamped four times (LightGray +7, DarkGray +4,
// Black +1, then a white face inside a 1 px black hairline). A pressed shape is the same stack inset.
namespace relief {

constexpr int kClockFontId = RELIEF_CLOCK_FONT_ID;    // Inter Bold 46 pt: digits, colon, percent
constexpr int kDigitsFontId = RELIEF_DIGITS_FONT_ID;  // Inter Bold 31 pt: digits, colon, percent, minus
constexpr int kTitleFontId = RELIEF_TITLE_FONT_ID;    // Inter Bold 16 pt: ASCII

void raised(const GfxRenderer& r, int x, int y, int w, int h, int radius, bool lifted = false);
void pressed(const GfxRenderer& r, int x, int y, int w, int h, int radius);
inline void surface(const GfxRenderer& r, int x, int y, int w, int h, int radius, bool focused, bool lifted = false) {
  if (focused)
    pressed(r, x, y, w, h, radius);
  else
    raised(r, x, y, w, h, radius, lifted);
}
void groove(const GfxRenderer& r, int x, int y, int w, int h);
void tube(const GfxRenderer& r, int x, int y, int w, int h, float level, bool knob = false);
// The inspiration's task pill: a raised capsule holding a well whose ink rises with a wave crest.
void liquidPill(const GfxRenderer& r, int x, int y, int w, int h, float level, const char* title, const char* big,
                const char* small, bool focused, int phase = 0);
void liquidColumn(const GfxRenderer& r, int x, int y, int w, int h, float level, int phase = 0);
// Round key: paper (off) or ink (on); focused keys sink into a pressed well.
void roundKey(const GfxRenderer& r, int cx, int cy, int d, const freeink::Icon* icon, bool ink, bool focused,
              char fallbackLetter = '?');
// Text helpers. y is the top of the line box, as for GfxRenderer::drawText.
int textWidth(const GfxRenderer& r, int fontId, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR);
void text(const GfxRenderer& r, int fontId, int x, int y, const char* text, bool black = true,
          EpdFontFamily::Style style = EpdFontFamily::REGULAR);
void textCentered(const GfxRenderer& r, int fontId, int x, int w, int y, const char* text, bool black = true,
                  EpdFontFamily::Style style = EpdFontFamily::REGULAR);
void textRight(const GfxRenderer& r, int fontId, int right, int y, const char* text, bool black = true,
               EpdFontFamily::Style style = EpdFontFamily::REGULAR);
// Display text with letter-spacing: each character is drawn on its own, `tracking` px apart (negative
// tightens). For the clock, where the design tracks the digits in by 3 px.
int trackedTextWidth(const GfxRenderer& r, int fontId, const char* text, int tracking);
void trackedText(const GfxRenderer& r, int fontId, int x, int y, const char* text, int tracking);
// 50 % (checker) text for large display lines only; below ~26 px the checker eats the strokes.
void greyText(const GfxRenderer& r, int fontId, int x, int y, const char* text,
              EpdFontFamily::Style style = EpdFontFamily::REGULAR);
// Title font when the text is plain ASCII, Inter 12 Bold otherwise (the title face is ASCII only).
int titleFontFor(const char* text);
// Two raised rockers drawn over the physical rockers; the Confirm half is ink when it acts.
void rockerHints(const GfxRenderer& r, const char* back, const char* confirm, const char* left, const char* right,
                 bool confirmHeld = false);
// Raised half-pills on both edges, level with the X3 side buttons, when they are live.
void sideNubs(const GfxRenderer& r, bool top, bool bottom);
void drawCoverBmp(const GfxRenderer& r, const char* path, int x, int y, int w, int h, int radius);

// The pressed edge without a fill: hairline plus the inset bands along the top and left. For frames
// drawn around content that is already on screen (text fields).
void pressedFrame(const GfxRenderer& r, int x, int y, int w, int h, int radius);
// A lifted card: the larger stack (LightGray +10, DarkGray +6, Black +2) for dialogs, popups and sheets.
inline void lifted(const GfxRenderer& r, int x, int y, int w, int h, int radius) {
  raised(r, x, y, w, h, radius, true);
}
// Horizontal liquid: a groove whose ink fill ends in a vertical wave (loaders and transfers).
void liquidBar(const GfxRenderer& r, int x, int y, int w, int h, float level, int phase = 0);
// 75 % ink (the inverse of LightGray): the "held" state.
void fillShade(const GfxRenderer& r, int x, int y, int w, int h, int radius);
// Toggle: a groove with a raised knob when off, an ink track with a paper knob when on.
void toggle(const GfxRenderer& r, int x, int y, bool on);
// Dialog / footer key: raised paper, or ink when primary; focused keys sink.
void key(const GfxRenderer& r, int x, int y, int w, int h, const char* label, bool focused, bool primary);
// A sheet risen from the bottom: paper with a 2 px ink top edge and a groove grip.
void sheetEdge(const GfxRenderer& r, int y, int bottom);

// Grey shadows (stretch): while recording, raised shapes log their geometry and draw their bands solid
// black, so a 4-level grey pass can lighten the bands to real grey. Fixed-size log (no heap); shapes past
// the limit keep dither. endShadowRecording() drops the log when the grey pass cannot run.
void beginShadowRecording();
void endShadowRecording();
// Draws the recorded bands into the current grayscale plane: the DarkGray band in both planes, the
// LightGray band in the MSB plane only. Call once in GRAYSCALE_LSB mode and once in GRAYSCALE_MSB mode.
void drawRecordedShadowPlane(const GfxRenderer& r, bool msbPlane);

}  // namespace relief
