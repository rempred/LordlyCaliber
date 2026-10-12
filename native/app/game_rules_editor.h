#pragma once
#include "engine/project.h"
#include <QWidget>
#include <QUndoStack>
#include <functional>
class QSpinBox;
namespace lordly {
class GameRulesEditor final : public QWidget {
  public:
    GameRulesEditor(Project *, QUndoStack *, std::function<void()>, const QStringList &groups, QWidget *parent=nullptr);
  private:
    void refresh();
    Project *project_;
    QMap<QString,QSpinBox *> fields_;
};
}
