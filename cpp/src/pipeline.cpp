#include "studio/pipeline.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "studio/parse.hpp"
#include "studio/prompts.hpp"

namespace studio {
namespace {

using nlohmann::json;
namespace fs = std::filesystem;

void writeFile(const fs::path& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("Could not write " + path.string());
  f << content;
}

std::string pretty(const json& j) { return j.dump(2) + "\n"; }

std::string describeCheck(const CheckResult& r) {
  std::string out = "Browser check (the page was opened in a browser):\n";
  if (r.errors.empty()) {
    out += "- No errors.\n";
  } else {
    for (const auto& e : r.errors) out += "- " + e + "\n";
  }
  out += r.isStatic ? "- Nothing moved between two screenshots taken one second apart."
                    : "- The animation is moving.";
  return out;
}

Review normalizeReview(const json& raw) {
  Review r;
  if (!raw.is_object()) return r;
  r.approved = raw.contains("approved") && raw["approved"].is_boolean() && raw["approved"].get<bool>();
  if (raw.contains("issues") && raw["issues"].is_array()) {
    for (const auto& i : raw["issues"]) r.issues.push_back(i.is_string() ? i.get<std::string>() : i.dump());
  }
  return r;
}

/** Browser errors and a frozen page are real problems even if the reviewer missed them. */
Review mergeBrowserFindings(Review review, const std::optional<CheckResult>& result) {
  if (!result) return review;
  const bool blocked = !result->errors.empty() || result->isStatic;
  if (!blocked) return review;
  for (const auto& e : result->errors) review.issues.push_back("Fix this browser error: " + e);
  if (result->isStatic)
    review.issues.push_back("Nothing moves on screen: make sure the animation starts automatically and keeps playing.");
  review.approved = false;
  return review;
}

}  // namespace

PipelineResult runPipeline(const PipelineOptions& o) {
  if (!o.runner) throw std::invalid_argument("runPipeline needs a runner");
  fs::create_directories(o.outputDir);
  const auto save = [&](const std::string& name, const std::string& content) {
    writeFile(o.outputDir / name, content);
  };
  const auto ask = [&](StageId agent, const std::string& context, const std::string& label) {
    if (o.cancelled && o.cancelled()) throw Cancelled();
    if (o.onProgress) o.onProgress(agent, label);
    return o.runner->run({agent, systemPrompt(agent), modeNote(o.mode) + "\n\n" + context});
  };

  save("prompt.txt", o.prompt + "\n");

  std::string brief = ask(StageId::Director, "Client's idea:\n" + o.prompt, "Director is refining the prompt");
  {
    const auto a = brief.find_first_not_of(" \t\r\n");
    brief = a == std::string::npos ? "" : brief.substr(a, brief.find_last_not_of(" \t\r\n") - a + 1);
  }
  save("brief.md", brief + "\n");

  const json storyboard = extractJson(
      ask(StageId::Storyboard, "Brief:\n" + brief, "Storyboard is planning scenes and timing"), "storyboard");
  save("storyboard.json", pretty(storyboard));

  const json design = extractJson(
      ask(StageId::Designer, "Brief:\n" + brief + "\n\nStoryboard:\n" + storyboard.dump(),
          "Designer is choosing characters, colors and objects"),
      "designer");
  save("design.json", pretty(design));

  const json animation = extractJson(
      ask(StageId::Animator,
          "Brief:\n" + brief + "\n\nStoryboard:\n" + storyboard.dump() + "\n\nDesign:\n" + design.dump(),
          "Animator is writing keyframes"),
      "animator");
  save("animation.json", pretty(animation));

  const std::string plan = "Brief:\n" + brief + "\n\nStoryboard:\n" + storyboard.dump() + "\n\nDesign:\n" +
                           design.dump() + "\n\nKeyframes:\n" + animation.dump();

  PipelineResult result;
  result.mode = o.mode;
  result.outputDir = o.outputDir;
  result.htmlPath = o.outputDir / "index.html";
  result.title = storyboard.is_object() && storyboard.contains("title") && storyboard["title"].is_string()
                     ? storyboard["title"].get<std::string>()
                     : "Untitled animation";

  std::string html = extractHtml(ask(StageId::Renderer, plan, "Renderer is building the animation"));
  Review review;
  json reviews = json::array();

  for (;;) {
    save("index.html", html);

    std::optional<CheckResult> browser;
    if (o.checker) {
      if (o.onProgress) o.onProgress(StageId::Reviewer, "Opening the animation in a browser");
      browser = o.checker->check(result.htmlPath);
      if (browser && !browser->screenshotPng.empty()) {
        result.previewPath = o.outputDir / "preview.png";
        writeFile(*result.previewPath, browser->screenshotPng);
      }
    }

    review = normalizeReview(extractJson(
        ask(StageId::Reviewer, plan + "\n\nHTML:\n" + html + (browser ? "\n\n" + describeCheck(*browser) : ""),
            "Reviewer is checking the result"),
        "reviewer"));
    review = mergeBrowserFindings(review, browser);

    json entry{{"approved", review.approved}, {"issues", review.issues}};
    if (browser) entry["browser"] = {{"errors", browser->errors}, {"static", browser->isStatic}};
    reviews.push_back(entry);
    if (review.approved || result.revisions >= o.maxRevisions) break;

    result.revisions += 1;
    std::string fixes;
    for (const auto& i : review.issues) fixes += "- " + i + "\n";
    html = extractHtml(ask(StageId::Renderer,
                           plan + "\n\nYour previous HTML:\n" + html + "\n\nThe reviewer asked for these fixes:\n" +
                               fixes + "\nReturn the full corrected HTML.",
                           "Renderer is applying fixes (revision " + std::to_string(result.revisions) + ")"));
  }

  save("review.json", pretty(json{{"revisions", result.revisions}, {"reviews", reviews}}));
  result.approved = review.approved;
  if (!review.approved) result.openIssues = review.issues;
  // A short summary that apps read to list past animations.
  save("result.json", pretty(json{{"title", result.title},
                                  {"mode", std::string(modeName(o.mode))},
                                  {"approved", result.approved},
                                  {"revisions", result.revisions},
                                  {"open_issues", result.openIssues}}));
  return result;
}

}  // namespace studio
