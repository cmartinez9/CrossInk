#pragma once

#include <algorithm>
#include <cmath>

// Pure layout math for the Relief theme (no renderer), shared by the drawing code and the native tests.
namespace relief {

// Corner radius that still fits the shape.
inline int clampRadius(const int radius, const int w, const int h) {
  return std::max(0, std::min({radius, w / 2, h / 2}));
}

// Wave crest of the liquid fill: the surface y for column `column` of a well whose top is `top` and
// height `h`, at `level` 0..1. Amplitude 5 px (4 % of short wells), wavelength 46 px; `phase` shifts it.
inline int liquidSurfaceY(const int top, const int h, const float level, const int column, const int phase) {
  const float lv = std::clamp(level, 0.0f, 1.0f);
  const float amp = std::min(5.0f, h * 0.04f);
  constexpr float kTwoPi = 6.2831853f;
  return static_cast<int>(top + h * (1.0f - lv) + amp * std::sin(kTwoPi * column / 46.0f + phase * 0.9f));
}

// First row of a `rows`-tall window over `count` items that keeps `selected` visible, moving the
// previous window `top` as little as possible.
inline int followWindow(int top, const int selected, const int rows, const int count) {
  if (rows <= 0 || count <= 0) return 0;
  if (selected >= 0) {
    if (selected < top) top = selected;
    if (selected >= top + rows) top = selected - rows + 1;
  }
  return std::clamp(top, 0, std::max(0, count - rows));
}

// Home "Up next" shelf: shows books 2-4 (index 1-3); when focus walks past them the window follows.
inline int upNextFirst(const bool inBooksRow, const int selectorIndex) {
  return (inBooksRow && selectorIndex > 3) ? selectorIndex - 2 : 1;
}

}  // namespace relief
