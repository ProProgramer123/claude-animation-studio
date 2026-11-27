#include "studio/mock_runner.hpp"

#include <nlohmann/json.hpp>

namespace studio {

std::string MockRunner::run(const AgentRequest& request) {
  calls.push_back(request);
  const bool is3d = request.prompt.find("Mode: 3D") != std::string::npos;
  using nlohmann::json;
  switch (request.agent) {
    case StageId::Director:
      return "# Brief\nA red ball bounces three times across a soft blue background, then rests.";
    case StageId::Storyboard:
      return json{{"title", "Bouncing ball"},
                  {"duration_seconds", 4},
                  {"scenes", json::array({{{"id", "s1"}, {"start", 0}, {"end", 4},
                                           {"description", "Ball bounces left to right"}}})}}
          .dump();
    case StageId::Designer:
      return json{{"background", "#cfe8ff"},
                  {"palette", {"#e63946", "#cfe8ff"}},
                  {"elements", json::array({{{"id", "ball"}, {"name", "Ball"},
                                             {"description", "Glossy red ball"}, {"colors", {"#e63946"}}}})}}
          .dump();
    case StageId::Animator:
      return json{{"loop", true},
                  {"tracks", json::array({{{"element", "ball"},
                                           {"keyframes", json::array({
                                                {{"t", 0}, {"props", {{"x", 0}, {"y", 0}}}},
                                                {{"t", 2}, {"props", {{"x", 50}, {"y", 100}}}},
                                                {{"t", 4}, {"props", {{"x", 100}, {"y", 0}}}},
                                            })}}})}}
          .dump();
    case StageId::Renderer:
      return is3d ? "```html\n<!doctype html><html><body><script type=\"module\">import * as THREE from \"three\";"
                    "</script></body></html>\n```"
                  : "```html\n<!doctype html><html><body><svg viewBox=\"0 0 100 100\"><circle r=\"5\" "
                    "fill=\"#e63946\"/></svg></body></html>\n```";
    case StageId::Reviewer: {
      reviewCount_ += 1;
      const bool approved = reviewCount_ > rejectFirstReviews_;
      return approved ? json{{"approved", true}, {"issues", json::array()}}.dump()
                      : json{{"approved", false}, {"issues", {"Ball never touches the ground."}}}.dump();
    }
  }
  return {};
}

}  // namespace studio
