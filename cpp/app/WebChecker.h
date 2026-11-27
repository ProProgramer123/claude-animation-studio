#pragma once

#include <QObject>

#include "studio/pipeline.hpp"

/** Opens each animation in Qt's built-in Chromium, off screen, and reports console errors,
 *  failed loads and whether anything moves. The reviewer gets these findings.
 *
 *  check() is called from the pipeline's worker thread; the page itself lives on the GUI
 *  thread, so each check is handed over to it and the worker waits for the answer. */
class WebChecker : public QObject, public studio::AnimationChecker {
  Q_OBJECT
 public:
  explicit WebChecker(QObject* parent = nullptr);
  std::string name() const override { return "qt-webengine"; }
  std::optional<studio::CheckResult> check(const std::filesystem::path& htmlPath) override;
};
