#pragma once
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

namespace lordly {
// A native OS drag has a platform drag session. An offscreen workspace supplies
// its own events, but should still reach the same view validation and undo command.
class ViewDrop {
  public:
    virtual ~ViewDrop() = default;
    virtual void dispatchDrop(QDragEnterEvent &, QDragMoveEvent &, QDropEvent &) = 0;
};
template <class View> class DroppableView : public View, public ViewDrop {
  public:
    using View::View;
    void dispatchDrop(QDragEnterEvent &enter, QDragMoveEvent &move, QDropEvent &drop) override {
        this->dragEnterEvent(&enter);
        if (!enter.isAccepted())
            return;
        this->dragMoveEvent(&move);
        this->dropEvent(&drop);
    }
};
} // namespace lordly
