#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QTableWidget;
class QPushButton;
namespace lordly {
class SpriteCanvas;
class PortraitEditor : public QWidget {
    Q_OBJECT
  public:
    PortraitEditor(Project *, QUndoStack *, std::function<void()> changed,
                   QWidget *parent = nullptr);
    ~PortraitEditor() override;
    void selectPortrait(int classId, int selector);
    void assignImage(const QImage &, bool reduceColors = false);
    QString copyToLibrary();
    void exportImage(const QString &) const;
    void importImage(const QString &);

  private:
    void refresh();
    void refreshPortrait();
    void prepareImage(const QImage &);
    void apply(const QByteArray &, const QString &);
    void guarded(const std::function<void()> &);
    void setColor(QColor);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int selected_ = 1, selector_ = 0;
    bool refreshing_ = false;
    SpriteCanvas *canvas_;
    QListWidget *classes_;
    QComboBox *variant_, *library_;
    QSpinBox *frame_;
    QCheckBox *both_;
    QLineEdit *search_;
    QLabel *notice_, *title_;
    QTableWidget *palette_;
    QPushButton *color_;
    QColor paint_ = Qt::black;
};
} // namespace lordly
