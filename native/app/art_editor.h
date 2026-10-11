#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QUndoStack;
namespace lordly {
class SpriteCanvas;
class ArtEditor : public QWidget {
    Q_OBJECT
  public:
    ArtEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);
    ~ArtEditor() override;
    void selectIcon(quint16);
    void assignImage(const QImage &, bool matchPalette = true, int alphaThreshold = 128);
    QString copyToLibrary();
    void exportImage(const QString &) const;
    void importImage(const QString &);

  private:
    void refresh();
    void refreshIcon();
    void apply(const QByteArray &, const QString &);
    void prepareImage(const QImage &);
    void guarded(const std::function<void()> &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    quint16 selected_ = 1;
    QComboBox *pack_, *library_;
    QSpinBox *frame_;
    QListWidget *icons_;
    QLineEdit *search_;
    QTableWidget *palette_;
    SpriteCanvas *canvas_;
    QLabel *notice_, *title_;
    bool refreshing_ = false;
};
} // namespace lordly
