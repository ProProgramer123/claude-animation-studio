#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace studio {

enum class Mode { TwoD, ThreeD };

/** "2d" or "3d". */
std::string_view modeName(Mode mode);
/** Parses "2d"/"3d" (any case). Returns nullopt for anything else. */
std::optional<Mode> parseMode(std::string_view text);

enum class StageId { Director, Storyboard, Designer, Animator, Sound, Renderer, Reviewer };

struct Stage {
  StageId id;
  std::string_view key;   // "storyboard", used in logs and file names
  std::string_view name;  // "Storyboard", shown to people
  std::string_view role;
  bool hidden = false;
};

/** The hidden director: people never see its output directly, only the finished animation. */
const Stage& director();

/** The six visible agents, in the order they work. */
const std::array<Stage, 6>& agents();

/** Looks up any stage, including the director. */
const Stage& stage(StageId id);

}  // namespace studio
