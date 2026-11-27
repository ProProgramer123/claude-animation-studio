#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

#include <atomic>
#include <memory>
#include <thread>

class QFile;
class QWebEngineView;

/** Records an animation by playing it in an off-screen browser and grabbing frames in real
 *  time, then writes an animated GIF (built in) or an MP4 (with ffmpeg). */
class Exporter : public QObject {
  Q_OBJECT
 public:
  enum class Format { Gif, Mp4 };

  explicit Exporter(QObject* parent = nullptr);
  ~Exporter() override;

  /** Finds ffmpeg: the given path if set, else on PATH. Empty when it isn't installed. */
  static QString findFfmpeg(const QString& configured);
  /** How long to record: the storyboard's length, kept between 2 and 20 seconds. */
  static double animationSeconds(const QString& folder);

  void start(const QString& folder, Format format, const QString& outPath, const QString& ffmpeg);
  void cancel();
  bool busy() const { return busy_; }

 signals:
  void progress(int percent, const QString& text);
  void finished(const QString& outPath);
  void failed(const QString& message);

 private:
  void beginRecording();
  void grabFrame();
  void encode();
  void fail(const QString& message);
  void cleanup();

  Format format_ = Format::Gif;
  QString outPath_, ffmpeg_;
  QSize size_;
  int fps_ = 15;
  int targetFrames_ = 0;
  int frames_ = 0;
  double seconds_ = 0;
  bool busy_ = false;
  std::atomic<bool> cancelled_{false};

  QWebEngineView* view_ = nullptr;
  QTimer tick_;
  QElapsedTimer clock_;
  std::unique_ptr<QTemporaryDir> temp_;
  std::unique_ptr<QFile> raw_;
  std::thread worker_;
};
