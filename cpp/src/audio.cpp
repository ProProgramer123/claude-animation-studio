// A small synthesizer that turns the Sound agent's score into a soundtrack.
// Everything is generated from maths (no sample files), so it works offline and the same
// score always gives the same sound.

#include "studio/audio.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>

namespace studio {
namespace {

constexpr double kPi = 3.14159265358979323846;

/** Deterministic random numbers, so a score always renders the same. */
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }  // [0,1)
  double noise() { return uniform() * 2 - 1; }
};

double midiToHz(double midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

/** A stereo bus that wraps around, so tails that run past the end land at the start and the
 *  soundtrack loops without a click. */
struct Bus {
  std::vector<float> l, r;
  explicit Bus(size_t n) : l(n, 0.f), r(n, 0.f) {}
  size_t size() const { return l.size(); }
  void add(long start, const std::vector<float>& mono, double gain, double pan) {
    const double gl = gain * std::cos((pan + 1) * kPi / 4), gr = gain * std::sin((pan + 1) * kPi / 4);
    const long n = static_cast<long>(size());
    for (size_t i = 0; i < mono.size(); ++i) {
      long k = (start + static_cast<long>(i)) % n;
      if (k < 0) k += n;
      l[k] += static_cast<float>(mono[i] * gl);
      r[k] += static_cast<float>(mono[i] * gr);
    }
  }
};

// ---- building blocks ----------------------------------------------------------------

/** Chamberlin state-variable filter. */
struct Svf {
  double low = 0, band = 0;
  double process(double in, double cutoff, double q, int sr, int mode /*0 low,1 band,2 high*/) {
    const double f = 2 * std::sin(kPi * std::min(cutoff, sr / 6.0) / sr);
    const double damp = 1.0 / std::max(0.5, q);
    low += f * band;
    const double high = in - low - damp * band;
    band += f * high;
    return mode == 0 ? low : mode == 1 ? band : high;
  }
};

/** Band-limited sawtooth (polyBLEP). */
struct Saw {
  double phase = 0;
  double next(double hz, int sr) {
    const double dt = hz / sr;
    phase += dt;
    if (phase >= 1) phase -= 1;
    double v = 2 * phase - 1;
    auto blep = [dt](double t) {
      if (t < dt) { t /= dt; return t + t - t * t - 1; }
      if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
      return 0.0;
    };
    return v - blep(phase);
  }
};

/** Attack-decay-sustain-release envelope for a note held `hold` seconds. */
double adsr(double t, double hold, double a, double d, double s, double r) {
  if (t < 0) return 0;
  double level;
  if (t < a) level = t / a;
  else if (t < a + d) level = 1 - (1 - s) * (t - a) / d;
  else level = s;
  if (t > hold) {
    const double rel = (t - hold) / r;
    if (rel >= 1) return 0;
    const double at = hold < a ? hold / a : (hold < a + d ? 1 - (1 - s) * (hold - a) / d : s);
    level = at * (1 - rel);
  }
  return level;
}

// ---- instruments: each returns a mono note ------------------------------------------------

std::vector<float> bell(double hz, double hold, int sr) {
  const double len = std::max(hold, 0.2) + 1.6;
  std::vector<float> out(static_cast<size_t>(len * sr));
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const double env = std::exp(-t * 2.6) * std::min(1.0, t / 0.002);
    const double index = 2.2 * std::exp(-t * 4);
    const double mod = std::sin(2 * kPi * hz * 3.5 * t) * index;
    out[i] = static_cast<float>(env * (0.8 * std::sin(2 * kPi * hz * t + mod) + 0.2 * std::sin(2 * kPi * hz * 2 * t)));
  }
  return out;
}

std::vector<float> piano(double hz, double hold, int sr) {
  const double len = hold + 1.2;
  std::vector<float> out(static_cast<size_t>(len * sr));
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    double v = 0;
    for (int k = 1; k <= 6; ++k) {
      if (hz * k > sr / 2.5) break;
      v += std::sin(2 * kPi * hz * k * t * (1 + 0.0004 * k * k)) / std::pow(k, 1.6) * std::exp(-t * (1.5 + 0.8 * k));
    }
    const double release = t > hold ? std::exp(-(t - hold) * 8) : 1.0;
    out[i] = static_cast<float>(0.7 * v * std::min(1.0, t / 0.004) * release);
  }
  return out;
}

std::vector<float> pluck(double hz, double hold, int sr, Rng& rng) {
  const double len = hold + 0.8;
  std::vector<float> out(static_cast<size_t>(len * sr));
  const size_t period = std::max<size_t>(2, static_cast<size_t>(sr / hz));
  std::vector<double> buf(period);
  for (auto& b : buf) b = rng.noise();
  size_t idx = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const size_t nxt = (idx + 1) % period;
    const double v = buf[idx];
    buf[idx] = 0.5 * (buf[idx] + buf[nxt]) * (t > hold ? 0.97 : 0.996);
    idx = nxt;
    out[i] = static_cast<float>(v * 0.8);
  }
  return out;
}

std::vector<float> flute(double hz, double hold, int sr, Rng& rng) {
  const double len = hold + 0.2;
  std::vector<float> out(static_cast<size_t>(len * sr));
  Svf breath;
  double phase = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const double vib = t > 0.15 ? 1 + 0.004 * std::sin(2 * kPi * 5.2 * t) : 1;
    phase += 2 * kPi * hz * vib / sr;
    const double env = adsr(t, hold, 0.06, 0.1, 0.85, 0.15);
    const double noise = breath.process(rng.noise(), hz * 2, 2, sr, 1) * 0.15;
    out[i] = static_cast<float>(env * (0.8 * std::sin(phase) + 0.12 * std::sin(2 * phase) + noise));
  }
  return out;
}

std::vector<float> synthLead(double hz, double hold, int sr) {
  const double len = hold + 0.25;
  std::vector<float> out(static_cast<size_t>(len * sr));
  Saw a, b;
  Svf f;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const double env = adsr(t, hold, 0.01, 0.2, 0.6, 0.2);
    const double cutoff = hz * (2 + 6 * std::exp(-t * 6));
    const double v = 0.5 * (a.next(hz, sr) + b.next(hz * 1.006, sr));
    out[i] = static_cast<float>(0.5 * env * f.process(v, cutoff, 1.2, sr, 0));
  }
  return out;
}

std::vector<float> padChord(const std::vector<double>& freqs, double hold, bool strings, int sr) {
  const double len = hold + 0.8;
  std::vector<float> out(static_cast<size_t>(len * sr), 0.f);
  for (double hz : freqs) {
    Saw s1, s2, s3;
    Svf f1, f2;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = double(i) / sr;
      const double vib = strings ? 1 + 0.003 * std::sin(2 * kPi * 5.5 * t + hz) : 1;
      const double env = adsr(t, hold, strings ? 0.25 : 0.45, 0.3, 0.8, 0.7);
      double v = (s1.next(hz * vib, sr) + s2.next(hz * vib * 1.004, sr) + s3.next(hz * vib * 0.996, sr)) / 3;
      const double cutoff = strings ? 2400 : 1100;
      v = f2.process(f1.process(v, cutoff, 0.7, sr, 0), cutoff, 0.7, sr, 0);
      out[i] += static_cast<float>(env * v * 0.5);
    }
  }
  return out;
}

std::vector<float> bassNote(double hz, double hold, bool synth, int sr) {
  const double len = hold + 0.2;
  std::vector<float> out(static_cast<size_t>(len * sr));
  Saw s;
  Svf f;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const double env = adsr(t, hold, 0.008, 0.25, synth ? 0.6 : 0.5, 0.12);
    double v;
    if (synth) v = f.process(s.next(hz, sr), hz * (3 + 5 * std::exp(-t * 10)), 1.5, sr, 0);
    else v = std::sin(2 * kPi * hz * t) + 0.25 * std::sin(4 * kPi * hz * t);
    out[i] = static_cast<float>(0.8 * env * v);
  }
  return out;
}

std::vector<float> kick(int sr) {
  std::vector<float> out(static_cast<size_t>(0.4 * sr));
  double phase = 0;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    phase += 2 * kPi * (45 + 75 * std::exp(-t * 30)) / sr;
    out[i] = static_cast<float>(std::sin(phase) * std::exp(-t * 9));
  }
  return out;
}

std::vector<float> snare(int sr, Rng& rng) {
  std::vector<float> out(static_cast<size_t>(0.25 * sr));
  Svf f;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    const double n = f.process(rng.noise(), 1800, 0.8, sr, 1);
    out[i] = static_cast<float>((0.7 * n * std::exp(-t * 18) + 0.4 * std::sin(2 * kPi * 185 * t) * std::exp(-t * 30)));
  }
  return out;
}

std::vector<float> hat(int sr, Rng& rng) {
  std::vector<float> out(static_cast<size_t>(0.06 * sr));
  Svf f;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = double(i) / sr;
    out[i] = static_cast<float>(0.5 * f.process(rng.noise(), 7000, 0.7, sr, 2) * std::exp(-t * 70));
  }
  return out;
}

// ---- sound effects ----------------------------------------------------------------------

std::vector<float> effect(const std::string& type, double length, double rootHz, int sr, Rng& rng, double& pan) {
  pan = 0;
  auto make = [sr](double secs) { return std::vector<float>(static_cast<size_t>(std::max(0.01, secs) * sr), 0.f); };
  auto t_ = [sr](size_t i) { return double(i) / sr; };

  if (type == "whoosh") {
    const double len = length > 0 ? length : 0.7;
    auto out = make(len);
    Svf f;
    for (size_t i = 0; i < out.size(); ++i) {
      const double x = t_(i) / len;
      const double env = std::sin(kPi * x) * std::sin(kPi * x);
      out[i] = static_cast<float>(1.6 * env * f.process(rng.noise(), 300 + 2600 * std::sin(kPi * x), 1.6, sr, 1));
    }
    pan = 0.4;
    return out;
  }
  if (type == "pop") {
    auto out = make(0.12);
    double ph = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      ph += 2 * kPi * (220 + 700 * std::exp(-t * 60)) / sr;
      out[i] = static_cast<float>(std::sin(ph) * std::exp(-t * 35));
    }
    return out;
  }
  if (type == "chime" || type == "bell") {
    const double base = type == "chime" ? rootHz * 4 : rootHz * 2;
    auto a = bell(base, 0.1, sr), b = bell(base * 1.5, 0.1, sr);
    if (type == "chime") {
      auto c = bell(base * 2, 0.1, sr);
      for (size_t i = 0; i < a.size(); ++i) a[i] = 0.5f * a[i] + 0.3f * b[i] + 0.2f * c[i];
    } else {
      for (size_t i = 0; i < a.size(); ++i) a[i] = 0.75f * a[i] + 0.25f * b[i];
    }
    return a;
  }
  if (type == "sparkle") {
    auto out = make(0.9);
    const int pent[] = {0, 2, 4, 7, 9};
    for (int k = 0; k < 9; ++k) {
      const double start = 0.07 * k + 0.02 * rng.uniform();
      const double hz = rootHz * 4 * std::pow(2.0, pent[rng.next() % 5] / 12.0) * (k % 3 == 2 ? 2 : 1);
      const size_t s0 = static_cast<size_t>(start * sr);
      for (size_t i = 0; i < static_cast<size_t>(0.25 * sr) && s0 + i < out.size(); ++i) {
        const double t = t_(i);
        out[s0 + i] += static_cast<float>(0.4 * std::sin(2 * kPi * hz * t) * std::exp(-t * 18));
      }
    }
    return out;
  }
  if (type == "thud") {
    auto out = make(0.4);
    Svf f;
    double ph = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      ph += 2 * kPi * (40 + 60 * std::exp(-t * 25)) / sr;
      out[i] = static_cast<float>((std::sin(ph) + 0.5 * f.process(rng.noise(), 300, 0.7, sr, 0)) * std::exp(-t * 12));
    }
    return out;
  }
  if (type == "boing") {
    auto out = make(0.7);
    double ph = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      const double hz = 180 + 140 * t + 90 * std::sin(2 * kPi * 11 * t) * std::exp(-t * 3);
      ph += 2 * kPi * hz / sr;
      out[i] = static_cast<float>(0.8 * std::sin(ph) * std::exp(-t * 4.5) * std::min(1.0, t / 0.005));
    }
    return out;
  }
  if (type == "splash") {
    auto out = make(length > 0 ? length : 1.0);
    Svf f;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      const double drops = rng.uniform() < 0.002 ? rng.noise() * 3 : 0;
      out[i] = static_cast<float>((f.process(rng.noise(), 2500 - 1500 * std::min(1.0, t), 0.9, sr, 1) * 1.4 + drops * 0.3) *
                                  std::exp(-t * 3.5) * std::min(1.0, t / 0.01));
    }
    return out;
  }
  if (type == "wind") {
    const double len = length > 0 ? length : 3.0;
    auto out = make(len);
    Svf f;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      const double fade = std::min({1.0, t / 0.6, (len - t) / 0.8});
      const double centre = 500 + 250 * std::sin(2 * kPi * 0.3 * t) + 120 * std::sin(2 * kPi * 0.77 * t);
      out[i] = static_cast<float>(1.3 * std::max(0.0, fade) * f.process(rng.noise(), centre, 4, sr, 1));
    }
    return out;
  }
  if (type == "rain") {
    const double len = length > 0 ? length : 3.0;
    auto out = make(len);
    Svf bed, dropF;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      const double fade = std::max(0.0, std::min({1.0, t / 0.5, (len - t) / 0.5}));
      const double drop = rng.uniform() < 0.004 ? rng.noise() : 0;
      out[i] = static_cast<float>(fade * (0.25 * bed.process(rng.noise(), 4000, 0.7, sr, 0) +
                                          1.5 * dropF.process(drop, 3000, 3, sr, 1)));
    }
    return out;
  }
  if (type == "click") {
    auto out = make(0.02);
    Svf f;
    for (size_t i = 0; i < out.size(); ++i)
      out[i] = static_cast<float>(f.process(rng.noise(), 5000, 0.7, sr, 2) * std::exp(-t_(i) * 300));
    return out;
  }
  if (type == "zap") {
    auto out = make(0.3);
    Saw s;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      out[i] = static_cast<float>(0.5 * s.next(100 + 1400 * std::exp(-t * 14), sr) * std::exp(-t * 9));
    }
    return out;
  }
  if (type == "rise") {
    const double len = length > 0 ? length : 1.5;
    auto out = make(len);
    Svf f;
    double ph = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      const double x = t_(i) / len;
      ph += 2 * kPi * (rootHz * (1 + 3 * x * x)) / sr;
      out[i] = static_cast<float>(x * x * (0.4 * std::sin(ph) + 0.6 * f.process(rng.noise(), 400 + 4000 * x, 1.5, sr, 1)) *
                                  std::min(1.0, (1 - x) * 40));
    }
    return out;
  }
  if (type == "rumble") {
    const double len = length > 0 ? length : 2.0;
    auto out = make(len);
    Svf f1, f2;
    for (size_t i = 0; i < out.size(); ++i) {
      const double t = t_(i);
      const double fade = std::max(0.0, std::min({1.0, t / 0.3, (len - t) / 0.6}));
      out[i] = static_cast<float>(3 * fade * f2.process(f1.process(rng.noise(), 120, 0.7, sr, 0), 120, 0.7, sr, 0));
    }
    return out;
  }
  if (type == "bird") {
    auto out = make(0.8);
    for (int c = 0; c < 3; ++c) {
      const size_t s0 = static_cast<size_t>((0.05 + 0.22 * c) * sr);
      double ph = 0;
      for (size_t i = 0; i < static_cast<size_t>(0.12 * sr) && s0 + i < out.size(); ++i) {
        const double x = t_(i) / 0.12;
        ph += 2 * kPi * (2600 + 1500 * x + 300 * std::sin(2 * kPi * 30 * t_(i))) / sr;
        out[s0 + i] += static_cast<float>(0.35 * std::sin(ph) * std::sin(kPi * x));
      }
    }
    return out;
  }
  return {};
}

// ---- music theory -------------------------------------------------------------------------

int keySemitone(const std::string& key) {
  static const std::map<char, int> base{{'C', 0}, {'D', 2}, {'E', 4}, {'F', 5}, {'G', 7}, {'A', 9}, {'B', 11}};
  if (key.empty()) return 0;
  const auto it = base.find(static_cast<char>(std::toupper(static_cast<unsigned char>(key[0]))));
  if (it == base.end()) return 0;
  int s = it->second;
  if (key.size() > 1 && key[1] == '#') ++s;
  if (key.size() > 1 && key[1] == 'b') --s;
  return (s + 12) % 12;
}

const std::vector<int>& scaleSteps(const std::string& scale) {
  static const std::map<std::string, std::vector<int>> scales{
      {"major", {0, 2, 4, 5, 7, 9, 11}},     {"minor", {0, 2, 3, 5, 7, 8, 10}},
      {"dorian", {0, 2, 3, 5, 7, 9, 10}},    {"lydian", {0, 2, 4, 6, 7, 9, 11}},
      {"pentatonic", {0, 2, 4, 7, 9}},       {"minor-pentatonic", {0, 3, 5, 7, 10}},
  };
  const auto it = scales.find(scale);
  return it == scales.end() ? scales.at("major") : it->second;
}

/** Harmony uses the seven-note scale a pentatonic comes from. */
const std::vector<int>& harmonyScale(const std::string& scale) {
  if (scale == "pentatonic") return scaleSteps("major");
  if (scale == "minor-pentatonic") return scaleSteps("minor");
  return scaleSteps(scale);
}

int degreeToSemis(const std::vector<int>& steps, int degree) {
  const int n = static_cast<int>(steps.size());
  const int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
  const int idx = degree - oct * n;
  return oct * 12 + steps[idx];
}

/** "IV", "vi", "V7", "ii°" -> scale degree 0..6. */
int romanToDegree(std::string r) {
  std::string up;
  for (char c : r)
    if (std::isalpha(static_cast<unsigned char>(c))) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  static const std::vector<std::string> numerals{"VII", "VI", "IV", "V", "III", "II", "I"};
  static const std::map<std::string, int> value{{"I", 0}, {"II", 1}, {"III", 2}, {"IV", 3}, {"V", 4}, {"VI", 5}, {"VII", 6}};
  for (const auto& n : numerals)
    if (up.rfind(n, 0) == 0) return value.at(n);
  return 0;
}

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string pick(const nlohmann::json& j, const char* key, const std::string& fallback,
                 const std::vector<std::string>& allowed) {
  if (!j.contains(key) || !j[key].is_string()) return fallback;
  const std::string v = lower(j[key].get<std::string>());
  return std::find(allowed.begin(), allowed.end(), v) != allowed.end() ? v : fallback;
}

double num(const nlohmann::json& j, const char* key, double fallback, double lo, double hi) {
  if (!j.contains(key) || !j[key].is_number()) return fallback;
  return std::clamp(j[key].get<double>(), lo, hi);
}

}  // namespace

const std::vector<std::string>& soundEffectTypes() {
  static const std::vector<std::string> t{"whoosh", "pop",   "chime", "bell", "sparkle", "thud",   "boing",
                                          "splash", "wind",  "rain",  "click", "zap",    "rise",   "rumble", "bird"};
  return t;
}

const std::vector<std::string>& leadInstruments() {
  static const std::vector<std::string> t{"bell", "piano", "pluck", "flute", "synth", "none"};
  return t;
}

double loopTempo(double tempo, double seconds) {
  const double bars = std::max(1.0, std::round(seconds * tempo / 240.0));
  return bars * 240.0 / seconds;
}

Score parseScore(const nlohmann::json& root) {
  Score s;
  if (!root.is_object()) return s;
  const nlohmann::json music = root.contains("music") && root["music"].is_object() ? root["music"] : nlohmann::json::object();
  if (root.contains("music") && root["music"].is_null()) s.music = false;
  if (music.contains("enabled") && music["enabled"].is_boolean()) s.music = music["enabled"].get<bool>();
  s.tempo = num(music, "tempo_bpm", s.tempo, 50, 180);
  if (music.contains("key") && music["key"].is_string() && !music["key"].get<std::string>().empty())
    s.key = music["key"].get<std::string>();
  s.scale = pick(music, "scale", s.scale, {"major", "minor", "pentatonic", "minor-pentatonic", "dorian", "lydian"});
  if (music.contains("chords") && music["chords"].is_array()) {
    std::vector<std::string> chords;
    for (const auto& c : music["chords"])
      if (c.is_string()) chords.push_back(c.get<std::string>());
    if (!chords.empty()) s.chords = chords;
  }
  const nlohmann::json inst = music.contains("instruments") && music["instruments"].is_object() ? music["instruments"] : nlohmann::json::object();
  s.lead = pick(inst, "lead", s.lead, leadInstruments());
  s.pad = pick(inst, "pad", s.pad, {"warm", "strings", "none"});
  s.bass = pick(inst, "bass", s.bass, {"soft", "synth", "none"});
  s.drums = pick(inst, "drums", s.drums, {"none", "soft", "beat"});
  s.musicVolume = num(music, "volume", s.musicVolume, 0, 1);
  if (music.contains("melody") && music["melody"].is_array()) {
    for (const auto& n : music["melody"]) {
      if (!n.is_object() || !n.contains("degree") || !n["degree"].is_number()) continue;
      MelodyNote m;
      m.beat = num(n, "beat", 0, 0, 10000);
      m.degree = std::clamp(n["degree"].get<int>(), -14, 21);
      m.beats = num(n, "beats", 1, 0.125, 16);
      s.melody.push_back(m);
    }
  }
  if (root.contains("effects") && root["effects"].is_array()) {
    for (const auto& e : root["effects"]) {
      if (!e.is_object()) continue;
      SoundEffect fx;
      fx.type = pick(e, "type", "", soundEffectTypes());
      if (fx.type.empty()) continue;
      fx.time = num(e, "t", 0, 0, 3600);
      fx.volume = num(e, "volume", 0.8, 0, 1);
      fx.length = num(e, "length", 0, 0, 60);
      s.effects.push_back(fx);
    }
  }
  return s;
}

std::vector<float> renderSoundtrack(const Score& score, double seconds, const SoundtrackOptions& opt) {
  const int sr = opt.sampleRate;
  seconds = std::clamp(seconds, 0.5, 600.0);
  const size_t n = static_cast<size_t>(seconds * sr);
  Bus dry(n), wet(n);
  Rng rng(std::hash<std::string>{}(score.key + score.scale + std::to_string(score.tempo) + std::to_string(n)));

  const int root = 60 + keySemitone(score.key);
  const double rootHz = midiToHz(root);

  if (opt.music && score.music) {
    const double tempo = loopTempo(score.tempo, seconds);
    const double beat = 60.0 / tempo;
    const int bars = static_cast<int>(std::round(seconds / (4 * beat)));
    const auto& harm = harmonyScale(score.scale);
    const auto& mel = scaleSteps(score.scale);
    const double mv = score.musicVolume;
    auto at = [&](double secs) { return static_cast<long>(std::llround(secs * sr)); };

    for (int bar = 0; bar < bars; ++bar) {
      const int deg = romanToDegree(score.chords[static_cast<size_t>(bar) % score.chords.size()]);
      const double t0 = bar * 4 * beat;
      const std::vector<int> tones{deg, deg + 2, deg + 4};

      if (score.pad != "none") {
        std::vector<double> freqs;
        for (int d : tones) freqs.push_back(midiToHz(root - 12 + degreeToSemis(harm, d)));
        const auto chord = padChord(freqs, 4 * beat, score.pad == "strings", sr);
        dry.add(at(t0), chord, 0.22 * mv, 0);
        wet.add(at(t0), chord, 0.18 * mv, 0);
      }
      if (score.bass != "none") {
        const double bassHz = midiToHz(root - 24 + degreeToSemis(harm, deg));
        if (score.bass == "synth") {
          for (int e = 0; e < 8; ++e)
            dry.add(at(t0 + e * beat / 2), bassNote(e % 4 == 3 ? bassHz * 1.5 : bassHz, beat * 0.4, true, sr), 0.28 * mv, 0);
        } else {
          dry.add(at(t0), bassNote(bassHz, beat * 1.8, false, sr), 0.4 * mv, 0);
          dry.add(at(t0 + 2 * beat), bassNote(bassHz * 1.5, beat * 1.8, false, sr), 0.32 * mv, 0);
        }
      }
      if (score.drums != "none") {
        const auto k = kick(sr), s = snare(sr, rng), h = hat(sr, rng);
        const double dv = score.drums == "soft" ? 0.5 : 0.8;
        for (int b = 0; b < 4; ++b) {
          if (b == 0 || (score.drums == "beat" && b == 2)) dry.add(at(t0 + b * beat), k, 0.55 * dv * mv, 0);
          if (score.drums == "beat" && (b == 1 || b == 3)) dry.add(at(t0 + b * beat), s, 0.35 * dv * mv, 0.1);
          for (int half = 0; half < 2; ++half)
            dry.add(at(t0 + (b + half * 0.5) * beat), h, (half ? 0.12 : 0.18) * dv * mv, -0.3);
        }
      }
      // Without a written melody, play a gentle line over each chord.
      if (score.melody.empty() && score.lead != "none") {
        for (int b = 0; b < 4; ++b) {
          if (b > 0 && rng.uniform() < 0.3) continue;
          const bool lastBar = (bar % 4) == 3;
          const int d = lastBar && b == 0 ? deg : tones[rng.next() % 3];
          const double hold = (lastBar || b == 3) ? beat * 1.8 : beat * 0.9;
          const double hz = midiToHz(root + 12 + degreeToSemis(harm, d));
          std::vector<float> note;
          if (score.lead == "piano") note = piano(hz, hold, sr);
          else if (score.lead == "pluck") note = pluck(hz, hold, sr, rng);
          else if (score.lead == "flute") note = flute(hz, hold, sr, rng);
          else if (score.lead == "synth") note = synthLead(hz, hold, sr);
          else note = bell(hz, hold, sr);
          dry.add(at(t0 + b * beat), note, 0.22 * mv, 0.15);
          wet.add(at(t0 + b * beat), note, 0.2 * mv, 0.15);
          if (lastBar) break;
        }
      }
    }
    if (!score.melody.empty() && score.lead != "none") {
      const double loopBeats = bars * 4.0;
      for (const auto& m : score.melody) {
        if (m.beat >= loopBeats) continue;
        const double hz = midiToHz(root + 12 + degreeToSemis(mel, m.degree));
        const double hold = m.beats * beat * 0.95;
        std::vector<float> note;
        if (score.lead == "piano") note = piano(hz, hold, sr);
        else if (score.lead == "pluck") note = pluck(hz, hold, sr, rng);
        else if (score.lead == "flute") note = flute(hz, hold, sr, rng);
        else if (score.lead == "synth") note = synthLead(hz, hold, sr);
        else note = bell(hz, hold, sr);
        dry.add(at(m.beat * beat), note, 0.24 * mv, 0.15);
        wet.add(at(m.beat * beat), note, 0.2 * mv, 0.15);
      }
    }
  }

  if (opt.effects) {
    for (const auto& fx : score.effects) {
      if (fx.time >= seconds) continue;
      double pan = 0;
      const auto sound = effect(fx.type, fx.length, rootHz, sr, rng, pan);
      // A riser builds up to its moment, so it ends at fx.time instead of starting there.
      const double at = fx.type == "rise" ? fx.time - double(sound.size()) / sr : fx.time;
      const long start = static_cast<long>(std::floor(at * sr));
      dry.add(start, sound, 0.55 * fx.volume, pan);
      wet.add(start, sound, 0.12 * fx.volume, pan);
    }
  }

  // A small room: a few feedback delays per side (Schroeder style), wrapped like the rest.
  std::vector<float> outL(dry.l), outR(dry.r);
  const int combs[] = {1557, 1617, 1491, 1422};
  for (int side = 0; side < 2; ++side) {
    const auto& in = side ? wet.r : wet.l;
    auto& out = side ? outR : outL;
    for (int c : combs) {
      const size_t d = static_cast<size_t>(c * sr / 44100.0) + (side ? 23 : 0);
      std::vector<float> buf(in.size(), 0.f);
      float damp = 0;
      // Two passes so the tail from the end wraps into the start for a seamless loop.
      for (int pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < in.size(); ++i) {
          const float delayed = buf[(i + in.size() - d % in.size()) % in.size()];
          damp = 0.35f * damp + 0.65f * delayed;  // darker repeats, like a real room
          buf[i] = in[i] + 0.8f * damp;
        }
      for (size_t i = 0; i < in.size(); ++i) out[i] += 0.12f * buf[i];
    }
  }

  // Bring every soundtrack to the same comfortable loudness, without clipping.
  float peak = 0;
  for (size_t i = 0; i < n; ++i) peak = std::max({peak, std::fabs(outL[i]), std::fabs(outR[i])});
  const float gain = peak > 1e-6f ? 0.85f / peak : 0.f;
  std::vector<float> stereo(n * 2);
  for (size_t i = 0; i < n; ++i) {
    stereo[2 * i] = outL[i] * gain;
    stereo[2 * i + 1] = outR[i] * gain;
  }
  return stereo;
}

void writeWav(const std::string& path, const std::vector<float>& stereo, int sampleRate) {
  std::ofstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("Could not write " + path);
  const uint32_t dataBytes = static_cast<uint32_t>(stereo.size() * 2);
  auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((v >> (8 * i)) & 0xff)); };
  auto u16 = [&](uint16_t v) { f.put(static_cast<char>(v & 0xff)); f.put(static_cast<char>(v >> 8)); };
  f.write("RIFF", 4);
  u32(36 + dataBytes);
  f.write("WAVEfmt ", 8);
  u32(16);
  u16(1);  // PCM
  u16(2);  // stereo
  u32(static_cast<uint32_t>(sampleRate));
  u32(static_cast<uint32_t>(sampleRate * 4));
  u16(4);
  u16(16);
  f.write("data", 4);
  u32(dataBytes);
  std::vector<char> pcm(stereo.size() * 2);
  for (size_t i = 0; i < stereo.size(); ++i) {
    const int v = static_cast<int>(std::lround(std::clamp(stereo[i], -1.f, 1.f) * 32767));
    pcm[2 * i] = static_cast<char>(v & 0xff);
    pcm[2 * i + 1] = static_cast<char>((v >> 8) & 0xff);
  }
  f.write(pcm.data(), static_cast<std::streamsize>(pcm.size()));
  if (!f) throw std::runtime_error("Could not write " + path);
}

}  // namespace studio
