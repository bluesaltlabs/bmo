// BMO Phase 2: audio-board stand-in. Sends simulated face events to the
// display board over UART2 with heartbeat, ACK and retry. No audio hardware yet.
#include <Arduino.h>

#ifndef BMO_PIN_LINK_RX
#define BMO_PIN_LINK_RX 16
#endif
#ifndef BMO_PIN_LINK_TX
#define BMO_PIN_LINK_TX 17
#endif
#ifndef BMO_PIN_BUTTON
#define BMO_PIN_BUTTON 0
#endif
#ifndef BMO_LINK_BAUD
#define BMO_LINK_BAUD 115200
#endif

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;
constexpr uint32_t kAckTimeoutMs = 250;
constexpr uint8_t kMaxRetries = 3;
constexpr uint32_t kLinkTimeoutMs = 3000;
constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kDemoIntervalMs = 3000;
constexpr size_t kBufferSize = 96;

const char *const kFaces[] = {"IDLE", "HAPPY", "THINKING", "LISTENING", "SPEAKING", "CONFUSED", "SLEEPING", "ANGRY", "SAD"};
constexpr size_t kFaceCount = sizeof(kFaces) / sizeof(kFaces[0]);

HardwareSerial &uartLink = Serial2;

char linkBuf[kBufferSize];
size_t linkLen = 0;
char usbBuf[kBufferSize];
size_t usbLen = 0;

bool linkUp = false;
uint32_t lastRxAtMs = 0;
uint32_t lastPingAtMs = 0;

// One outstanding command at a time; retried until ACKed.
char pending[kBufferSize] = {};
bool hasPending = false;
uint8_t attempts = 0;
uint32_t lastSendAtMs = 0;

size_t faceIndex = 0;
bool demoEnabled = false;
uint32_t lastDemoAtMs = 0;

bool lastButtonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t buttonChangedAtMs = 0;

void sendRaw(const char *text) {
  uartLink.println(text);
}

void sendReliable(const char *command) {
  strncpy(pending, command, kBufferSize - 1);
  pending[kBufferSize - 1] = '\0';
  hasPending = true;
  attempts = 1;
  lastSendAtMs = millis();
  sendRaw(pending);
  Serial.printf("TX %s\n", pending);
}

void sendFace(const char *name) {
  char command[kBufferSize];
  snprintf(command, sizeof(command), "FACE %s", name);
  sendReliable(command);
}

void handleLinkLine(char *line) {
  if (line[0] == '\0') {
    return;
  }
  lastRxAtMs = millis();
  if (!linkUp) {
    linkUp = true;
    Serial.println("LINK UP");
  }
  if (strcmp(line, "PONG") == 0) {
    return;
  }
  if (strncmp(line, "ACK ", 4) == 0) {
    if (hasPending && strcmp(line + 4, pending) == 0) {
      hasPending = false;
      Serial.printf("RX %s\n", line);
    }
    return;
  }
  Serial.printf("RX %s\n", line);
}

void pollStream(Stream &port, char *buffer, size_t &length, void (*handler)(char *)) {
  while (port.available() > 0) {
    const char c = static_cast<char>(port.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      buffer[length] = '\0';
      length = 0;
      handler(buffer);
      continue;
    }
    if (length + 1 >= kBufferSize) {
      length = 0;
      continue;
    }
    buffer[length++] = c;
  }
}

// USB console: "DEMO ON|OFF", "RAW <text>" (sent unmodified, for malformed-input
// tests), or a face name. Anything else is sent as a FACE command.
void handleUsbLine(char *line) {
  for (char *p = line; *p; ++p) {
    *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
  }
  if (line[0] == '\0') {
    return;
  }
  if (strcmp(line, "DEMO ON") == 0 || strcmp(line, "DEMO OFF") == 0) {
    demoEnabled = line[6] == 'N';
    Serial.printf("DEMO %s\n", demoEnabled ? "ON" : "OFF");
    return;
  }
  if (strncmp(line, "RAW ", 4) == 0) {
    sendRaw(line + 4);
    Serial.printf("TX(raw) %s\n", line + 4);
    return;
  }
  sendFace(line);
}

void pollButton(uint32_t nowMs) {
  const bool raw = digitalRead(BMO_PIN_BUTTON);
  if (raw != lastButtonRaw) {
    lastButtonRaw = raw;
    buttonChangedAtMs = nowMs;
  }
  if ((nowMs - buttonChangedAtMs) >= kDebounceMs && raw != buttonStable) {
    buttonStable = raw;
    if (buttonStable == LOW) {
      faceIndex = (faceIndex + 1) % kFaceCount;
      sendFace(kFaces[faceIndex]);
    }
  }
}

}  // namespace

void setup() {
  pinMode(BMO_PIN_BUTTON, INPUT_PULLUP);
  Serial.begin(115200);
  uartLink.begin(BMO_LINK_BAUD, SERIAL_8N1, BMO_PIN_LINK_RX, BMO_PIN_LINK_TX);
  delay(200);
  Serial.println();
  Serial.println("BMO Phase 2 audio-board link sender");
  Serial.println("BOOT button: next face. USB console: <FACE>, DEMO ON|OFF, RAW <text>");
}

void loop() {
  pollStream(uartLink, linkBuf, linkLen, handleLinkLine);
  pollStream(Serial, usbBuf, usbLen, handleUsbLine);
  // Sampled after polling: handlers stamp times with millis(), and an older
  // nowMs would make the unsigned elapsed-time math underflow.
  const uint32_t nowMs = millis();
  pollButton(nowMs);

  if ((nowMs - lastPingAtMs) >= kHeartbeatIntervalMs) {
    lastPingAtMs = nowMs;
    sendRaw("PING");
  }

  if (hasPending && (nowMs - lastSendAtMs) >= kAckTimeoutMs) {
    if (attempts >= kMaxRetries) {
      Serial.printf("GIVE UP %s (no ACK)\n", pending);
      hasPending = false;
    } else {
      attempts++;
      lastSendAtMs = nowMs;
      sendRaw(pending);
      Serial.printf("RETRY %u %s\n", attempts, pending);
    }
  }

  if (linkUp && (nowMs - lastRxAtMs) > kLinkTimeoutMs) {
    linkUp = false;
    hasPending = false;
    Serial.println("LINK LOST");
  }

  if (demoEnabled && (nowMs - lastDemoAtMs) >= kDemoIntervalMs) {
    lastDemoAtMs = nowMs;
    faceIndex = (faceIndex + 1) % kFaceCount;
    sendFace(kFaces[faceIndex]);
  }
}
