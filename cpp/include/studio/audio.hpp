#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace studio {

/** One timed sound effect from the Sound agent's score. */
struct SoundEffect {
  double time = 0;      // seconds from the start
  std::string type;     // one of soundEffectTypes()
  double volume = 0.8;  // 0..1
  double length = 0;    // seconds, for sounds that last (wind, rain, rumble); 0 = natural length
};

/** One melody note. `beat` counts from 0 at the start; `degree` is a scale step (0 = key note,
 *  7 = an octave up, negative = below). */
struct MelodyNote {
  double beat = 0;
  int degree = 0;
  double beats = 1;
};

/** The soundtrack, as the Sound agent describes it. Everything has a safe default, so a
 *  sparse or slightly wrong score still sounds musical. */
struct Score {
  bool music = true;
  double tempo = 96;           // beats per minute, 50..180
  std::string key = "C";       // C, C#, Db, D, … B
  std::string scale = "major"; // major, minor, pentatonic, minor-pentatonic, dorian, lydian
  std::vector<std::string> chords{"I", "V", "vi", "IV"};  // Roman numerals, one per bar, repeating
  std::string lead = "bell";   // bell, piano, pluck, flute, synth, none
  std::string pad = "warm";    // warm, strings, none
  std::string bass = "soft";   // soft, synth, none
  std::string drums = "none";  // none, soft, beat
  std::vector<MelodyNote> melody;  // empty: one is written from the chords
  double musicVolume = 0.6;
  std::vector<SoundEffect> effects;
};

/** The sound effects the synthesizer knows. */
const std::vector<std::string>& soundEffectTypes();
const std::vector<std::string>& leadInstruments();

/** Reads the Sound agent's JSON, keeping only what is valid. Never throws on odd values. */
Score parseScore(const nlohmann::json& j);

struct SoundtrackOptions {
  bool music = true;
  bool effects = true;
  int sampleRate = 44100;
};

/** Renders the score to interleaved stereo samples in -1..1, exactly `seconds` long.
 *  The tempo is nudged so a whole number of bars fills the length, so the music loops cleanly
 *  with an animation that loops. */
std::vector<float> renderSoundtrack(const Score& score, double seconds, const SoundtrackOptions& options = {});

/** Writes interleaved stereo samples as a 16-bit PCM WAV file. Throws on I/O errors. */
void writeWav(const std::string& path, const std::vector<float>& stereo, int sampleRate = 44100);

/** The tempo actually used for a given length (see renderSoundtrack). */
double loopTempo(double tempo, double seconds);

}  // namespace studio
