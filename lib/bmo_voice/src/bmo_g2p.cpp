// Compact rule-based English letter-to-sound plus the phoneme acoustic table.
// Deliberately small (fits on an ESP32); common words use explicit pronunciations.
#include <ctype.h>
#include <string.h>

#include "bmo_phon.h"

namespace bmo {

#define VOW(a, b, c, d) {a, b, c, d, 100, 0, 0, 0}
#define SON(a, b, c, d, v) {a, b, c, d, v, 0, 0, 0}
#define FRIC(d, v, n, f, bw) {0, 0, 0, d, v, n, f, bw}
#define QUIET(d, v) {0, 0, 0, d, v, 0, 0, 0}

const PhDef kPhones[PH_COUNT] = {
    {"IY", 1, true, {VOW(270, 2290, 3010, 105)}},
    {"IH", 1, true, {VOW(390, 1990, 2550, 85)}},
    {"EH", 1, true, {VOW(530, 1840, 2480, 95)}},
    {"AE", 1, true, {VOW(660, 1720, 2410, 115)}},
    {"AA", 1, true, {VOW(730, 1090, 2440, 115)}},
    {"AO", 1, true, {VOW(570, 840, 2410, 115)}},
    {"UH", 1, true, {VOW(440, 1020, 2240, 85)}},
    {"UW", 1, true, {VOW(300, 870, 2240, 110)}},
    {"AH", 1, true, {VOW(640, 1190, 2390, 80)}},
    {"ER", 1, true, {VOW(490, 1350, 1690, 110)}},
    {"AY", 2, true, {VOW(730, 1090, 2440, 100), VOW(400, 1990, 2550, 90)}},
    {"EY", 2, true, {VOW(530, 1840, 2480, 90), VOW(300, 2200, 2900, 85)}},
    {"OW", 2, true, {VOW(570, 900, 2410, 90), VOW(330, 870, 2240, 85)}},
    {"OY", 2, true, {VOW(570, 840, 2410, 100), VOW(330, 2100, 2800, 85)}},
    {"AW", 2, true, {VOW(730, 1090, 2440, 100), VOW(340, 900, 2240, 90)}},
    {"AX", 1, true, {VOW(480, 1450, 2500, 65)}}, // reduced unstressed vowel (schwa)

    {"W", 1, false, {SON(290, 610, 2150, 55, 80)}},
    {"Y", 1, false, {SON(260, 2070, 3020, 55, 80)}},
    {"L", 1, false, {SON(360, 1300, 2800, 65, 90)}},
    {"R", 1, false, {SON(310, 1060, 1380, 70, 90)}},
    {"M", 1, false, {SON(260, 1100, 2200, 70, 55)}},
    {"N", 1, false, {SON(260, 1600, 2600, 65, 55)}},
    {"NG", 1, false, {SON(260, 2000, 2700, 75, 55)}},

    {"F", 1, false, {FRIC(90, 0, 40, 4500, 5000)}},
    {"V", 1, false, {FRIC(70, 35, 30, 4500, 5000)}},
    {"S", 1, false, {FRIC(105, 0, 85, 5600, 2200)}},
    {"Z", 1, false, {FRIC(85, 35, 55, 5600, 2200)}},
    {"SH", 1, false, {FRIC(110, 0, 90, 3000, 1800)}},
    {"ZH", 1, false, {FRIC(90, 35, 60, 3000, 1800)}},
    {"TH", 1, false, {FRIC(90, 0, 25, 4800, 5000)}},
    {"DH", 1, false, {FRIC(55, 35, 20, 4800, 5000)}},
    {"HH", 1, false, {{0, 0, 0, 70, 0, 70, -1, 0}}},

    {"P", 3, false, {QUIET(55, 0), {0, 0, 0, 10, 0, 90, 1100, 2500}, {0, 0, 0, 25, 0, 40, -1, 0}}},
    {"B", 2, false, {QUIET(45, 30), {0, 0, 0, 10, 70, 45, 900, 2500}}},
    {"T", 3, false, {QUIET(55, 0), {0, 0, 0, 10, 0, 90, 4600, 2500}, {0, 0, 0, 25, 0, 40, -1, 0}}},
    {"D", 2, false, {QUIET(45, 30), {0, 0, 0, 10, 70, 45, 4300, 2500}}},
    {"K", 3, false, {QUIET(55, 0), {0, 0, 0, 12, 0, 90, 2100, 1400}, {0, 0, 0, 30, 0, 45, -1, 0}}},
    {"G", 2, false, {QUIET(45, 30), {0, 0, 0, 12, 70, 45, 2000, 1400}}},
    {"CH", 2, false, {QUIET(45, 0), FRIC(90, 0, 85, 3200, 1900)}},
    {"JH", 2, false, {QUIET(40, 30), FRIC(80, 35, 60, 3200, 1900)}},

    {"_,", 1, false, {QUIET(190, 0)}},
    {"_.", 1, false, {QUIET(330, 0)}},
};

namespace {

struct Entry {
  const char* word;
  const char* ph;  // phoneme names; '~' prefix = unstressed function word, ''' marks stress
};

const Entry kDict[] = {
    {"the", "~DH AX"},     {"a", "~AX"},          {"an", "~AX N"},       {"of", "~AX V"},
    {"to", "~T UW"},       {"and", "~AX N D"},    {"is", "~IH Z"},       {"are", "~AA R"},
    {"was", "~W AH Z"},    {"in", "~IH N"},       {"on", "~AA N"},       {"at", "~AE T"},
    {"it", "~IH T"},       {"my", "~M AY"},       {"for", "~F AO R"},    {"or", "~AO R"},
    {"as", "~AE Z"},       {"be", "B IY"},        {"i", "AY"},           {"you", "Y UW"},
    {"your", "Y AO R"},    {"me", "M IY"},        {"we", "W IY"},        {"he", "HH IY"},
    {"she", "SH IY"},      {"they", "DH EY"},     {"them", "DH EH M"},   {"then", "DH EH N"},
    {"this", "DH IH S"},   {"that", "DH AE T"},   {"there", "DH EH R"},  {"here", "HH IY R"},
    {"these", "DH IY Z"},  {"those", "DH OW Z"},  {"with", "~W IH DH"},  {"what", "W AH T"},
    {"where", "W EH R"},   {"when", "W EH N"},    {"who", "HH UW"},      {"why", "W AY"},
    {"how", "HH AW"},      {"now", "N AW"},       {"wow", "W AW"},       {"cow", "K AW"},
    {"do", "D UW"},        {"does", "D AH Z"},    {"done", "D AH N"},    {"one", "W AH N"},
    {"two", "T UW"},       {"four", "F AO R"},    {"eight", "EY T"},     {"said", "S EH D"},
    {"have", "HH AE V"},   {"give", "G IH V"},    {"live", "L IH V"},    {"love", "L AH V"},
    {"come", "K AH M"},    {"some", "S AH M"},    {"get", "G EH T"},     {"good", "G UH D"},
    {"look", "L UH K"},    {"book", "B UH K"},    {"put", "P UH T"},     {"full", "F UH L"},
    {"yes", "Y EH S"},     {"no", "N OW"},        {"so", "S OW"},        {"go", "G OW"},
    {"oh", "OW"},          {"hi", "HH AY"},       {"hey", "HH EY"},      {"bye", "B AY"},
    {"hello", "HH AX 'L OW"},  {"okay", "OW 'K EY"},  {"please", "P L IY Z"},
    {"friend", "F R EH N D"},  {"friends", "F R EH N D Z"},
    {"bmo", "'B IY M OW"}, {"video", "V IH D IY OW"},  {"games", "G EY M Z"},
    {"adventure", "AE D 'V EH N CH ER"},  {"world", "W ER L D"},   {"word", "W ER D"},
    {"work", "W ER K"},    {"were", "W ER"},      {"very", "V EH R IY"}, {"people", "P IY P AH L"},
    {"beautiful", "B Y UW T AH F AH L"},  {"robot", "R OW B AA T"},
    {"want", "W AA N T"},  {"could", "K UH D"},   {"would", "W UH D"},   {"should", "SH UH D"},
    {"because", "B IH K AO Z"},  {"again", "AH G EH N"},  {"every", "EH V R IY"},
    {"know", "N OW"},      {"thanks", "TH AE NG K S"},    {"computer", "K AH M P Y UW T ER"},
    {"night", "N AY T"},   {"light", "L AY T"},   {"play", "P L EY"},
    {"today", "T AX 'D EY"},    {"tomorrow", "T AX 'M AA R OW"},   {"sorry", "S AA R IY"},
    {"made", "M EY D"},    {"name", "N EY M"},    {"zero", "Z IY R OW"}, {"three", "TH R IY"},
    {"five", "F AY V"},    {"six", "S IH K S"},   {"seven", "S EH V AH N"},  {"nine", "N AY N"},
    {"hundred", "HH AH N D R AH D"},   {"thousand", "TH AW Z AH N D"},
    {"am", "~AE M"}, {"i'm", "AY M"}, {"i've", "AY V"}, {"i'll", "AY L"},
    {"you're", "Y AO R"}, {"you've", "Y UW V"}, {"you'll", "Y UW L"},
    {"we're", "W IY R"}, {"we've", "W IY V"}, {"we'll", "W IY L"},
    {"don't", "D OW N T"}, {"can't", "K AE N T"}, {"won't", "W OW N T"},
    {"isn't", "IH Z AX N T"}, {"aren't", "AA R N T"},
    {"it's", "~IH T S"}, {"that's", "DH AE T S"}, {"let's", "L EH T S"},
    {"grown", "G R OW N"}, {"everything", "EH V R IY TH IH NG"},
    {"still", "S T IH L"}, {"waking", "W EY K IH NG"}, {"wait", "W EY T"},
    {"something", "S AH M TH IH NG"}, {"working", "W ER K IH NG"},
    {"morning", "M AO R N IH NG"}, {"messages", "M EH S AX JH IH Z"},
    {"battery", "B AE T ER IY"}, {"electricity", "IH L EH K 'T R IH S AX T IY"},
    {"eleven", "IH 'L EH V AX N"}, {"twelve", "T W EH L V"},
    {"thirteen", "TH ER 'T IY N"}, {"fourteen", "F AO R 'T IY N"},
    {"fifteen", "F IH F 'T IY N"}, {"sixteen", "S IH K S 'T IY N"},
    {"seventeen", "S EH V AX N 'T IY N"}, {"eighteen", "EY 'T IY N"}, {"nineteen", "N AY N 'T IY N"},
};

bool isVow(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

int lookupPh(const char* name, size_t len) {
  for (int i = 0; i < PH_COUNT - 2; ++i) {
    if (strlen(kPhones[i].name) == len && strncmp(kPhones[i].name, name, len) == 0) return i;
  }
  return -1;
}

struct Out {
  uint8_t* p;
  size_t n, max;
  void put(Ph ph) {
    if (n < max) p[n++] = ph;
  }
  uint8_t last() const { return n ? (p[n - 1] & 0x7F) : 0xFF; }
};

size_t fromDict(const char* spec, Out& o) {
  bool unstressed = false;
  if (*spec == '~') {
    unstressed = true;
    ++spec;
  }
  bool marked = strchr(spec, '\'') != nullptr;
  bool stressDone = false;
  bool pendingStress = false;
  while (*spec) {
    while (*spec == ' ') ++spec;
    if (*spec == '\'') {
      pendingStress = true;
      ++spec;
    }
    const char* s = spec;
    while (*spec && *spec != ' ') ++spec;
    int id = lookupPh(s, spec - s);
    if (id < 0 || o.n >= o.max) continue;
    uint8_t v = static_cast<uint8_t>(id);
    if (kPhones[id].vowel && !unstressed && !stressDone && (pendingStress || !marked)) {
      v |= 0x80;
      stressDone = true;
    }
    o.p[o.n++] = v;
  }
  return o.n;
}

struct Word {
  char s[40];
  int n;
  char at(int i) const { return (i >= 0 && i < n) ? s[i] : 0; }
  bool endsAt(int i) const { return i >= n; }
  // Vowel, consonant, then a silent final "e" (optionally followed by s/d): "make", "makes", "named".
  bool magicE(int i) const {
    char c1 = at(i + 1);
    if (!c1 || isVow(c1) || at(i + 2) != 'e') return false;
    if (i + 3 == n) return true;
    return i + 4 == n && (at(i + 3) == 's' || at(i + 3) == 'd');
  }
  bool hasVowelLetterOtherThan(int skip) const {
    for (int i = 0; i < n; ++i)
      if (i != skip && isVow(s[i])) return true;
    return false;
  }
};

void rules(const Word& w, Out& o) {
  const int n = w.n;
  for (int i = 0; i < n; ++i) {
    char c = w.s[i];
    char c1 = w.at(i + 1), c2 = w.at(i + 2), c3 = w.at(i + 3);
    auto pr = [&](Ph a) { o.put(a); };

    // Doubled consonants sound like one.
    if (!isVow(c) && c == c1 && c != 'c' && c != 'g') continue;

    switch (c) {
      case 'a':
        if (c1 == 'i' || c1 == 'y') { pr(EY); ++i; }
        else if (c1 == 'u' || c1 == 'w') { pr(AO); ++i; }
        else if (c1 == 'r') { pr(AA); pr(R); ++i; }
        else if (c1 == 'l' && c2 == 'l') { pr(AO); }
        else if (c1 == 'l' && c2 == 'k') { pr(AO); ++i; }
        else if (w.magicE(i)) pr(EY);
        else pr(AE);
        break;
      case 'e':
        if (i == n - 1 && n > 2) break;  // silent final e
        if (i == n - 2 && c1 == 's' && n > 3 && !isVow(w.at(i - 1))) {
          char p = w.at(i - 1);
          if (p == 's' || p == 'z' || p == 'x' || p == 'c' || p == 'g' || (p == 'h' && (w.at(i - 2) == 's' || w.at(i - 2) == 'c'))) {
            pr(IH); pr(Z); ++i;
          } else { pr(Z); ++i; }
          break;
        }
        if (i == n - 2 && c1 == 'd' && n > 3 && !isVow(w.at(i - 1))) {
          char p = w.at(i - 1);
          if (p == 't' || p == 'd') { pr(IH); pr(D); }
          else if (strchr("pkfsxh", p)) pr(T);
          else pr(D);
          ++i;
          break;
        }
        if (c1 == 'e' || c1 == 'a') { pr(IY); ++i; }
        else if (c1 == 'w') { pr(Y); pr(UW); ++i; }
        else if (c1 == 'r') { pr(ER); ++i; }
        else if (c1 == 'y' && i + 2 == n) { pr(IY); ++i; }
        else if (c1 == 'i' && c2 == 'g' && c3 == 'h') { pr(EY); i += 3; }
        else if (c1 == 'i') { pr(IY); ++i; }
        else pr(EH);
        break;
      case 'i':
        if (c1 == 'g' && c2 == 'h') { pr(AY); i += 2; }
        else if (c1 == 'e' && i + 2 == n) { pr(AY); ++i; }
        else if (c1 == 'e') { pr(IY); ++i; }
        else if (c1 == 'r') { pr(ER); ++i; }
        else if (c1 == 'n' && c2 == 'g' && i + 3 == n) { pr(IH); }
        else if ((c1 == 'n' || c1 == 'l') && c2 == 'd') pr(AY);
        else if (i == n - 1 && n <= 2) pr(AY);
        else if (w.magicE(i)) pr(AY);
        else pr(IH);
        break;
      case 'o':
        if (c1 == 'o' && c2 == 'k') { pr(UH); ++i; }
        else if (c1 == 'o' && c2 == 'd') { pr(UH); ++i; }
        else if (c1 == 'o') { pr(UW); ++i; }
        else if (c1 == 'a') { pr(OW); ++i; }
        else if (c1 == 'u') { pr(AW); ++i; }
        else if (c1 == 'w' && (i + 2 == n)) { pr(OW); ++i; }
        else if (c1 == 'w') { pr(AW); ++i; }
        else if (c1 == 'i' || c1 == 'y') { pr(OY); ++i; }
        else if (c1 == 'r') { pr(AO); pr(R); ++i; }
        else if (c1 == 'l' && (c2 == 'd' || c2 == 't')) pr(OW);
        else if (w.magicE(i)) pr(OW);
        else if (i == n - 1 && n > 1) pr(OW);
        else pr(AA);
        break;
      case 'u':
        if (c1 == 'e' && i + 2 == n) { pr(UW); ++i; }
        else if (c1 == 'r') { pr(ER); ++i; }
        else if (w.magicE(i)) { if (!strchr("rlj", w.at(i - 1) ? w.at(i - 1) : 'x')) pr(Y); pr(UW); }
        else pr(AH);
        break;
      case 'y':
        if (i == 0) pr(Y);
        else if (i == n - 1) { if (w.hasVowelLetterOtherThan(i)) pr(IY); else pr(AY); }
        else pr(IH);
        break;
      case 'b':
        if (i == n - 1 && w.at(i - 1) == 'm') break;
        pr(B);
        break;
      case 'c':
        if (c1 == 'h') { pr(CH); ++i; }
        else if (c1 == 'k') { pr(K); ++i; }
        else if (c1 == 'e' || c1 == 'i' || c1 == 'y') pr(S);
        else pr(K);
        break;
      case 'd':
        if (c1 == 'g' && c2 == 'e') { pr(JH); i += 1; }
        else pr(D);
        break;
      case 'f': pr(F); break;
      case 'g':
        if (c1 == 'h') { if (i == 0) { pr(G); } ++i; }
        else if (c1 == 'n' && i == 0) {}
        else if (c1 == 'e' || c1 == 'i' || c1 == 'y') { if (i == 0 && c1 != 'e') pr(G); else pr(JH); }
        else pr(G);
        break;
      case 'h': pr(HH); break;
      case 'j': pr(JH); break;
      case 'k':
        if (c1 == 'n' && i == 0) break;
        pr(K);
        break;
      case 'l':
        if (c1 == 'e' && i + 2 == n && i > 0 && !isVow(w.at(i - 1))) { pr(AH); pr(L); ++i; break; }
        pr(L);
        break;
      case 'm': pr(M); break;
      case 'n':
        if (c1 == 'g' && (i + 2 == n || !isVow(c2))) { pr(NG); ++i; }
        else if (c1 == 'k') { pr(NG); }
        else pr(N);
        break;
      case 'p':
        if (c1 == 'h') { pr(F); ++i; }
        else pr(P);
        break;
      case 'q':
        pr(K); pr(W);
        if (c1 == 'u') ++i;
        break;
      case 'r': pr(R); break;
      case 's':
        if (c1 == 'h') { pr(SH); ++i; }
        else if (c1 == 'i' && c2 == 'o' && c3 == 'n') { pr(ZH); pr(AH); pr(N); i += 3; }
        else if (i == n - 1 && i > 0 && (isVow(w.at(i - 1)) || strchr("bdglmnrvy", w.at(i - 1)))) pr(Z);
        else if (i > 0 && isVow(w.at(i - 1)) && isVow(c1)) pr(Z);
        else pr(S);
        break;
      case 't':
        if (c1 == 'i' && c2 == 'o' && c3 == 'n') { pr(SH); pr(AH); pr(N); i += 3; }
        else if (c1 == 'c' && c2 == 'h') { pr(CH); i += 2; }
        else if (c1 == 'h') {
          bool voiced = (c2 == 'e' && (w.at(i + 3) == 'r' || i + 3 >= n)) && i > 0;
          pr(voiced ? DH : TH);
          ++i;
        } else pr(T);
        break;
      case 'v': pr(V); break;
      case 'w':
        if (c1 == 'h') { pr(W); ++i; }
        else if (c1 == 'r') {}
        else pr(W);
        break;
      case 'x': if (i == 0) pr(Z); else { pr(K); pr(S); } break;
      case 'z': pr(Z); break;
      default: break;
    }
  }
}

void autoStress(uint8_t* p, size_t n, size_t wordLen) {
  if (wordLen < 3) return;
  for (size_t i = 0; i < n; ++i) {
    if (kPhones[p[i] & 0x7F].vowel) {
      p[i] |= 0x80;
      return;
    }
  }
}

}  // namespace

size_t wordToPhonemes(const char* word, size_t len, uint8_t* out, size_t max) {
  Word w;
  w.n = 0;
  for (size_t i = 0; i < len && w.n < 39; ++i) {
    char c = static_cast<char>(tolower(static_cast<unsigned char>(word[i])));
    w.s[w.n++] = c;
  }
  w.s[w.n] = 0;
  Out o{out, 0, max};
  if (w.n == 0) return 0;

  for (const Entry& e : kDict) {
    if (static_cast<size_t>(w.n) == strlen(e.word) && memcmp(e.word, w.s, w.n) == 0) return fromDict(e.ph, o);
  }
  // Keep apostrophes for dictionary lookup: "I'll" and "ill", "we're" and
  // "were" need different pronunciations. Unknown words use the old fallback.
  int letters = 0;
  for (int i = 0; i < w.n; ++i)
    if (w.s[i] != '\'') w.s[letters++] = w.s[i];
  w.n = letters;
  w.s[w.n] = 0;
  rules(w, o);
  autoStress(out, o.n, w.n);
  return o.n;
}

size_t numberToWords(const char* d, size_t len, char* out, size_t max) {
  static const char* ones[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                               "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                               "seventeen", "eighteen", "nineteen"};
  static const char* tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
  size_t n = 0;
  auto add = [&](const char* s) {
    size_t l = strlen(s);
    if (n + l + 2 > max) return;
    if (n) out[n++] = ' ';
    memcpy(out + n, s, l);
    n += l;
  };
  if (len > 4) {  // too long to read as a quantity: digit by digit
    for (size_t i = 0; i < len; ++i) add(ones[d[i] - '0']);
    out[n] = 0;
    return n;
  }
  int v = 0;
  for (size_t i = 0; i < len; ++i) v = v * 10 + (d[i] - '0');
  if (v >= 1000) { add(ones[v / 1000]); add("thousand"); v %= 1000; }
  if (v >= 100) { add(ones[v / 100]); add("hundred"); v %= 100; }
  if (v >= 20) { add(tens[v / 10]); if (v % 10) add(ones[v % 10]); }
  else if (v > 0 || n == 0) add(ones[v]);
  out[n] = 0;
  return n;
}

}  // namespace bmo
