// Internal: phoneme set, acoustic targets and letter-to-sound interface.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace bmo {

enum Ph : uint8_t {
  IY, IH, EH, AE, AA, AO, UH, UW, AH, ER, AY, EY, OW, OY, AW,
  W, Y, L, R, M, N, NG,
  F, V, S, Z, SH, ZH, TH, DH, HH,
  P, B, T, D, K, G, CH, JH,
  PAUSE_COMMA, PAUSE_STOP,
  PH_COUNT
};

// One acoustic target. A formant of 0 means "keep the current value".
// av/an are voicing/noise amplitude in percent; nf<0 routes noise through the vocal tract (aspiration).
struct Seg {
  int16_t f1, f2, f3;
  uint16_t ms;
  uint8_t av, an;
  int16_t nf;
  uint16_t nbw;
};

struct PhDef {
  const char* name;
  uint8_t nseg;
  bool vowel;
  Seg seg[3];
};

extern const PhDef kPhones[PH_COUNT];

// Convert one word (letters/apostrophes, any case) to phonemes. Bit 7 of an entry marks the stressed vowel.
// Returns the number of entries written (<= max).
size_t wordToPhonemes(const char* word, size_t len, uint8_t* out, size_t max);

// Spell out a run of digits as English words (e.g. "42" -> "forty two"). Returns length written.
size_t numberToWords(const char* digits, size_t len, char* out, size_t max);

}  // namespace bmo
