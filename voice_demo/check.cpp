// Host regression checks for pronunciation, streaming and standalone behavior.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "bmo_phon.h"
#include "bmo_voice.h"

static std::vector<int16_t> render(bmo::Voice& voice, const char* text, size_t block) {
  voice.speak(text);
  std::vector<int16_t> pcm, buffer(block);
  while (voice.speaking()) {
    size_t n = voice.render(buffer.data(), buffer.size());
    assert(n <= buffer.size());
    pcm.insert(pcm.end(), buffer.begin(), buffer.begin() + n);
    assert(pcm.size() < voice.sampleRate() * 120u);
  }
  assert(voice.render(buffer.data(), buffer.size()) == 0);
  return pcm;
}

static void pronunciation(const char* word, const char* expected, bmo::Ph stress) {
  uint8_t phones[40];
  size_t n = bmo::wordToPhonemes(word, strlen(word), phones, sizeof(phones));
  char actual[160] = {};
  int stressed = -1;
  for (size_t i = 0; i < n; ++i) {
    if (i) strcat(actual, " ");
    strcat(actual, bmo::kPhones[phones[i] & 0x7f].name);
    if (phones[i] & 0x80) {
      assert(stressed == -1);
      stressed = phones[i] & 0x7f;
    }
  }
  assert(strcmp(actual, expected) == 0);
  assert(stressed == stress);
}

int main() {
  pronunciation("BMO", "B IY M OW", bmo::IY);
  pronunciation("hello", "HH AX L OW", bmo::OW);
  pronunciation("okay", "OW K EY", bmo::EY);
  pronunciation("today", "T AX D EY", bmo::EY);
  pronunciation("I'm", "AY M", bmo::AY);
  pronunciation("I'll", "AY L", bmo::AY);
  pronunciation("ill", "IH L", bmo::IH);
  pronunciation("we're", "W IY R", bmo::IY);
  pronunciation("grown", "G R OW N", bmo::OW);

  bmo::Voice voice;
  voice.begin(16000);
  const char* text = "Hello! I am BMO. Do you want to play a game?";
  const auto expected = render(voice, text, 256);
  assert(expected.size() > 16000);
  assert(std::any_of(expected.begin(), expected.end(), [](int16_t s) { return s != 0; }));
  render(voice, "A completely different vowel and question?", 111);
  assert(render(voice, text, 1) == expected);
  assert(render(voice, text, 257) == expected);
  voice.stop();
  assert(!voice.speaking() && voice.mouth() == 0);
  for (const char* empty : {static_cast<const char*>(nullptr), "", "...?!"}) {
    const auto silence = render(voice, empty, 256);
    assert(std::all_of(silence.begin(), silence.end(), [](int16_t s) { return s == 0; }));
  }

  // De-essing must leave aspiration and vowels available. Independent sources
  // prevent the old fricative/aspiration route switch from reusing a loud tail.
  auto noFrication = bmo::VoiceConfig::bmo();
  noFrication.frication = 0;
  noFrication.breathiness = 0;
  voice.setConfig(noFrication);
  const auto consonant = render(voice, "sss", 256);
  assert(std::all_of(consonant.begin(), consonant.end(), [](int16_t s) { return s == 0; }));
  const auto aspiration = render(voice, "hhh", 256);
  assert(std::any_of(aspiration.begin(), aspiration.end(), [](int16_t s) { return s != 0; }));

  for (uint32_t rate : {8000u, 16000u, 44100u, 48000u}) {
    voice.begin(rate);
    auto samples = render(voice, "Hello! I am BMO. I'm still waking up. You have 12 messages.", 256);
    assert(std::none_of(samples.begin(), samples.end(), [](int16_t s) { return s == 32767 || s == -32768; }));
    auto config = voice.config();
    config.volume = 0;
    voice.setConfig(config);
    samples = render(voice, "Please wait.", 256);
    assert(std::all_of(samples.begin(), samples.end(), [](int16_t s) { return s == 0; }));
  }

  bmo::VoiceConfig invalid;
  invalid.pitch_hz = INFINITY;
  invalid.formant_scale = 0;
  invalid.speed = NAN;
  invalid.crush_bits = 255;
  invalid.hold = 0;
  invalid.frication = INFINITY;
  invalid.lowpass_hz = NAN;
  voice.begin(0, invalid);
  assert(voice.sampleRate() == 8000);
  assert(isfinite(voice.config().pitch_hz));
  assert(voice.config().formant_scale > 0);
  assert(voice.config().speed >= 0.2f);
  assert(isfinite(voice.config().frication));
  assert(voice.config().lowpass_hz == 0);
  render(voice, "BMO", 256);
  printf("Voice regression checks passed (Voice object: %zu bytes).\n", sizeof(voice));
}
