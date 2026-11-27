#include "studio/roster.hpp"

#include <cctype>

namespace studio {

std::string_view modeName(Mode mode) { return mode == Mode::ThreeD ? "3d" : "2d"; }

std::optional<Mode> parseMode(std::string_view text) {
  std::string lower;
  for (char c : text) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower == "2d") return Mode::TwoD;
  if (lower == "3d") return Mode::ThreeD;
  return std::nullopt;
}

const Stage& director() {
  static const Stage d{StageId::Director, "director", "Director",
                       "Turns the rough prompt into a detailed creative brief.", true};
  return d;
}

const std::array<Stage, 5>& agents() {
  static const std::array<Stage, 5> list{{
      {StageId::Storyboard, "storyboard", "Storyboard", "Breaks the brief into scenes and timing."},
      {StageId::Designer, "designer", "Designer", "Decides the characters, colors and objects."},
      {StageId::Animator, "animator", "Animator", "Writes the movement and keyframes."},
      {StageId::Renderer, "renderer", "Renderer", "Turns the plan into working animation code."},
      {StageId::Reviewer, "reviewer", "Reviewer",
       "Checks the result and sends fixes back if something looks off."},
  }};
  return list;
}

const Stage& stage(StageId id) {
  if (id == StageId::Director) return director();
  for (const auto& s : agents())
    if (s.id == id) return s;
  return director();
}

}  // namespace studio
