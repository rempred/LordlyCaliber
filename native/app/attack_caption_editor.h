#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QListWidget;
class QLineEdit;
class QLabel;
class QUndoStack;
namespace lordly {
class SpriteCanvas;
class AttackCaptionEditor : public QWidget {
    Q_OBJECT
  public:
    AttackCaptionEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectAttack(int);
    void importImage(const QImage &);
    void exportImage(const QString &) const;
  signals:
    void openAttack(int);

  private:
    void refresh();
    void showCaption();
    void publish(const AttackData &, const QString &);
    void guarded(const std::function<void()> &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *list_;
    QLineEdit *search_;
    QLabel *info_, *preview_;
    SpriteCanvas *canvas_;
    int selected_ = 25;
    bool refreshing_ = false;
};
} // namespace lordly
