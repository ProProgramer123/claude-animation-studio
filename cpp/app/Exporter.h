#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class QFile;
class QProcess;
class QWebEngineView;

/** Records an animation by playing it in an off-screen browser and grabbing frames in real
 *  time, then writes an animated GIF (built in) or an MP4 (with ffmpeg). The MP4 gets the
 *  animation's soundtrack and the person's own music, lined up with the picture; GIFs are silent. */
class Exporter : public QObject {
  Q_OBJECT
 public:
  enum class Format { Gif, Mp4 };

  explicit Exporter(QObject* parent = nullptr);
  ~Exporter() override;

  /** Finds ffmpeg: the given path if set, else on PATH. Empty when it isn't installed. */
  static QString findFfmpeg(const QString& configured);
  /** How long to record: the animation's length, from 2 seconds up to 30 (GIF) or 60 (MP4). */
  static double animationSeconds(const QString& folder, Format format);
  /** The sound files an animation plays (soundtrack.wav, the person's music), as full paths. */
  static QStringList audioTracks(const QString& folder);
  /** ffmpeg arguments that retime the recorded video and add the looping sound tracks. */
  static std::vector<std::string> muxArguments(const QString& video, double timeScale, const QStringList& tracks,
                                               double audioOffset, double seconds, const QString& out);

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
  bool startEncoder();
  void encode();
  void finish(double fps);
  void fail(const QString& message);
  void cleanup();

  Format format_ = Format::Gif;
  QString outPath_, ffmpeg_;
  QSize size_;
  int fps_ = 15;
  int targetFrames_ = 0;
  int frames_ = 0;
  double seconds_ = 0;
  double audioOffset_ = 0;
  QStringList tracks_;
  QProcess* encoder_ = nullptr;
  bool busy_ = false;
  std::atomic<bool> cancelled_{false};

  QWebEngineView* view_ = nullptr;
  QTimer tick_;
  QElapsedTimer clock_;
  std::unique_ptr<QTemporaryDir> temp_;
  std::unique_ptr<QFile> raw_;
  std::thread worker_;
};
