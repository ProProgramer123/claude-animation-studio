#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

/** Settings people rarely change, kept out of the main window. Saved with QSettings. */
struct AppSettings {
  QString outputDir;
  QString claudePath;
  QString model;
  int revisions = 2;
  bool browserCheck = true;
  bool offlineDemo = false;

  static AppSettings load();
  void save() const;
  static QString defaultOutputDir();
};

class SettingsDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SettingsDialog(const AppSettings& current, QWidget* parent = nullptr);
  AppSettings result() const;

 private:
  QLineEdit* outputDir_;
  QLineEdit* claudePath_;
  QLineEdit* model_;
  QSpinBox* revisions_;
  QCheckBox* browserCheck_;
  QCheckBox* offlineDemo_;
};
