#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QProgressDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include "Exporter.h"
#include "Styles.h"
#include "WebChecker.h"
#include "studio/paths.hpp"

namespace {

// The seven steps people see. The first is the hidden director, shown without its name.
struct StepInfo {
  studio::StageId id;
  const char* label;
  const char* tip;
};
const StepInfo kSteps[] = {
    {studio::StageId::Director, "Your idea", "Turning your idea into a detailed brief"},
    {studio::StageId::Storyboard, "Storyboard", "Breaks the brief into scenes and timing"},
    {studio::StageId::Designer, "Designer", "Decides the characters, colors and objects"},
    {studio::StageId::Animator, "Animator", "Writes the movement and keyframes"},
    {studio::StageId::Sound, "Sound", "Writes the music and times the sound effects"},
    {studio::StageId::Renderer, "Renderer", "Turns the plan into working animation code"},
    {studio::StageId::Reviewer, "Reviewer", "Checks the result and sends fixes back"},
};
constexpr int kStepCount = 7;

int stepIndex(studio::StageId id) {
  for (int i = 0; i < kStepCount; ++i)
    if (kSteps[i].id == id) return i;
  return 0;
}

QString titleCase(const QString& mode) { return mode.toUpper(); }

}  // namespace

MainWindow::MainWindow(bool forceOffline, QWidget* parent)
    : QMainWindow(parent), settings_(AppSettings::load()), forceOffline_(forceOffline) {
  setWindowTitle("Claude Animation Studio");
  resize(1280, 820);

  run_ = new StudioRun(this);
  checker_ = new WebChecker(this);
  exporter_ = new Exporter(this);
  connect(exporter_, &Exporter::progress, this, [this](int pct, const QString& text) {
    if (!exportProgress_) return;
    exportProgress_->setValue(pct);
    exportProgress_->setLabelText(text);
  });
  connect(exporter_, &Exporter::finished, this, [this](const QString& path) {
    if (exportProgress_) {
      exportProgress_->reset();
      exportProgress_->hide();
    }
    QMessageBox box(this);
    box.setWindowTitle("Export finished");
    box.setText("Saved " + QFileInfo(path).fileName());
    box.setInformativeText(QDir::toNativeSeparators(path));
    auto* open = box.addButton("Open", QMessageBox::AcceptRole);
    auto* folder = box.addButton("Show folder", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.exec();
    if (box.clickedButton() == open) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (box.clickedButton() == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
  });
  connect(exporter_, &Exporter::failed, this, [this](const QString& message) {
    if (exportProgress_) {
      exportProgress_->reset();
      exportProgress_->hide();
    }
    if (!message.isEmpty()) QMessageBox::warning(this, "Export failed", message);
  });
  connect(run_, &StudioRun::progress, this, &MainWindow::onProgress);
  connect(run_, &StudioRun::finished, this, &MainWindow::onFinished);
  connect(run_, &StudioRun::failed, this, &MainWindow::onFailed);
  connect(&tick_, &QTimer::timeout, this, &MainWindow::updateElapsed);
  tick_.setInterval(1000);

  auto* right = new QWidget;
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(20, 18, 20, 16);
  rightLayout->setSpacing(14);
  rightLayout->addWidget(buildComposer());
  rightLayout->addWidget(buildProgress());
  rightLayout->addWidget(buildPreview(), 1);

  auto* splitter = new QSplitter;
  splitter->addWidget(buildHistory());
  splitter->addWidget(right);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({260, 1020});
  splitter->setChildrenCollapsible(false);
  setCentralWidget(splitter);

  buildMenus();
  resetSteps();
  progressPanel_->hide();
  refreshHistory();
  if (forceOffline_) statusBar()->showMessage("Offline demo: agents give canned replies, Claude is not used.");
  idea_->setFocus();
}

// ---- building the window ------------------------------------------------------

QWidget* MainWindow::buildComposer() {
  auto* box = new QWidget;
  box->setObjectName("card");
  auto* layout = new QVBoxLayout(box);
  layout->setContentsMargins(16, 14, 16, 14);

  auto* heading = new QLabel("What should your animation show?");
  heading->setObjectName("heading");

  idea_ = new QPlainTextEdit;
  idea_->setPlaceholderText("Describe it in a sentence, like “a paper boat sailing across a sunset sea”.");
  idea_->setFixedHeight(76);
  idea_->setTabChangesFocus(true);
  idea_->installEventFilter(this);

  auto* mode2d = new QPushButton("2D");
  auto* mode3d = new QPushButton("3D");
  mode2d->setToolTip("Flat SVG or canvas animation");
  mode3d->setToolTip("A Three.js 3D scene");
  for (auto* b : {mode2d, mode3d}) {
    b->setCheckable(true);
    b->setObjectName("segment");
    b->setCursor(Qt::PointingHandCursor);
  }
  mode2d->setChecked(true);
  modeGroup_ = new QButtonGroup(this);
  modeGroup_->addButton(mode2d, 0);
  modeGroup_->addButton(mode3d, 1);
  auto* modeRow = new QHBoxLayout;
  modeRow->setSpacing(0);
  modeRow->addWidget(mode2d);
  modeRow->addWidget(mode3d);

  style_ = new QComboBox;
  for (const auto& s : stylePresets()) style_->addItem(s.name);
  style_->setToolTip("A look for the animation. The director takes it into the brief.");
  length_ = new QComboBox;
  for (const auto& l : lengthPresets()) length_->addItem(l.name);
  length_->setToolTip("How long the animation runs. Custom lets you pick 2 to 60 seconds.");
  connect(length_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
    const int customIndex = length_->count() - 1;
    if (index != customIndex) return;
    bool ok = false;
    const int seconds = QInputDialog::getInt(this, "Custom length", "Length in seconds (2 to 60):",
                                             customLength_ > 0 ? customLength_ : 20, 2, 60, 1, &ok);
    if (ok) customLength_ = seconds;
    if (customLength_ > 0) {
      length_->setItemText(customIndex, QString("Custom (%1 s)").arg(customLength_));
    } else {
      length_->setCurrentIndex(0);
    }
  });

  surprise_ = new QPushButton("Surprise me");
  surprise_->setToolTip("Fill in a random idea");
  connect(surprise_, &QPushButton::clicked, this, [this] {
    const auto& ideas = sampleIdeas();
    idea_->setPlainText(ideas.at(QRandomGenerator::global()->bounded(ideas.size())));
  });

  animate_ = new QPushButton("Animate");
  animate_->setObjectName("primary");
  animate_->setToolTip("Start the studio (Ctrl+Enter)");
  animate_->setCursor(Qt::PointingHandCursor);
  connect(animate_, &QPushButton::clicked, this, &MainWindow::animate);
  stop_ = new QPushButton("Stop");
  stop_->hide();
  connect(stop_, &QPushButton::clicked, this, [this] {
    run_->cancel();
    stop_->setEnabled(false);
    status_->setText("Stopping…");
  });

  auto* row = new QHBoxLayout;
  row->addLayout(modeRow);
  row->addSpacing(8);
  row->addWidget(new QLabel("Style"));
  row->addWidget(style_);
  row->addWidget(new QLabel("Length"));
  row->addWidget(length_);
  row->addStretch(1);
  row->addWidget(surprise_);
  row->addWidget(stop_);
  row->addWidget(animate_);

  layout->addWidget(heading);
  layout->addWidget(idea_);
  layout->addLayout(row);
  return box;
}

QWidget* MainWindow::buildProgress() {
  progressPanel_ = new QWidget;
  progressPanel_->setObjectName("card");
  auto* layout = new QVBoxLayout(progressPanel_);
  layout->setContentsMargins(16, 12, 16, 12);

  auto* steps = new QHBoxLayout;
  for (int i = 0; i < kStepCount; ++i) {
    auto* col = new QVBoxLayout;
    auto* icon = new QLabel;
    icon->setAlignment(Qt::AlignCenter);
    icon->setObjectName("stepIcon");
    icon->setFixedSize(30, 30);
    auto* name = new QLabel(kSteps[i].label);
    name->setAlignment(Qt::AlignCenter);
    name->setObjectName("stepName");
    name->setToolTip(kSteps[i].tip);
    icon->setToolTip(kSteps[i].tip);
    col->addWidget(icon, 0, Qt::AlignHCenter);
    col->addWidget(name);
    steps->addLayout(col, 1);
    if (i + 1 < kStepCount) {
      auto* line = new QLabel("—");
      line->setObjectName("stepLine");
      steps->addWidget(line, 0, Qt::AlignTop);
    }
    stepIcons_.push_back(icon);
    stepNames_.push_back(name);
  }

  status_ = new QLabel;
  status_->setObjectName("status");
  elapsedLabel_ = new QLabel;
  elapsedLabel_->setObjectName("hint");
  auto* statusRow = new QHBoxLayout;
  statusRow->addWidget(status_, 1);
  statusRow->addWidget(elapsedLabel_);

  layout->addLayout(steps);
  layout->addLayout(statusRow);
  return progressPanel_;
}

QWidget* MainWindow::buildPreview() {
  auto* box = new QWidget;
  box->setObjectName("card");
  auto* layout = new QVBoxLayout(box);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  previewStack_ = new QStackedWidget;
  placeholder_ = new QLabel(
      "<p style='font-size:17px'>Your animation will play here.</p>"
      "<p>Type an idea above and press <b>Animate</b>. A director turns it into a brief,<br>"
      "then five agents plan, design, animate, build and review it. It takes a few minutes.</p>");
  placeholder_->setAlignment(Qt::AlignCenter);
  placeholder_->setObjectName("placeholder");
  placeholder_->setWordWrap(true);
  preview_ = new QWebEngineView;
  preview_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  preview_->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
  previewStack_->addWidget(placeholder_);
  previewStack_->addWidget(preview_);

  resultTitle_ = new QLabel;
  resultTitle_->setObjectName("resultTitle");
  resultNote_ = new QLabel;
  resultNote_->setObjectName("hint");
  resultNote_->setWordWrap(true);
  replay_ = new QPushButton("Replay");
  connect(replay_, &QPushButton::clicked, this, [this] { preview_->reload(); });
  openBrowser_ = new QPushButton("Open in browser");
  connect(openBrowser_, &QPushButton::clicked, this, [this] {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(currentFolder_).filePath("index.html")));
  });
  showFiles_ = new QPushButton("Show files");
  showFiles_->setToolTip("Open the folder with the HTML file and every agent's work");
  connect(showFiles_, &QPushButton::clicked, this,
          [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(currentFolder_)); });
  reuse_ = new QPushButton("Try again");
  reuse_->setToolTip("Put this idea back in the box so you can tweak it and animate again");
  connect(reuse_, &QPushButton::clicked, this, [this] {
    for (const auto& e : entries_)
      if (e.folder == currentFolder_) {
        idea_->setPlainText(e.prompt);
        modeGroup_->button(e.mode == "3d" ? 1 : 0)->setChecked(true);
      }
    idea_->setFocus();
  });

  export_ = new QPushButton("Export");
  export_->setToolTip("Save the animation as a GIF or an MP4 video");
  auto* exportMenu = new QMenu(export_);
  exportMenu->addAction("Animated GIF (for sharing)…", this, [this] { exportAs(int(Exporter::Format::Gif)); });
  exportMenu->addAction("MP4 video…", this, [this] { exportAs(int(Exporter::Format::Mp4)); });
  export_->setMenu(exportMenu);

  auto* text = new QVBoxLayout;
  text->addWidget(resultTitle_);
  text->addWidget(resultNote_);
  auto* bar = new QHBoxLayout;
  bar->setContentsMargins(16, 10, 16, 12);
  bar->addLayout(text, 1);
  for (auto* b : {reuse_, replay_, showFiles_, openBrowser_, export_}) bar->addWidget(b);
  export_->setObjectName("exportButton");
  auto* barWidget = new QWidget;
  barWidget->setLayout(bar);
  barWidget->setObjectName("resultBar");

  layout->addWidget(previewStack_, 1);
  layout->addWidget(barWidget);
  for (auto* b : {reuse_, replay_, showFiles_, openBrowser_, export_}) b->setEnabled(false);
  return box;
}

QWidget* MainWindow::buildHistory() {
  auto* box = new QWidget;
  box->setObjectName("sidebar");
  auto* layout = new QVBoxLayout(box);
  layout->setContentsMargins(12, 18, 12, 12);

  auto* fresh = new QPushButton("+  New animation");
  fresh->setObjectName("newButton");
  connect(fresh, &QPushButton::clicked, this, &MainWindow::newAnimation);
  auto* label = new QLabel("Your animations");
  label->setObjectName("sidebarHeading");
  history_ = new QListWidget;
  history_->setObjectName("history");
  history_->setWordWrap(true);
  history_->setTextElideMode(Qt::ElideRight);
  connect(history_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0 && row < entries_.size() && !run_->running()) showAnimation(entries_[row].folder);
  });

  layout->addWidget(fresh);
  layout->addSpacing(8);
  layout->addWidget(label);
  layout->addWidget(history_, 1);
  box->setMinimumWidth(220);
  return box;
}

void MainWindow::buildMenus() {
  auto* file = menuBar()->addMenu("&File");
  file->addAction("&New animation", QKeySequence::New, this, &MainWindow::newAnimation);
  file->addAction("&Open animation folder…", QKeySequence::Open, this, [this] {
    const QString dir = QFileDialog::getExistingDirectory(this, "Open an animation folder", settings_.outputDir);
    HistoryEntry e;
    if (dir.isEmpty()) return;
    if (!readEntry(dir, e)) {
      QMessageBox::information(this, "No animation here", "That folder has no index.html.");
      return;
    }
    entries_.push_front(e);
    history_->insertItem(0, e.title);
    history_->setCurrentRow(0);
  });
  file->addSeparator();
  file->addAction("&Settings…", QKeySequence::Preferences, this, &MainWindow::openSettings);
  file->addSeparator();
  file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);

  auto* help = menuBar()->addMenu("&Help");
  help->addAction("&About", this, [this] {
    QMessageBox::about(this, "About Claude Animation Studio",
                       "<b>Claude Animation Studio</b><br>Describe an animation and a team of AI agents "
                       "builds it for you in 2D or 3D.<br><br>Agents run through your Claude Code login.<br>"
                       "<a href='https://github.com/ProProgramer123/claude-animation-studio'>"
                       "github.com/ProProgramer123/claude-animation-studio</a>");
  });

  auto* go = new QShortcut(QKeySequence("Ctrl+Return"), this);
  connect(go, &QShortcut::activated, this, &MainWindow::animate);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  // Enter starts the studio; Shift+Enter adds a new line.
  if (watched == idea_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
      animate();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

// ---- running the studio ---------------------------------------------------------

void MainWindow::animate() {
  if (run_->running()) return;
  const QString idea = idea_->toPlainText().trimmed();
  if (idea.isEmpty()) {
    idea_->setFocus();
    status_->setText("Type an idea first.");
    statusBar()->showMessage("Type an idea first, or press Surprise me.", 4000);
    return;
  }
  const auto mode = modeGroup_->checkedId() == 1 ? studio::Mode::ThreeD : studio::Mode::TwoD;
  const QString prompt =
      buildPrompt(idea, stylePresets().at(style_->currentIndex()));
  const int preset = lengthPresets().at(length_->currentIndex()).seconds;
  const double lengthSeconds = preset < 0 ? customLength_ : preset;

  QDir().mkpath(settings_.outputDir);
  const auto dir = studio::animationDir(idea.toStdString(), std::filesystem::u8path(settings_.outputDir.toStdString()));
  currentFolder_ = QString::fromStdString(dir.u8string());

  StudioRun::Settings s;
  s.runner = (forceOffline_ || settings_.offlineDemo) ? "mock" : "claude-code";
  s.claudePath = settings_.claudePath;
  s.model = settings_.model;
  s.revisions = settings_.revisions;

  resetSteps();
  progressPanel_->show();
  previewStack_->setCurrentWidget(placeholder_);
  placeholder_->setText("<p style='font-size:17px'>The studio is working on it…</p>"
                        "<p>It usually takes a few minutes. You can keep watching the steps above.</p>");
  resultTitle_->setText(idea.left(80));
  resultNote_->setText(QString("%1 · %2").arg(mode == studio::Mode::ThreeD ? "3D" : "2D", style_->currentText()));
  for (auto* b : {reuse_, replay_, showFiles_, openBrowser_, export_}) b->setEnabled(false);
  status_->setText("Starting the studio…");
  elapsed_.start();
  updateElapsed();
  tick_.start();
  setRunning(true);
  run_->start(prompt, mode, lengthSeconds, studio::SoundOptions{}, currentFolder_, s, settings_.browserCheck ? checker_ : nullptr);
}

void MainWindow::onProgress(studio::StageId stage, const QString& message) {
  const int index = stepIndex(stage);
  for (int i = 0; i < kStepCount; ++i)
    setStep(i, i < index ? StepState::Done : i == index ? StepState::Working : StepState::Waiting);
  // The director is a secret: say what is happening without naming it.
  status_->setText(stage == studio::StageId::Director ? "Turning your idea into a detailed brief…" : message + "…");
}

void MainWindow::onFinished(const studio::PipelineResult& r) {
  setRunning(false);
  for (int i = 0; i < kStepCount; ++i) setStep(i, StepState::Done);
  const QString time = elapsedLabel_->text();
  status_->setText(r.approved ? "Done! The reviewer approved it." : "Done, with a few notes from the reviewer.");
  refreshHistory(QString::fromStdString(r.outputDir.u8string()));
  showAnimation(QString::fromStdString(r.outputDir.u8string()));
  if (!r.approved && !r.openIssues.empty()) {
    QStringList issues;
    for (const auto& i : r.openIssues) issues << QString::fromStdString(i);
    resultNote_->setText("Reviewer notes: " + issues.join(" · "));
  }
  statusBar()->showMessage(QString("Finished in %1.").arg(time), 8000);
  if (quitWhenDone_) qApp->quit();
}

void MainWindow::onFailed(const QString& message, bool cancelled) {
  setRunning(false);
  for (int i = 0; i < kStepCount; ++i)
    if (stepIcons_[i]->property("state").toInt() == int(StepState::Working)) setStep(i, StepState::Failed);
  if (quitWhenDone_) {
    qApp->quit();
    return;
  }
  if (cancelled) {
    status_->setText("Stopped.");
    placeholder_->setText("<p style='font-size:17px'>Stopped.</p><p>Change your idea and press Animate to try again.</p>");
    return;
  }
  status_->setText("The studio could not finish.");
  placeholder_->setText("<p style='font-size:17px'>The studio could not finish.</p><p>" + message.toHtmlEscaped() +
                        "</p><p>Check that Claude Code is installed and signed in, then try again. "
                        "Settings lets you point to the program.</p>");
}

void MainWindow::setRunning(bool running) {
  animate_->setVisible(!running);
  surprise_->setEnabled(!running);
  stop_->setVisible(running);
  stop_->setEnabled(running);
  idea_->setReadOnly(running);
  style_->setEnabled(!running);
  length_->setEnabled(!running);
  for (auto* b : modeGroup_->buttons()) b->setEnabled(!running);
  history_->setEnabled(!running);
  if (!running) tick_.stop();
}

void MainWindow::setStep(int index, StepState state) {
  static const char* icons[] = {"", "●", "✓", "!"};
  static const char* names[] = {"waiting", "working", "done", "failed"};
  QLabel* icon = stepIcons_[index];
  icon->setText(state == StepState::Waiting ? QString::number(index + 1) : QString::fromUtf8(icons[int(state)]));
  icon->setProperty("state", int(state));
  icon->setProperty("look", names[int(state)]);
  stepNames_[index]->setProperty("look", names[int(state)]);
  for (QWidget* w : {static_cast<QWidget*>(icon), static_cast<QWidget*>(stepNames_[index])}) {
    w->style()->unpolish(w);
    w->style()->polish(w);
  }
}

void MainWindow::resetSteps() {
  for (int i = 0; i < kStepCount; ++i) setStep(i, StepState::Waiting);
}

void MainWindow::updateElapsed() {
  const qint64 s = elapsed_.elapsed() / 1000;
  elapsedLabel_->setText(QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
}

// ---- history and preview ----------------------------------------------------------

void MainWindow::refreshHistory(const QString& select) {
  entries_ = loadHistory(settings_.outputDir);
  const QSignalBlocker block(history_);
  history_->clear();
  for (const auto& e : entries_) {
    auto* item = new QListWidgetItem(QString("%1\n%2 · %3").arg(e.title, titleCase(e.mode),
                                                               e.created.toString("d MMM, h:mm ap")));
    item->setToolTip(e.prompt);
    history_->addItem(item);
  }
  for (int i = 0; i < entries_.size(); ++i)
    if (QDir(entries_[i].folder) == QDir(select)) history_->setCurrentRow(i);
}

void MainWindow::showAnimation(const QString& folder) {
  HistoryEntry e;
  if (!readEntry(folder, e)) return;
  currentFolder_ = e.folder;
  preview_->load(QUrl::fromLocalFile(QDir(folder).filePath("index.html")));
  previewStack_->setCurrentWidget(preview_);
  resultTitle_->setText(e.title);
  resultNote_->setText(QString("%1 · “%2”").arg(titleCase(e.mode), e.prompt.left(120)));
  for (auto* b : {reuse_, replay_, showFiles_, openBrowser_, export_}) b->setEnabled(true);
}

void MainWindow::newAnimation() {
  if (run_->running()) return;
  history_->clearSelection();
  history_->setCurrentRow(-1);
  idea_->clear();
  idea_->setFocus();
  progressPanel_->hide();
  preview_->setUrl(QUrl("about:blank"));
  previewStack_->setCurrentWidget(placeholder_);
  placeholder_->setText(
      "<p style='font-size:17px'>Your animation will play here.</p>"
      "<p>Type an idea above and press <b>Animate</b>.</p>");
  resultTitle_->clear();
  resultNote_->clear();
  for (auto* b : {reuse_, replay_, showFiles_, openBrowser_, export_}) b->setEnabled(false);
}

void MainWindow::exportAs(int f) {
  const auto format = static_cast<Exporter::Format>(f);
  if (currentFolder_.isEmpty() || exporter_->busy()) return;
  const QString ffmpeg = Exporter::findFfmpeg(settings_.ffmpegPath);
  if (format == Exporter::Format::Mp4 && ffmpeg.isEmpty()) {
    QMessageBox::information(this, "MP4 needs ffmpeg",
                             "MP4 export uses the free ffmpeg program, which isn't installed.<br><br>"
                             "Install it from <a href='https://ffmpeg.org/download.html'>ffmpeg.org</a> "
                             "(or <code>brew install ffmpeg</code>, <code>winget install ffmpeg</code>, "
                             "<code>sudo apt install ffmpeg</code>), or set its location in Settings.<br><br>"
                             "Animated GIF export works without it.");
    return;
  }
  const QString ext = format == Exporter::Format::Gif ? "gif" : "mp4";
  QString name = resultTitle_->text().trimmed();
  name = QString::fromStdString(studio::slugify(name.toStdString()));
  const QString suggested = QDir(currentFolder_).filePath(name + "." + ext);
  const QString path = QFileDialog::getSaveFileName(
      this, format == Exporter::Format::Gif ? "Save as GIF" : "Save as MP4", suggested,
      format == Exporter::Format::Gif ? "Animated GIF (*.gif)" : "MP4 video (*.mp4)");
  if (path.isEmpty()) return;

  if (!exportProgress_) {
    exportProgress_ = new QProgressDialog(this);
    exportProgress_->setWindowTitle("Exporting");
    exportProgress_->setWindowModality(Qt::WindowModal);
    exportProgress_->setMinimumDuration(0);
    exportProgress_->setAutoReset(false);
    exportProgress_->setAutoClose(false);
    exportProgress_->setMinimumWidth(380);
    connect(exportProgress_, &QProgressDialog::canceled, exporter_, &Exporter::cancel);
  }
  exportProgress_->setRange(0, 100);
  exportProgress_->setValue(0);
  exportProgress_->setLabelText(QString("Recording the animation (about %1 seconds)…")
                                    .arg(Exporter::animationSeconds(currentFolder_), 0, 'f', 0));
  exportProgress_->show();
  exporter_->start(currentFolder_, format, path, ffmpeg);
}

void MainWindow::openSettings() {
  SettingsDialog dialog(settings_, this);
  if (dialog.exec() != QDialog::Accepted) return;
  settings_ = dialog.result();
  settings_.save();
  refreshHistory(currentFolder_);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (!run_->running()) {
    event->accept();
    return;
  }
  const auto answer = QMessageBox::question(this, "Stop the studio?",
                                            "An animation is still being made. Stop it and quit?");
  if (answer != QMessageBox::Yes) {
    event->ignore();
    return;
  }
  // Let the run wind down (it may be waiting on the browser check, which needs this thread).
  quitWhenDone_ = true;
  run_->cancel();
  hide();
  event->ignore();
}
