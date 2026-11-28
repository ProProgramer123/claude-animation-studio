// Engine tests. No framework: each TEST registers itself and main() runs them all.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "studio/claude_code_runner.hpp"
#include "studio/audio.hpp"
#include "studio/gif.hpp"
#include "studio/mock_runner.hpp"
#include "studio/parse.hpp"
#include "studio/paths.hpp"
#include "studio/pipeline.hpp"
#include "studio/process.hpp"
#include "studio/prompts.hpp"

namespace fs = std::filesystem;
using namespace studio;

namespace {

struct Test { const char* name; std::function<void()> fn; };
std::vector<Test>& registry() { static std::vector<Test> t; return t; }
struct Register { Register(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };

#define TEST(name) \
  static void name(); \
  static Register reg_##name(#name, name); \
  static void name()

#define CHECK(cond) \
  do { if (!(cond)) { std::ostringstream m; m << __FILE__ << ":" << __LINE__ << ": " #cond; throw Failure(m.str()); } } while (0)

#define CHECK_THROWS(expr, needle) \
  do { bool threw = false; try { expr; } catch (const std::exception& e) { threw = true; \
    CHECK(std::string(e.what()).find(needle) != std::string::npos); } CHECK(threw); } while (0)

fs::path tempDir(const std::string& name) {
  auto dir = fs::temp_directory_path() / ("studio-test-" + name);
  fs::remove_all(dir);
  return dir;
}

std::string readFile(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}

class FakeChecker : public AnimationChecker {
 public:
  std::vector<CheckResult> results;  // one per call; last one repeats
  int calls = 0;
  std::string name() const override { return "fake"; }
  std::optional<CheckResult> check(const fs::path& html) override {
    CHECK(fs::exists(html));
    const auto& r = results[std::min<size_t>(calls, results.size() - 1)];
    ++calls;
    return r;
  }
};

}  // namespace

// ---- parsing ---------------------------------------------------------------

TEST(extractJson_plain) {
  CHECK(extractJson(R"({"a": 1})", "x")["a"] == 1);
}

TEST(extractJson_fenced_with_prose) {
  const auto j = extractJson("Sure! Here it is:\n```json\n{\"title\": \"Boat\"}\n```\nEnjoy.", "x");
  CHECK(j["title"] == "Boat");
}

TEST(extractJson_unlabelled_fence) {
  CHECK(extractJson("```\n{\"ok\": true}\n```", "x")["ok"] == true);
}

TEST(extractJson_errors_name_agent) {
  CHECK_THROWS(extractJson("no json here", "storyboard"), "storyboard agent did not return JSON");
  CHECK_THROWS(extractJson("{not: valid}", "designer"), "designer agent returned invalid JSON");
}

TEST(extractHtml_fenced) {
  CHECK(extractHtml("Here:\n```html\n<!doctype html><html></html>\n```") == "<!doctype html><html></html>");
}

TEST(extractHtml_unfenced_with_prose) {
  CHECK(extractHtml("Okay, the page:\n<!DOCTYPE html><html><body></body></html>") ==
        "<!DOCTYPE html><html><body></body></html>");
}

TEST(extractHtml_uppercase_fence) {
  CHECK(extractHtml("```HTML\n<html></html>\n```") == "<html></html>");
}

// ---- roster, prompts, paths --------------------------------------------------

TEST(roster_has_hidden_director_and_five_agents) {
  CHECK(director().hidden);
  CHECK(agents().size() == 6);
  CHECK(agents().front().key == "storyboard");
  CHECK(agents().back().key == "reviewer");
}

TEST(modes_parse) {
  CHECK(parseMode("3D") == Mode::ThreeD);
  CHECK(parseMode("2d") == Mode::TwoD);
  CHECK(!parseMode("4d"));
  CHECK(modeNote(Mode::ThreeD).find("Three.js") != std::string::npos);
}

TEST(prompts_ask_for_json_where_needed) {
  for (auto id : {StageId::Storyboard, StageId::Designer, StageId::Animator, StageId::Reviewer})
    CHECK(systemPrompt(id).find("JSON object only") != std::string::npos);
  CHECK(systemPrompt(StageId::Renderer).find("```html") != std::string::npos);
}

TEST(slugify_prompts) {
  CHECK(slugify("A Paper Boat, in a storm!") == "a-paper-boat-in-a-storm");
  CHECK(slugify("!!!") == "animation");
  CHECK(slugify(std::string(100, 'a')).size() == 40);
}

TEST(animation_dir_uses_base_and_slug) {
  const auto dir = animationDir("Hello world", "base");
  CHECK(dir.is_absolute());
  CHECK(dir.parent_path().filename() == "base");
  const auto name = dir.filename().string();
  CHECK(name.size() > 20 && name.substr(name.size() - 11) == "hello-world");
}

// ---- pipeline ---------------------------------------------------------------

TEST(pipeline_runs_every_agent_and_saves_work) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "a bouncing ball";
  o.runner = &runner;
  o.outputDir = tempDir("pipeline");
  std::vector<std::string> progress;
  o.onProgress = [&](StageId, const std::string& m) { progress.push_back(m); };

  const auto r = runPipeline(o);
  CHECK(r.approved);
  CHECK(r.revisions == 0);
  CHECK(r.title == "Bouncing ball");
  CHECK(runner.calls.size() == 7);
  CHECK(runner.calls[0].agent == StageId::Director);
  CHECK(runner.calls[4].agent == StageId::Sound);
  CHECK(runner.calls[0].prompt.find("Mode: 2D") == 0);
  CHECK(runner.calls[0].prompt.find("a bouncing ball") != std::string::npos);
  for (const char* f : {"prompt.txt", "brief.md", "storyboard.json", "design.json", "animation.json",
                        "index.html", "review.json", "result.json", "score.json", "soundtrack.wav"})
    CHECK(fs::exists(o.outputDir / f));
  CHECK(readFile(r.htmlPath).find("<svg") != std::string::npos);
  CHECK(progress.size() == 7);
  CHECK(readFile(r.htmlPath).find("soundtrack.wav") != std::string::npos);
  CHECK(r.soundtrackPath && r.durationSeconds == 4);
  CHECK(progress.front() == "Director is refining the prompt");
  const auto summary = nlohmann::json::parse(readFile(o.outputDir / "result.json"));
  CHECK(summary["title"] == "Bouncing ball" && summary["mode"] == "2d" && summary["approved"] == true);
}

TEST(pipeline_3d_mode_reaches_every_agent) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "a lighthouse";
  o.mode = Mode::ThreeD;
  o.runner = &runner;
  o.outputDir = tempDir("pipeline3d");
  const auto r = runPipeline(o);
  for (const auto& c : runner.calls) CHECK(c.prompt.find("Mode: 3D") == 0);
  CHECK(readFile(r.htmlPath).find("three") != std::string::npos);
}

TEST(pipeline_sends_reviewer_issues_back_to_renderer) {
  MockRunner runner(1);
  PipelineOptions o;
  o.prompt = "ball";
  o.runner = &runner;
  o.outputDir = tempDir("revise");
  const auto r = runPipeline(o);
  CHECK(r.approved);
  CHECK(r.revisions == 1);
  CHECK(runner.calls.size() == 9);
  CHECK(runner.calls[7].agent == StageId::Renderer);
  CHECK(runner.calls[7].prompt.find("Ball never touches the ground.") != std::string::npos);
}

TEST(pipeline_stops_after_max_revisions) {
  MockRunner runner(10);
  PipelineOptions o;
  o.prompt = "ball";
  o.runner = &runner;
  o.maxRevisions = 2;
  o.outputDir = tempDir("maxrev");
  const auto r = runPipeline(o);
  CHECK(!r.approved);
  CHECK(r.revisions == 2);
  CHECK(r.openIssues.size() == 1);
  const auto review = nlohmann::json::parse(readFile(o.outputDir / "review.json"));
  CHECK(review["reviews"].size() == 3);
}

TEST(browser_errors_block_approval_and_reach_renderer) {
  MockRunner runner;
  FakeChecker checker;
  checker.results = {{{"ReferenceError: x is not defined"}, false, "PNG"}, {{}, false, "PNG"}};
  PipelineOptions o;
  o.prompt = "ball";
  o.runner = &runner;
  o.checker = &checker;
  o.outputDir = tempDir("checker");
  const auto r = runPipeline(o);
  CHECK(checker.calls == 2);
  CHECK(r.approved);
  CHECK(r.revisions == 1);
  CHECK(runner.calls[6].prompt.find("ReferenceError") != std::string::npos);  // reviewer saw it
  CHECK(runner.calls[7].prompt.find("Fix this browser error: ReferenceError") != std::string::npos);
  CHECK(r.previewPath && readFile(*r.previewPath) == "PNG");
}

TEST(frozen_page_blocks_approval) {
  MockRunner runner;
  FakeChecker checker;
  checker.results = {{{}, true, ""}};
  PipelineOptions o;
  o.prompt = "ball";
  o.runner = &runner;
  o.checker = &checker;
  o.maxRevisions = 0;
  o.outputDir = tempDir("frozen");
  const auto r = runPipeline(o);
  CHECK(!r.approved);
  CHECK(r.openIssues.size() == 1);
  CHECK(r.openIssues[0].find("Nothing moves") == 0);
  CHECK(!r.previewPath);
}

TEST(pipeline_can_be_cancelled) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "ball";
  o.runner = &runner;
  o.outputDir = tempDir("cancel");
  o.cancelled = [&] { return runner.calls.size() >= 2; };
  CHECK_THROWS(runPipeline(o), "cancelled");
  CHECK(runner.calls.size() == 2);
}

TEST(custom_length_reaches_director_and_soundtrack) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "ball";
  o.lengthSeconds = 25;
  o.runner = &runner;
  o.outputDir = tempDir("length");
  const auto r = runPipeline(o);
  CHECK(runner.calls[0].prompt.find("exactly 25 seconds") != std::string::npos);
  CHECK(runner.calls[5].prompt.find("Total length: 25 seconds") != std::string::npos);
  CHECK(r.durationSeconds == 25);
  // 16-bit stereo at 44.1 kHz: 4 bytes per sample frame, plus a 44-byte header.
  CHECK(fs::file_size(*r.soundtrackPath) == 44 + 25 * 44100 * 4);
}

TEST(no_sound_skips_the_sound_agent) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "ball";
  o.sound = {false, false};
  o.runner = &runner;
  o.outputDir = tempDir("nosound");
  const auto r = runPipeline(o);
  CHECK(runner.calls.size() == 6);
  CHECK(!r.soundtrackPath);
  CHECK(!fs::exists(o.outputDir / "soundtrack.wav"));
  CHECK(readFile(r.htmlPath).find("soundtrack.wav") == std::string::npos);
}

TEST(effects_only_is_asked_for) {
  MockRunner runner;
  PipelineOptions o;
  o.prompt = "ball";
  o.sound = {false, true};
  o.runner = &runner;
  o.outputDir = tempDir("fxonly");
  runPipeline(o);
  CHECK(runner.calls[4].prompt.find("sound effects only") != std::string::npos);
}

// ---- soundtrack ------------------------------------------------------------------

TEST(score_parsing_keeps_valid_parts) {
  const auto s = parseScore(nlohmann::json::parse(R"({
    "music": {"tempo_bpm": 999, "key": "F#", "scale": "minor", "chords": ["i", "VI"],
              "instruments": {"lead": "kazoo", "pad": "strings", "drums": "beat"},
              "melody": [{"beat": 0, "degree": 2, "beats": 2}, {"oops": 1}]},
    "effects": [{"t": 1.5, "type": "splash"}, {"t": 2, "type": "explosion"}, "junk"]})"));
  CHECK(s.tempo == 180);
  CHECK(s.key == "F#" && s.scale == "minor");
  CHECK(s.chords.size() == 2);
  CHECK(s.lead == "bell");  // unknown instrument falls back
  CHECK(s.pad == "strings" && s.drums == "beat");
  CHECK(s.melody.size() == 1 && s.melody[0].degree == 2);
  CHECK(s.effects.size() == 1 && s.effects[0].type == "splash" && s.effects[0].time == 1.5);
  CHECK(parseScore("not an object").music);  // defaults still make music
}

TEST(loop_tempo_fits_whole_bars) {
  for (double secs : {2.0, 5.0, 7.3, 12.0, 60.0}) {
    const double bars = loopTempo(100, secs) * secs / 240;
    CHECK(std::fabs(bars - std::round(bars)) < 1e-9);
  }
}

static double rms(const std::vector<float>& v, size_t from, size_t to) {
  double e = 0;
  for (size_t i = from; i < to; ++i) e += double(v[i]) * v[i];
  return std::sqrt(e / double(to - from));
}

TEST(soundtrack_is_the_right_length_and_never_clips) {
  Score score;
  score.drums = "beat";
  score.effects = {{1.0, "whoosh"}, {2.0, "chime"}};
  const auto stereo = renderSoundtrack(score, 3.0);
  CHECK(stereo.size() == size_t(3 * 44100 * 2));
  float peak = 0;
  for (float v : stereo) peak = std::max(peak, std::fabs(v));
  CHECK(peak > 0.5f && peak <= 0.86f);
}

TEST(every_effect_makes_sound_at_its_time) {
  for (const auto& type : soundEffectTypes()) {
    Score score;
    score.music = false;
    score.effects = {{1.0, type, 1.0, 0}};
    const auto st = renderSoundtrack(score, 4.0);
    if (type == "rise") {  // a riser builds up to its time instead
      score.effects[0].time = 3.0;
      const auto r = renderSoundtrack(score, 4.0);
      CHECK(rms(r, size_t(2.7 * 44100) * 2, size_t(3.0 * 44100) * 2) > 10 * rms(r, size_t(3.1 * 44100) * 2, r.size()));
      continue;
    }
    const double before = rms(st, 0, size_t(0.9 * 44100) * 2);
    const double after = rms(st, size_t(1.0 * 44100) * 2, size_t(1.3 * 44100) * 2);
    if (!(after > 0.01 && after > before * 10)) throw Failure("effect is silent or mistimed: " + type);
  }
}

TEST(music_only_and_effects_only) {
  Score score;
  score.effects = {{0.5, "pop"}};
  SoundtrackOptions none;
  none.music = false;
  none.effects = false;
  const auto silent = renderSoundtrack(score, 2.0, none);
  CHECK(rms(silent, 0, silent.size()) == 0);
  SoundtrackOptions fx;
  fx.music = false;
  const auto onlyFx = renderSoundtrack(score, 2.0, fx);
  CHECK(rms(onlyFx, 0, size_t(0.45 * 44100) * 2) < 1e-4);  // nothing before the pop
}

TEST(wav_file_has_a_valid_header) {
  const auto path = fs::temp_directory_path() / "studio-test.wav";
  writeWav(path.string(), std::vector<float>(44100 * 2, 0.25f));
  const std::string d = readFile(path);
  CHECK(d.rfind("RIFF", 0) == 0 && d.substr(8, 8) == "WAVEfmt " && d.substr(36, 4) == "data");
  CHECK(d.size() == 44 + 44100 * 4);
}

TEST(player_is_added_once_before_body_end) {
  const std::string page = "<html><body><p>hi</p></BODY></html>";
  const std::string once = addSoundtrackPlayer(page, 6);
  CHECK(once.find("soundtrack.wav") < once.find("</BODY>"));
  CHECK(once.find("const loop = 6") != std::string::npos);
  CHECK(addSoundtrackPlayer(once, 6) == once);
}

// ---- GIF export ----------------------------------------------------------------

TEST(quantize_keeps_few_colours_exact) {
  const uint8_t px[] = {255, 0, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 10, 200, 30, 255};
  std::vector<uint8_t> palette;
  const auto idx = quantize(px, 4, 256, palette);
  CHECK(idx[0] == idx[2]);
  CHECK(idx[0] != idx[1] && idx[1] != idx[3]);
  CHECK(palette[idx[3] * 3] == 10 && palette[idx[3] * 3 + 1] == 200 && palette[idx[3] * 3 + 2] == 30);
}

TEST(quantize_limits_palette) {
  std::vector<uint8_t> px;
  for (int i = 0; i < 4096; ++i) px.insert(px.end(), {uint8_t(i * 7), uint8_t(i * 13), uint8_t(i), 255});
  std::vector<uint8_t> palette;
  const auto idx = quantize(px.data(), 4096, 16, palette);
  CHECK(*std::max_element(idx.begin(), idx.end()) < 16);
}

TEST(gif_writer_makes_a_looping_gif) {
  const auto path = fs::temp_directory_path() / "studio-test.gif";
  const int w = 64, h = 48;
  {
    GifWriter gif(path.string(), w, h);
    std::vector<uint8_t> frame(w * h * 4);
    for (int f = 0; f < 3; ++f) {
      for (int i = 0; i < w * h; ++i) {
        frame[i * 4] = uint8_t((i % w) * 4 + f * 40);
        frame[i * 4 + 1] = uint8_t((i / w) * 5);
        frame[i * 4 + 2] = uint8_t(f * 80);
      }
      gif.addFrame(frame.data(), 5);
    }
  }
  const std::string data = readFile(path);
  CHECK(data.rfind("GIF89a", 0) == 0);
  CHECK(data.find("NETSCAPE2.0") != std::string::npos);
  CHECK(data.back() == 0x3B);
  CHECK(uint8_t(data[6]) == w && uint8_t(data[8]) == h);
}

// ---- Claude Code runner -------------------------------------------------------

TEST(claude_reply_parsing) {
  CHECK(parseClaudeReply("designer", 0, R"({"result": "hello", "is_error": false})", "") == "hello");
  CHECK_THROWS(parseClaudeReply("designer", 0, R"({"result": "Rate limited", "is_error": true})", ""),
               "designer agent failed (exit 0): Rate limited");
  CHECK_THROWS(parseClaudeReply("animator", 1, "", "not logged in"), "animator agent failed (exit 1): not logged in");
  CHECK_THROWS(parseClaudeReply("animator", 1, "", ""), "no output");
}

TEST(claude_arguments_disable_tools_and_mcp) {
  ClaudeCodeRunnerOptions opts;
  opts.command = "claude";
  opts.model = "opus";
  const auto args = ClaudeCodeRunner(opts).arguments("sys.txt");
  const auto has = [&](const std::string& a) { return std::find(args.begin(), args.end(), a) != args.end(); };
  CHECK(args[0] == "-p");
  CHECK(has("--strict-mcp-config"));
  CHECK(has("--no-session-persistence"));
  CHECK(has("--system-prompt-file") && has("sys.txt"));
  CHECK(args[args.size() - 2] == "--model" && args.back() == "opus");
}

TEST(missing_claude_gives_a_clear_error) {
  ClaudeCodeRunnerOptions opts;
  opts.command = "definitely-not-a-real-program-xyz";
  ClaudeCodeRunner runner(opts);
  CHECK_THROWS(runner.run({StageId::Director, "s", "p"}), "Could not start Claude Code");
}

#ifndef _WIN32
TEST(process_pipes_stdin_and_collects_output) {
  const std::string big(200000, 'x');  // larger than a pipe buffer
  const auto r = runProcess("cat", {}, big, std::chrono::seconds(10));
  CHECK(r.started);
  CHECK(r.exitCode == 0);
  CHECK(r.out == big);
}

TEST(process_times_out) {
  const auto r = runProcess("sleep", {"5"}, "", std::chrono::milliseconds(200));
  CHECK(r.timedOut);
}

TEST(process_can_be_stopped) {
  const auto start = std::chrono::steady_clock::now();
  const auto r = runProcess("sleep", {"5"}, "", std::chrono::seconds(10), [&] {
    return std::chrono::steady_clock::now() - start > std::chrono::milliseconds(100);
  });
  CHECK(r.stopped && !r.timedOut);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
}

TEST(claude_runner_talks_to_a_fake_cli) {
  // A stand-in for `claude` that echoes the system prompt file and stdin inside the JSON envelope.
  const auto dir = tempDir("fakecli");
  fs::create_directories(dir);
  const auto script = dir / "claude";
  std::ofstream(script) << "#!/bin/sh\n"
                           "input=$(cat)\n"
                           "while [ \"$1\" != --system-prompt-file ]; do shift; done\n"
                           "sys=$(cat \"$2\")\n"
                           "printf '{\"type\":\"result\",\"is_error\":false,\"result\":\"%s got %s\"}' \"$sys\" \"$input\"\n";
  fs::permissions(script, fs::perms::owner_all);
  ClaudeCodeRunnerOptions opts;
  opts.command = script.string();
  ClaudeCodeRunner runner(opts);
  CHECK(runner.run({StageId::Director, "director", "hello"}) == "director got hello");
}
#endif

int main() {
  int failed = 0;
  for (const auto& t : registry()) {
    try {
      t.fn();
      std::cout << "ok   " << t.name << "\n";
    } catch (const std::exception& e) {
      ++failed;
      std::cout << "FAIL " << t.name << "\n     " << e.what() << "\n";
    }
  }
  std::cout << "\n" << registry().size() - failed << "/" << registry().size() << " passed\n";
  return failed ? 1 : 0;
}
