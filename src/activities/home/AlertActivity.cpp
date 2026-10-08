#include "AlertActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointState.h"
#include "components/UITheme.h"
#include "components/themes/relief/ReliefKit.h"
#include "fontIds.h"

void AlertActivity::onEnter() {
  Activity::onEnter();
  title = APP_STATE.pendingAlertTitle;
  body = APP_STATE.pendingAlertBody;
  goHomeOnBack = APP_STATE.pendingAlertGoHomeOnBack.exchange(false, std::memory_order_relaxed);
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("ALERT", "Alert screen could not be rendered synchronously");
    requestUpdate();
  }
}

void AlertActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (goHomeOnBack) {
      onGoHome();
    } else {
      finish();
    }
  }
}

void AlertActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (SETTINGS.uiTheme == CrossPointSettings::UI_THEME::RELIEF) {
    renderRelief();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto contentWidth = pageWidth - 2 * metrics.contentSidePadding;
  const auto x = metrics.contentSidePadding;
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  auto bodyLines = renderer.wrappedText(UI_10_FONT_ID, body.c_str(), contentWidth, 10);
  for (const auto& line : bodyLines) {
    renderer.drawText(UI_10_FONT_ID, x, y, line.c_str());
    y += lineHeight;
  }

  const auto labels = mappedInput.mapLabels(
      goHomeOnBack ? mappedInput.withBackArrow(tr(STR_HOME)) : mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

// Relief message layout: a raised glyph key, a display title and a centred body.
void AlertActivity::renderRelief() {
  using namespace relief;
  const int W = renderer.getScreenWidth();
  const int cx = W / 2;
  raised(renderer, cx - 48, 202, 96, 96, 48);
  textCentered(renderer, kTitleFontId, cx - 48, 96, 230, "!");
  const int titleFont = titleFontFor(title.c_str());
  const auto titleStyle = titleFont == kTitleFontId ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  const std::string t = renderer.truncatedText(titleFont, title.c_str(), W - 48, titleStyle);
  textCentered(renderer, titleFont, 0, W, 330, t.c_str(), true, titleStyle);
  const int lh = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  int y = 384;
  for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, body.c_str(), W - 112, 8)) {
    textCentered(renderer, UI_10_FONT_ID, 0, W, y, line.c_str());
    y += lh;
  }
  const auto labels = mappedInput.mapLabels(
      goHomeOnBack ? mappedInput.withBackArrow(tr(STR_HOME)) : mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
