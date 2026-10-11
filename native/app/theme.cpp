#include "app/theme.h"
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace lordly {
void applyEditorTheme(QApplication &app) {
    app.setStyle(QStyleFactory::create("Fusion"));
    // Player's Companion: wood framing, parchment, gold and wax-red accents.
    // Serif navigation/headings give the shell character; dense editing fields
    // keep Segoe UI. Artwork canvases retain their ROM colors and backgrounds.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#ecdcb8"));
    palette.setColor(QPalette::WindowText, QColor("#2a1a08"));
    palette.setColor(QPalette::Base, QColor("#fbf2dc"));
    palette.setColor(QPalette::AlternateBase, QColor("#f1e3c4"));
    palette.setColor(QPalette::Text, QColor("#2a1a08"));
    palette.setColor(QPalette::Button, QColor("#dcc286"));
    palette.setColor(QPalette::ButtonText, QColor("#2a1a08"));
    palette.setColor(QPalette::Light, QColor("#fff2d1"));
    palette.setColor(QPalette::Midlight, QColor("#ecd8a8"));
    palette.setColor(QPalette::Mid, QColor("#9a7a3e"));
    palette.setColor(QPalette::Dark, QColor("#7a5a24"));
    palette.setColor(QPalette::Shadow, QColor("#3d2914"));
    palette.setColor(QPalette::Highlight, QColor("#852c24"));
    palette.setColor(QPalette::HighlightedText, QColor("#fff5dc"));
    palette.setColor(QPalette::Accent, QColor("#982018"));
    palette.setColor(QPalette::PlaceholderText, QColor("#806648"));
    palette.setColor(QPalette::Link, QColor("#982018"));
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
    background: #2a1d0f;
}
QMenuBar {
    color: #f3cc78;
    padding: 2px 4px;
    border-bottom: 1px solid #a07b2a;
}
QMenuBar::item { padding: 5px 9px; background: transparent; }
QMenuBar::item:selected, QMenuBar::item:pressed {
    background: #5a3a1c; color: #fff5dc;
}
QMenu {
    background: #fbf2dc; color: #2a1a08;
    border: 1px solid #9a7a3e; padding: 4px;
}
QMenu::item { padding: 5px 24px; }
QMenu::item:selected { background: #852c24; color: #fff5dc; }
QMenu::item:disabled { color: #806c50; }
QMenu::separator { height: 1px; background: #c8a566; margin: 4px 8px; }
QToolBar {
    background: #3d2914; border: none;
    border-bottom: 1px solid #a07b2a; spacing: 4px;
}
QToolBar QLabel { color: #ecd8a8; }
QToolBar::separator { background: #a07b2a; width: 1px; margin: 4px; }
QToolBar#projectActions {
    background: transparent; border: none; spacing: 3px; padding: 0 4px;
}
QToolBar#projectActions QToolButton {
    background: transparent; color: #ecd8a8;
    border: 1px solid transparent; padding: 4px 8px;
}
QToolBar#projectActions QToolButton:hover {
    background: #5a3a1c; border-color: #9a7a3e; color: #f3cc78;
}
QToolBar#projectActions QToolButton:pressed {
    background: #852c24; color: #fff5dc;
}
QToolBar#projectActions QToolButton:focus { border-color: #e0b35a; }
QToolBar#projectActions QToolButton:disabled { color: #b09a70; }
QStatusBar {
    background: #3d2914; color: #ecd8a8; border-top: 1px solid #a07b2a;
}
QStatusBar QLabel { color: #ecd8a8; }
QStatusBar::item { border: none; }
QLabel#terrainMapContext {
    font-family: "Palatino Linotype", Georgia, serif;
    font-size: 17pt; font-weight: bold;
    color: #f3cc78;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5a3a1c, stop:1 #2a1d0f);
    border: 1px solid #a07b2a; border-left: 4px solid #982018;
    padding: 10px 14px;
}
/* Gold section tabs sit below the dark workspace row. */
QTabWidget::pane { border: 1px solid #b69a64; background: #ecdcb8; }
QTabBar { background: #dcc286; font-weight: 600; }
QTabBar::tab {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ecd8a8, stop:1 #dcc286);
    color: #55391a;
    border: none; border-right: 1px solid #b69a64;
    border-top: 1px solid #f9eac7; border-bottom: 3px solid #b69a64;
    padding: 8px 13px;
}
QTabBar::tab:hover { background: #f3cc78; color: #70221c; }
QTabBar::tab:selected {
    background: #5a3a1c; color: #fff1cb;
    border-top-color: #e0b35a; border-bottom-color: #982018;
}
QTabBar::tab:disabled { color: #806c50; }
QTabBar#workspaceNavigation {
    background: #3d2914;
    font-family: Georgia, "Palatino Linotype", serif;
    font-size: 10pt; font-weight: bold;
}
QTabBar#workspaceNavigation::tab {
    background: #50371e; color: #f2dfb5;
    border: 1px solid #94703b; border-bottom: 3px solid #684b28;
    border-top-left-radius: 5px; border-top-right-radius: 5px;
    margin: 4px 3px 0px 0px;
    padding: 9px 12px;
}
QTabBar#workspaceNavigation::tab:hover {
    background: #694a27; color: #fff1cb; border-color: #d0a85c;
}
QTabBar#workspaceNavigation::tab:selected {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #982f25, stop:1 #70221c);
    color: #fff1cb; border-color: #f3cc78; border-bottom-color: #f3cc78;
}
QTabBar#workspaceNavigation::tab:disabled { color: #b09a70; }
QPushButton, QToolButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fbf2dc, stop:1 #e4cfa1);
    color: #2a1a08; border: 1px solid #a58a54; border-radius: 4px;
    padding: 5px 10px;
}
QPushButton:hover, QToolButton:hover {
    background: #fff1cb; border-color: #982018; color: #852c24;
}
QPushButton:pressed, QToolButton:pressed,
QPushButton:checked, QToolButton:checked,
QPushButton:default {
    background: #852c24; color: #fff5dc; border-color: #70221c;
}
QPushButton:default:hover { background: #982018; }
QPushButton:focus, QToolButton:focus { border: 1px solid #982018; }
QPushButton:disabled, QToolButton:disabled {
    background: #e1d3b2; color: #806c50; border-color: #bdab87;
}
QLineEdit, QComboBox {
    background: #fbf2dc; color: #2a1a08;
    border: 1px solid #b69a64; border-radius: 3px; padding: 3px 5px;
    selection-background-color: #852c24; selection-color: #fff5dc;
}
QLineEdit:hover, QComboBox:hover { border-color: #8a6838; }
QLineEdit:focus, QComboBox:focus { border-color: #982018; }
QLineEdit:disabled, QComboBox:disabled {
    background: #e1d3b2; color: #806c50; border-color: #bdab87;
}
QComboBox QAbstractItemView {
    background: #fbf2dc; color: #2a1a08; border: 1px solid #9a7a3e;
    selection-background-color: #852c24; selection-color: #fff5dc;
}
QAbstractItemView, QTextEdit, QPlainTextEdit {
    background: #fbf2dc; alternate-background-color: #f1e3c4; color: #2a1a08;
    border: 1px solid #b69a64;
    selection-background-color: #852c24; selection-color: #fff5dc;
    gridline-color: #dfcca5;
}
QTextEdit:focus, QPlainTextEdit:focus { border-color: #982018; }
QAbstractItemView::item:hover { background: #ecd8a8; }
QAbstractItemView::item:selected { background: #852c24; color: #fff5dc; }
QListView::item { padding: 3px 4px; }
QHeaderView::section, QTableCornerButton::section {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e9d5a8, stop:1 #dcc286);
    color: #55391a; font-weight: 600;
    border: none; border-right: 1px solid #c8ae79; border-bottom: 2px solid #9a7a3e;
    padding: 6px;
}
QGroupBox {
    border: 1px solid #b69a64; border-radius: 4px;
    margin-top: 14px; padding-top: 6px;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 10px; padding: 0 5px;
    color: #852c24; font-family: Georgia, "Palatino Linotype", serif; font-weight: bold;
}
QCheckBox, QRadioButton { spacing: 6px; }
QCheckBox:focus, QRadioButton:focus { color: #852c24; }
QSplitter::handle { background: #d6bd8c; }
QSplitter::handle:hover { background: #a07b2a; }
QScrollBar:vertical { background: #e4d5b5; width: 13px; margin: 0; }
QScrollBar:horizontal { background: #e4d5b5; height: 13px; margin: 0; }
QScrollBar::handle {
    background: #b29560; border: 2px solid #e4d5b5; border-radius: 5px;
}
QScrollBar::handle:vertical { min-height: 24px; }
QScrollBar::handle:horizontal { min-width: 24px; }
QScrollBar::handle:hover { background: #8a6838; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QToolTip {
    background: #3d2914; color: #fff1cb; border: 1px solid #e0b35a; padding: 6px;
}
QProgressBar {
    border: 1px solid #b69a64; border-radius: 3px;
    background: #fbf2dc; color: #2a1a08; text-align: center;
}
QProgressBar::chunk { background: #c8a566; }
)QSS"));
}
}
