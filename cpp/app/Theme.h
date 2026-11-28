#pragma once

#include <QApplication>
#include <QFont>
#include <QStyleFactory>

/** One calm, light look on every platform. */
inline void applyTheme(QApplication& app) {
  app.setStyle(QStyleFactory::create("Fusion"));
  QFont font = app.font();
  if (font.pointSizeF() < 10) font.setPointSizeF(10);
  app.setFont(font);
  app.setStyleSheet(R"CSS(
    QMainWindow, QDialog { background: #f6f4ef; }
    QWidget { color: #2b2a27; }
    QMenuBar { background: #f6f4ef; }
    #sidebar { background: #eeebe3; border-right: 1px solid #e0dccf; }
    #sidebarHeading { color: #7a766c; font-size: 11px; text-transform: uppercase; letter-spacing: 1px; padding: 4px 2px; }
    #newButton { text-align: left; padding: 9px 12px; font-weight: 600; background: #ffffff;
                 border: 1px solid #ddd8ca; border-radius: 8px; }
    #newButton:hover { background: #faf9f6; }
    #history { background: transparent; border: none; outline: none; }
    #history::item { padding: 8px 10px; border-radius: 7px; margin: 1px 0; color: #3d3b36; }
    #history::item:hover { background: #e6e2d7; }
    #history::item:selected { background: #ffffff; color: #2b2a27; }
    #card { background: #ffffff; border: 1px solid #e4e0d5; border-radius: 12px; }
    #heading { font-size: 16px; font-weight: 600; }
    #hint { color: #7a766c; }
    #status { color: #3d3b36; font-weight: 500; }
    #placeholder { color: #7a766c; background: #fbfaf7; border-top-left-radius: 12px; border-top-right-radius: 12px; }
    #resultBar { border-top: 1px solid #ece8de; }
    #resultTitle { font-size: 15px; font-weight: 600; }
    QPlainTextEdit, QLineEdit, QSpinBox { background: #fbfaf7; border: 1px solid #ddd8ca; border-radius: 8px;
                                          padding: 6px 8px; font-size: 14px; selection-background-color: #f0c9b6; }
    QPlainTextEdit:focus, QLineEdit:focus { border: 1px solid #d97757; }
    QComboBox { background: #ffffff; border: 1px solid #ddd8ca; border-radius: 7px; padding: 5px 26px 5px 10px; min-width: 120px; }
    QPushButton { background: #ffffff; border: 1px solid #ddd8ca; border-radius: 7px; padding: 6px 14px; }
    QPushButton:hover { background: #faf9f6; border-color: #cfc9b8; }
    QPushButton:disabled { color: #b3afa5; }
    #primary { background: #d97757; color: white; border: none; font-weight: 600; padding: 8px 26px; font-size: 14px; }
    #primary:hover { background: #c96646; }
    #segment { min-width: 46px; padding: 6px 12px; font-weight: 600; border-radius: 0; }
    #segment:first-child { border-top-left-radius: 7px; border-bottom-left-radius: 7px; }
    #segment:checked { background: #2b2a27; color: white; border-color: #2b2a27; }
    #stepIcon { border-radius: 15px; font-weight: 700; background: #f1eee6; color: #9a968b; border: 1px solid #e0dccf; }
    #stepIcon[look="working"] { background: #d97757; color: white; border-color: #d97757; }
    #stepIcon[look="done"] { background: #5f8a5a; color: white; border-color: #5f8a5a; }
    #stepIcon[look="failed"] { background: #b5483a; color: white; border-color: #b5483a; }
    #stepName { color: #9a968b; font-size: 12px; }
    #stepName[look="working"] { color: #2b2a27; font-weight: 600; }
    #stepName[look="done"] { color: #3d3b36; }
    #stepLine { color: #d6d1c3; padding-top: 4px; }
    QStatusBar { color: #7a766c; }
  )CSS");
}
