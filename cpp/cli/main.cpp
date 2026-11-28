// animation-studio: make a 2D or 3D animation from one sentence.
//
//   animation-studio "a paper boat sails through a storm"
//   animation-studio --mode 3d "a lighthouse at night"
//   animation-studio --runner mock "anything"        # offline, canned agents

#include <cstdlib>
#include <iostream>
#include <string>

#include "studio/claude_code_runner.hpp"
#include "studio/paths.hpp"
#include "studio/pipeline.hpp"
#include "studio/roster.hpp"
#include "studio/runner.hpp"

namespace {

void usage(std::ostream& out) {
  out << "Usage: animation-studio [options] \"what the animation should show\"\n\n"
         "Options:\n"
         "  --mode 2d|3d        2D (SVG/canvas) or 3D (Three.js). Default: 2d\n"
         "  --length SECONDS    Exact length, 2 to 60. Default: the director chooses\n"
         "  --no-music          Sound effects only\n"
         "  --no-effects        Music only\n"
         "  --no-sound          No soundtrack at all\n"
         "  --out DIR           Folder for the animation. Default: ./animations/<time>-<prompt>\n"
         "  --runner NAME       claude-code (default, uses your Claude Code login) or mock\n"
         "  --model NAME        Model alias or id passed to Claude Code\n"
         "  --revisions N       How many times the reviewer may send work back. Default: 2\n"
         "  --agents            List the studio's agents and exit\n"
         "  --version           Print the version and exit\n"
         "  -h, --help          Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
  using namespace studio;
  std::string prompt, runnerName, model, out;
  Mode mode = Mode::TwoD;
  int revisions = 2;
  double length = 0;
  studio::SoundOptions sound;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto value = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << flag << " needs a value\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "-h" || arg == "--help") { usage(std::cout); return 0; }
    if (arg == "--version") { std::cout << "animation-studio 0.3.0\n"; return 0; }
    if (arg == "--agents") {
      std::cout << director().name << " (hidden): " << director().role << "\n";
      for (const auto& a : agents()) std::cout << a.name << ": " << a.role << "\n";
      return 0;
    }
    if (arg == "--mode") {
      const auto m = parseMode(value("--mode"));
      if (!m) { std::cerr << "--mode must be 2d or 3d\n"; return 2; }
      mode = *m;
    } else if (arg == "--length") {
      length = std::atof(value("--length").c_str());
      if (length < 2 || length > 60) { std::cerr << "--length must be between 2 and 60 seconds\n"; return 2; }
    } else if (arg == "--no-music") {
      sound.music = false;
    } else if (arg == "--no-effects") {
      sound.effects = false;
    } else if (arg == "--no-sound") {
      sound = {false, false};
    } else if (arg == "--out") {
      out = value("--out");
    } else if (arg == "--runner") {
      runnerName = value("--runner");
    } else if (arg == "--model") {
      model = value("--model");
    } else if (arg == "--revisions") {
      revisions = std::atoi(value("--revisions").c_str());
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "Unknown option " << arg << "\n\n";
      usage(std::cerr);
      return 2;
    } else {
      prompt += (prompt.empty() ? "" : " ") + arg;
    }
  }
  if (prompt.empty()) { usage(std::cerr); return 2; }

  try {
    std::unique_ptr<AgentRunner> runner;
    if (!model.empty() && (runnerName.empty() || runnerName == "claude-code")) {
      ClaudeCodeRunnerOptions opts;
      opts.model = model;
      runner = std::make_unique<ClaudeCodeRunner>(opts);
    } else {
      runner = createRunner(runnerName);
    }

    PipelineOptions options;
    options.prompt = prompt;
    options.mode = mode;
    options.runner = runner.get();
    options.maxRevisions = revisions;
    options.lengthSeconds = length;
    options.sound = sound;
    options.outputDir = out.empty() ? animationDir(prompt) : std::filesystem::absolute(out);
    options.onProgress = [](StageId, const std::string& message) { std::cerr << "• " << message << "\n"; };

    const PipelineResult r = runPipeline(options);
    std::cout << "Made a " << (mode == Mode::ThreeD ? "3D" : "2D") << " animation: \"" << r.title << "\"\n"
              << "Open it: " << r.htmlPath.string() << "\n";
    if (r.soundtrackPath) std::cout << "Soundtrack: " << r.soundtrackPath->string() << "\n";
    if (r.approved) {
      std::cout << "Reviewer approved it";
      if (r.revisions) std::cout << " after " << r.revisions << " revision(s)";
      std::cout << ".\n";
    } else {
      std::cout << "Reviewer still had concerns after " << r.revisions << " revision(s):\n";
      for (const auto& i : r.openIssues) std::cout << "  - " << i << "\n";
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "The studio could not finish: " << e.what() << "\n";
    return 1;
  }
}
