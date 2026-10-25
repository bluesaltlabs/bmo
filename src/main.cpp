#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>

#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef BMO_PIN_BOOT
#define BMO_PIN_BOOT 9
#endif
#ifndef BMO_PIN_LCD_MOSI
#define BMO_PIN_LCD_MOSI 6
#endif
#ifndef BMO_PIN_LCD_SCLK
#define BMO_PIN_LCD_SCLK 7
#endif
#ifndef BMO_PIN_LCD_CS
#define BMO_PIN_LCD_CS 14
#endif
#ifndef BMO_PIN_LCD_DC
#define BMO_PIN_LCD_DC 15
#endif
#ifndef BMO_PIN_LCD_RST
#define BMO_PIN_LCD_RST 21
#endif
#ifndef BMO_PIN_LCD_BL
#define BMO_PIN_LCD_BL 22
#endif
#ifndef BMO_PIN_LINK_RX
#define BMO_PIN_LINK_RX 17
#endif
#ifndef BMO_PIN_LINK_TX
#define BMO_PIN_LINK_TX 16
#endif
#ifndef BMO_LINK_BAUD
#define BMO_LINK_BAUD 115200
#endif
#ifndef BMO_LCD_WIDTH
#define BMO_LCD_WIDTH 172
#endif
#ifndef BMO_LCD_HEIGHT
#define BMO_LCD_HEIGHT 320
#endif
#ifndef BMO_LCD_COL_OFFSET
#define BMO_LCD_COL_OFFSET 34
#endif
#ifndef BMO_LCD_ROW_OFFSET
#define BMO_LCD_ROW_OFFSET 0
#endif
#ifndef BMO_LCD_ROTATION
#define BMO_LCD_ROTATION 1
#endif
#ifndef BMO_LCD_IPS
#define BMO_LCD_IPS 1
#endif

namespace {

constexpr uint32_t kFrameIntervalMs = 45;
constexpr uint32_t kBlinkDurationMs = 170;
constexpr uint32_t kAutoCycleIntervalMs = 4000;
constexpr uint32_t kButtonDebounceMs = 30;
constexpr uint32_t kButtonDoublePressWindowMs = 350;
constexpr uint32_t kLongPressMs = 1200;
constexpr uint32_t kVeryLongPressMs = 3000;
constexpr size_t kCommandBufferSize = 96;
constexpr int16_t kCanvasWidth = (BMO_LCD_ROTATION % 2 == 0) ? BMO_LCD_WIDTH : BMO_LCD_HEIGHT;
constexpr int16_t kCanvasHeight = (BMO_LCD_ROTATION % 2 == 0) ? BMO_LCD_HEIGHT : BMO_LCD_WIDTH;

enum class Expression : uint8_t {
  Idle = 0,
  Happy,
  Thinking,
  Listening,
  Speaking,
  Confused,
  Sleeping,
  Angry,
  Sad,
  Count
};

struct ButtonTracker {
  bool stablePressed = false;
  bool rawPressed = false;
  uint32_t lastDebounceMs = 0;
  uint32_t pressedAtMs = 0;
  uint32_t lastReleaseMs = 0;
  uint8_t shortPressCount = 0;
};

struct PupilOffset {
  int16_t x;
  int16_t y;
};

struct PanelLayout {
  int16_t screenWidth = BMO_LCD_WIDTH;
  int16_t screenHeight = BMO_LCD_HEIGHT;
  int16_t panelX = 8;
  int16_t panelY = 8;
  int16_t panelW = BMO_LCD_WIDTH - 16;
  int16_t panelH = BMO_LCD_HEIGHT - 16;
  int16_t panelRadius = 12;
};

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    BMO_PIN_LCD_DC,
    BMO_PIN_LCD_CS,
    BMO_PIN_LCD_SCLK,
    BMO_PIN_LCD_MOSI,
    GFX_NOT_DEFINED);

Arduino_ST7789 *display = new Arduino_ST7789(
    bus,
    BMO_PIN_LCD_RST,
    BMO_LCD_ROTATION,
    (BMO_LCD_IPS != 0),
    BMO_LCD_WIDTH,
    BMO_LCD_HEIGHT,
    BMO_LCD_COL_OFFSET,
    BMO_LCD_ROW_OFFSET,
    BMO_LCD_COL_OFFSET,
    BMO_LCD_ROW_OFFSET);

Arduino_Canvas *gfx = new Arduino_Canvas(kCanvasWidth, kCanvasHeight, display);

Expression currentExpression = Expression::Idle;
ButtonTracker button{};
bool talkingAnimationEnabled = false;
bool debugAutoCycleEnabled = false;
bool blinkActive = false;
uint32_t blinkStartedAtMs = 0;
uint32_t nextBlinkAtMs = 0;
uint32_t lastFrameAtMs = 0;
uint32_t lastDebugCycleAtMs = 0;
char commandBuffer[kCommandBufferSize] = {};
size_t commandBufferLength = 0;
PanelLayout panelLayout{};
bool frameBaseDrawn = false;

// Reference palette: #63bda4 body, #171717 ink, #0099cb dot, #ffb600 debug, #ff66cc talk.
uint16_t colorScreen;
uint16_t colorInk;
uint16_t colorMouth;
uint16_t colorTongue;
uint16_t colorTeeth;
uint16_t colorTalk;
uint16_t colorDebug;

struct ScreenRect {
  int16_t x = 0;
  int16_t y = 0;
  int16_t w = 0;
  int16_t h = 0;
};
ScreenRect screenRect{};

int16_t clampToRange(int32_t value, int16_t minValue, int16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return static_cast<int16_t>(value);
}

const char *expressionToString(Expression expression) {
  switch (expression) {
    case Expression::Idle:
      return "IDLE";
    case Expression::Happy:
      return "HAPPY";
    case Expression::Thinking:
      return "THINKING";
    case Expression::Listening:
      return "LISTENING";
    case Expression::Speaking:
      return "SPEAKING";
    case Expression::Confused:
      return "CONFUSED";
    case Expression::Sleeping:
      return "SLEEPING";
    case Expression::Angry:
      return "ANGRY";
    case Expression::Sad:
      return "SAD";
    default:
      return "UNKNOWN";
  }
}

void trimInPlace(char *value) {
  char *start = value;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != value) {
    memmove(value, start, strlen(start) + 1);
  }

  size_t len = strlen(value);
  while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t')) {
    value[len - 1] = '\0';
    --len;
  }
}

void uppercaseInPlace(char *value) {
  for (size_t i = 0; value[i] != '\0'; ++i) {
    value[i] = static_cast<char>(toupper(static_cast<unsigned char>(value[i])));
  }
}

bool startsWith(const char *value, const char *prefix) {
  return strncmp(value, prefix, strlen(prefix)) == 0;
}

bool parseExpression(char *value, Expression &outExpression) {
  if (strcmp(value, "IDLE") == 0) {
    outExpression = Expression::Idle;
    return true;
  }
  if (strcmp(value, "HAPPY") == 0) {
    outExpression = Expression::Happy;
    return true;
  }
  if (strcmp(value, "THINKING") == 0) {
    outExpression = Expression::Thinking;
    return true;
  }
  if (strcmp(value, "LISTENING") == 0) {
    outExpression = Expression::Listening;
    return true;
  }
  if (strcmp(value, "SPEAKING") == 0) {
    outExpression = Expression::Speaking;
    return true;
  }
  if (strcmp(value, "CONFUSED") == 0) {
    outExpression = Expression::Confused;
    return true;
  }
  if (strcmp(value, "SLEEPING") == 0) {
    outExpression = Expression::Sleeping;
    return true;
  }
  if (strcmp(value, "ANGRY") == 0) {
    outExpression = Expression::Angry;
    return true;
  }
  if (strcmp(value, "SAD") == 0) {
    outExpression = Expression::Sad;
    return true;
  }
  return false;
}

void refreshLayout() {
  panelLayout.screenWidth = static_cast<int16_t>(gfx->width());
  panelLayout.screenHeight = static_cast<int16_t>(gfx->height());

  const int16_t minSide = panelLayout.screenWidth < panelLayout.screenHeight
                              ? panelLayout.screenWidth
                              : panelLayout.screenHeight;
  int16_t margin = minSide / 18;
  if (margin < 6) {
    margin = 6;
  }

  panelLayout.panelX = margin;
  panelLayout.panelY = margin;
  panelLayout.panelW = panelLayout.screenWidth - (margin * 2);
  panelLayout.panelH = panelLayout.screenHeight - (margin * 2);
  panelLayout.panelRadius = clampToRange(minSide / 9, 10, 22);
}

void drawFrameBase() {
  // Face only: the physical device provides the body, bezel and button.
  gfx->fillScreen(colorScreen);
  screenRect.x = 0;
  screenRect.y = 0;
  screenRect.w = panelLayout.screenWidth;
  screenRect.h = panelLayout.screenHeight;
  frameBaseDrawn = true;
}

void scheduleNextBlink(uint32_t nowMs) {
  nextBlinkAtMs = nowMs + static_cast<uint32_t>(random(2500, 6800));
}

void startBlink(uint32_t nowMs) {
  if (currentExpression == Expression::Sleeping) {
    return;
  }
  blinkActive = true;
  blinkStartedAtMs = nowMs;
}

void setExpression(Expression expression) {
  currentExpression = expression;
  lastDebugCycleAtMs = millis();
}

void cycleExpression() {
  uint8_t raw = static_cast<uint8_t>(currentExpression);
  raw = static_cast<uint8_t>((raw + 1) % static_cast<uint8_t>(Expression::Count));
  setExpression(static_cast<Expression>(raw));
}

float currentEyeOpenFactor(uint32_t nowMs) {
  if (currentExpression == Expression::Sleeping) {
    return 0.05f;
  }

  if (!blinkActive) {
    return 1.0f;
  }

  const uint32_t elapsed = nowMs - blinkStartedAtMs;
  if (elapsed >= kBlinkDurationMs) {
    blinkActive = false;
    scheduleNextBlink(nowMs);
    return 1.0f;
  }

  const float progress = static_cast<float>(elapsed) / static_cast<float>(kBlinkDurationMs);
  if (progress < 0.5f) {
    return 1.0f - (progress * 2.0f);
  }
  return (progress - 0.5f) * 2.0f;
}

PupilOffset getPupilOffset(bool leftEye, uint32_t nowMs) {
  const float t = static_cast<float>(nowMs) / 1000.0f;
  const float baseX = sinf((t * 0.9f) + (leftEye ? 0.45f : -0.2f));
  const float baseY = cosf((t * 0.65f) + (leftEye ? 0.2f : 0.6f));
  int16_t dx = static_cast<int16_t>(roundf(baseX * 3.0f));
  int16_t dy = static_cast<int16_t>(roundf(baseY * 2.0f));

  switch (currentExpression) {
    case Expression::Thinking:
      dx -= 3;
      dy -= 2;
      break;
    case Expression::Listening:
      dy -= 1;
      break;
    case Expression::Speaking:
      dx += 1;
      break;
    case Expression::Confused:
      dx = leftEye ? -2 : 3;
      dy = (nowMs / 350U) % 2U == 0U ? -1 : 1;
      break;
    case Expression::Sleeping:
      dx = 0;
      dy = 0;
      break;
    default:
      break;
  }

  return {dx, dy};
}

void drawArc(
    int16_t centerX,
    int16_t centerY,
    int16_t radius,
    int16_t startDeg,
    int16_t endDeg,
    int16_t thickness,
    uint16_t color) {
  constexpr float kDegToRad = 3.14159265f / 180.0f;
  for (int16_t ring = 0; ring < thickness; ++ring) {
    const int16_t radiusWithThickness = radius + ring;
    const float stepRad = 0.5f / static_cast<float>(radiusWithThickness);
    const float endRad = static_cast<float>(endDeg) * kDegToRad;
    for (float rad = static_cast<float>(startDeg) * kDegToRad; rad <= endRad; rad += stepRad) {
      const int16_t x = centerX + static_cast<int16_t>(roundf(cosf(rad) * radiusWithThickness));
      const int16_t y = centerY + static_cast<int16_t>(roundf(sinf(rad) * radiusWithThickness));
      gfx->drawPixel(x, y, color);
    }
  }
}

void fillHalfEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, bool bottom, uint16_t color) {
  if (ry < 1) {
    return;
  }
  for (int16_t dy = 0; dy <= ry; ++dy) {
    const float ratio = static_cast<float>(dy) / static_cast<float>(ry);
    const int16_t halfW = static_cast<int16_t>(roundf(static_cast<float>(rx) * sqrtf(1.0f - (ratio * ratio))));
    gfx->drawFastHLine(cx - halfW, bottom ? cy + dy : cy - dy, (halfW * 2) + 1, color);
  }
}

void fillCapsule(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t r, uint16_t color) {
  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float len = sqrtf((dx * dx) + (dy * dy));
  gfx->fillCircle(x0, y0, r, color);
  gfx->fillCircle(x1, y1, r, color);
  if (len < 1.0f) {
    return;
  }
  const int16_t nx = static_cast<int16_t>(roundf((-dy / len) * r));
  const int16_t ny = static_cast<int16_t>(roundf((dx / len) * r));
  gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, color);
  gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, color);
}

// Shallow arc through endpoints (cx +/- halfWidth, cy) that bulges by `sag` pixels.
void drawCurve(int16_t cx, int16_t cy, int16_t halfWidth, int16_t sag, int16_t thickness, bool smile, uint16_t color) {
  const float hw = static_cast<float>(halfWidth);
  const float s = static_cast<float>(sag);
  const float radius = ((hw * hw) + (s * s)) / (2.0f * s);
  const float halfAngle = asinf(hw / radius) * 180.0f / 3.14159265f;
  const int16_t r = static_cast<int16_t>(roundf(radius));
  if (smile) {
    drawArc(cx, cy + sag - r, r, static_cast<int16_t>(90.0f - halfAngle), static_cast<int16_t>(90.0f + halfAngle), thickness, color);
  } else {
    drawArc(cx, cy - sag + r, r, static_cast<int16_t>(270.0f - halfAngle), static_cast<int16_t>(270.0f + halfAngle), thickness, color);
  }
}

void drawDotEye(int16_t x, int16_t y, int16_t r, float openFactor) {
  int16_t ry = static_cast<int16_t>(roundf(static_cast<float>(r) * openFactor));
  if (ry < 1) {
    ry = 1;
  }
  gfx->fillEllipse(x, y, r, ry, colorInk);
}

void drawClosedEye(int16_t x, int16_t y, bool happy) {
  if (happy) {
    drawArc(x, y + 8, 9, 215, 325, 3, colorInk);
  } else {
    drawArc(x, y - 8, 9, 35, 145, 3, colorInk);
  }
}

// D-shaped open mouth: flat top at y, curved bottom.
void drawOpenMouth(int16_t cx, int16_t y, int16_t rx, int16_t ry, bool teeth) {
  fillHalfEllipse(cx, y, rx + 2, ry + 2, true, colorInk);
  gfx->fillRect(cx - rx - 2, y - 1, (rx * 2) + 5, 3, colorInk);
  fillHalfEllipse(cx, y + 1, rx, ry, true, colorMouth);
  if (ry >= 11) {
    gfx->fillEllipse(cx, y + ((ry * 7) / 10), (rx * 55) / 100, (ry * 28) / 100, colorTongue);
  }
  if (teeth) {
    gfx->fillRect(cx - ((rx * 85) / 100), y + 1, (rx * 170) / 100, 5, colorTeeth);
  }
}

void drawEyesAndMouth(uint32_t nowMs) {
  const int16_t cx = screenRect.x + (screenRect.w / 2);
  int16_t faceW = static_cast<int16_t>((screenRect.h * 17) / 10);
  if (faceW > screenRect.w) {
    faceW = screenRect.w;
  }
  const int16_t eyeDX = (faceW * 32) / 100;
  const int16_t leftX = cx - eyeDX;
  const int16_t rightX = cx + eyeDX;
  const int16_t eyeY = screenRect.y + ((screenRect.h * 36) / 100);
  const int16_t mouthY = screenRect.y + ((screenRect.h * 56) / 100);

  const float open = currentEyeOpenFactor(nowMs);
  const PupilOffset offL = getPupilOffset(true, nowMs);
  const PupilOffset offR = getPupilOffset(false, nowMs);

  const uint16_t phase = (nowMs / 90U) % 8U;
  const int16_t talkHeight = phase < 4 ? phase * 3 : (8 - phase) * 3;
  const bool speakingMouth = currentExpression == Expression::Speaking ||
                             (talkingAnimationEnabled && currentExpression != Expression::Sleeping);

  switch (currentExpression) {
    case Expression::Idle:
      drawDotEye(leftX + offL.x, eyeY + offL.y, 6, open);
      drawDotEye(rightX + offR.x, eyeY + offR.y, 6, open);
      gfx->fillRect(rightX + 2, eyeY - 12, 10, 2, colorInk);
      break;
    case Expression::Happy:
      drawClosedEye(leftX, eyeY, true);
      drawClosedEye(rightX, eyeY, true);
      break;
    case Expression::Thinking:
      drawDotEye(leftX + offL.x, eyeY + offL.y, 6, open);
      gfx->drawCircle(rightX, eyeY, 8, colorInk);
      gfx->drawCircle(rightX, eyeY, 4, colorInk);
      gfx->fillCircle(rightX, eyeY, 1, colorInk);
      break;
    case Expression::Listening:
      drawDotEye(leftX + offL.x, eyeY + offL.y, 8, open);
      drawDotEye(rightX + offR.x, eyeY + offR.y, 8, open);
      break;
    case Expression::Speaking:
      drawDotEye(leftX + offL.x, eyeY + offL.y, 12, open);
      drawDotEye(rightX + offR.x, eyeY + offR.y, 12, open);
      break;
    case Expression::Confused: {
      const int16_t wob = (nowMs / 300U) % 2U == 0U ? 0 : 2;
      gfx->fillEllipse(leftX - 4, eyeY - 6 + wob, 6, 9, colorInk);
      gfx->fillEllipse(rightX + 4, eyeY - 8 - wob, 6, 9, colorInk);
      for (int8_t i = 0; i < 3; ++i) {
        const int16_t zx = leftX - 14 + (i * 5);
        const int16_t zy = eyeY + 12 + ((i % 2) * 5);
        gfx->drawLine(zx, zy, zx + 5, (i % 2) == 0 ? zy + 5 : zy - 5, colorInk);
        const int16_t wx = rightX + 6 + (i * 5);
        gfx->drawLine(wx, zy, wx + 5, (i % 2) == 0 ? zy + 5 : zy - 5, colorInk);
      }
      break;
    }
    case Expression::Sleeping:
      drawClosedEye(leftX, eyeY, false);
      drawClosedEye(rightX, eyeY, false);
      gfx->setTextColor(colorInk);
      gfx->setTextSize(2);
      gfx->setCursor(rightX + 12, eyeY - 30);
      gfx->print("z");
      gfx->setTextSize(1);
      gfx->print("Z");
      break;
    case Expression::Angry: {
      for (int8_t side = -1; side <= 1; side += 2) {
        const int16_t ex = side < 0 ? leftX : rightX;
        drawDotEye(ex, eyeY, 5, open);
        gfx->drawFastHLine(ex - 11, eyeY - 9, 22, colorInk);
        gfx->drawFastHLine(ex - 9, eyeY + 9, 18, colorInk);
        gfx->drawLine(ex - (side * 12), eyeY - 15, ex + (side * 10), eyeY - 8, colorInk);
        gfx->drawLine(ex - (side * 12), eyeY - 14, ex + (side * 10), eyeY - 7, colorInk);
      }
      break;
    }
    case Expression::Sad:
      for (int8_t side = -1; side <= 1; side += 2) {
        const int16_t ex = side < 0 ? leftX : rightX;
        drawDotEye(ex + (side < 0 ? offL.x : offR.x), eyeY + 2, 5, open);
        gfx->fillRect(ex - 8, eyeY - 6, 17, 3, colorInk);
      }
      break;
    default:
      break;
  }

  // Mouths
  if (speakingMouth) {
    int16_t ry = static_cast<int16_t>(6 + talkHeight);
    drawOpenMouth(cx, mouthY - 6, 26, ry, ry >= 12);
    return;
  }

  switch (currentExpression) {
    case Expression::Idle:
      drawCurve(cx, mouthY, 46, 6, 3, true, colorInk);
      break;
    case Expression::Happy:
      drawArc(cx, mouthY - 16, 20, 20, 160, 4, colorInk);
      break;
    case Expression::Thinking: {
      const int16_t bob = static_cast<int16_t>(sinf(static_cast<float>(nowMs) / 400.0f) * 2.0f);
      fillCapsule(cx - 6, mouthY + 12 + bob, cx + 8, mouthY - 6 + bob, 10, colorInk);
      fillCapsule(cx - 6, mouthY + 12 + bob, cx + 8, mouthY - 6 + bob, 8, colorMouth);
      break;
    }
    case Expression::Listening:
      drawOpenMouth(cx, mouthY - 2, 22, 8 + static_cast<int16_t>(sinf(static_cast<float>(nowMs) / 500.0f) * 2.0f), false);
      break;
    case Expression::Confused: {
      const int16_t r = 15;
      const int16_t my = mouthY + 18;
      drawArc(cx, my, r - 5, 180, 360, 11, colorInk);
      gfx->fillCircle(cx - r, my, 5, colorInk);
      gfx->fillCircle(cx + r, my, 5, colorInk);
      drawArc(cx, my, r - 3, 180, 360, 7, colorMouth);
      gfx->fillCircle(cx - r, my, 3, colorMouth);
      gfx->fillCircle(cx + r, my, 3, colorMouth);
      break;
    }
    case Expression::Sleeping: {
      const int16_t breath = static_cast<int16_t>(sinf(static_cast<float>(nowMs) / 700.0f) * 3.0f);
      const int16_t ry = 12 + breath;
      fillHalfEllipse(cx, mouthY + 14, 20, ry + 2, false, colorInk);
      fillHalfEllipse(cx, mouthY + 14, 18, ry, false, colorMouth);
      break;
    }
    case Expression::Angry: {
      const int16_t shake = static_cast<int16_t>((nowMs / 60U) % 3U) - 1;
      const int16_t mx = cx + shake;
      gfx->fillRoundRect(mx - 32, mouthY - 10, 64, 28, 9, colorInk);
      gfx->fillRoundRect(mx - 29, mouthY - 7, 58, 22, 7, colorTeeth);
      gfx->fillRect(mx - 29, mouthY + 2, 58, 4, colorMouth);
      gfx->fillRect(mx - 15, mouthY - 7, 2, 22, colorInk);
      gfx->fillRect(mx, mouthY - 7, 2, 22, colorInk);
      gfx->fillRect(mx + 15, mouthY - 7, 2, 22, colorInk);
      break;
    }
    case Expression::Sad:
      drawCurve(cx, mouthY + 4, 36, 5, 3, false, colorInk);
      break;
    default:
      break;
  }
}

void drawFace(uint32_t nowMs) {
  drawFrameBase();
  drawEyesAndMouth(nowMs);

  const int16_t indicatorY = 8;
  const int16_t dotX = screenRect.w - 10;
  if (talkingAnimationEnabled) {
    gfx->fillCircle(dotX, indicatorY, 4, colorTalk);
  }
  if (debugAutoCycleEnabled) {
    gfx->fillCircle(dotX - 12, indicatorY, 4, colorDebug);
    gfx->setTextColor(colorInk);
    gfx->setTextSize(1);
    gfx->setCursor(6, 4);
    gfx->print(expressionToString(currentExpression));
  }

  gfx->flush();
}

// Replies go to whichever link (USB or link UART) sent the command.
Print *replyTo = &Serial;

constexpr uint32_t kLinkTimeoutMs = 3000;
bool linkAlive = false;
uint32_t lastLinkRxAtMs = 0;
bool commandFromLink = false;

void printHelp() {
  replyTo->println("Commands:");
  replyTo->println("  HELP");
  replyTo->println("  STATUS");
  replyTo->println("  NEXT");
  replyTo->println("  BLINK");
  replyTo->println("  STATE <IDLE|HAPPY|THINKING|LISTENING|SPEAKING|CONFUSED|SLEEPING|ANGRY|SAD>");
  replyTo->println("  <IDLE|HAPPY|THINKING|LISTENING|SPEAKING|CONFUSED|SLEEPING|ANGRY|SAD>  (shorthand)");
  replyTo->println("  FACE <IDLE|...>  (link command, replies ACK FACE <name>)");
  replyTo->println("  PING  (heartbeat, replies PONG)");
  replyTo->println("  TALK <ON|OFF|TOGGLE>");
  replyTo->println("  DEBUG <ON|OFF|TOGGLE>");
}

void printStatus() {
  replyTo->printf(
      "{\"state\":\"%s\",\"talk\":%s,\"debug\":%s,\"link\":%s}\n",
      expressionToString(currentExpression),
      talkingAnimationEnabled ? "true" : "false",
      debugAutoCycleEnabled ? "true" : "false",
      linkAlive ? "true" : "false");
}

bool parseOnOffToggle(const char *value, bool &target, const char *label) {
  if (strcmp(value, "ON") == 0) {
    target = true;
    replyTo->printf("OK %s ON\n", label);
    return true;
  }
  if (strcmp(value, "OFF") == 0) {
    target = false;
    replyTo->printf("OK %s OFF\n", label);
    return true;
  }
  if (strcmp(value, "TOGGLE") == 0) {
    target = !target;
    replyTo->printf("OK %s %s\n", label, target ? "ON" : "OFF");
    return true;
  }
  return false;
}

void processCommand(char *line) {
  trimInPlace(line);
  if (line[0] == '\0') {
    return;
  }

  uppercaseInPlace(line);

  // Any well-formed command from the link counts as a heartbeat.
  const bool fromLink = commandFromLink;
  auto markLinkAlive = [&]() {
    if (fromLink) {
      if (!linkAlive) {
        Serial.println("LINK UP");
      }
      linkAlive = true;
      lastLinkRxAtMs = millis();
    }
  };

  if (strcmp(line, "PING") == 0) {
    markLinkAlive();
    replyTo->println("PONG");
    return;
  }
  if (startsWith(line, "FACE ")) {
    char *faceArg = line + 5;
    trimInPlace(faceArg);
    Expression parsedFace = Expression::Idle;
    if (!parseExpression(faceArg, parsedFace)) {
      replyTo->println("ERR unknown face");
      return;
    }
    markLinkAlive();
    setExpression(parsedFace);
    replyTo->printf("ACK FACE %s\n", expressionToString(currentExpression));
    return;
  }

  if (strcmp(line, "HELP") == 0) {
    printHelp();
    return;
  }
  if (strcmp(line, "STATUS") == 0) {
    printStatus();
    return;
  }
  if (strcmp(line, "NEXT") == 0) {
    cycleExpression();
    replyTo->printf("OK STATE %s\n", expressionToString(currentExpression));
    return;
  }
  if (strcmp(line, "BLINK") == 0) {
    startBlink(millis());
    replyTo->println("OK BLINK");
    return;
  }
  if (startsWith(line, "STATE ")) {
    char *stateArg = line + 6;
    trimInPlace(stateArg);
    Expression parsed = Expression::Idle;
    if (!parseExpression(stateArg, parsed)) {
      replyTo->println("ERR unknown state");
      return;
    }
    setExpression(parsed);
    replyTo->printf("OK STATE %s\n", expressionToString(currentExpression));
    return;
  }
  {
    Expression shorthand = Expression::Idle;
    if (parseExpression(line, shorthand)) {
      setExpression(shorthand);
      replyTo->printf("OK STATE %s\n", expressionToString(currentExpression));
      return;
    }
  }
  if (startsWith(line, "TALK ")) {
    char *arg = line + 5;
    trimInPlace(arg);
    if (!parseOnOffToggle(arg, talkingAnimationEnabled, "TALK")) {
      replyTo->println("ERR TALK expects ON/OFF/TOGGLE");
    }
    return;
  }
  if (startsWith(line, "DEBUG ")) {
    char *arg = line + 6;
    trimInPlace(arg);
    if (!parseOnOffToggle(arg, debugAutoCycleEnabled, "DEBUG")) {
      replyTo->println("ERR DEBUG expects ON/OFF/TOGGLE");
    }
    if (debugAutoCycleEnabled) {
      lastDebugCycleAtMs = millis();
    }
    return;
  }

  replyTo->println("ERR unknown command");
}

void pollStream(Stream &port, char *buffer, size_t &length, bool isLink) {
  while (port.available() > 0) {
    const char incoming = static_cast<char>(port.read());
    if (incoming == '\r') {
      continue;
    }
    if (incoming == '\n') {
      buffer[length] = '\0';
      length = 0;
      replyTo = &port;
      commandFromLink = isLink;
      processCommand(buffer);
      commandFromLink = false;
      replyTo = &Serial;
      continue;
    }
    if (length + 1 >= kCommandBufferSize) {
      length = 0;
      port.println("ERR command too long");
      continue;
    }
    buffer[length++] = incoming;
  }
}

char linkBuffer[kCommandBufferSize] = {};
size_t linkBufferLength = 0;

void pollSerial() {
  pollStream(Serial, commandBuffer, commandBufferLength, false);
  pollStream(Serial1, linkBuffer, linkBufferLength, true);

  // Fail safe: if the audio board goes silent, return to idle once.
  if (linkAlive && (millis() - lastLinkRxAtMs) > kLinkTimeoutMs) {
    linkAlive = false;
    setExpression(Expression::Idle);
    talkingAnimationEnabled = false;
    Serial.println("LINK LOST -> IDLE");
  }
}

void handleButtonActionShortPress() {
  cycleExpression();
  Serial.printf("BTN SHORT -> STATE %s\n", expressionToString(currentExpression));
}

void handleButtonActionDoublePress() {
  startBlink(millis());
  Serial.println("BTN DOUBLE -> BLINK");
}

void handleButtonActionLongPress() {
  talkingAnimationEnabled = !talkingAnimationEnabled;
  Serial.printf("BTN LONG -> TALK %s\n", talkingAnimationEnabled ? "ON" : "OFF");
}

void handleButtonActionVeryLongPress() {
  debugAutoCycleEnabled = !debugAutoCycleEnabled;
  lastDebugCycleAtMs = millis();
  Serial.printf("BTN VERY_LONG -> DEBUG %s\n", debugAutoCycleEnabled ? "ON" : "OFF");
}

void pollButton(uint32_t nowMs) {
  const bool currentRawPressed = digitalRead(BMO_PIN_BOOT) == LOW;

  if (currentRawPressed != button.rawPressed) {
    button.rawPressed = currentRawPressed;
    button.lastDebounceMs = nowMs;
  }

  if ((nowMs - button.lastDebounceMs) < kButtonDebounceMs) {
    return;
  }

  if (button.stablePressed != button.rawPressed) {
    button.stablePressed = button.rawPressed;

    if (button.stablePressed) {
      button.pressedAtMs = nowMs;
      return;
    }

    const uint32_t pressDurationMs = nowMs - button.pressedAtMs;
    if (pressDurationMs >= kVeryLongPressMs) {
      button.shortPressCount = 0;
      handleButtonActionVeryLongPress();
      return;
    }
    if (pressDurationMs >= kLongPressMs) {
      button.shortPressCount = 0;
      handleButtonActionLongPress();
      return;
    }

    ++button.shortPressCount;
    button.lastReleaseMs = nowMs;
  }

  if (button.shortPressCount == 0) {
    return;
  }

  if ((nowMs - button.lastReleaseMs) < kButtonDoublePressWindowMs) {
    return;
  }

  if (button.shortPressCount == 1) {
    handleButtonActionShortPress();
  } else {
    handleButtonActionDoublePress();
  }
  button.shortPressCount = 0;
}

void updateAnimationState(uint32_t nowMs) {
  if (debugAutoCycleEnabled && (nowMs - lastDebugCycleAtMs) >= kAutoCycleIntervalMs) {
    cycleExpression();
    lastDebugCycleAtMs = nowMs;
  }

  if (!blinkActive && currentExpression != Expression::Sleeping && nowMs >= nextBlinkAtMs) {
    startBlink(nowMs);
  }
}

}  // namespace

void setup() {
  pinMode(BMO_PIN_BOOT, INPUT_PULLUP);
  pinMode(BMO_PIN_LCD_BL, OUTPUT);
  digitalWrite(BMO_PIN_LCD_BL, HIGH);

  Serial.begin(115200);
  Serial1.begin(BMO_LINK_BAUD, SERIAL_8N1, BMO_PIN_LINK_RX, BMO_PIN_LINK_TX);
  delay(200);
  Serial.println();
  Serial.println("BMO Phase 1 face engine booting...");

  randomSeed(micros());
  scheduleNextBlink(millis());

  if (!gfx->begin()) {
    Serial.println("FATAL: LCD or framebuffer initialization failed");
    Serial.flush();
    while (true) {
      delay(1000);
    }
  }

  // BMO palette: #63bda4 body, #171717 ink, #0099cb dot, #ffb600 debug, #ff66cc talk.
  colorScreen = gfx->color565(210, 252, 228);
  colorInk = gfx->color565(23, 23, 23);
  colorMouth = gfx->color565(67, 148, 89);
  colorTongue = gfx->color565(120, 205, 130);
  colorTeeth = gfx->color565(255, 253, 253);
  colorTalk = gfx->color565(255, 102, 204);
  colorDebug = gfx->color565(255, 182, 0);

  refreshLayout();
  drawFrameBase();

  gfx->setTextWrap(false);
  printHelp();
}

void loop() {
  const uint32_t nowMs = millis();
  pollSerial();
  pollButton(nowMs);
  updateAnimationState(nowMs);

  if ((nowMs - lastFrameAtMs) >= kFrameIntervalMs) {
    drawFace(nowMs);
    lastFrameAtMs = nowMs;
  }
}
