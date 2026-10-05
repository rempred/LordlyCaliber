#pragma once
#include "engine/project.h"
#include <QUndoCommand>
namespace lordly {
class SpriteLibraryCommand final : public QUndoCommand {
  public:
    SpriteLibraryCommand(Project *project, QString label, SpriteLibrary after)
        : QUndoCommand(label), project_(project), before_(project->sprites()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setSprites(before_);
    }
    void redo() override {
        project_->setSprites(after_);
    }

  private:
    Project *project_;
    SpriteLibrary before_, after_;
};
} // namespace lordly
