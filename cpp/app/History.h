#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

/** One finished animation on disk. */
struct HistoryEntry {
  QString folder;
  QString title;
  QString prompt;
  QString mode;  // "2d" or "3d"
  bool approved = false;
  QDateTime created;
};

/** Finds the animations in `base`, newest first. Folders without an index.html are skipped. */
QVector<HistoryEntry> loadHistory(const QString& base);

/** Reads one animation folder. Returns false when it has no index.html. */
bool readEntry(const QString& folder, HistoryEntry& out);
