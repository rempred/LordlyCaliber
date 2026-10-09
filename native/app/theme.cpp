#include "app/theme.h"
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace lordly {
void applyEditorTheme(QApplication &app) {
    app.setStyle(QStyleFactory::create("Fusion"));
    // Match the HTML editor's parchment, wood, gold and ink tokens in style.css.
    // Keep the native type size and geometry; artwork canvases retain their own
    // neutral backgrounds and the colors decoded from the user's ROM.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#ecd8a8"));
    palette.setColor(QPalette::WindowText, QColor("#2a1a08"));
    palette.setColor(QPalette::Base, QColor("#f4e6c3"));
    palette.setColor(QPalette::AlternateBase, QColor("#e5cf9a"));
    palette.setColor(QPalette::Text, QColor("#2a1a08"));
    palette.setColor(QPalette::Button, QColor("#dcc286"));
    palette.setColor(QPalette::ButtonText, QColor("#2a1a08"));
    palette.setColor(QPalette::Light, QColor("#fff2d1"));
    palette.setColor(QPalette::Midlight, QColor("#ecd8a8"));
    palette.setColor(QPalette::Mid, QColor("#9a7a3e"));
    palette.setColor(QPalette::Dark, QColor("#7a5a24"));
    palette.setColor(QPalette::Shadow, QColor("#3d2914"));
    palette.setColor(QPalette::Highlight, QColor("#c8a566"));
    palette.setColor(QPalette::HighlightedText, QColor("#2a1a08"));
    palette.setColor(QPalette::Accent, QColor("#982018"));
    palette.setColor(QPalette::PlaceholderText, QColor("#806648"));
    palette.setColor(QPalette::Link, QColor("#7d3519"));
    palette.setColor(QPalette::LinkVisited, QColor("#6a3a8a"));
    palette.setColor(QPalette::ToolTipBase, QColor("#3d2914"));
    palette.setColor(QPalette::ToolTipText, QColor("#ecd8a8"));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#806c50"));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#e1cfaa"));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor("#d6c39c"));
    app.setPalette(palette);
    app.setFont(QFont("Segoe UI", 10));
    app.setStyleSheet(QStringLiteral(R"QSS(
QMainWindow, QMenuBar {
    background: #3d2914;
}
QMenuBar {
    color: #ecd8a8;
}
QMenuBar::item:selected, QMenuBar::item:pressed {
    background: #c8a566;
    color: #2a1a08;
}
QMenu {
    background: #ecd8a8;
    color: #2a1a08;
    border: 1px solid #7a5a24;
}
QMenu::item:selected {
    background: #c8a566;
    color: #2a1a08;
}
QMenu::item:disabled { color: #806c50; }
QMenu::separator { height: 1px; background: #c8a566; margin: 4px 6px; }
QToolBar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #5a3a1c, stop:1 #1a0f06);
    border: none;
    border-bottom: 1px solid #a07b2a;
    spacing: 4px;
}
QToolBar QLabel { color: #ecd8a8; }
QToolBar::separator { background: #a07b2a; width: 1px; margin: 4px; }
QToolBar#projectActions {
    background: transparent;
    border: none;
    spacing: 3px;
    padding: 2px 4px;
}
QToolBar#projectActions QToolButton {
    background: transparent;
    color: #ecd8a8;
    border: 1px solid transparent;
    padding: 3px 8px;
}
QToolBar#projectActions QToolButton:hover {
    background: #5a3a1c;
    border-color: #9a7a3e;
}
QToolBar#projectActions QToolButton:pressed {
    background: #c8a566;
    color: #2a1a08;
}
QToolBar#projectActions QToolButton:focus { border-color: #e0b35a; }
QToolBar#projectActions QToolButton:disabled { color: #806c50; }
QStatusBar { background: #dcc286; color: #2a1a08; border-top: 1px solid #7a5a24; }
QStatusBar::item { border: none; }
QLabel#terrainMapContext {
    font-size: 16pt;
    font-weight: 600;
    color: #fff2d1;
    background: #3d2914;
    border: 1px solid #9a7a3e;
    border-left: 4px solid #c8a566;
    padding: 10px 12px;
}
QTabWidget::pane { border: 1px solid #9a7a3e; background: #ecd8a8; }
QTabBar { background: #3d2914; }
QTabBar::tab {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #5a3a1c, stop:1 #3d2914);
    color: #dcc286;
    border: 1px solid #1a0f06;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
    padding: 6px 10px;
}
QTabBar::tab:hover { background: #7a5120; color: #f3cc78; }
QTabBar::tab:selected {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #ecd8a8, stop:1 #dcc286);
    color: #2a1a08;
    border-color: #e0b35a;
}
QTabBar::tab:disabled { color: #b09a70; background: #3d2914; }
QPushButton, QToolButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #ecd8a8, stop:0.55 #dcc286, stop:1 #c8a566);
    color: #2a1a08;
    border: 1px solid #7a5a24;
    border-radius: 3px;
    padding: 4px 8px;
}
QPushButton:hover, QToolButton:hover { background: #f3cc78; border-color: #5a3a1c; }
QPushButton:pressed, QToolButton:pressed,
QPushButton:checked, QToolButton:checked {
    background: #7a5120;
    color: #fff2d1;
    border-color: #3d2914;
}
QPushButton:default, QPushButton:focus, QToolButton:focus { border-color: #982018; }
QPushButton:disabled, QToolButton:disabled {
    background: #d6c39c;
    color: #806c50;
    border-color: #b09a70;
}
QLineEdit, QComboBox {
    background: #f4e6c3;
    color: #2a1a08;
    border: 1px solid #9a7a3e;
    border-radius: 2px;
    padding: 2px 4px;
    selection-background-color: #c8a566;
    selection-color: #2a1a08;
}
QLineEdit:focus, QComboBox:focus { border-color: #982018; }
QLineEdit:disabled, QComboBox:disabled {
    background: #e1cfaa; color: #806c50; border-color: #b09a70;
}
QComboBox QAbstractItemView {
    background: #f4e6c3;
    color: #2a1a08;
    border: 1px solid #7a5a24;
    selection-background-color: #c8a566;
    selection-color: #2a1a08;
}
QAbstractItemView, QTextEdit, QPlainTextEdit {
    background: #f4e6c3;
    alternate-background-color: #e5cf9a;
    color: #2a1a08;
    border: 1px solid #9a7a3e;
    selection-background-color: #c8a566;
    selection-color: #2a1a08;
    gridline-color: #c8ae79;
}
QAbstractItemView::item:hover { background: #ecd8a8; }
QAbstractItemView::item:selected { background: #c8a566; color: #2a1a08; }
QHeaderView::section, QTableCornerButton::section {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #5a3a1c, stop:1 #3d2914);
    color: #f3cc78;
    border: none;
    border-right: 1px solid #9a7a3e;
    border-bottom: 1px solid #7a5a24;
    padding: 4px;
}
QGroupBox { border: 1px solid #9a7a3e; border-radius: 3px; margin-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #55391a; }
QSplitter::handle { background: #c8a566; }
QSplitter::handle:hover { background: #e0b35a; }
QScrollBar { background: #3d2914; }
QScrollBar::handle {
    background: #c89a4a; border: 1px solid #7a5120; border-radius: 3px;
    min-width: 18px; min-height: 18px;
}
QScrollBar::handle:hover { background: #e0b35a; }
QScrollBar::add-page, QScrollBar::sub-page { background: #3d2914; }
QToolTip { background: #3d2914; color: #ecd8a8; border: 1px solid #e0b35a; padding: 4px; }
QProgressBar { border: 1px solid #9a7a3e; background: #f4e6c3; color: #2a1a08; text-align: center; }
QProgressBar::chunk { background: #c8a566; }
)QSS"));
}
}
