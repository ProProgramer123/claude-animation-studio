#include "studio/pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "studio/audio.hpp"
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

const char* kPlayerMarker = "<!-- studio-soundtrack -->";

}  // namespace

std::string addSoundtrackPlayer(const std::string& html, double duration) {
  if (html.find(kPlayerMarker) != std::string::npos) return html;
  // Plays soundtrack.wav in a loop from the moment the page starts, so it stays in step with
  // the animation. Browsers may block sound until a click; then the button says so.
  const std::string player = std::string(kPlayerMarker) + R"(
<style>#studio-sound{position:fixed;right:12px;bottom:12px;z-index:2147483647;font:13px system-ui,sans-serif;
border:0;border-radius:16px;padding:6px 11px;background:rgba(0,0,0,.45);color:#fff;cursor:pointer;opacity:.75}
#studio-sound:hover{opacity:1}</style>
<script>(() => {
  const loop = )" + std::to_string(duration) + R"(, start = performance.now();
  const audio = new Audio('soundtrack.wav');
  audio.loop = true;
  const button = document.createElement('button');
  button.id = 'studio-sound';
  let on = false;
  const show = () => { button.textContent = on ? '\u{1F50A} Sound on' : '\u{1F507} Sound off'; };
  const play = () => {
    audio.currentTime = ((performance.now() - start) / 1000) % loop;
    return audio.play().then(() => { on = true; show(); });
  };
  const mute = () => { audio.pause(); on = false; show(); };
  button.onclick = () => { if (on) mute(); else play().catch(() => {}); };
  window.studioSound = { audio, play, mute };
  const attach = () => {
    document.body.appendChild(button);
    show();
    play().catch(() => { button.textContent = '\u{1F508} Click for sound'; });
  };
  document.body ? attach() : addEventListener('DOMContentLoaded', attach);
})();</script>
)";
  std::string lower = html;
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const size_t body = lower.rfind("</body>");
  if (body == std::string::npos) return html + "\n" + player;
  return html.substr(0, body) + player + html.substr(body);
}

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

  const double length = o.lengthSeconds > 0 ? std::clamp(o.lengthSeconds, 2.0, 60.0) : 0.0;
  std::string idea = o.prompt;
  if (length > 0) {
    char buf[112];
    std::snprintf(buf, sizeof buf, "\n\nLength: exactly %g seconds (this overrides the usual 3 to 15 seconds).", length);
    idea += buf;
  }

  std::string brief = ask(StageId::Director, "Client's idea:\n" + idea, "Director is refining the prompt");
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

  PipelineResult result;
  double storyLength = 0;
  if (storyboard.is_object() && storyboard.contains("duration_seconds") && storyboard["duration_seconds"].is_number())
    storyLength = storyboard["duration_seconds"].get<double>();
  result.durationSeconds = length > 0 ? length : std::clamp(storyLength > 0 ? storyLength : 8.0, 2.0, 60.0);

  // The Sound agent scores the animation, and the built-in synthesizer turns the score into a WAV.
  if (o.sound.any()) {
    const std::string wants = o.sound.music && o.sound.effects ? "music and sound effects"
                              : o.sound.music                  ? "music only (no sound effects)"
                                                               : "sound effects only (no music)";
    char len[64];
    std::snprintf(len, sizeof len, "%g", result.durationSeconds);
    const json score = extractJson(
        ask(StageId::Sound,
            "Brief:\n" + brief + "\n\nStoryboard:\n" + storyboard.dump() + "\n\nKeyframes:\n" + animation.dump() +
                "\n\nLength: " + len + " seconds. The client wants " + wants + ".",
            "Sound is writing the music and sound effects"),
        "sound");
    save("score.json", pretty(score));
    SoundtrackOptions so;
    so.music = o.sound.music;
    so.effects = o.sound.effects;
    const auto samples = renderSoundtrack(parseScore(score), result.durationSeconds, so);
    result.soundtrackPath = o.outputDir / "soundtrack.wav";
    writeWav(result.soundtrackPath->string(), samples);
  }

  char lengthLine[192];
  std::snprintf(lengthLine, sizeof lengthLine,
                "\n\nTotal length: %g seconds. The animation must run exactly this long and then loop from the start%s.",
                result.durationSeconds, result.soundtrackPath ? ", so it stays in time with its soundtrack" : "");
  const std::string plan = "Brief:\n" + brief + "\n\nStoryboard:\n" + storyboard.dump() + "\n\nDesign:\n" +
                           design.dump() + "\n\nKeyframes:\n" + animation.dump() + lengthLine;
  const auto withSound = [&](const std::string& page) {
    return result.soundtrackPath ? addSoundtrackPlayer(page, result.durationSeconds) : page;
  };
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
    save("index.html", withSound(html));

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
                                  {"duration_seconds", result.durationSeconds},
                                  {"sound", result.soundtrackPath.has_value()},
                                  {"revisions", result.revisions},
                                  {"open_issues", result.openIssues}}));
  return result;
}

}  // namespace studio
