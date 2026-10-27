"""Export a Piper training checkpoint and matching config for CPU/Pi inference."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile

from speech import ROOT
from speech.backend import PiperEngine


def export_voice(checkpoint: Path, config_path: Path, output: Path):
    if not output.resolve().is_relative_to(ROOT):
        raise ValueError("Keep generated models inside the repository")
    if output.exists() or Path(str(output) + ".json").exists():
        raise ValueError("Choose a new model filename; existing models will not be overwritten")
    import torch
    from piper.phoneme_ids import phonemes_to_ids
    from piper.phonemize_espeak import EspeakPhonemizer
    from piper.train.vits.lightning import VitsModel

    config = json.loads(config_path.read_text())
    # Checkpoints are from the explicitly downloaded/trained local Piper model.
    model = VitsModel.load_from_checkpoint(checkpoint, map_location="cpu", weights_only=False)
    network = model.model_g.eval()
    if network.n_speakers != config["num_speakers"]:
        raise ValueError("Checkpoint speaker count does not match the training config JSON")
    network.dec.remove_weight_norm()

    def forward(tokens, lengths, scales, sid=None):
        return network.infer(tokens, lengths, noise_scale=scales[0], length_scale=scales[1],
                             noise_scale_w=scales[2], sid=sid)[0]

    network.forward = forward
    phonemes = EspeakPhonemizer().phonemize(config["espeak"]["voice"], "Beemo.")[0]
    ids = phonemes_to_ids(phonemes, id_map=config["phoneme_id_map"])
    tokens = torch.tensor([ids], dtype=torch.long)
    inputs = (tokens, torch.tensor([len(ids)]), torch.tensor([0.667, 1.0, 0.8]))
    names = ["input", "input_lengths", "scales"]
    if network.n_speakers > 1:
        inputs += (torch.tensor([0], dtype=torch.long),)
        names.append("sid")
    with tempfile.TemporaryDirectory(prefix="piper-export-", dir=ROOT / ".tmp") as temporary:
        stage = Path(temporary) / "voice.onnx"
        # Explicit legacy export matches Piper's VITS exporter and works with
        # modern Torch releases whose default exporter otherwise needs onnxscript.
        with torch.no_grad():
            torch.onnx.export(network, inputs, stage, input_names=names, output_names=["output"],
                              opset_version=15, dynamo=False,
                              dynamic_axes={"input": {1: "text_length"}, "output": {2: "audio_length"}})
        shutil.copy2(config_path, Path(str(stage) + ".json"))
        engine = PiperEngine(stage)
        for emotion in (list(engine.emotions) or [None]):
            audio = engine.synthesize("Hello! I am BMO.", emotion=emotion)
            if not len(audio.samples):
                raise ValueError("Exported model produced empty audio")
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(stage), output)
        shutil.move(str(stage) + ".json", str(output) + ".json")
    print(f"CPU export smoke check passed: {output}")
    print("Voice quality/emotion and Raspberry Pi throughput still require separate validation.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True, help="voice.onnx.json written by training")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    export_voice(args.checkpoint, args.config, args.out)


if __name__ == "__main__":
    main()
