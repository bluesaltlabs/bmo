"""Host/Pi speech; the ESP32's standalone synthesizer stays in lib/bmo_voice."""
import os
from pathlib import Path

# Set before Piper/ONNX Runtime is imported: no uploader or persistent device ID.
# https://github.com/microsoft/onnxruntime/blob/main/docs/Privacy.md
os.environ["ORT_DISABLE_TELEMETRY"] = "1"

ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / "models/piper"
DEFAULT_VOICE = "en_US-amy-medium"
