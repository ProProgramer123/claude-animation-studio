#include "Exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <algorithm>
#include <vector>

#include "studio/gif.hpp"
#include "studio/process.hpp"

namespace {

QJsonObject readJson(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(f.readAll()).object();
}

}  // namespace

Exporter::Exporter(QObject* parent) : QObject(parent) {
  tick_.setTimerType(Qt::PreciseTimer);
  connect(&tick_, &QTimer::timeout, this, &Exporter::grabFrame);
}

Exporter::~Exporter() {
  cancelled_ = true;
  if (encoder_) {
    encoder_->kill();
    encoder_->waitForFinished(2000);
  }
  if (worker_.joinable()) worker_.join();
}

QString Exporter::findFfmpeg(const QString& configured) {
  if (!configured.isEmpty()) return QFileInfo(configured).isExecutable() ? configured : QString();
  return QStandardPaths::findExecutable("ffmpeg");
}

double Exporter::animationSeconds(const QString& folder, Format format) {
  double s = readJson(QDir(folder).filePath("result.json")).value("duration_seconds").toDouble();
  if (s <= 0) s = readJson(QDir(folder).filePath("storyboard.json")).value("duration_seconds").toDouble();
  if (s <= 0) s = 8;
  // GIFs get big fast, so they stop at 30 seconds. Videos can run the full 60.
  return std::clamp(s, 2.0, format == Format::Gif ? 30.0 : 60.0);
}

QStringList Exporter::audioTracks(const QString& folder) {
  const QDir dir(folder);
  QStringList tracks;
  const QJsonObject result = readJson(dir.filePath("result.json"));
  QString soundtrack = result.value("soundtrack").toString();
  if (soundtrack.isEmpty() && !result.contains("soundtrack")) soundtrack = "soundtrack.wav";
  for (const QString& name : {soundtrack, result.value("music_file").toString()}) {
    if (name.isEmpty() || name.contains('/') || name.contains('\\')) continue;
    if (QFileInfo(dir.filePath(name)).isFile()) tracks << dir.filePath(name);
  }
  return tracks;
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
  seconds_ = animationSeconds(folder, format);
  targetFrames_ = static_cast<int>(seconds_ * fps_);
  frames_ = 0;
  audioOffset_ = 0;
  tracks_ = format == Format::Mp4 ? audioTracks(folder) : QStringList();

  temp_ = std::make_unique<QTemporaryDir>();
  if (!temp_->isValid()) {
    fail("Could not create a temporary folder for the frames.");
    return;
  }
  if (format == Format::Gif) {
    raw_ = std::make_unique<QFile>(temp_->filePath("frames.rgba"));
    if (!raw_->open(QIODevice::WriteOnly)) {
      fail("Could not create a temporary folder for the frames.");
      return;
    }
  }

  view_ = new QWebEngineView;
  view_->setAttribute(Qt::WA_DontShowOnScreen);
  view_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  view_->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
  view_->page()->setAudioMuted(true);  // recording happens silently; the video gets the sound files instead
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
  // The page's sound started when the page did; the video starts now. Measure the gap so the
  // soundtrack lines up with the first recorded frame.
  view_->page()->runJavaScript(
      "window.studioSound && window.studioSound.start !== undefined"
      " ? (performance.now() - window.studioSound.start) / 1000 : performance.now() / 1000",
      [this](const QVariant& v) {
        if (!view_) return;
        audioOffset_ = std::max(0.0, v.toDouble());
        if (format_ == Format::Mp4 && !startEncoder()) return;
        clock_.start();
        tick_.start(1000 / fps_);
        grabFrame();
      });
}

bool Exporter::startEncoder() {
  // Frames stream straight into ffmpeg, so even a 60 second video needs no big temporary file.
  encoder_ = new QProcess(this);
  encoder_->setProcessChannelMode(QProcess::SeparateChannels);
  encoder_->start(ffmpeg_, {"-y", "-loglevel", "error",
                            "-f", "rawvideo", "-pixel_format", "rgba",
                            "-video_size", QString("%1x%2").arg(size_.width()).arg(size_.height()),
                            "-framerate", QString::number(fps_),
                            "-i", "-",
                            "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", "-preset", "fast",
                            temp_->filePath("video.mp4")});
  if (!encoder_->waitForStarted(10000)) {
    fail("Could not start ffmpeg: " + encoder_->errorString());
    return false;
  }
  return true;
}

void Exporter::grabFrame() {
  if (cancelled_) {
    fail({});
    return;
  }
  QImage img = view_->grab().toImage();
  if (img.size() != size_) img = img.scaled(size_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  img = img.convertToFormat(QImage::Format_RGBA8888);
  QIODevice* out = encoder_ ? static_cast<QIODevice*>(encoder_) : raw_.get();
  for (int y = 0; y < img.height(); ++y)
    out->write(reinterpret_cast<const char*>(img.constScanLine(y)), img.width() * 4);
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
  view_->deleteLater();
  view_ = nullptr;
  emit progress(60, format_ == Format::Gif ? "Making the GIF…" : "Making the video…");

  if (encoder_) {
    // Let ffmpeg finish the video track, then add the timing fix and the sound in a second pass.
    encoder_->closeWriteChannel();
    connect(encoder_, &QProcess::finished, this, [this, fps](int code, QProcess::ExitStatus status) {
      const QString err = QString::fromLocal8Bit(encoder_->readAllStandardError()).left(400);
      encoder_->deleteLater();
      encoder_ = nullptr;
      if (cancelled_) {
        fail({});
        return;
      }
      if (status != QProcess::NormalExit || code != 0) {
        fail("ffmpeg could not make the video: " + err);
        return;
      }
      finish(fps);
    });
    return;
  }
  raw_->close();
  finish(fps);
}

void Exporter::finish(double fps) {
  const QString rawPath = raw_ ? raw_->fileName() : QString();
  const QString videoPath = temp_->filePath("video.mp4");
  const int w = size_.width(), h = size_.height(), count = frames_;
  const Format format = format_;
  const QString out = outPath_, ffmpeg = ffmpeg_;
  const QStringList tracks = tracks_;
  const double offset = audioOffset_, nominal = fps_, length = count / fps;

  worker_ = std::thread([=] {
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
        QMetaObject::invokeMethod(
            this, [this, n = tracks.size()] { emit progress(85, n ? "Adding the sound…" : "Finishing the video…"); },
            Qt::QueuedConnection);
        const auto args = muxArguments(videoPath, nominal / fps, tracks, offset, length, out);
        const auto r = studio::runProcess(ffmpeg.toStdString(), args, "", std::chrono::minutes(10),
                                          [this] { return cancelled_.load(); });
        if (r.stopped) throw std::runtime_error("cancelled");
        if (!r.started) throw std::runtime_error("Could not start ffmpeg: " + r.startError);
        if (r.exitCode != 0)
          throw std::runtime_error("ffmpeg could not add the sound: " + r.err.substr(0, 400));
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

std::vector<std::string> Exporter::muxArguments(const QString& video, double timeScale, const QStringList& tracks,
                                                double audioOffset, double seconds, const QString& out) {
  const auto num = [](double v) { return QString::number(v, 'f', 4).toStdString(); };
  // The video was labelled at the planned frame rate; -itsscale stretches it to real time.
  std::vector<std::string> args{"-y", "-loglevel", "error", "-itsscale", num(timeScale), "-i", video.toStdString()};
  for (const QString& t : tracks) {
    args.insert(args.end(), {"-stream_loop", "-1", "-i", t.toStdString()});
  }
  args.insert(args.end(), {"-map", "0:v"});
  if (!tracks.isEmpty()) {
    // Each track loops like it does on the page, starting where the page was when recording began.
    std::string graph;
    for (int i = 0; i < tracks.size(); ++i) {
      graph += "[" + std::to_string(i + 1) + ":a]atrim=start=" + num(audioOffset) +
               ",asetpts=PTS-STARTPTS,aformat=sample_rates=44100:channel_layouts=stereo[a" +
               std::to_string(i) + "];";
    }
    if (tracks.size() == 1) {
      graph += "[a0]anull[aout]";
    } else {
      for (int i = 0; i < tracks.size(); ++i) graph += "[a" + std::to_string(i) + "]";
      // amix lowers each input; put the level back, and keep the peaks out of clipping.
      graph += "amix=inputs=" + std::to_string(tracks.size()) + ":duration=longest,volume=" +
               std::to_string(tracks.size()) + ",alimiter=limit=0.95[aout]";
    }
    args.insert(args.end(), {"-filter_complex", graph, "-map", "[aout]", "-c:a", "aac", "-b:a", "192k"});
  }
  args.insert(args.end(), {"-c:v", "copy", "-t", num(seconds), "-movflags", "+faststart", out.toStdString()});
  return args;
}

void Exporter::cancel() {
  cancelled_ = true;
  if (encoder_ && !tick_.isActive()) encoder_->kill();  // still encoding: stop it now
}

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
  if (encoder_) {
    encoder_->disconnect(this);
    encoder_->kill();
    encoder_->waitForFinished(2000);
    encoder_->deleteLater();
    encoder_ = nullptr;
  }
  if (raw_ && raw_->isOpen()) raw_->close();
  raw_.reset();
  temp_.reset();
  busy_ = false;
}
