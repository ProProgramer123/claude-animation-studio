#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>

#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("Claude Animation Studio");
  QApplication::setOrganizationName("Claude Animation Studio");
  QApplication::setApplicationVersion("0.3.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("Describe an animation and a team of AI agents builds it.");
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption offline("offline", "Use canned agents instead of Claude (a quick demo, no login needed).");
  parser.addOption(offline);
  parser.process(app);

  applyTheme(app);
  MainWindow window(parser.isSet(offline));
  window.show();
  return app.exec();
}
