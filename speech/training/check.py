"""Dataset preparation checks with synthetic WAV fixtures; no voice training."""
import csv
import json
from pathlib import Path
import tempfile
import unittest
import wave

import numpy as np
from scipy.io import wavfile

from speech import ROOT
from .prepare import FIELDS, audit, prepare


class DatasetChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bmo-dataset-check-", dir=ROOT / ".tmp")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.manifest = self.directory / "corpus.csv"
        self.checkpoint = self.directory / "fixture.ckpt"
        # Preparation records the path; it does not load or train this fixture.
        self.checkpoint.write_bytes(b"checkpoint-path fixture, not actual weights")
        self.rows = []
        for index, (emotion, split) in enumerate((('neutral', 'train'), ('excited', 'train'),
                                                 ('neutral', 'holdout'), ('excited', 'holdout'))):
            audio = self.directory / f"clip{index}.wav"
            t = np.arange(16000) / 16000
            samples = np.round(0.2 * np.sin(2 * np.pi * (180 + index * 13) * t) * 32767).astype(np.int16)
            wavfile.write(audio, 16000, samples)
            self.rows.append(dict(id=f"clip{index}", audio=audio.name, text="Hello, BMO.", emotion=emotion,
                                  split=split, source_group=f"session_{split}", kind="speech",
                                  reviewed="yes", source="synthetic engineering test fixture"))
        self.write_manifest()

    def write_manifest(self):
        with self.manifest.open("w", newline="") as source:
            writer = csv.DictWriter(source, fieldnames=FIELDS, delimiter="|")
            writer.writeheader()
            writer.writerows(self.rows)

    def test_preparation_normalizes_name_and_preserves_holdout(self):
        output = self.directory / "prepared"
        report = prepare(self.manifest, output, self.checkpoint, minimum_train=1, minimum_holdout=1)
        self.assertTrue(report["ready_for_pilot"])
        with (output / "train.csv").open() as source:
            rows = list(csv.reader(source, delimiter="|"))
        self.assertEqual([row[1] for row in rows], ["emotion:neutral", "emotion:excited"])
        self.assertTrue(all(row[2] == "Hello, Beemo." for row in rows))
        self.assertFalse(any("clip2" in row[0] or "clip3" in row[0] for row in rows))
        config = json.loads((output / "train_config.json").read_text())
        self.assertEqual(config["model"]["num_speakers"], 2)
        self.assertEqual(config["model"]["warmstart_ckpt"], str(self.checkpoint))
        self.assertTrue(config["data"]["csv_path"].endswith("train.csv"))
        with wave.open(str(output / "audio/clip0.wav"), "rb") as wav:
            self.assertEqual((wav.getframerate(), wav.getnchannels(), wav.getsampwidth()), (22050, 1, 2))

    def test_unreviewed_and_source_leakage_are_rejected(self):
        self.rows[0]["reviewed"] = "no"
        self.rows[2]["source_group"] = "session_train"
        self.write_manifest()
        report, _ = audit(self.manifest, 1, 1)
        self.assertFalse(report["ready_for_pilot"])
        self.assertTrue(any("human review" in error for error in report["errors"]))
        self.assertTrue(any("crosses train/holdout" in error for error in report["errors"]))

    def test_duplicate_audio_and_missing_files_are_rejected(self):
        self.rows[2]["audio"] = self.rows[0]["audio"]
        self.rows[3]["audio"] = "missing.wav"
        self.write_manifest()
        report, _ = audit(self.manifest, 1, 1)
        self.assertFalse(report["ready_for_pilot"])
        self.assertTrue(any("duplicate recording" in error for error in report["errors"]))
        self.assertTrue(any("unreadable WAV" in error for error in report["errors"]))

    def test_songs_are_kept_out_of_speech_training(self):
        self.rows.append(dict(id="song1", audio="separate-song.wav", text="", emotion="", split="",
                              source_group="", kind="song", reviewed="no", source=""))
        self.write_manifest()
        report, rows = audit(self.manifest, 1, 1)
        self.assertTrue(report["ready_for_pilot"])
        self.assertEqual(report["excluded_assets"], {"song": 1})
        self.assertEqual(len(rows), 4)

    def test_empty_corpus_is_not_ready(self):
        self.rows = []
        self.write_manifest()
        report, _ = audit(self.manifest)
        self.assertFalse(report["ready_for_pilot"])
        self.assertEqual(report["speech_clips"], 0)

    def test_existing_prepared_run_is_not_overwritten(self):
        output = self.directory / "prepared"
        output.mkdir()
        (output / "keep.txt").write_text("existing data")
        with self.assertRaises(ValueError):
            prepare(self.manifest, output, self.checkpoint, minimum_train=1, minimum_holdout=1)
        self.assertEqual((output / "keep.txt").read_text(), "existing data")
