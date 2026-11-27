#pragma once

#include <string>

#include "studio/roster.hpp"

namespace studio {

/** The role and rules each agent works under. */
const std::string& systemPrompt(StageId id);

/** The line that tells every agent which kind of animation is being made. */
const std::string& modeNote(Mode mode);

}  // namespace studio
