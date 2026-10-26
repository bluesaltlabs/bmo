#include "bmo_voice.h"

#include <math.h>
#include <string.h>

#include "bmo_phon.h"

namespace bmo {

namespace {

const float kPi = 3.14159265f;
const int kTable = 256;
float gDeriv[kTable];  // glottal flow derivative over one pitch period, peak-normalised
bool gTableReady = false;

void buildTable() {
  if (gTableReady) return;
  const float openFrac = 0.58f;
  float g[kTable + 1];
  for (int i = 0; i <= kTable; ++i) {
    float p = static_cast<float>(i) / kTable;
    g[i] = p < openFrac ? 0.5f * (1.0f - cosf(kPi * p / openFrac))
                        : cosf(0.5f * kPi * (p - openFrac) / (1.0f - openFrac));
  }
  float peak = 0;
  for (int i = 0; i < kTable; ++i) {
    gDeriv[i] = g[i + 1] - g[i];
    if (fabsf(gDeriv[i]) > peak) peak = fabsf(gDeriv[i]);
  }
  for (int i = 0; i < kTable; ++i) gDeriv[i] /= peak;
  gTableReady = true;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float smoothCoef(float tauMs, float fs) { return 1.0f - expf(-1000.0f / (tauMs * fs)); }

}  // namespace

void Voice::Reso::set(float freq, float bw, float fs) {
  freq = clampf(freq, 50.0f, fs * 0.45f);
  float T = 1.0f / fs;
  float radius = expf(-kPi * bw * T);
  float theta = 2.0f * kPi * freq * T;
  c = -radius * radius;
  b = 2.0f * radius * cosf(theta);
  float re = 1.0f - b * cosf(theta) - c * cosf(2.0f * theta);
  float im = b * sinf(theta) + c * sinf(2.0f * theta);
  a = sqrtf(re * re + im * im);
}

void Voice::begin(uint32_t sample_rate, const VoiceConfig& cfg) {
  buildTable();
  rate_ = sample_rate;
  text_[0] = 0;
  setConfig(cfg);
  stop();
}

void Voice::setConfig(const VoiceConfig& cfg) {
  cfg_ = cfg;
  const float fs = static_cast<float>(rate_);
  const float sp = cfg_.speed > 0.2f ? cfg_.speed : 0.2f;
  aF_ = smoothCoef(22.0f / sp, fs);
  aA_ = smoothCoef(7.0f, fs);
  aN_ = smoothCoef(2.0f, fs);
  aP_ = smoothCoef(22.0f / sp, fs);
  float cutoff = fs < 12000.0f ? fs * 0.35f : 4200.0f;
  aSoft_ = 1.0f - expf(-2.0f * kPi * cutoff / fs);
}

void Voice::stop() {
  speaking_ = false;
  releasing_ = false;
  pos_ = nullptr;
  qHead_ = qLen_ = 0;
  curSeg_ = curSegs_ = 0;
  segLeft_ = 0;
  tav_ = tan_ = 0;
  clauseLeft_ = 0;
}

void Voice::speak(const char* text) {
  stop();
  size_t n = text ? strlen(text) : 0;
  if (n > kMaxText) n = kMaxText;
  memcpy(text_, text, n);
  text_[n] = 0;
  pos_ = text_;
  speaking_ = true;
  releasing_ = false;
  for (auto& r : r_) r.y1 = r.y2 = 0;
  nr_.y1 = nr_.y2 = 0;
  held_ = 0;
  soft_ = 0;
}

float Voice::noise() {
  rng_ = rng_ * 1664525u + 1013904223u;
  return static_cast<float>(static_cast<int32_t>(rng_)) * (1.0f / 2147483648.0f);
}

namespace {
bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isClauseEnd(char c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':'; }
}  // namespace

// Pull the next word (or number, or punctuation pause) from the text into the phoneme queue.
bool Voice::fetchWord() {
  while (pos_ && *pos_) {
    char c = *pos_;
    if (isClauseEnd(c)) {
      while (isClauseEnd(*pos_)) {
        if (*pos_ != ',' && *pos_ != ';' && *pos_ != ':') c = *pos_;
        ++pos_;
      }
      queue_[qLen_++] = Item{static_cast<uint8_t>((c == '.' || c == '!' || c == '?') ? PAUSE_STOP : PAUSE_COMMA), 0, 100};
      qHead_ = 0;
      clauseLeft_ = 0;
      return true;
    }
    if (!isLetter(c) && !isDigit(c)) {
      ++pos_;
      continue;
    }

    // Start of a clause: count its words and note how it ends, for the pitch contour.
    if (clauseLeft_ == 0) {
      int words = 0;
      bool in = false;
      const char* p = pos_;
      clauseEnd_ = '.';
      for (; *p; ++p) {
        if (isClauseEnd(*p)) {
          clauseEnd_ = *p;
          break;
        }
        bool wc = isLetter(*p) || isDigit(*p) || *p == '\'';
        if (wc && !in) ++words;
        in = wc;
      }
      clauseTotal_ = clauseLeft_ = words > 0 ? words : 1;
    }

    const char* start = pos_;
    uint8_t ph[40];
    size_t np = 0;
    if (isDigit(c)) {
      while (isDigit(*pos_)) ++pos_;
      char words[96];
      size_t wl = numberToWords(start, pos_ - start, words, sizeof(words) - 1);
      size_t i = 0;
      while (i < wl && np < sizeof(ph) - 8) {
        size_t j = i;
        while (j < wl && words[j] != ' ') ++j;
        np += wordToPhonemes(words + i, j - i, ph + np, sizeof(ph) - np);
        i = j + 1;
      }
    } else {
      while (isLetter(*pos_) || *pos_ == '\'') ++pos_;
      np = wordToPhonemes(start, pos_ - start, ph, sizeof(ph));
    }

    const bool lastInClause = (clauseLeft_ == 1);
    const float pos01 = clauseTotal_ > 1 ? 1.0f - static_cast<float>(clauseLeft_ - 1) / (clauseTotal_ - 1) : 1.0f;
    --clauseLeft_;

    // Declination across the clause plus a little random bounce per word.
    float     base = 1.0f + 0.07f * cfg_.inflection * (0.5f - pos01) * 2.0f;
    base += 0.015f * cfg_.inflection * noise();
    int lastVowel = -1;
    for (size_t i = 0; i < np; ++i)
      if (kPhones[ph[i] & 0x7F].vowel) lastVowel = static_cast<int>(i);

    qHead_ = 0;
    qLen_ = 0;
    for (size_t i = 0; i < np && qLen_ < 46; ++i) {
      bool stressed = ph[i] & 0x80;
      float pm = base;
      if (stressed) pm += 0.10f * cfg_.inflection;
      if (lastInClause && static_cast<int>(i) >= lastVowel && lastVowel >= 0) {
        if (clauseEnd_ == '?') pm += 0.22f * cfg_.inflection;
        else if (clauseEnd_ == '!') pm += 0.08f * cfg_.inflection;
        else if (clauseEnd_ == ',') pm += 0.04f * cfg_.inflection;
        else pm -= 0.12f * cfg_.inflection;
      }
      pm = clampf(pm, 0.5f, 2.0f);
      queue_[qLen_++] = Item{static_cast<uint8_t>(ph[i] & 0x7F), static_cast<uint8_t>(stressed), static_cast<uint8_t>(pm * 100.0f)};
    }
    if (qLen_ > 0) return true;
  }
  return false;
}

void Voice::loadSegment(const void* segp, const Item& it) {
  const Seg& s = *static_cast<const Seg*>(segp);
  const float fsc = cfg_.formant_scale;
  if (s.f1) tf_[0] = s.f1 * fsc;
  if (s.f2) tf_[1] = s.f2 * fsc;
  if (s.f3) tf_[2] = s.f3 * fsc;
  tav_ = s.av * 0.01f * (it.stress ? 1.08f : 1.0f);
  tan_ = s.an * 0.01f;
  noiseThroughTract_ = s.nf < 0;
  if (s.nf > 0) {
    tnf_ = s.nf * (1.0f + (fsc - 1.0f) * 0.5f);
    tnbw_ = s.nbw;
  }
  tp_ = it.pitch * 0.01f;
  float ms = s.ms;
  if (it.stress) ms *= 1.2f;
  const float sp = cfg_.speed > 0.2f ? cfg_.speed : 0.2f;
  segLeft_ = static_cast<uint32_t>(ms * rate_ * 0.001f / sp) + 1;
}

bool Voice::nextSegment() {
  while (true) {
    if (curSeg_ < curSegs_) {
      loadSegment(&kPhones[cur_.ph].seg[curSeg_++], cur_);
      return true;
    }
    if (qHead_ >= qLen_) {
      qHead_ = qLen_ = 0;
      if (!fetchWord()) return false;
    }
    cur_ = queue_[qHead_++];
    curSeg_ = 0;
    curSegs_ = kPhones[cur_.ph].nseg;
  }
}

void Voice::updateCoefs() {
  const float fs = static_cast<float>(rate_);
  const float fsc = cfg_.formant_scale;
  r_[0].set(sf_[0], 100.0f, fs);
  r_[1].set(sf_[1], 140.0f, fs);
  r_[2].set(sf_[2], 200.0f, fs);
  r_[3].set(3300.0f * fsc, 360.0f, fs);
  r_[4].set(4300.0f * fsc, 440.0f, fs);
  nr_.set(snf_, snbw_, fs);
  vibPhase_ += 16.0f * 5.5f / fs;
  if (vibPhase_ > 1.0f) vibPhase_ -= 1.0f;
  vib_ = cfg_.vibrato * sinf(2.0f * kPi * vibPhase_);
}

int16_t Voice::sample() {
  const float fs = static_cast<float>(rate_);
  for (int i = 0; i < 3; ++i) sf_[i] += (tf_[i] - sf_[i]) * aF_;
  sav_ += (tav_ - sav_) * aA_;
  san_ += (tan_ - san_) * aN_;
  snf_ += (tnf_ - snf_) * aN_;
  snbw_ += (tnbw_ - snbw_) * aN_;
  sp_ += (tp_ - sp_) * aP_;
  if ((ctr_++ & 15) == 0) updateCoefs();

  float f0 = cfg_.pitch_hz * sp_ * (1.0f + vib_) * jit_;
  phase_ += f0 / fs;
  if (phase_ >= 1.0f) {
    phase_ -= 1.0f;
    float targetJitter = 1.0f + cfg_.roughness * 0.02f * noise();
    jit_ += (targetJitter - jit_) * 0.2f;
  }
  float tablePos = phase_ * kTable;
  int tableIndex = static_cast<int>(tablePos) & (kTable - 1);
  float tableFrac = tablePos - static_cast<int>(tablePos);
  float glottal = gDeriv[tableIndex] + (gDeriv[(tableIndex + 1) & (kTable - 1)] - gDeriv[tableIndex]) * tableFrac;
  float src = glottal * sav_;
  float asp = noise() * (cfg_.breathiness * sav_ + (noiseThroughTract_ ? san_ * 0.5f : 0.0f));

  float tract = src * 0.55f + asp;
  float y = r_[0].run(tract)
          + 0.72f * r_[1].run(tract)
          + 0.48f * r_[2].run(tract)
          + 0.11f * r_[3].run(tract)
          + 0.04f * r_[4].run(tract);
  y *= 0.65f;

  float fric = noiseThroughTract_ ? 0.0f : nr_.run(noise() * san_) * 0.65f;
  float out = (y + fric) * cfg_.volume;
  soft_ += (out - soft_) * aSoft_;
  out = soft_;

  // Retro console grit: sample-and-hold decimation and bit reduction.
  if (cfg_.hold > 1) {
    if (holdCtr_++ % cfg_.hold == 0) held_ = out;
    out = held_;
  }
  if (cfg_.crush_bits > 0) {
    float levels = static_cast<float>(1u << (cfg_.crush_bits - 1));
    out = floorf(out * levels + 0.5f) / levels;
  }
  // Soft clip.
  float a = fabsf(out);
  out = out / (1.0f + 0.35f * a * a);

  float open = clampf((sf_[0] / cfg_.formant_scale - 250.0f) / 500.0f, 0.2f, 1.0f);
  float mt = sav_ * open;
  float mf = san_ * 0.3f;
  mouth_ += ((mt > mf ? mt : mf) - mouth_) * 0.004f * 4.0f;

  int v = static_cast<int>(out * 52000.0f);
  return static_cast<int16_t>(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}

size_t Voice::render(int16_t* out, size_t n) {
  size_t i = 0;
  if (!speaking_) return 0;
  for (; i < n; ++i) {
    if (segLeft_ == 0) {
      if (releasing_) {
        speaking_ = false;
        break;
      }
      if (!nextSegment()) {
        releasing_ = true;
        segLeft_ = rate_ / 40;
        tav_ = tan_ = 0;
      }
    }
    --segLeft_;
    out[i] = sample();
  }
  return i;
}

}  // namespace bmo
