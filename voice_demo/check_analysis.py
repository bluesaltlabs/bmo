#!/usr/bin/env python3
"""Check measurement, prosody and noise balance after building voice_demo/run.sh.

BMO_CHECK_BINARY can select another executable for an isolated evaluation.
"""
import subprocess
import os
import unittest

import numpy as np

from evaluate import ROOT, read_audio, pitch_track


class AnalysisChecks(unittest.TestCase):
    def test_known_pitches(self):
        t = np.arange(16000) / 16000
        for frequency in (120, 190, 260, 440, 600):
            # Include harmonics: the tracker should find f0, not simply the
            # largest spectral peak. The second harmonic is deliberately louder.
            samples = 0.12 * np.sin(2 * np.pi * frequency * t)
            samples += 0.2 * np.sin(4 * np.pi * frequency * t)
            pitches = pitch_track(samples)
            self.assertGreater(len(pitches), 80)
            self.assertAlmostEqual(float(np.median(pitches[:, 1])) / frequency, 1, delta=0.01)

    def test_silence(self):
        self.assertEqual(len(pitch_track(np.zeros(16000))), 0)

    def samples(self, text, *options):
        binary = ROOT / os.environ.get("BMO_CHECK_BINARY", ".tmp/voice/bmo_say")
        output = ROOT / ".tmp/voice/prosody_check.wav"
        subprocess.run([str(binary), *options, "--wav", str(output), text], check=True, capture_output=True, cwd=ROOT)
        return read_audio(output)

    def synth(self, text, *options):
        return pitch_track(self.samples(text, *options))

    def test_sibilants_do_not_overpower_vowels(self):
        # This small, fixed corpus caught the reported imbalance: the old /s/
        # and /sh/ renders were over 7 dB louder than these vowel renders.
        # It is a regression bound, not a universal rule about natural speech.
        def rms(text):
            return np.sqrt(np.mean(self.samples(text) ** 2))
        vowel = max(rms("aaa"), rms("eee"))
        for text in ("sss", "sh", "fff"):
            self.assertLess(rms(text), vowel * 1.8)

    def test_vowel_brightness_is_preserved(self):
        binary = ROOT / os.environ.get("BMO_CHECK_BINARY", ".tmp/voice/bmo_say")
        def spectrum(lowpass):
            output = ROOT / ".tmp/voice/brightness_check.wav"
            subprocess.run([str(binary), "--breath", "0", "--frication", "0", "--lowpass", str(lowpass), "--wav", str(output), "ee"], check=True, capture_output=True, cwd=ROOT)
            samples = read_audio(output)
            power = abs(np.fft.rfft(samples * np.hanning(len(samples)))) ** 2
            hz = np.fft.rfftfreq(len(samples), 1 / 16000)
            return power[(hz > 2000) & (hz < 7000)].sum()
        self.assertGreater(spectrum(0), spectrum(4200) * 1.2)

    def test_yes_no_question_rises(self):
        statement = self.synth("You are here.")
        question = self.synth("You are here?")
        def ending(track):
            return np.median(track[track[:, 0] > track[-1, 0] - 0.15, 1])
        self.assertGreater(ending(question), ending(statement) * 1.2)

    def test_exclamation_has_larger_accent(self):
        neutral = self.synth("Hello.")
        emphatic = self.synth("Hello!")
        self.assertGreater(np.percentile(emphatic[:, 1], 90), np.percentile(neutral[:, 1], 90) * 1.2)

    def test_monotone_setting(self):
        track = self.synth("Hello. I am BMO.", "--inflect", "0", "--rough", "0", "--vibrato", "0")
        self.assertAlmostEqual(float(np.median(track[:, 1])), 260, delta=3)
        self.assertLess(float(np.percentile(track[:, 1], 90) - np.percentile(track[:, 1], 10)), 5)


if __name__ == "__main__":
    unittest.main()
