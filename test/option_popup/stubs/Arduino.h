#pragma once

// Test clock for OptionPopup's press echo.
inline unsigned long& fakeMillisNow() {
  static unsigned long now = 0;
  return now;
}
inline unsigned long millis() { return fakeMillisNow(); }
