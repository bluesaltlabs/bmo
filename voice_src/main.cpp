// Phase 3 voice bring-up for the audio ESP32-WROOM: type a line on the USB serial console and BMO says it
// through an I2S DAC/amp. Uses the same bmo_voice library as the host demo (voice_demo/).
#include <Arduino.h>
#include <ESP_I2S.h>

#include "bmo_voice.h"

#ifndef BMO_PIN_I2S_BCLK
#define BMO_PIN_I2S_BCLK 26
#endif
#ifndef BMO_PIN_I2S_LRC
#define BMO_PIN_I2S_LRC 25
#endif
#ifndef BMO_PIN_I2S_DOUT
#define BMO_PIN_I2S_DOUT 22
#endif

static const uint32_t kRate = 16000;
static I2SClass i2s;
static bmo::Voice voice;

static void say(const char* text) {
  Serial.printf("BMO: %s\n", text);
  voice.speak(text);
  int16_t buf[256];
  while (true) {
    size_t n = voice.render(buf, 256);
    if (n) i2s.write(reinterpret_cast<uint8_t*>(buf), n * sizeof(int16_t));
    // voice.mouth() (0..255) is ready to drive the face's talking animation.
    if (n < 256) break;
  }
}

void setup() {
  Serial.begin(115200);
  i2s.setPins(BMO_PIN_I2S_BCLK, BMO_PIN_I2S_LRC, BMO_PIN_I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S init failed");
  }
  voice.begin(kRate);
  say("Hello! I am BMO.");
  Serial.println("Type a line and press enter.");
}

void loop() {
  static char line[bmo::Voice::kMaxText + 1];
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) {
        line[len] = 0;
        say(line);
        len = 0;
      }
    } else if (len < bmo::Voice::kMaxText) {
      line[len++] = c;
    }
  }
}
