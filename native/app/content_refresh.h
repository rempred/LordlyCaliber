#pragma once
#include <QUndoCommand>
#include <QUndoStack>
#include <algorithm>
#include <functional>

namespace lordly {
// These commands change only Tools game rules. Artwork, scenario data and
// other content editors do not need to rebuild when a rule changes.
class GameRuleUndoCommand : public QUndoCommand {
  public:
    using QUndoCommand::QUndoCommand;
};

inline void watchContentChanges(QUndoStack *undo, QObject *receiver,
                                std::function<void()> refresh) {
    QObject::connect(undo, &QUndoStack::indexChanged, receiver,
        [undo, refresh=std::move(refresh), previous=undo->index()](int index) mutable {
            bool rulesOnly=index!=previous;
            for(int i=std::min(previous,index);rulesOnly && i<std::max(previous,index);++i)
                rulesOnly=i<undo->count() &&
                    dynamic_cast<const GameRuleUndoCommand *>(undo->command(i));
            previous=index;
            if(!rulesOnly)refresh();
        });
}
} // namespace lordly
