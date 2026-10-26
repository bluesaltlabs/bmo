// BMO voice engine: text -> phonemes -> formant synthesis -> 16-bit mono PCM.
// No dependencies beyond libc/libm and no heap use, so the same code runs on the
// ESP32 (feed render() into I2S) and on a computer (write a WAV / play it).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace bmo {

struct VoiceConfig {
  float pitch_hz = 340.0f;      // average fundamental frequency
  float speed = 1.0f;           // 1.0 = normal, higher = faster
  float formant_scale = 1.28f;  // >1 shifts formants up (smaller, childlike vocal tract)
  float inflection = 1.25f;     // pitch movement: 0 = monotone, 1 = natural, >1 = chirpy
  float roughness = 0.12f;      // pitch jitter / raspiness (0..1)
  float breathiness = 0.025f;   // aspiration noise mixed into voiced sounds (0..1)
  float vibrato = 0.022f;       // pitch wobble depth (fraction of f0)
  uint8_t crush_bits = 0;       // quantisation depth for the retro game-console grit (0 = off)
  uint8_t hold = 1;             // sample-and-hold factor (1 = off); lowers effective bandwidth
  float volume = 0.85f;         // 0..1

  // The default BMO preset.
  static VoiceConfig bmo() { return VoiceConfig(); }
  // Clean, unprocessed output of the synthesizer (useful for comparison).
  static VoiceConfig clean() {
    VoiceConfig c;
    c.pitch_hz = 220.0f;
    c.formant_scale = 1.0f;
    c.inflection = 1.0f;
    c.roughness = 0.0f;
    c.vibrato = 0.0f;
    c.crush_bits = 0;
    c.hold = 1;
    return c;
  }
};

class Voice {
 public:
  static const size_t kMaxText = 320;

  // sample_rate: 16000 recommended (anything 8000..48000 works).
  void begin(uint32_t sample_rate, const VoiceConfig& cfg = VoiceConfig::bmo());
  void setConfig(const VoiceConfig& cfg);
  const VoiceConfig& config() const { return cfg_; }

  // Queue text to speak (copied; truncated to kMaxText). Replaces anything in progress.
  void speak(const char* text);
  void stop();
  bool speaking() const { return speaking_; }

  // Fill `out` with up to n samples. Returns the number written; fewer than n
  // (possibly 0) means the utterance finished.
  size_t render(int16_t* out, size_t n);

  // Mouth openness 0..255 for the face animation, updated by render().
  uint8_t mouth() const { return static_cast<uint8_t>(mouth_ * 255.0f); }

  uint32_t sampleRate() const { return rate_; }

 private:
  struct Item {
    uint8_t ph;
    uint8_t stress;
    uint8_t pitch;  // pitch multiplier in percent
  };
  struct Reso {
    float a = 0, b = 0, c = 0, y1 = 0, y2 = 0;
    void set(float freq, float bw, float fs);
    float run(float x) {
      float y = a * x + b * y1 + c * y2;
      y2 = y1;
      y1 = y;
      return y;
    }
  };

  bool fetchWord();
  bool nextSegment();
  void loadSegment(const void* seg, const Item& it);
  void updateCoefs();
  float noise();
  int16_t sample();

  VoiceConfig cfg_;
  uint32_t rate_ = 16000;
  bool speaking_ = false;
  bool releasing_ = false;

  char text_[kMaxText + 1];
  const char* pos_ = nullptr;
  int clauseLeft_ = 0, clauseTotal_ = 0;
  char clauseEnd_ = '.';

  Item queue_[48];
  uint8_t qHead_ = 0, qLen_ = 0;
  Item cur_;
  uint8_t curSeg_ = 0, curSegs_ = 0;
  uint32_t segLeft_ = 0;

  // Targets (t*) and smoothed state (s*).
  float tf_[3] = {500, 1500, 2500}, sf_[3] = {500, 1500, 2500};
  float tav_ = 0, sav_ = 0, tan_ = 0, san_ = 0, tnf_ = 3000, snf_ = 3000, tnbw_ = 2000, snbw_ = 2000;
  float tp_ = 1, sp_ = 1;
  bool noiseThroughTract_ = false;
  float aF_ = 0, aA_ = 0, aN_ = 0, aP_ = 0, aSoft_ = 0;

  Reso r_[5], nr_;
  float phase_ = 0, jit_ = 1, vibPhase_ = 0, vib_ = 0;
  uint32_t rng_ = 12345, ctr_ = 0, holdCtr_ = 0;
  float held_ = 0, mouth_ = 0, soft_ = 0;
};

}  // namespace bmo
