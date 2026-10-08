#include <gtest/gtest.h>

#include "ReliefGeometry.h"

using relief::arcReach;
using relief::clampRadius;
using relief::followWindow;
using relief::liquidSurfaceY;
using relief::upNextFirst;

TEST(ReliefGeometry, ClampRadiusFitsTheShape) {
  EXPECT_EQ(clampRadius(24, 100, 40), 20);
  EXPECT_EQ(clampRadius(24, 30, 100), 15);
  EXPECT_EQ(clampRadius(8, 100, 100), 8);
  EXPECT_EQ(clampRadius(-3, 100, 100), 0);
}

TEST(ReliefGeometry, LiquidSurfaceStaysWithinTheCrestOfItsLevel) {
  constexpr int top = 100, h = 200;
  for (int column = 0; column < 120; ++column) {
    // Empty: the crest hugs the bottom; full: it hugs the top. Amplitude is at most 5 px.
    EXPECT_NEAR(liquidSurfaceY(top, h, 0.0f, column, 0), top + h, 5);
    EXPECT_NEAR(liquidSurfaceY(top, h, 1.0f, column, 0), top, 5);
    EXPECT_NEAR(liquidSurfaceY(top, h, 0.5f, column, 3), top + h / 2, 5);
  }
}

TEST(ReliefGeometry, LiquidLevelIsClamped) {
  EXPECT_EQ(liquidSurfaceY(0, 100, -1.0f, 0, 0), liquidSurfaceY(0, 100, 0.0f, 0, 0));
  EXPECT_EQ(liquidSurfaceY(0, 100, 2.0f, 0, 0), liquidSurfaceY(0, 100, 1.0f, 0, 0));
}

TEST(ReliefGeometry, ShortWellsGetASmallerCrest) {
  // A 50 px well has a 2 px amplitude (4 % of its height), so the crest never crosses the well.
  for (int column = 0; column < 46; ++column) {
    EXPECT_NEAR(liquidSurfaceY(0, 50, 0.5f, column, 0), 25, 2);
  }
}

TEST(ReliefGeometry, CrestFlattensNearEmptyAndFull) {
  // A nearly empty well keeps its liquid on the floor (no lump above a 2 px meniscus), and a full well
  // reaches its top in every column (no white sliver under the top edge).
  constexpr int top = 0, h = 160;
  for (int column = 0; column < 46; ++column) {
    EXPECT_GE(liquidSurfaceY(top, h, 0.02f, column, 0), top + h - 3 - 2);
    EXPECT_EQ(liquidSurfaceY(top, h, 1.0f, column, 0), top);
    EXPECT_EQ(liquidSurfaceY(top, h, 0.0f, column, 0), top + h);
  }
}

TEST(ReliefGeometry, ArcReachMatchesTheCircle) {
  EXPECT_EQ(arcReach(10, 0), 10);
  EXPECT_EQ(arcReach(10, 10), 0);
  EXPECT_EQ(arcReach(10, 11), -1);
  EXPECT_EQ(arcReach(10, -6), 8);  // 6^2 + 8^2 = 10^2
  for (int r = 1; r < 60; ++r) {
    for (int a = 0; a <= r; ++a) {
      const int d = arcReach(r, a);
      EXPECT_LE(a * a + d * d, r * r);
      EXPECT_GT(a * a + (d + 1) * (d + 1), r * r);
    }
  }
}

TEST(ReliefGeometry, PhaseMovesTheCrest) {
  bool differs = false;
  for (int column = 0; column < 46; ++column) {
    differs |= liquidSurfaceY(0, 200, 0.5f, column, 0) != liquidSurfaceY(0, 200, 0.5f, column, 1);
  }
  EXPECT_TRUE(differs);
}

TEST(ReliefGeometry, FollowWindowKeepsTheSelectionVisible) {
  EXPECT_EQ(followWindow(0, 0, 5, 20), 0);
  EXPECT_EQ(followWindow(0, 4, 5, 20), 0);
  EXPECT_EQ(followWindow(0, 5, 5, 20), 1);
  EXPECT_EQ(followWindow(10, 3, 5, 20), 3);
  EXPECT_EQ(followWindow(0, 19, 5, 20), 15);
  // Never scrolls past the end, and short lists stay at the top.
  EXPECT_EQ(followWindow(18, 19, 5, 20), 15);
  EXPECT_EQ(followWindow(3, 1, 5, 3), 0);
  EXPECT_EQ(followWindow(4, -1, 5, 20), 4);
  EXPECT_EQ(followWindow(0, 0, 0, 20), 0);
}

TEST(ReliefGeometry, UpNextShelfFollowsFocusPastTheThirdBook) {
  EXPECT_EQ(upNextFirst(true, 0), 1);
  EXPECT_EQ(upNextFirst(true, 3), 1);
  EXPECT_EQ(upNextFirst(true, 4), 2);
  EXPECT_EQ(upNextFirst(true, 7), 5);
  EXPECT_EQ(upNextFirst(false, 9), 1);
}
