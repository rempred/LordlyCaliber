#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QUndoStack;
class QListWidget;
class QLineEdit;
class QLabel;
class QTableWidget;
namespace lordly {
class SpriteCanvas;
class CombatEditor : public QWidget {
    Q_OBJECT
  public:
    CombatEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);
    ~CombatEditor() override;
    void selectBinding(const QString &, int child = 0);
    void assignImage(const QImage &);
    QString copyToLibrary();
    void importImage(const QString &);
    void exportImage(const QString &) const;

  private:
    void refresh();
    void refreshMembers();
    void refreshPixels();
    void apply(const SpritePixels &, const QString &);
    void prepareImage(const QImage &);
    void guarded(const std::function<void()> &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QString selected_;
    int child_ = 0;
    bool refreshing_ = false;
    QComboBox *descriptors_, *channel_, *library_;
    QSpinBox *childSelect_, *ink_, *libraryFrame_;
    QListWidget *members_;
    QLineEdit *search_;
    QLabel *notice_, *info_, *preview_;
    QTableWidget *palette_;
    SpriteCanvas *canvas_;
};
} // namespace lordly
