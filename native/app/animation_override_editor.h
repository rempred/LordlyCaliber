#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QTableWidget;
class QUndoStack;
namespace lordly {
class AnimationOverrideEditor : public QWidget {
    Q_OBJECT
  public:
    AnimationOverrideEditor(Project *, QUndoStack *, std::function<void()> changed,
                            QWidget *parent = nullptr);
    ~AnimationOverrideEditor() override;

  private:
    void refresh();
    void choices(bool stored = false);
    void selectRow();
    void apply(QJsonArray, const QString &, const QString &selection);
    void guarded(const std::function<void()> &);
    QJsonObject input() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    bool refreshing_ = false;
    QString selection_;
    QComboBox *classes_ = nullptr, *actions_ = nullptr, *routes_ = nullptr, *normal_ = nullptr,
              *blocked_ = nullptr;
    QTableWidget *table_ = nullptr;
    QWidget *controls_ = nullptr;
    QLabel *notice_ = nullptr, *summary_ = nullptr;
};
} // namespace lordly
