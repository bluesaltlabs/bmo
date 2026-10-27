# Expression: an expressive Piper voice for the Pi

The user prefers Amy's identity to Jenny's, but judged every stock voice/pacing
comparison emotionally flat. BMO's target performance is childlike, emotional,
playful and unpredictable, with substantial pitch and rhythm variation. The name
is pronounced **BEEMO**; speech-only text normalization now handles this.

## Chosen architecture

**Live synthesis must run locally on the Pi.** Training may happen on the Mac or
another training machine. Deploy a standard CPU Piper ONNX model and matching
JSON; the robot must not need a running Mac or a cloud speech service.

Start from Amy's training checkpoint and adapt a medium Piper model to one
consistent voice with recorded performance conditions. Piper's multi-speaker
IDs carry names such as `emotion:neutral`, `emotion:excited`,
`emotion:concerned` and `emotion:playful`. These represent acting by the same
speaker. The model must learn the differences from actual recordings.

This follows an existing Piper conditioning pattern: the official
[Thorsten emotional voice config](https://huggingface.co/rhasspy/piper-voices/raw/main/de/de_DE/thorsten_emotional/medium/de_DE-thorsten_emotional-medium.onnx.json)
uses speaker IDs for emotions. That supports the method; stock Amy has no
emotional conditioning to turn on.

## Current state

- Amy remains the working default; formant synthesis remains the ESP32 backup.
- The separate training environment, native alignment extension and Amy seed
  checkpoint are installed. Amy initializes a multi-condition model successfully.
- Dataset audit/preparation, training and ONNX export commands are available in
  [the training guide](training/README.md). CPU export was smoke-tested with Amy;
  it is still stock Amy, not an emotionally trained BMO voice.
- There are **zero curated training recordings**. The existing one-minute BMO
  monologue is a listening reference, without checked utterance transcripts or
  emotion labels. It is insufficient evidence for an expressive voice.
- No training fit, Pi deployment or Pi performance validation has occurred.

Amy's generated `.tmp/voice/amy_reference.wav` preserves a starting identity for
comparison. Neutral synthetic audio alone cannot teach the required acting.
The `neutral/lively/gentle` presets remain pacing utilities only.

## Recordings and singing

Collect clean, varied performances of one consistent voice, with exact
transcripts and checked emotion labels. Start with neutral, excited, concerned
and playful; add other conditions only when there is enough real material.
Keep whole recording sessions or source scenes in either training or holdout,
so evaluation cannot reuse the same source. Background music and overlapping
dialogue are unsuitable; source separation does not guarantee clean training
audio.

Sing-song speech belongs in expressive spoken performances. Actual songs,
melodic passages and nonverbal vocalizations stay as separate stored clips
initially. Piper speech training does not provide reliable arbitrary singing
or melody control.

## Acceptance

1. Generate identical held-out sentences in each actual trained condition,
   alongside stock Amy and real BMO reference performances.
2. Have the user judge voice similarity and emotional acting separately.
   Reject a model that only changes speed, loses intelligibility or drifts in
   identity. Pitch statistics and unit tests do not establish voice quality.
3. Preserve native-rate samples and record checkpoint, condition and settings.
4. Run `speech.benchmark --require-pi` on the deployment Pi. Require warm
   synthesis faster than playback and check memory/load time with the robot's
   other work. Mac timing cannot qualify the Pi.
5. Keep Amy as fallback until listening and Pi checks pass. UART PCM transport
   and speaker/DAC playback remain separate hardware integration work.
