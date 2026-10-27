"""Real-model integration checks. Run after voice_demo/setup_piper.sh.

    .venv-piper/bin/python -m unittest speech.check -v
"""
import math
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest
from unittest import mock
import wave

import numpy as np

from . import ROOT
from .backend import PiperEngine, voice_path
from .delivery import DELIVERY, join_sentences
from .text import spoken_text
from .emotions import emotion_ids, emotion_speaker


class PiperChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # ONNX telemetry is disabled by speech.__init__ before import. Also
        # prohibit Python networking throughout model loading/phonemization.
        cls.enterClassContext(mock.patch.object(socket, "create_connection", side_effect=AssertionError("runtime attempted networking")))
        cls.enterClassContext(mock.patch.object(socket, "getaddrinfo", side_effect=AssertionError("runtime attempted DNS")))
        cls.enterClassContext(mock.patch.object(socket.socket, "connect", side_effect=AssertionError("runtime attempted networking")))
        cls.engine = PiperEngine()
        cls.audio = cls.engine.synthesize("Hello! I am BMO. Do you want to play a game with me?")

    def test_real_cpu_model_offline(self):
        self.assertEqual(os.environ["ORT_DISABLE_TELEMETRY"], "1")
        self.assertEqual(self.engine.voice.session.get_providers(), ["CPUExecutionProvider"])
        self.assertEqual(self.audio.sample_rate, self.engine.sample_rate)
        self.assertTrue(np.isfinite(self.audio.samples).all())
        self.assertGreater(len(self.audio.samples), self.audio.sample_rate)
        self.assertGreater(float(np.sqrt(np.mean(self.audio.samples ** 2))), 0.001)
        self.assertLessEqual(float(abs(self.audio.samples).max()), 0.851)

    def test_bmo_is_pronounced_beemo(self):
        expected = self.engine.voice.phonemize("Hello! I am Beemo.")
        actual = self.engine.voice.phonemize(spoken_text("Hello! I am BMO."))
        self.assertEqual(actual, expected)
        self.assertNotEqual(actual, self.engine.voice.phonemize("Hello! I am B M O."))
        with mock.patch.object(self.engine.voice, "synthesize", wraps=self.engine.voice.synthesize) as synth:
            self.engine.synthesize("BMO is here.")
            self.assertEqual(synth.call_args.args[0], "Beemo is here.")

    def test_resampling_preserves_duration(self):
        for rate in (8000, 16000, 44100, 48000):
            result = self.audio.resampled(rate)
            self.assertEqual(result.sample_rate, rate)
            self.assertEqual(len(result.samples), math.ceil(len(self.audio.samples) * rate / self.audio.sample_rate))
            self.assertTrue(np.isfinite(result.samples).all())

    def test_wav_and_raw_format(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp", prefix="piper-check-") as temporary:
            output = Path(temporary) / "audio.wav"
            audio = self.audio.resampled(16000)
            audio.write_wav(output)
            with wave.open(str(output), "rb") as wav:
                self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (1, 2, 16000))
                self.assertEqual(wav.readframes(wav.getnframes()), audio.pcm16())
            self.assertEqual(len(audio.pcm16()), len(audio.samples) * 2)

    def test_silence_and_empty_input(self):
        self.assertEqual(len(self.engine.synthesize(" \n").samples), 0)
        silent = self.engine.synthesize("Please wait.", volume=0)
        self.assertGreater(len(silent.samples), 0)
        self.assertTrue((silent.samples == 0).all())

    def test_bad_settings_fail(self):
        for speed in (float("nan"), float("inf"), 0, 3):
            with self.assertRaises(ValueError):
                self.engine.synthesize("Hello", speed=speed)
        for volume in (-1, float("nan"), 2):
            with self.assertRaises(ValueError):
                self.engine.synthesize("Hello", volume=volume)
        with self.assertRaises(ValueError):
            self.engine.synthesize("Hello", speaker=self.engine.voice.config.num_speakers)
        with self.assertRaises(ValueError):
            self.engine.synthesize("Hello", rate=0)
        with self.assertRaises(ValueError):
            self.engine.synthesize("Hello", style="pretend-emotion")
        with self.assertRaisesRegex(ValueError, "no trained 'excited' emotion"):
            self.engine.synthesize("Hello!", emotion="excited")
        with self.assertRaisesRegex(ValueError, "not both"):
            self.engine.synthesize("Hello!", speaker=0, emotion="neutral")

    def test_delivery_presets_produce_valid_speech(self):
        for style in DELIVERY:
            audio = self.engine.synthesize("Hello! I am BMO. You did it!", style=style, rate=16000)
            self.assertEqual(audio.sample_rate, 16000)
            self.assertGreater(len(audio.samples), 16000)
            self.assertTrue(np.isfinite(audio.samples).all())
            self.assertGreater(float(abs(audio.samples).max()), 0.01)
            self.assertLessEqual(float(abs(audio.samples).max()), 1)

    @unittest.skipUnless(voice_path("en_GB-jenny_dioco-medium").is_file(), "optional Jenny (Dioco) comparison voice is not installed")
    def test_alternative_voice_offline(self):
        engine = PiperEngine(voice_path("en_GB-jenny_dioco-medium"))
        audio = engine.synthesize("Hello! Do you want to play a game?", style="lively")
        self.assertGreater(len(audio.samples), engine.sample_rate)
        self.assertTrue(np.isfinite(audio.samples).all())

    def test_missing_model_does_not_download(self):
        with self.assertRaises(FileNotFoundError):
            PiperEngine(ROOT / ".tmp/missing-piper-voice.onnx")
        with self.assertRaises(ValueError):
            voice_path("../../outside-repo")

    def test_cli_selects_piper_and_exports_pcm(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp", prefix="piper-cli-") as temporary:
            target = Path(temporary) / "voice.pcm"
            result = subprocess.run([str(ROOT / "voice_demo/run.sh"), "--style", "lively", "--raw", str(target), "Hello! I am BMO."], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("BMO (Piper):", result.stdout)
            self.assertIn("16000 Hz", result.stdout)
            self.assertIn("lively delivery", result.stderr)
            self.assertGreater(target.stat().st_size, 16000)
            self.assertEqual(target.stat().st_size % 2, 0)

    def test_explicit_formant_backup(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp", prefix="formant-cli-") as temporary:
            target = Path(temporary) / "voice.wav"
            result = subprocess.run([str(ROOT / "voice_demo/run.sh"), "--engine", "formant", "--wav", str(target), "Hello! I am BMO."], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            with wave.open(str(target), "rb") as wav:
                self.assertEqual(wav.getframerate(), 16000)
                self.assertGreater(wav.getnframes(), 16000)

    def test_default_demo_without_arguments(self):
        # Exercise the exact macOS Bash 3.2 entry point; suppress only the
        # speaker player so automated tests do not play audio on the desk.
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp", prefix="piper-player-") as temporary:
            player = Path(temporary) / ("afplay" if os.uname().sysname == "Darwin" else "aplay")
            player.write_text("#!/bin/sh\nexit 0\n")
            player.chmod(0o755)
            env = dict(os.environ, PATH=temporary + os.pathsep + os.environ["PATH"])
            result = subprocess.run([str(ROOT / "voice_demo/run.sh")], cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.count("BMO (Piper):"), 5)

    def test_interactive_reuses_loaded_model(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp", prefix="piper-interactive-") as temporary:
            target = Path(temporary) / "voice.wav"
            result = subprocess.run([str(ROOT / "voice_demo/run.sh"), "-i", "--wav", str(target)], cwd=ROOT,
                                    input="Hello BMO.\nGood morning!\n\n", capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr.count("loaded in"), 1)
            self.assertEqual(result.stdout.count("BMO (Piper):"), 2)


class DeliveryChecks(unittest.TestCase):
    def test_neutral_preserves_native_chunks_exactly(self):
        chunks = [np.array([0, 0.002, 0.2, 0.001, 0]), np.array([0, 0.03, 0.1, 0])]
        np.testing.assert_array_equal(join_sentences(chunks, 1000, 0), np.concatenate(chunks))

    def test_pause_addition_preserves_soft_endings(self):
        before = np.array([0.01, 0.25, 0.0002, 0, 0])
        after = np.array([0, 0, 0, 0.0002, 0.5, 0.02])
        result = join_sentences([before, after], 1000, 0.01)
        self.assertEqual(len(result), len(before) + len(after) + 5)
        original = np.concatenate([before, after])
        np.testing.assert_array_equal(result[result != 0], original[original != 0])

    def test_existing_long_pause_is_not_duplicated_or_trimmed(self):
        before = np.concatenate([np.array([0.1, 0.2]), np.zeros(20)])
        after = np.concatenate([np.zeros(20), np.array([0.1, 0.2])])
        np.testing.assert_array_equal(join_sentences([before, after], 1000, 0.01), np.concatenate([before, after]))


class PronunciationChecks(unittest.TestCase):
    def test_case_punctuation_and_possessive(self):
        self.assertEqual(spoken_text("BMO, bmo! Bmo's game."), "Beemo, Beemo! Beemo's game.")

    def test_unrelated_words_and_explicit_phonemes_are_preserved(self):
        text = "ABMO BMO2 _BMO [[bmo]] [[bˈiːmoʊ]]"
        self.assertEqual(spoken_text(text), text)


class EmotionMappingChecks(unittest.TestCase):
    def test_training_labels_map_to_real_speaker_conditioning(self):
        mapping = {"emotion:neutral": 0, "emotion:excited": 1, "different_speaker": 2}
        self.assertEqual(emotion_ids(mapping), {"neutral": 0, "excited": 1})
        self.assertEqual(emotion_speaker(mapping, 3, "excited"), 1)
        with self.assertRaises(ValueError):
            emotion_speaker(mapping, 3, "sad")

    def test_invalid_emotion_id_is_rejected(self):
        with self.assertRaises(ValueError):
            emotion_speaker({"emotion:excited": 7}, 2, "excited")
