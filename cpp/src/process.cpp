#include "studio/process.hpp"

#include <algorithm>
#include <chrono>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <thread>

namespace studio {
namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

// Quotes one argument the way CommandLineToArgvW / the MSVC runtime parse it.
void appendQuoted(std::wstring& cmd, const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    cmd += arg;
    return;
  }
  cmd += L'"';
  for (auto it = arg.begin();; ++it) {
    size_t slashes = 0;
    while (it != arg.end() && *it == L'\\') { ++it; ++slashes; }
    if (it == arg.end()) { cmd.append(slashes * 2, L'\\'); break; }
    if (*it == L'"') { cmd.append(slashes * 2 + 1, L'\\'); cmd += L'"'; }
    else { cmd.append(slashes, L'\\'); cmd += *it; }
  }
  cmd += L'"';
}

// Finds the program on PATH, trying the usual executable extensions (claude is often claude.cmd).
std::wstring resolve(const std::wstring& program) {
  for (const wchar_t* ext : {L".exe", L".cmd", L".bat", L""}) {
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, program.c_str(), ext, MAX_PATH, buf, nullptr)) return buf;
  }
  return program;
}

void drain(HANDLE h, std::string* out) {
  char buf[4096];
  DWORD n = 0;
  while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n > 0) out->append(buf, n);
}

}  // namespace

ProcessResult runProcess(const std::string& program, const std::vector<std::string>& args,
                         const std::string& input, std::chrono::milliseconds timeout,
                         const std::function<bool()>& shouldStop) {
  ProcessResult result;
  SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
  HANDLE inR, inW, outR, outW, errR, errW;
  if (!CreatePipe(&inR, &inW, &sa, 0) || !CreatePipe(&outR, &outW, &sa, 0) ||
      !CreatePipe(&errR, &errW, &sa, 0)) {
    result.startError = "could not create pipes";
    return result;
  }
  SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);

  const std::wstring exe = resolve(widen(program));
  std::wstring cmd;
  const bool script = exe.size() > 4 &&
      (_wcsicmp(exe.c_str() + exe.size() - 4, L".cmd") == 0 || _wcsicmp(exe.c_str() + exe.size() - 4, L".bat") == 0);
  if (script) cmd = L"cmd.exe /d /s /c \"";
  appendQuoted(cmd, exe);
  for (const auto& a : args) { cmd += L' '; appendQuoted(cmd, widen(a)); }
  if (script) cmd += L'"';

  STARTUPINFOW si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = inR;
  si.hStdOutput = outW;
  si.hStdError = errW;
  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessW(script ? nullptr : exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  CloseHandle(inR);
  CloseHandle(outW);
  CloseHandle(errW);
  if (!ok) {
    result.startError = "error " + std::to_string(GetLastError());
    CloseHandle(inW); CloseHandle(outR); CloseHandle(errR);
    return result;
  }
  result.started = true;

  std::thread outThread(drain, outR, &result.out);
  std::thread errThread(drain, errR, &result.err);
  std::thread inThread([&] {
    DWORD written = 0;
    size_t off = 0;
    while (off < input.size() &&
           WriteFile(inW, input.data() + off, static_cast<DWORD>(input.size() - off), &written, nullptr))
      off += written;
    CloseHandle(inW);
  });

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT) {
    const bool late = std::chrono::steady_clock::now() >= deadline;
    const bool stop = !late && shouldStop && shouldStop();
    if (late || stop) {
      (late ? result.timedOut : result.stopped) = true;
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, INFINITE);
      break;
    }
  }
  inThread.join();
  outThread.join();
  errThread.join();
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  result.exitCode = static_cast<int>(code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(outR);
  CloseHandle(errR);
  return result;
}

}  // namespace studio

#else  // POSIX

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

extern char** environ;

namespace studio {

ProcessResult runProcess(const std::string& program, const std::vector<std::string>& args,
                         const std::string& input, std::chrono::milliseconds timeout,
                         const std::function<bool()>& shouldStop) {
  ProcessResult result;
  int inPipe[2], outPipe[2], errPipe[2];
  if (pipe(inPipe) || pipe(outPipe) || pipe(errPipe)) {
    result.startError = std::strerror(errno);
    return result;
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, inPipe[0], 0);
  posix_spawn_file_actions_adddup2(&actions, outPipe[1], 1);
  posix_spawn_file_actions_adddup2(&actions, errPipe[1], 2);
  for (int fd : {inPipe[0], inPipe[1], outPipe[0], outPipe[1], errPipe[0], errPipe[1]})
    posix_spawn_file_actions_addclose(&actions, fd);

  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(program.c_str()));
  for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int rc = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(inPipe[0]);
  close(outPipe[1]);
  close(errPipe[1]);
  if (rc != 0) {
    result.startError = std::strerror(rc);
    close(inPipe[1]); close(outPipe[0]); close(errPipe[0]);
    return result;
  }
  result.started = true;

  // Don't die if the child exits before reading all of stdin.
  struct sigaction ignore {}, previous {};
  ignore.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &ignore, &previous);

  fcntl(inPipe[1], F_SETFL, O_NONBLOCK);
  int inFd = input.empty() ? (close(inPipe[1]), -1) : inPipe[1];
  int outFd = outPipe[0], errFd = errPipe[0];
  size_t written = 0;
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  while (outFd >= 0 || errFd >= 0) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    if (left.count() <= 0 || (shouldStop && shouldStop())) {
      (left.count() <= 0 ? result.timedOut : result.stopped) = true;
      kill(pid, SIGTERM);
      break;
    }
    pollfd fds[3];
    int n = 0;
    if (inFd >= 0) fds[n++] = {inFd, POLLOUT, 0};
    if (outFd >= 0) fds[n++] = {outFd, POLLIN, 0};
    if (errFd >= 0) fds[n++] = {errFd, POLLIN, 0};
    const int ready = poll(fds, static_cast<nfds_t>(n), static_cast<int>(std::min<long long>(left.count(), 200)));
    if (ready < 0 && errno != EINTR) break;
    for (int i = 0; i < n; ++i) {
      if (!fds[i].revents) continue;
      if (fds[i].fd == inFd) {
        const ssize_t w = write(inFd, input.data() + written, input.size() - written);
        if (w > 0) written += static_cast<size_t>(w);
        if (w < 0 && errno != EAGAIN) written = input.size();
        if (written >= input.size()) { close(inFd); inFd = -1; }
      } else {
        char buf[8192];
        const ssize_t r = read(fds[i].fd, buf, sizeof buf);
        std::string& sink = fds[i].fd == outFd ? result.out : result.err;
        if (r > 0) {
          sink.append(buf, static_cast<size_t>(r));
        } else if (r == 0 || errno != EINTR) {
          close(fds[i].fd);
          (fds[i].fd == outFd ? outFd : errFd) = -1;
        }
      }
    }
  }
  for (int fd : {inFd, outFd, errFd})
    if (fd >= 0) close(fd);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  sigaction(SIGPIPE, &previous, nullptr);
  if (WIFEXITED(status)) result.exitCode = WEXITSTATUS(status);
  else if (WIFSIGNALED(status)) result.exitCode = 128 + WTERMSIG(status);
  return result;
}

}  // namespace studio

#endif
