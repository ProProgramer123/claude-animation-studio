#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace studio {

/** A short, file-safe name made from the prompt, e.g. "a-red-ball-bounces". */
std::string slugify(std::string_view prompt);

/** Folder for a new animation: <base>/<UTC timestamp>-<slug>.
 *  `base` defaults to $ANIMATION_STUDIO_OUTPUT_DIR, then "./animations". */
std::filesystem::path animationDir(std::string_view prompt, std::filesystem::path base = {});

}  // namespace studio
