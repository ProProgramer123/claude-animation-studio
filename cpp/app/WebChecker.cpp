#include "WebChecker.h"

#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <functional>
#include <future>
#include <memory>

namespace {

// Counts animation frames and timers so we can tell whether the page is moving.
const char* kProbe = R"JS(
(() => {
  let webgl = false;
  try { const c = document.createElement('canvas'); webgl = !!(c.getContext('webgl2') || c.getContext('webgl')); } catch (e) {}
  window.__studio = { frames: 0, timers: 0, webgl };
  const raf = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = (cb) => raf((t) => { window.__studio.frames++; cb(t); });
  const si = window.setInterval.bind(window);
  window.setInterval = (cb, ms, ...a) => si((...x) => { window.__studio.timers++; typeof cb === 'function' && cb(...x); }, ms, ...a);
})();
)JS";

// Asked one second after load: is anything animating?
const char* kMoving = R"JS(
(() => {
  const s = window.__studio || { frames: 0, timers: 0 };
  let running = 0;
  try { running = document.getAnimations().filter(a => a.playState === 'running').length; } catch (e) {}
  const smil = document.querySelectorAll('animate, animateTransform, animateMotion, set').length;
  return { moving: s.frames > 5 || s.timers > 2 || running > 0 || smil > 0, webgl: s.webgl !== false };
})()
)JS";

bool mentionsWebGL(const std::vector<std::string>& errors) {
  for (const auto& e : errors)
    if (e.find("WebGL") != std::string::npos) return true;
  return false;
}

class ProbePage : public QWebEnginePage {
 public:
  explicit ProbePage(QObject* parent) : QWebEnginePage(parent) {}
  std::vector<std::string> errors;

 protected:
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int line,
                                const QString& source) override {
    if (level != ErrorMessageLevel) return;
    QString where = source.isEmpty() ? QString() : QString(" (%1:%2)").arg(QUrl(source).fileName()).arg(line);
    errors.push_back((message + where).toStdString());
  }
};

}  // namespace

WebChecker::WebChecker(QObject* parent) : QObject(parent) {}

std::optional<studio::CheckResult> WebChecker::check(const std::filesystem::path& htmlPath) {
  auto promise = std::make_shared<std::promise<std::optional<studio::CheckResult>>>();
  auto future = promise->get_future();
  const QUrl url = QUrl::fromLocalFile(QString::fromStdString(htmlPath.string()));

  QMetaObject::invokeMethod(this, [this, promise, url] {
    // Chromium pauses animation frames in pages nobody can see, so the page gets a real view
    // that is laid out and painted but never shown on screen.
    auto* view = new QWebEngineView;
    view->setAttribute(Qt::WA_DontShowOnScreen);
    view->resize(960, 600);
    auto* page = new ProbePage(view);
    view->setPage(page);
    view->show();
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    page->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    QWebEngineScript probe;
    probe.setSourceCode(kProbe);
    probe.setInjectionPoint(QWebEngineScript::DocumentCreation);
    probe.setWorldId(QWebEngineScript::MainWorld);
    page->scripts().insert(probe);

    auto done = std::make_shared<bool>(false);
    auto finish = [promise, view, done](std::optional<studio::CheckResult> r) {
      if (*done) return;
      *done = true;
      promise->set_value(std::move(r));
      view->deleteLater();
    };

    // Never hold the studio up for long if the page hangs.
    QTimer::singleShot(20000, page, [page, finish] {
      studio::CheckResult r;
      r.errors = page->errors;
      r.errors.push_back("The page took more than 20 seconds to load.");
      finish(r);
    });

    connect(page, &QWebEnginePage::loadFinished, page, [page, finish](bool ok) { 
      if (!ok) {
        studio::CheckResult r;
        r.errors = page->errors;
        r.errors.push_back("The page failed to load.");
        finish(r);
        return;
      }
      // Ask a few times: a 3D page may still be fetching Three.js when it first loads.
      auto poll = std::make_shared<std::function<void(int)>>();
      *poll = [page, finish, self = std::weak_ptr<std::function<void(int)>>(poll)](int triesLeft) {
        // The pending answer keeps this function alive; the function itself only holds a weak link.
        page->runJavaScript(kMoving, [page, finish, me = self.lock(), triesLeft](const QVariant& answer) {
          const QVariantMap a = answer.toMap();
          const bool moving = a.value("moving").toBool();
          // Without WebGL on this computer a 3D page can't be judged fairly, so skip the check
          // rather than send the renderer chasing an error that isn't in its code.
          if (!a.value("webgl", true).toBool() && mentionsWebGL(page->errors)) {
            finish(std::nullopt);
            return;
          }
          if (moving || triesLeft == 0) {
            studio::CheckResult r;
            r.errors = page->errors;
            r.isStatic = !moving;
            finish(r);
            return;
          }
          QTimer::singleShot(1000, page, [me, triesLeft] { (*me)(triesLeft - 1); });
        });
      };
      QTimer::singleShot(1500, page, [poll] { (*poll)(8); });
    });
    page->load(url);
  });

  return future.get();
}
