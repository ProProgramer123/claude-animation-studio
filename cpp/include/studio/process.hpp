#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace studio {

struct ProcessResult {
  bool started = false;   // false when the program could not be launched at all
  bool timedOut = false;  // true when it was killed for running too long
  bool stopped = false;   // true when it was killed because shouldStop returned true
  int exitCode = -1;
  std::string out;
  std::string err;
  std::string startError;  // why it could not start, when started == false
};

/** Runs `program` (looked up on PATH) with `args`, writes `input` to its stdin,
 *  and collects stdout and stderr. Arguments are passed directly, never through a shell.
 *  `shouldStop`, when given, is polled a few times a second; returning true kills the program. */
ProcessResult runProcess(const std::string& program, const std::vector<std::string>& args,
                         const std::string& input, std::chrono::milliseconds timeout,
                         const std::function<bool()>& shouldStop = {});

}  // namespace studio
