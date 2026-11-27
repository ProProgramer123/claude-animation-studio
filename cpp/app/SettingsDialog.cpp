#include "SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

QString AppSettings::defaultOutputDir() {
  QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if (docs.isEmpty()) docs = QDir::homePath();
  return QDir(docs).filePath("Claude Animation Studio");
}

AppSettings AppSettings::load() {
  QSettings s;
  AppSettings a;
  a.outputDir = s.value("outputDir", defaultOutputDir()).toString();
  a.claudePath = s.value("claudePath").toString();
  a.model = s.value("model").toString();
  a.ffmpegPath = s.value("ffmpegPath").toString();
  a.revisions = s.value("revisions", 2).toInt();
  a.browserCheck = s.value("browserCheck", true).toBool();
  a.offlineDemo = s.value("offlineDemo", false).toBool();
  return a;
}

void AppSettings::save() const {
  QSettings s;
  s.setValue("outputDir", outputDir);
  s.setValue("claudePath", claudePath);
  s.setValue("model", model);
  s.setValue("ffmpegPath", ffmpegPath);
  s.setValue("revisions", revisions);
  s.setValue("browserCheck", browserCheck);
  s.setValue("offlineDemo", offlineDemo);
}

SettingsDialog::SettingsDialog(const AppSettings& a, QWidget* parent) : QDialog(parent) {
  setWindowTitle("Settings");
  setMinimumWidth(520);

  outputDir_ = new QLineEdit(a.outputDir);
  auto* browse = new QPushButton("Browse…");
  connect(browse, &QPushButton::clicked, this, [this] {
    const QString dir = QFileDialog::getExistingDirectory(this, "Save animations in", outputDir_->text());
    if (!dir.isEmpty()) outputDir_->setText(dir);
  });
  auto* folderRow = new QHBoxLayout;
  folderRow->addWidget(outputDir_);
  folderRow->addWidget(browse);

  claudePath_ = new QLineEdit(a.claudePath);
  claudePath_->setPlaceholderText("claude (found on your PATH)");
  model_ = new QLineEdit(a.model);
  model_->setPlaceholderText("Your Claude Code default");
  ffmpeg_ = new QLineEdit(a.ffmpegPath);
  ffmpeg_->setPlaceholderText("ffmpeg (found on your PATH), needed for MP4 export");
  revisions_ = new QSpinBox;
  revisions_->setRange(0, 5);
  revisions_->setValue(a.revisions);
  browserCheck_ = new QCheckBox("Test each animation in the built-in browser before the reviewer signs off");
  browserCheck_->setChecked(a.browserCheck);
  offlineDemo_ = new QCheckBox("Offline demo: use canned agents instead of Claude (for trying the app)");
  offlineDemo_->setChecked(a.offlineDemo);

  auto* form = new QFormLayout;
  form->addRow("Save animations in", folderRow);
  form->addRow("Claude Code program", claudePath_);
  form->addRow("Model", model_);
  form->addRow("Reviewer fix rounds", revisions_);
  form->addRow("ffmpeg program", ffmpeg_);

  auto* note = new QLabel("Agents run through your Claude Code login, so no API key is needed.");
  note->setWordWrap(true);
  note->setObjectName("hint");

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(browserCheck_);
  layout->addWidget(offlineDemo_);
  layout->addWidget(note);
  layout->addWidget(buttons);
}

AppSettings SettingsDialog::result() const {
  AppSettings a;
  a.outputDir = outputDir_->text().trimmed().isEmpty() ? AppSettings::defaultOutputDir() : outputDir_->text().trimmed();
  a.claudePath = claudePath_->text().trimmed();
  a.model = model_->text().trimmed();
  a.ffmpegPath = ffmpeg_->text().trimmed();
  a.revisions = revisions_->value();
  a.browserCheck = browserCheck_->isChecked();
  a.offlineDemo = offlineDemo_->isChecked();
  return a;
}
