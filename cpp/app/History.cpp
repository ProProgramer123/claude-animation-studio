#include "History.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QTimeZone>

#include <algorithm>

namespace {

QJsonObject readJson(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(f.readAll()).object();
}

}  // namespace

bool readEntry(const QString& folder, HistoryEntry& e) {
  const QDir dir(folder);
  if (!dir.exists("index.html")) return false;
  e.folder = dir.absolutePath();
  // Folders are named <UTC time>-<idea>; fall back to the file time for folders made elsewhere.
  e.created = QDateTime::fromString(dir.dirName().left(19), "yyyy-MM-ddTHH-mm-ss");
  if (e.created.isValid()) {
    e.created.setTimeZone(QTimeZone::utc());
    e.created = e.created.toLocalTime();
  } else {
    e.created = QFileInfo(dir.filePath("index.html")).lastModified();
  }

  const QJsonObject summary = readJson(dir.filePath("result.json"));
  const QJsonObject storyboard = readJson(dir.filePath("storyboard.json"));
  e.title = summary.value("title").toString(storyboard.value("title").toString());
  if (e.title.isEmpty()) e.title = dir.dirName();
  e.mode = summary.value("mode").toString();
  e.approved = summary.value("approved").toBool();

  QFile prompt(dir.filePath("prompt.txt"));
  if (prompt.open(QIODevice::ReadOnly)) {
    e.prompt = QString::fromUtf8(prompt.readAll()).trimmed();
    // Keep only the idea the person typed, not the style and length lines the app added.
    e.prompt = e.prompt.section("\n\nStyle: ", 0, 0).section("\n\nLength: ", 0, 0);
  }
  if (e.mode.isEmpty()) {
    QFile html(dir.filePath("index.html"));
    e.mode = html.open(QIODevice::ReadOnly) && html.readAll().contains("three") ? "3d" : "2d";
  }
  return true;
}

QVector<HistoryEntry> loadHistory(const QString& base) {
  QVector<HistoryEntry> list;
  const QDir dir(base);
  for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    HistoryEntry e;
    if (readEntry(dir.filePath(name), e)) list.push_back(e);
  }
  std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.created > b.created; });
  return list;
}
