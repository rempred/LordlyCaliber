#pragma once
#include <QAbstractItemView>
#include <QPointer>
#include <QScrollBar>
#include <QVector>
namespace lordly {
// Refreshing data must not move the user's viewport. Selection is owned by
// each editor; intentional navigation after refresh can still reveal its row.
class ViewPosition {
    struct Entry { QPointer<QAbstractItemView> view; int x, y; };
    QVector<Entry> entries_;
  public:
    explicit ViewPosition(QWidget *page, bool retain = true) {
        if (!retain) return;
        for (auto view : page->findChildren<QAbstractItemView *>())
            entries_.push_back({view, view->horizontalScrollBar()->value(), view->verticalScrollBar()->value()});
    }
    ~ViewPosition() {
        for (const auto &entry : entries_) if (entry.view) {
            entry.view->doItemsLayout();
            entry.view->horizontalScrollBar()->setValue(entry.x);
            entry.view->verticalScrollBar()->setValue(entry.y);
        }
    }
};
}
