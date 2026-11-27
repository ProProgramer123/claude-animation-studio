#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace studio {

/** Pulls the first JSON object out of a model reply, tolerating ```json fences and stray prose.
 *  Throws std::runtime_error naming the agent when there is no valid JSON. */
nlohmann::json extractJson(std::string_view text, std::string_view agent);

/** Pulls an HTML document out of a model reply, with or without a ```html fence. */
std::string extractHtml(std::string_view text);

}  // namespace studio
