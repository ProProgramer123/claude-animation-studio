#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "studio/roster.hpp"
#include "studio/runner.hpp"

namespace studio {

/** What a browser saw when it opened the animation. */
struct CheckResult {
  std::vector<std::string> errors;  // uncaught exceptions, console errors, failed loads
  bool isStatic = false;            // true when nothing moved between two screenshots
  std::string screenshotPng;        // PNG bytes, empty when none was taken
};

/** Optional check that opens the animation in a real browser. The desktop app supplies one. */
class AnimationChecker {
 public:
  virtual ~AnimationChecker() = default;
  virtual std::string name() const = 0;
  /** Returns nullopt when the check can't run (for example, no browser available). */
  virtual std::optional<CheckResult> check(const std::filesystem::path& htmlPath) = 0;
};

struct Review {
  bool approved = false;
  std::vector<std::string> issues;
};

/** Which parts of the soundtrack to make. Both off: no Sound agent, no soundtrack.
 *  With musicFile set, that file is the music (copied next to the animation as music.<ext>),
 *  so the synthesizer makes only the effects, if they are on. */
struct SoundOptions {
  bool music = true;
  bool effects = true;
  std::filesystem::path musicFile;
  bool ownMusic() const { return !musicFile.empty(); }
  bool any() const { return music || effects || ownMusic(); }
};

/** Audio formats a music file may have: ones the app's preview and every browser can play. */
bool isSupportedMusicFile(const std::filesystem::path& file);

struct PipelineOptions {
  std::string prompt;
  Mode mode = Mode::TwoD;
  /** Exact length in seconds (2 to 60). 0 lets the director choose. */
  double lengthSeconds = 0;
  SoundOptions sound;
  AgentRunner* runner = nullptr;
  /** Folder where this animation's files are written. */
  std::filesystem::path outputDir;
  /** How many times the reviewer may send work back to the renderer. */
  int maxRevisions = 2;
  /** Optional browser check. Its findings go to the reviewer, and errors block approval. */
  AnimationChecker* checker = nullptr;
  /** Called before each agent starts, with a sentence people can read. */
  std::function<void(StageId, const std::string&)> onProgress;
  /** Polled between agents; return true to stop early (throws Cancelled). */
  std::function<bool()> cancelled;
};

struct PipelineResult {
  std::string title;
  Mode mode = Mode::TwoD;
  std::filesystem::path outputDir;
  std::filesystem::path htmlPath;
  bool approved = false;
  int revisions = 0;
  std::vector<std::string> openIssues;
  /** Length the animation and soundtrack were made for, in seconds. */
  double durationSeconds = 0;
  /** soundtrack.wav, when the synthesizer made music or effects. */
  std::optional<std::filesystem::path> soundtrackPath;
  /** The person's own music, copied next to the animation (music.mp3 and so on). */
  std::optional<std::filesystem::path> musicPath;
  /** Screenshot of the finished animation, when the browser check ran. */
  std::optional<std::filesystem::path> previewPath;
};

/** Thrown when PipelineOptions::cancelled returns true. */
struct Cancelled : std::runtime_error {
  Cancelled() : std::runtime_error("The animation was cancelled.") {}
};

/** Runs the studio: hidden director -> storyboard -> designer -> animator -> renderer -> reviewer.
 *  If the reviewer finds problems, the renderer gets the issues and tries again.
 *  Every stage's output is saved next to the final index.html. Throws std::runtime_error on failure. */
PipelineResult runPipeline(const PipelineOptions& options);

/** Adds the soundtrack player to an HTML page: each track (a file name next to the page) loops in
 *  time with the animation, with one Sound on/off button. Adding it twice does nothing. */
std::string addSoundtrackPlayer(const std::string& html, double durationSeconds,
                                const std::vector<std::string>& tracks = {"soundtrack.wav"});

}  // namespace studio
