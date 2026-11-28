#include "StudioRun.h"

#include "studio/claude_code_runner.hpp"
#include "studio/mock_runner.hpp"

StudioRun::StudioRun(QObject* parent) : QObject(parent) {
  qRegisterMetaType<studio::StageId>();
  qRegisterMetaType<studio::PipelineResult>();
}

StudioRun::~StudioRun() {
  cancelled_ = true;
  if (thread_.joinable()) thread_.join();
}

void StudioRun::start(const QString& prompt, studio::Mode mode, double lengthSeconds, studio::SoundOptions sound,
                      const QString& outputDir, const Settings& s, studio::AnimationChecker* checker) {
  if (running_) return;
  if (thread_.joinable()) thread_.join();
  running_ = true;
  cancelled_ = false;

  thread_ = std::thread([this, prompt, mode, lengthSeconds, sound, outputDir, s, checker] {
    const auto isCancelled = [this] { return cancelled_.load(); };
    try {
      std::unique_ptr<studio::AgentRunner> runner;
      if (s.runner == "mock") {
        runner = std::make_unique<studio::MockRunner>();
      } else {
        studio::ClaudeCodeRunnerOptions opts;
        opts.command = s.claudePath.toStdString();
        opts.model = s.model.toStdString();
        opts.cancelled = isCancelled;
        runner = std::make_unique<studio::ClaudeCodeRunner>(opts);
      }

      studio::PipelineOptions o;
      o.prompt = prompt.toStdString();
      o.mode = mode;
      o.lengthSeconds = lengthSeconds;
      o.sound = sound;
      o.runner = runner.get();
      o.outputDir = std::filesystem::u8path(outputDir.toStdString());
      o.maxRevisions = s.revisions;
      o.checker = checker;
      o.cancelled = isCancelled;
      o.onProgress = [this](studio::StageId stage, const std::string& message) {
        const QString text = QString::fromStdString(message);
        QMetaObject::invokeMethod(this, [this, stage, text] { emit progress(stage, text); }, Qt::QueuedConnection);
      };

      const studio::PipelineResult result = studio::runPipeline(o);
      QMetaObject::invokeMethod(this, [this, result] {
        running_ = false;
        emit finished(result);
      }, Qt::QueuedConnection);
    } catch (const std::exception& e) {
      const QString message = QString::fromStdString(e.what());
      const bool wasCancelled = cancelled_.load();
      QMetaObject::invokeMethod(this, [this, message, wasCancelled] {
        running_ = false;
        emit failed(message, wasCancelled);
      }, Qt::QueuedConnection);
    }
  });
}
