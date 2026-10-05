#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QComboBox;
class QSpinBox;
class QListWidget;
class QLineEdit;
class QLabel;
class QTableWidget;
class QPushButton;
namespace lordly {
class SpriteCanvas;
class ArmyEditor : public QWidget {
    Q_OBJECT
  public:
    ArmyEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);
    ~ArmyEditor() override;
    void selectModel(int atlas, int model, int palette = 0);
    void assignImage(const QImage &, int alphaThreshold = 128);
    QString copyToLibrary();
    void importImage(const QString &);
    void exportImage(const QString &) const;

  private:
    void refresh();
    void refreshPlane();
    void refreshRoute();
    void prepareImage(const QImage &);
    void apply(const QByteArray &, const QString &);
    void guarded(const std::function<void()> &);
    QString modelName(int model) const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int atlas_ = 0, model_ = 0, paletteOrdinal_ = 0;
    bool refreshing_ = false;
    QComboBox *atlasSelect_, *paletteSelect_, *library_, *routeClass_;
    QSpinBox *frame_, *ink_;
    QListWidget *models_;
    QLineEdit *search_;
    QTableWidget *colors_;
    SpriteCanvas *canvas_;
    QLabel *title_, *consumers_, *alternate_, *notice_, *routeInfo_;
    QPushButton *create_, *assignRoute_, *restoreRoute_;
};
} // namespace lordly
