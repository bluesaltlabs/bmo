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

uint16_t colorBackground;
uint16_t colorPanel;
uint16_t colorPanelAccent;
uint16_t colorFaceDark;
uint16_t colorFaceLight;
uint16_t colorMouth;
uint16_t colorText;
uint16_t colorDebug;

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
  gfx->fillScreen(colorBackground);
  gfx->fillRoundRect(
      panelLayout.panelX,
      panelLayout.panelY,
      panelLayout.panelW,
      panelLayout.panelH,
      panelLayout.panelRadius,
      colorPanelAccent);
  gfx->drawRoundRect(
      panelLayout.panelX,
      panelLayout.panelY,
      panelLayout.panelW,
      panelLayout.panelH,
      panelLayout.panelRadius,
      colorPanel);
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
    if (currentExpression == Expression::Thinking) {
      return 0.85f;
    }
    if (currentExpression == Expression::Confused) {
      return 0.75f;
    }
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
    for (int16_t deg = startDeg; deg <= endDeg; ++deg) {
      const float rad = static_cast<float>(deg) * kDegToRad;
      const int16_t x = centerX + static_cast<int16_t>(roundf(cosf(rad) * radiusWithThickness));
      const int16_t y = centerY + static_cast<int16_t>(roundf(sinf(rad) * radiusWithThickness));
      gfx->drawPixel(x, y, color);
    }
  }
}

void drawEye(
    int16_t centerX,
    int16_t centerY,
    int16_t width,
    int16_t height,
    float openFactor,
    const PupilOffset &pupilOffset) {
  if (openFactor <= 0.08f) {
    gfx->fillRoundRect(centerX - (width / 2), centerY - 1, width, 3, 1, colorFaceDark);
    return;
  }

  int16_t renderedHeight = static_cast<int16_t>(roundf(static_cast<float>(height) * openFactor));
  if (renderedHeight < 4) {
    renderedHeight = 4;
  }

  const int16_t top = centerY - (renderedHeight / 2);
  const int16_t radius = renderedHeight > 8 ? 6 : (renderedHeight / 2);

  gfx->fillRoundRect(centerX - (width / 2), top, width, renderedHeight, radius, colorFaceLight);
  gfx->drawRoundRect(centerX - (width / 2), top, width, renderedHeight, radius, colorPanel);

  int16_t pupilRadius = renderedHeight / 4;
  if (pupilRadius < 2) {
    pupilRadius = 2;
  }

  const int16_t pupilClampX = (width / 2) - pupilRadius - 2;
  const int16_t pupilClampY = (renderedHeight / 2) - pupilRadius - 1;
  const int16_t pupilX = centerX + constrain(pupilOffset.x, -pupilClampX, pupilClampX);
  const int16_t pupilY = centerY + constrain(pupilOffset.y, -pupilClampY, pupilClampY);
  gfx->fillCircle(pupilX, pupilY, pupilRadius, colorFaceDark);
}

void drawMouth(uint32_t nowMs, int16_t centerX, int16_t centerY, int16_t mouthWidth) {
  const uint16_t phase = (nowMs / 90U) % 8U;
  const int16_t talkingHeight = phase < 4 ? phase * 3 : (8 - phase) * 3;
  const bool animateSpeaking = talkingAnimationEnabled || currentExpression == Expression::Speaking;
  const int16_t halfMouthWidth = mouthWidth / 2;

  switch (currentExpression) {
    case Expression::Idle:
      gfx->fillRoundRect(centerX - halfMouthWidth, centerY - 2, mouthWidth, 4, 2, colorMouth);
      break;
    case Expression::Happy:
      drawArc(centerX, centerY - 6, halfMouthWidth + 2, 22, 158, 3, colorMouth);
      break;
    case Expression::Thinking:
      gfx->fillRoundRect(centerX - (halfMouthWidth - 4), centerY - 2, mouthWidth - 8, 4, 2, colorMouth);
      gfx->fillCircle(centerX + halfMouthWidth + 4, centerY, 2, colorMouth);
      gfx->fillCircle(centerX + halfMouthWidth + 12, centerY + 2, 3, colorMouth);
      break;
    case Expression::Listening:
      gfx->drawRoundRect(centerX - 12, centerY - 8, 24, 16, 6, colorMouth);
      break;
    case Expression::Speaking: {
      int16_t mouthHeight = animateSpeaking ? (10 + talkingHeight) : 10;
      if (mouthHeight > 22) {
        mouthHeight = 22;
      }
      gfx->fillRoundRect(
          centerX - (halfMouthWidth + 6),
          centerY - (mouthHeight / 2),
          mouthWidth + 12,
          mouthHeight,
          8,
          colorMouth);
      break;
    }
    case Expression::Confused:
      gfx->drawLine(centerX - halfMouthWidth, centerY + 4, centerX - (halfMouthWidth / 2), centerY - 2, colorMouth);
      gfx->drawLine(centerX - (halfMouthWidth / 2), centerY - 2, centerX, centerY + 4, colorMouth);
      gfx->drawLine(centerX, centerY + 4, centerX + (halfMouthWidth / 2), centerY - 2, colorMouth);
      gfx->drawLine(centerX + (halfMouthWidth / 2), centerY - 2, centerX + halfMouthWidth, centerY + 4, colorMouth);
      break;
    case Expression::Sleeping:
      gfx->fillRoundRect(centerX - 10, centerY + 2, 20, 3, 1, colorMouth);
      break;
    default:
      break;
  }
}

void drawFace(uint32_t nowMs) {
  if (!frameBaseDrawn) {
    drawFrameBase();
  }

  const int16_t panelInnerX = panelLayout.panelX + 2;
  const int16_t panelInnerY = panelLayout.panelY + 2;
  const int16_t panelInnerW = panelLayout.panelW - 4;
  const int16_t panelInnerH = panelLayout.panelH - 4;
  const int16_t eyeWidth = clampToRange(panelLayout.screenWidth / 5, 36, 70);
  const int16_t eyeHeight = clampToRange(panelLayout.screenHeight / 5, 20, 38);
  const int16_t leftEyeX = (panelLayout.screenWidth * 32) / 100;
  const int16_t rightEyeX = panelLayout.screenWidth - leftEyeX;
  const int16_t eyeY = (panelLayout.screenHeight * 40) / 100;
  const int16_t mouthY = (panelLayout.screenHeight * 72) / 100;
  const int16_t mouthWidth = clampToRange(panelLayout.screenWidth / 6, 40, 60);

  auto clearZone = [&](int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w <= 0 || h <= 0) {
      return;
    }
    if (x < panelInnerX) {
      w -= (panelInnerX - x);
      x = panelInnerX;
    }
    if (y < panelInnerY) {
      h -= (panelInnerY - y);
      y = panelInnerY;
    }
    const int16_t panelInnerMaxX = panelInnerX + panelInnerW;
    const int16_t panelInnerMaxY = panelInnerY + panelInnerH;
    if ((x + w) > panelInnerMaxX) {
      w = panelInnerMaxX - x;
    }
    if ((y + h) > panelInnerMaxY) {
      h = panelInnerMaxY - y;
    }
    if (w > 0 && h > 0) {
      gfx->fillRect(x, y, w, h, colorPanelAccent);
    }
  };

  clearZone(panelInnerX + 8, panelInnerY + 4, panelInnerW - 16, 18);
  clearZone(leftEyeX - (eyeWidth / 2) - 14, eyeY - (eyeHeight / 2) - 14, (rightEyeX - leftEyeX) + eyeWidth + 28, eyeHeight + 34);
  clearZone((panelLayout.screenWidth / 2) - 94, mouthY - 32, 188, 50);

  const float eyeOpenFactor = currentEyeOpenFactor(nowMs);

  if (currentExpression == Expression::Sleeping) {
    drawEye(leftEyeX, eyeY, eyeWidth, eyeHeight, 0.02f, {0, 0});
    drawEye(rightEyeX, eyeY, eyeWidth, eyeHeight, 0.02f, {0, 0});
    gfx->setTextColor(colorMouth);
    gfx->setTextSize(1);
    gfx->setCursor(rightEyeX + (eyeWidth / 2) - 8, eyeY - eyeHeight);
    gfx->print("zZ");
  } else if (currentExpression == Expression::Happy) {
    drawEye(leftEyeX, eyeY, eyeWidth + 2, eyeHeight, 0.75f * eyeOpenFactor, getPupilOffset(true, nowMs));
    drawEye(rightEyeX, eyeY, eyeWidth + 2, eyeHeight, 0.75f * eyeOpenFactor, getPupilOffset(false, nowMs));
  } else if (currentExpression == Expression::Confused) {
    drawEye(leftEyeX, eyeY, eyeWidth - 4, eyeHeight - 4, eyeOpenFactor, getPupilOffset(true, nowMs));
    drawEye(rightEyeX, eyeY + 3, eyeWidth - 10, eyeHeight - 8, eyeOpenFactor * 0.85f, getPupilOffset(false, nowMs));
  } else {
    drawEye(leftEyeX, eyeY, eyeWidth, eyeHeight, eyeOpenFactor, getPupilOffset(true, nowMs));
    drawEye(rightEyeX, eyeY, eyeWidth, eyeHeight, eyeOpenFactor, getPupilOffset(false, nowMs));
  }

  drawMouth(nowMs, panelLayout.screenWidth / 2, mouthY, mouthWidth);

  gfx->setTextColor(colorText);
  gfx->setTextSize(1);
  gfx->setCursor(panelLayout.panelX + 10, panelLayout.panelY + 8);
  gfx->print(expressionToString(currentExpression));

  if (talkingAnimationEnabled) {
    gfx->fillCircle(panelLayout.panelX + panelLayout.panelW - 16, panelLayout.panelY + 11, 4, colorText);
  }
  if (debugAutoCycleEnabled) {
    gfx->fillCircle(panelLayout.panelX + panelLayout.panelW - 30, panelLayout.panelY + 11, 4, colorDebug);
  }

  gfx->flush();
}

// Replies go to whichever link (USB or link UART) sent the command.
Print *replyTo = &Serial;

void printHelp() {
  replyTo->println("Commands:");
  replyTo->println("  HELP");
  replyTo->println("  STATUS");
  replyTo->println("  NEXT");
  replyTo->println("  BLINK");
  replyTo->println("  STATE <IDLE|HAPPY|THINKING|LISTENING|SPEAKING|CONFUSED|SLEEPING>");
  replyTo->println("  <IDLE|HAPPY|THINKING|LISTENING|SPEAKING|CONFUSED|SLEEPING>  (shorthand)");
  replyTo->println("  TALK <ON|OFF|TOGGLE>");
  replyTo->println("  DEBUG <ON|OFF|TOGGLE>");
}

void printStatus() {
  replyTo->printf(
      "{\"state\":\"%s\",\"talk\":%s,\"debug\":%s}\n",
      expressionToString(currentExpression),
      talkingAnimationEnabled ? "true" : "false",
      debugAutoCycleEnabled ? "true" : "false");
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

void pollStream(Stream &port, char *buffer, size_t &length) {
  while (port.available() > 0) {
    const char incoming = static_cast<char>(port.read());
    if (incoming == '\r') {
      continue;
    }
    if (incoming == '\n') {
      buffer[length] = '\0';
      length = 0;
      replyTo = &port;
      processCommand(buffer);
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
  pollStream(Serial, commandBuffer, commandBufferLength);
  pollStream(Serial1, linkBuffer, linkBufferLength);
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

  colorBackground = gfx->color565(8, 22, 120);
  colorPanel = gfx->color565(6, 16, 88);
  colorPanelAccent = gfx->color565(18, 42, 168);
  colorFaceDark = gfx->color565(2, 8, 40);
  colorFaceLight = gfx->color565(232, 242, 255);
  colorMouth = gfx->color565(6, 14, 64);
  colorText = gfx->color565(214, 232, 255);
  colorDebug = gfx->color565(255, 215, 64);

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
