// Host demo for the BMO voice engine: renders text to a WAV file and plays it with afplay (macOS).
//   bmo_say "Hello, I am BMO!"          speak a line
//   bmo_say                             play the built-in demo phrases
//   bmo_say -i                          interactive: type lines, hear them
//   bmo_say --wav out.wav "text"        write only, no playback
// Tuning: --pitch HZ --speed X --formant X --inflect X --rough X --breath X --crush BITS --hold N --volume X
//         --clean (unprocessed synth preset) --mouth (print mouth-openness trace)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "bmo_voice.h"

static const uint32_t kRate = 16000;

static const char* kDemo[] = {
    "Hello! I am BMO.",
    "Do you want to play a game with me?",
    "Oh no! Something is not working. Can you help me fix it?",
    "Good morning! You have 3 new messages and 12 things to do today.",
    "Beep boop. Battery is low, please feed me electricity.",
};

static bool writeWav(const char* path, const std::vector<int16_t>& pcm, uint32_t rate) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * 2), riff = 36 + dataBytes, fmtLen = 16, byteRate = rate * 2;
  uint16_t pcmFmt = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtLen, 4, 1, f); fwrite(&pcmFmt, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
  fwrite(pcm.data(), 2, pcm.size(), f);
  fclose(f);
  return true;
}

static std::vector<int16_t> synth(bmo::Voice& v, const char* text, bool mouthTrace) {
  std::vector<int16_t> pcm;
  int16_t buf[256];
  v.speak(text);
  size_t total = 0;
  while (true) {
    size_t n = v.render(buf, 256);
    pcm.insert(pcm.end(), buf, buf + n);
    total += n;
    if (mouthTrace && n) printf("%6.0f ms  mouth %3u %.*s\n", total * 1000.0 / kRate, v.mouth(), v.mouth() / 8, "################################");
    if (n < 256) break;
  }
  pcm.insert(pcm.end(), kRate / 8, 0);
  return pcm;
}

static void play(const std::string& wav) {
  std::string cmd = "afplay '" + wav + "'";
  if (system(cmd.c_str()) != 0) fprintf(stderr, "playback failed (afplay missing?). Use --wav to write a file.\n");
}

int main(int argc, char** argv) {
  bmo::VoiceConfig cfg = bmo::VoiceConfig::bmo();
  std::string wavPath, text;
  bool interactive = false, mouth = false, writeOnly = false;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> float { return i + 1 < argc ? static_cast<float>(atof(argv[++i])) : 0.0f; };
    if (a == "--pitch") cfg.pitch_hz = val();
    else if (a == "--speed") cfg.speed = val();
    else if (a == "--formant") cfg.formant_scale = val();
    else if (a == "--inflect") cfg.inflection = val();
    else if (a == "--rough") cfg.roughness = val();
    else if (a == "--breath") cfg.breathiness = val();
    else if (a == "--crush") cfg.crush_bits = static_cast<uint8_t>(val());
    else if (a == "--hold") cfg.hold = static_cast<uint8_t>(val());
    else if (a == "--volume") cfg.volume = val();
    else if (a == "--clean") cfg = bmo::VoiceConfig::clean();
    else if (a == "--mouth") mouth = true;
    else if (a == "-i") interactive = true;
    else if (a == "--wav" && i + 1 < argc) { wavPath = argv[++i]; writeOnly = true; }
    else { if (!text.empty()) text += ' '; text += a; }
  }

  bmo::Voice voice;
  voice.begin(kRate, cfg);
  const std::string tmpWav = wavPath.empty() ? ".tmp/voice/bmo_say.wav" : wavPath;

  auto say = [&](const char* t) {
    printf("BMO: %s\n", t);
    std::vector<int16_t> pcm = synth(voice, t, mouth);
    if (!writeWav(tmpWav.c_str(), pcm, kRate)) { fprintf(stderr, "cannot write %s\n", tmpWav.c_str()); return; }
    if (writeOnly) printf("wrote %s (%.1fs)\n", tmpWav.c_str(), pcm.size() / double(kRate));
    else play(tmpWav);
  };

  if (interactive) {
    char line[512];
    printf("Type a line and press enter (empty line quits).\n> ");
    while (fgets(line, sizeof(line), stdin) && line[0] != '\n') {
      say(line);
      printf("> ");
    }
  } else if (!text.empty()) {
    say(text.c_str());
  } else {
    for (const char* t : kDemo) say(t);
  }
  return 0;
}
