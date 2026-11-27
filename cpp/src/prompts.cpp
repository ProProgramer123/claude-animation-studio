#include "studio/prompts.hpp"

namespace studio {
namespace {

const std::string kJsonOnly = "Reply with one JSON object only, no prose and no code fences.";

std::string join(std::initializer_list<std::string> parts) {
  std::string out;
  for (const auto& p : parts) {
    if (!out.empty()) out += ' ';
    out += p;
  }
  return out;
}

}  // namespace

const std::string& modeNote(Mode mode) {
  static const std::string twoD =
      "Mode: 2D. The final animation is a single self-contained HTML page using inline SVG "
      "(with CSS or Web Animations API) or a <canvas> with requestAnimationFrame. No external libraries.";
  static const std::string threeD =
      "Mode: 3D. The final animation is a single self-contained HTML page that renders a Three.js scene. "
      "Load Three.js with an import map pointing to https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.js "
      "and import it as `import * as THREE from \"three\"`.";
  return mode == Mode::ThreeD ? threeD : twoD;
}

const std::string& systemPrompt(StageId id) {
  static const std::string director = join({
      "You are the director of an animation studio. A client gives you a rough idea.",
      "Rewrite it as a detailed creative brief for your team: subject, setting, mood, style,",
      "key moments, pacing, total length (keep it between 3 and 15 seconds unless the client asks otherwise),",
      "and anything the client clearly expects. Fill gaps with sensible creative choices, but never",
      "contradict the client. Write the brief in Markdown, under 300 words.",
  });
  static const std::string storyboard = join({
      "You are the storyboard artist. Break the brief into scenes with exact timing in seconds.",
      kJsonOnly,
      R"(Shape: {"title": string, "duration_seconds": number, "scenes": [{"id": string, "start": number,)",
      R"("end": number, "description": string, "camera"?: string}]}. Scenes must cover 0 to duration_seconds with no gaps.)",
  });
  static const std::string designer = join({
      "You are the designer. Decide every visual element the storyboard needs: characters, objects,",
      "background, palette and lighting. Keep shapes simple enough to build in code.",
      kJsonOnly,
      R"(Shape: {"background": string, "palette": string[], "lighting"?: string, "elements": [{"id": string,)",
      R"("name": string, "description": string, "colors": string[], "size"?: string}]}.)",
  });
  static const std::string animator = join({
      "You are the animator. Using the storyboard timing and the designer's elements, write the motion",
      "as keyframe tracks. Use element ids from the design. Times are in seconds.",
      kJsonOnly,
      R"(Shape: {"loop": boolean, "tracks": [{"element": string, "easing"?: string, "keyframes":)",
      R"([{"t": number, "props": object}]}]}. Props are things like x, y, z, rotation, scale, opacity, color.)",
  });
  static const std::string renderer = join({
      "You are the renderer. Turn the storyboard, design and keyframes into one complete, working HTML file.",
      "It must run when opened directly in a modern browser, fill the window, start automatically,",
      "and follow the timing and keyframes closely. Put all CSS and JavaScript inline.",
      "Reply with the full HTML document inside a single ```html code block and nothing else.",
  });
  static const std::string reviewer = join({
      "You are the reviewer. Read the brief, the plan and the HTML. Check that the code would run without errors,",
      "that it matches the brief, storyboard, design and keyframes, and that it looks polished.",
      "Only flag real problems, and describe each fix precisely enough for the renderer to apply it.",
      kJsonOnly,
      R"(Shape: {"approved": boolean, "issues": string[]}. Approve when there are no real problems.)",
  });

  switch (id) {
    case StageId::Director: return director;
    case StageId::Storyboard: return storyboard;
    case StageId::Designer: return designer;
    case StageId::Animator: return animator;
    case StageId::Renderer: return renderer;
    case StageId::Reviewer: return reviewer;
  }
  return director;
}

}  // namespace studio
