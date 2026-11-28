#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <thread>

#include "studio/pipeline.hpp"

/** Runs one animation through the studio on a background thread and reports back on the
 *  GUI thread through signals. */
class StudioRun : public QObject {
  Q_OBJECT
 public:
  struct Settings {
    QString runner = "claude-code";  // or "mock"
    QString claudePath;              // empty: find `claude` on PATH
    QString model;                   // empty: your Claude Code default
    int revisions = 2;
  };

  StudioRun(QObject* parent = nullptr);
  ~StudioRun() override;

  bool running() const { return running_; }

  /** Starts a run. `checker` may be null. */
  void start(const QString& prompt, studio::Mode mode, double lengthSeconds, studio::SoundOptions sound,
             const QString& outputDir, const Settings& settings, studio::AnimationChecker* checker);
  /** Asks the run to stop; the agent that is working is stopped straight away. */
  void cancel() { cancelled_ = true; }

 signals:
  void progress(studio::StageId stage, const QString& message);
  void finished(const studio::PipelineResult& result);
  void failed(const QString& message, bool cancelled);

 private:
  std::thread thread_;
  std::atomic<bool> cancelled_{false};
  bool running_ = false;
};

Q_DECLARE_METATYPE(studio::StageId)
Q_DECLARE_METATYPE(studio::PipelineResult)
