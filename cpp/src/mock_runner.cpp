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
    case StageId::Sound:
      return json{{"music", {{"mood", "playful"}, {"tempo_bpm", 110}, {"key", "C"}, {"scale", "major"},
                             {"chords", {"I", "V", "vi", "IV"}},
                             {"instruments", {{"lead", "bell"}, {"pad", "warm"}, {"bass", "soft"}, {"drums", "soft"}}},
                             {"volume", 0.6}}},
                  {"effects", json::array({{{"t", 1}, {"type", "boing"}, {"volume", 0.8}},
                                           {{"t", 3}, {"type", "boing"}, {"volume", 0.8}}})}}
          .dump();
    case StageId::Renderer:
      return is3d ? "```html\n<!doctype html><html><body><script type=\"module\">import * as THREE from \"three\";"
                    "</script></body></html>\n```"
                  : "```html\n<!doctype html><html><head><style>html,body{margin:0;height:100%;background:#cfe8ff}"
                    "svg{display:block;width:100%;height:100%}</style></head><body><svg viewBox=\"0 0 100 60\" preserveAspectRatio=\"xMidYMid slice\">"
                    "<rect y=\"52\" width=\"100\" height=\"8\" fill=\"#8fbf7a\"/>"
                    "<circle cx=\"10\" cy=\"45\" r=\"6\" fill=\"#e63946\">"
                    "<animate attributeName=\"cx\" values=\"10;90;10\" dur=\"4s\" repeatCount=\"indefinite\"/>"
                    "<animate attributeName=\"cy\" values=\"45;12;45;12;45\" dur=\"2s\" repeatCount=\"indefinite\"/>"
                    "</circle></svg></body></html>\n```";
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
