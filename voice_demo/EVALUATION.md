# BMO voice evaluation — 2026-10-26

This report covers the standalone **formant backup**. The preferred host/Pi
voice is now Piper; see [Piper setup](../speech/README.md).

The latest hiss/clarity revision is documented at the end of this report. The
pitch-calibration measurements below describe the preceding tuning pass; both
local A/B pages are preserved.

The revised engine is closer to the reference's estimated speaking register and
fixes pronunciation and phrasing defects. It remains a small formant synthesizer;
this work does not establish a faithful match to BMO's timbre, accent or acting.
No blinded listener assessment or physical speaker audition was performed.

## Reference and method

Used the reference already recorded in `DECISIONS.md`:
[BMO — If I was grown](https://www.youtube.com/watch?v=qswatNKP3YY), uploaded by
SvenStojakovich, described as a scene from *The More You Moe, The Moe You Know*.
The retrieved audio is 60.453 seconds. Three consecutive 20-second excerpts are
available on the local comparison page. Attempts to retrieve a Cartoon Network
compilation and an HBO Max introduction were unavailable; no second independent
recording was used for tuning.

The original engine was built from the unchanged `9eb88a3` worktree before edits
and preserved at `.tmp/voice/evaluation/bmo_say_before`. Both versions render the
same six sentences at 16 kHz, mono, signed 16-bit PCM. The reference and synthesized
sentences are different texts and performances, so these are directional acoustic
diagnostics, not a matched-utterance similarity score.

`evaluate.py` measures original samples and creates separate audition copies
with a common active-RMS target and peak headroom. This reduces loudness bias;
it does not perform calibrated perceptual loudness matching. Its pitch tracker
uses the YIN difference function and cumulative mean normalization, a 40 ms
comparison window, 10 ms hop, 100–700 Hz range, an energy gate and 0.15 acceptance
threshold, with parabolic interpolation. Method reference:
[de Cheveigné & Kawahara (2002)](https://doi.org/10.1121/1.1458024).

The reference has 1,069 accepted pitch frames (roughly 10.7 seconds of frame
positions), with estimated 10th/50th/90th percentiles **188 / 246 / 448 Hz**.
Background audio, aperiodic speech, low-energy sounds and octave errors limit
these estimates. These numbers are not universal pitch targets for the character.

## Results

Estimated median voiced pitch, rounded to Hz:

| Synthesized sentence | Original | Revised |
|---|---:|---:|
| If I was grown, I would know everything. | 364 | 276 |
| Hello! I am BMO. | 329 | 246 |
| Who wants to play video games? | 368 | 261 |
| I am still waking up. Please wait. | 350 | 269 |
| Oh no! Something is not working. Can you help me fix it? | 367 | 289 |
| Good morning! You have 3 new messages and 12 things to do today. | 348 | 269 |

All twelve synthesized renders have zero samples at the PCM rails. The main
test phrase changes from 364 Hz median and 304–401 Hz 10th–90th percentile range
to 276 Hz and 207–337 Hz. The relative range widens from about 4.8 to 8.5
semitones, while the reference remains wider at about 15 semitones. Matching this
distribution exactly across different sentences would not demonstrate a voice match.

## Changes

- Lowered the preset register from 340 to 260 Hz and vibrato from 0.022 to 0.006.
  The base setting is not the resulting median: syllable accents and clause
  contours move pitch above and below it.
- Replaced flat per-phoneme stress steps with continuous vowel accents, shared
  across both parts of a diphthong. Exclamations get stronger accents. Ordinary
  wh questions fall; yes/no questions rise. Final vowels receive extra duration.
- Added a smooth glottal closing phase and a closed interval, removing the
  excitation discontinuity at the period boundary. This is a synthesis design
  change, not a measured reconstruction of the actor's glottal waveform.
- Fixed explicit stress markers before consonants: `BMO`, `hello`, `okay` and
  other dictionary words previously lost their stress. Added an unstressed
  schwa, common phrase pronunciations, teen numbers and contractions. Preserved
  the distinction between `I'll`/`ill` and `we're`/`were`.
- Made `/h/` anticipate the following vowel. Reset synthesis state between
  utterances so output is reproducible across playback history and block size.
- Bounded invalid configuration values and mouth openness; retained the shared
  C++ implementation with no heap, IO or network dependency.

## Listen and reproduce

Open `.tmp/voice/evaluation/index.html` in a browser for references and A/B players.
Raw measurements, reference provenance and its SHA-256 are in the adjacent
`metrics.json`. The downloaded show recording and all generated audio are kept
in the gitignored `.tmp/` directory, not embedded in firmware.

Build the current host demo and write a sample:

```sh
voice_demo/run.sh --engine formant --wav .tmp/voice/current.wav "Hello! I am BMO."
```

The analysis tools alone need numpy/scipy in the repo-local venv; the engine and
normal host demo do not. With the preserved baseline binary and local reference:

```sh
export TMPDIR="$PWD/.tmp"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Ilib/bmo_voice/src \
  voice_demo/bmo_say.cpp lib/bmo_voice/src/bmo_voice.cpp \
  lib/bmo_voice/src/bmo_g2p.cpp -o .tmp/voice/evaluation/bmo_say_after
.venv/bin/python voice_demo/evaluate.py \
  --reference .tmp/voice/evaluation/reference_monologue.wav \
  --before .tmp/voice/evaluation/bmo_say_before \
  --after .tmp/voice/evaluation/bmo_say_after
```

To rebuild the baseline after removing generated files, extract
`voice_demo/bmo_say.cpp` and the files under `lib/bmo_voice/src/` from revision
`9eb88a3` into a directory under `.tmp/`, then compile them using the same command
and their own include directory. The baseline executable does not contain source.

## Verification and remaining work

Passed the host C++ checks under AddressSanitizer and UndefinedBehaviorSanitizer:
known pronunciations, output independent of render block size and preceding
utterances, null/empty/punctuation-only input, stop behavior, volume-zero output,
invalid configuration handling and unclipped speech at 8/16/44.1/48 kHz.

Passed five analysis checks: known harmonic signals from 120–600 Hz (within 1%),
silence rejection, rising yes/no questions, stronger exclamation accents, and
constant pitch when inflection, jitter and vibrato are disabled. Run them with:

```sh
c++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ilib/bmo_voice/src voice_demo/check.cpp lib/bmo_voice/src/bmo_voice.cpp \
  lib/bmo_voice/src/bmo_g2p.cpp -o .tmp/voice/evaluation/check
.tmp/voice/evaluation/check
voice_demo/run.sh --engine formant --wav .tmp/voice/check.wav "Hello! I am BMO."
.venv/bin/python voice_demo/check_analysis.py
```

The pitch-pass `.venv/bin/pio run -e voice_esp32_wroom` build passes: **321,860 bytes
flash, 24,756 bytes static RAM** for the bring-up firmware. The host `Voice`
object is 896 bytes, plus the shared 1 KiB excitation table. No device was flashed;
real-time performance, DAC output and enclosure/speaker response remain untested.

The main fidelity gap is now the handcrafted source, vocal-tract model, English
rules and prosody. Those cannot establish the identity and delivery of the
original performance through pitch tuning alone. The existing flash-clip design
can reproduce a recording for a fixed phrase; arbitrary speech with the actor's
timbre requires a different speech source/model. The current runtime architecture
and phase boundaries are preserved.

## Hiss and clarity follow-up

The user found the pitch pass better, but reported hiss on some syllables and
loss of clarity. The prior pass's pitch statistics were insufficient to identify
that problem. This revision addresses noise balance and preserves the approved
pitch/prosody settings.

The old frication path used the vocal resonator topology on noise and mixed it
at high gain. Its shared envelope was also abruptly routed between frication
and aspiration, allowing the preceding sound's noise tail to enter a different
path. The 4.2 kHz whole-voice low-pass also attenuated vowel harmonics.

Changes:

- Replaced the frication filter with a constant-peak band-pass with zeros at DC
  and Nyquist, using the [W3C/RBJ cookbook](https://www.w3.org/TR/audio-eq-cookbook/).
  The phoneme-specific frequency and bandwidth targets remain distinct for
  `s`, `sh`, `f`, and the other consonants.
- Separated aspiration and frication envelopes; added short smooth onset/offset
  ramps to the noise targets. This retains `/h/` and consonant identity while
  preventing instantaneous noise rerouting.
- Set frication gain to 0.30; reduced voiced breathiness to 0.012 and made its
  noise level follow the glottal cycle. Pitch and formant targets are unchanged.
- Bypassed the whole-voice low-pass by default. Added `--frication` and
  `--lowpass` controls; `--lowpass 4200` enables the previous cutoff for comparison.

Identical isolated test strings at the same output volume produce these raw
RMS levels, measured over complete WAVs including the same trailing silence:

| Test string | Previous dBFS | Current dBFS | Change |
|---|---:|---:|---:|
| `sss` | -19.14 | -28.91 | -9.77 dB |
| `sh` | -16.97 | -24.55 | -7.58 dB |
| `fff` | -22.00 | -30.36 | -8.36 dB |
| `aaa` | -26.70 | -26.62 | +0.08 dB |
| `eee` | -25.54 | -25.52 | +0.02 dB |
| `hhh` | -34.40 | -33.84 | +0.56 dB |

The stress sentence “She sees six fish. This is something special.” drops from
-19.01 to -24.68 dBFS, while “Hello! I am BMO.” stays within 0.1 dB of its
previous RMS level. This demonstrates targeted noise reduction rather than
simply turning down the entire voice. These diagnostics do not establish a
perceptual naturalness score.

Open `.tmp/voice/noise_evaluation/index.html` for the new A/B comparison: the
version the user heard versus the current engine, using all five built-in demo
phrases plus two sentences rich in sibilants. The original show excerpts remain
available there. All fourteen full-sentence renders have zero rail samples.
The previous executable is preserved at
`.tmp/voice/noise_evaluation/bmo_say_before`; the final executable is adjacent.

The C++ checks pass under AddressSanitizer/UndefinedBehaviorSanitizer, including
a new test that disabling frication suppresses `/s/` without suppressing `/h/`.
Seven Python checks pass, including sibilant/vowel balance and preserved vowel
harmonics with the output low-pass bypassed. Build the demo and run the checks:

```sh
voice_demo/run.sh --engine formant --wav .tmp/voice/check.wav "Hello! I am BMO."
.venv/bin/python voice_demo/check_analysis.py
```

The current ESP32-WROOM build passes: **322,564 bytes flash, 24,772 bytes static
RAM**; the host `Voice` object is 912 bytes. No hardware was flashed or auditioned.

Piper has since been installed as the preferred host/Pi voice, while this compact
engine remains available for standalone behavior. See [the new speech backend](../speech/README.md).
A suitable neural voice model is still needed to match BMO's timbre and accent;
the stock Piper voice is a natural-speech starting point.
