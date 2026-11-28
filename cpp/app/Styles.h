#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One-click choices that turn a short idea into a complete request for the director.

struct StylePreset {
  QString name;
  QString instruction;  // empty for "Let the director choose"
};

inline const QVector<StylePreset>& stylePresets() {
  static const QVector<StylePreset> list{
      {"Let the director choose", ""},
      {"Cartoon", "Bold outlines, bright flat colors and bouncy, exaggerated cartoon motion."},
      {"Watercolor", "Soft watercolor look: gentle washes of color, paper texture, slow calm motion."},
      {"Pixel art", "Retro pixel-art look with a limited palette and crisp, chunky pixels."},
      {"Neon", "Glowing neon lines on a dark background, with a synthwave feel."},
      {"Flat motion graphics", "Clean flat shapes, smooth eased transitions, like a modern explainer video."},
      {"Paper cut-out", "Layered paper cut-out look with soft drop shadows between layers."},
      {"Low-poly", "Low-poly faceted shapes with simple, soft lighting."},
      {"Realistic lighting", "Rich, realistic lighting and materials, with a cinematic camera."},
  };
  return list;
}

struct LengthPreset {
  QString name;
  int seconds;  // 0: let the director choose; -1: ask for a custom length
};

inline const QVector<LengthPreset>& lengthPresets() {
  static const QVector<LengthPreset> list{{"Any length", 0},     {"Short (5 s)", 5}, {"Medium (10 s)", 10},
                                          {"Long (15 s)", 15},   {"Extra long (30 s)", 30},
                                          {"Custom…", -1}};
  return list;
}

/** Ideas for the "Surprise me" button. */
inline const QStringList& sampleIdeas() {
  static const QStringList list{
      "a paper boat sailing across a sunset sea",
      "a lighthouse on a rocky island at night, its beam sweeping over the waves",
      "a red kite dancing in the wind over green hills",
      "a tiny robot watering a flower that grows into a tree",
      "fireflies lighting up a forest clearing at dusk",
      "a hot air balloon rising through fluffy clouds",
      "a cat chasing a ball of yarn across a wooden floor",
      "planets orbiting a glowing sun",
      "a rocket launching into a starry sky",
      "autumn leaves falling and swirling in the wind",
      "a jellyfish drifting through a deep blue ocean",
      "a city skyline going from day to night as the lights turn on",
  };
  return list;
}

/** The request sent to the director: the idea, plus the chosen style. (The length is passed
 *  to the engine separately, so the soundtrack is made the same length.) */
inline QString buildPrompt(const QString& idea, const StylePreset& style) {
  QString prompt = idea.trimmed();
  if (!style.instruction.isEmpty()) prompt += "\n\nStyle: " + style.instruction;
  return prompt;
}
