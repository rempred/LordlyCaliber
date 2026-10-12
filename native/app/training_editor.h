#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QLabel;
namespace lordly {
class SquadMembersEditor;
class TrainingEditor : public QWidget {
  public:
    TrainingEditor(Project *,QUndoStack *,std::function<void()>,QWidget *parent=nullptr);
  private:
    int row() const;
    void refresh();
    void save();
    void previewFee();
    Project *project_; QUndoStack *undo_; std::function<void()> changed_;
    QComboBox *node_,*slot_; QSpinBox *price_,*level_; QLabel *fee_,*notice_;
    SquadMembersEditor *members_; bool loading_=false;
};
}
