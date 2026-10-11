#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QLabel;
class QComboBox;
namespace lordly {
class SpriteCanvas;
class CategoryIconEditor : public QWidget {
    Q_OBJECT
  public:
    CategoryIconEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectCategory(int);
    void selectFamily(int);
    int selectedCategory() const {
        return selected_;
    }
    void assignImage(const QImage &);
    void createIcon(int family = 0, bool copy = false);

  private:
    void refresh();
    void commit(const QString &, const CategoryIcons &);
    void guarded(const std::function<void()> &);
    void prepareImage(const QImage &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *icons_ = nullptr;
    SpriteCanvas *canvas_ = nullptr;
    QLabel *notice_ = nullptr, *usage_ = nullptr;
    QComboBox *family_ = nullptr;
    int selected_ = 1;
};
} // namespace lordly
