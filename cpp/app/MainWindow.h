#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>
#include <QVector>

#include "History.h"
#include "SettingsDialog.h"
#include "StudioRun.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QWebEngineView;
class Exporter;
class QProgressDialog;
class WebChecker;

/** The studio in one window: type an idea, pick 2D or 3D and a style, press Animate,
 *  watch the agents work, then watch the result. Past animations are listed on the left. */
class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(bool forceOffline, QWidget* parent = nullptr);

 protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  enum class StepState { Waiting, Working, Done, Failed };

  QWidget* buildComposer();
  QWidget* buildProgress();
  QWidget* buildPreview();
  QWidget* buildHistory();
  void buildMenus();

  void animate();
  void onProgress(studio::StageId stage, const QString& message);
  void onFinished(const studio::PipelineResult& result);
  void onFailed(const QString& message, bool cancelled);
  void setRunning(bool running);
  void setStep(int index, StepState state);
  void resetSteps();
  void refreshHistory(const QString& select = {});
  void showAnimation(const QString& folder);
  void newAnimation();
  void openSettings();
  void updateElapsed();
  void exportAs(int format);

  AppSettings settings_;
  bool forceOffline_;
  StudioRun* run_;
  WebChecker* checker_;
  Exporter* exporter_;
  QProgressDialog* exportProgress_ = nullptr;
  bool quitWhenDone_ = false;
  QString currentFolder_;
  QElapsedTimer elapsed_;
  QTimer tick_;

  QPlainTextEdit* idea_;
  QButtonGroup* modeGroup_;
  QComboBox* style_;
  QComboBox* length_;
  QPushButton* animate_;
  QPushButton* stop_;
  QPushButton* surprise_;

  QWidget* progressPanel_;
  QVector<QLabel*> stepIcons_;
  QVector<QLabel*> stepNames_;
  QLabel* status_;
  QLabel* elapsedLabel_;

  QStackedWidget* previewStack_;
  QLabel* placeholder_;
  QWebEngineView* preview_;
  QLabel* resultTitle_;
  QLabel* resultNote_;
  QPushButton* replay_;
  QPushButton* openBrowser_;
  QPushButton* showFiles_;
  QPushButton* reuse_;
  QPushButton* export_;

  QListWidget* history_;
  QVector<HistoryEntry> entries_;
};
