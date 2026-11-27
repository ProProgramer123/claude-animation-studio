// Engine tests. No framework: each TEST registers itself and main() runs them all.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "studio/claude_code_runner.hpp"
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
  CHECK(agents().size() == 5);
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
  CHECK(runner.calls.size() == 6);
  CHECK(runner.calls[0].agent == StageId::Director);
  CHECK(runner.calls[0].prompt.find("Mode: 2D") == 0);
  CHECK(runner.calls[0].prompt.find("a bouncing ball") != std::string::npos);
  for (const char* f : {"prompt.txt", "brief.md", "storyboard.json", "design.json", "animation.json",
                        "index.html", "review.json", "result.json"})
    CHECK(fs::exists(o.outputDir / f));
  CHECK(readFile(r.htmlPath).find("<svg") != std::string::npos);
  CHECK(progress.size() == 6);
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
  CHECK(runner.calls.size() == 8);
  CHECK(runner.calls[6].agent == StageId::Renderer);
  CHECK(runner.calls[6].prompt.find("Ball never touches the ground.") != std::string::npos);
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
  CHECK(runner.calls[5].prompt.find("ReferenceError") != std::string::npos);  // reviewer saw it
  CHECK(runner.calls[6].prompt.find("Fix this browser error: ReferenceError") != std::string::npos);
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
