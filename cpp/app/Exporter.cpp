#include "Exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <algorithm>
#include <vector>

#include "studio/gif.hpp"
#include "studio/process.hpp"

Exporter::Exporter(QObject* parent) : QObject(parent) {
  tick_.setTimerType(Qt::PreciseTimer);
  connect(&tick_, &QTimer::timeout, this, &Exporter::grabFrame);
}

Exporter::~Exporter() {
  cancelled_ = true;
  if (worker_.joinable()) worker_.join();
}

QString Exporter::findFfmpeg(const QString& configured) {
  if (!configured.isEmpty()) return QFileInfo(configured).isExecutable() ? configured : QString();
  return QStandardPaths::findExecutable("ffmpeg");
}

double Exporter::animationSeconds(const QString& folder) {
  QFile f(QDir(folder).filePath("storyboard.json"));
  double s = 8;
  if (f.open(QIODevice::ReadOnly)) {
    const double d = QJsonDocument::fromJson(f.readAll()).object().value("duration_seconds").toDouble();
    if (d > 0) s = d;
  }
  return std::clamp(s, 2.0, 20.0);
}

void Exporter::start(const QString& folder, Format format, const QString& outPath, const QString& ffmpeg) {
  if (busy_) return;
  busy_ = true;
  cancelled_ = false;
  format_ = format;
  outPath_ = outPath;
  ffmpeg_ = ffmpeg;
  // GIFs stay small enough to share; MP4s get full HD-ish quality.
  size_ = format == Format::Gif ? QSize(640, 360) : QSize(1280, 720);
  fps_ = format == Format::Gif ? 15 : 30;
  seconds_ = animationSeconds(folder);
  targetFrames_ = static_cast<int>(seconds_ * fps_);
  frames_ = 0;

  temp_ = std::make_unique<QTemporaryDir>();
  raw_ = std::make_unique<QFile>(temp_->filePath("frames.rgba"));
  if (!temp_->isValid() || !raw_->open(QIODevice::WriteOnly)) {
    fail("Could not create a temporary folder for the frames.");
    return;
  }

  view_ = new QWebEngineView;
  view_->setAttribute(Qt::WA_DontShowOnScreen);
  view_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  view_->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
  view_->page()->setAudioMuted(true);  // recording happens silently in the background
  view_->resize(size_);
  view_->show();
  connect(view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
    if (!ok) {
      fail("The animation would not load.");
      return;
    }
    // Give scripts and fonts a moment to settle, then start from the top.
    QTimer::singleShot(600, this, &Exporter::beginRecording);
  });
  emit progress(0, "Loading the animation…");
  view_->load(QUrl::fromLocalFile(QDir(folder).filePath("index.html")));
}

void Exporter::beginRecording() {
  if (cancelled_) {
    fail({});
    return;
  }
  clock_.start();
  tick_.start(1000 / fps_);
  grabFrame();
}

void Exporter::grabFrame() {
  if (cancelled_) {
    fail({});
    return;
  }
  QImage img = view_->grab().toImage();
  if (img.size() != size_) img = img.scaled(size_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  img = img.convertToFormat(QImage::Format_RGBA8888);
  for (int y = 0; y < img.height(); ++y)
    raw_->write(reinterpret_cast<const char*>(img.constScanLine(y)), img.width() * 4);
  ++frames_;
  emit progress(frames_ * 60 / targetFrames_, QString("Recording… %1 of %2 seconds")
                                                   .arg(qMin(clock_.elapsed() / 1000.0, seconds_), 0, 'f', 0)
                                                   .arg(seconds_, 0, 'f', 0));
  if (frames_ >= targetFrames_) {
    tick_.stop();
    encode();
  }
}

void Exporter::encode() {
  // Real time may have run a little slower than planned; use the rate we actually got.
  const double elapsed = std::max(0.001, clock_.elapsed() / 1000.0);
  const double fps = std::max(1.0, frames_ / elapsed);
  raw_->close();
  view_->deleteLater();
  view_ = nullptr;
  emit progress(60, format_ == Format::Gif ? "Making the GIF…" : "Making the video…");

  const QString rawPath = raw_->fileName();
  const int w = size_.width(), h = size_.height(), count = frames_;
  const Format format = format_;
  const QString out = outPath_, ffmpeg = ffmpeg_;

  worker_ = std::thread([this, rawPath, w, h, count, fps, format, out, ffmpeg] {
    QString error;
    try {
      if (format == Format::Gif) {
        QFile in(rawPath);
        if (!in.open(QIODevice::ReadOnly)) throw std::runtime_error("Could not read the recorded frames.");
        studio::GifWriter gif(out.toStdString(), w, h);
        std::vector<uint8_t> frame(static_cast<size_t>(w) * h * 4);
        const int delay = std::max(2, static_cast<int>(100.0 / fps + 0.5));
        for (int i = 0; i < count; ++i) {
          if (cancelled_) throw std::runtime_error("cancelled");
          if (in.read(reinterpret_cast<char*>(frame.data()), static_cast<qint64>(frame.size())) !=
              static_cast<qint64>(frame.size()))
            break;
          gif.addFrame(frame.data(), delay);
          const int pct = 60 + (i + 1) * 40 / count;
          QMetaObject::invokeMethod(this, [this, pct] { emit progress(pct, "Making the GIF…"); }, Qt::QueuedConnection);
        }
        gif.close();
      } else {
        const std::vector<std::string> args{
            "-y", "-loglevel", "error",
            "-f", "rawvideo", "-pixel_format", "rgba",
            "-video_size", QString("%1x%2").arg(w).arg(h).toStdString(),
            "-framerate", QString::number(fps, 'f', 3).toStdString(),
            "-i", rawPath.toStdString(),
            "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", "-preset", "medium",
            "-movflags", "+faststart",
            out.toStdString()};
        const auto r = studio::runProcess(ffmpeg.toStdString(), args, "", std::chrono::minutes(10),
                                          [this] { return cancelled_.load(); });
        if (r.stopped) throw std::runtime_error("cancelled");
        if (!r.started) throw std::runtime_error("Could not start ffmpeg: " + r.startError);
        if (r.exitCode != 0)
          throw std::runtime_error("ffmpeg could not make the video: " + r.err.substr(0, 400));
      }
    } catch (const std::exception& e) {
      error = cancelled_ ? QString() : QString::fromStdString(e.what());
      if (cancelled_ || !error.isEmpty()) QFile::remove(out);
      if (cancelled_) error = "cancelled";
    }
    QMetaObject::invokeMethod(this, [this, error] {
      if (worker_.joinable()) worker_.join();
      if (error.isEmpty()) {
        cleanup();
        emit finished(outPath_);
      } else {
        fail(error == "cancelled" ? QString() : error);
      }
    }, Qt::QueuedConnection);
  });
}

void Exporter::cancel() { cancelled_ = true; }

void Exporter::fail(const QString& message) {
  tick_.stop();
  cleanup();
  emit failed(message);  // empty message: cancelled
}

void Exporter::cleanup() {
  if (view_) {
    view_->deleteLater();
    view_ = nullptr;
  }
  if (raw_ && raw_->isOpen()) raw_->close();
  raw_.reset();
  temp_.reset();
  busy_ = false;
}
