#pragma once
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QHeaderView>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QTableWidget>
#include <functional>
namespace lordly {
inline constexpr auto poolMime = "application/x-lordly-encounter-pool";
inline void dragPool(QWidget *source, int pool, int row, const QString &label) {
    auto data = new QMimeData;
    data->setData(poolMime, QByteArray::number(pool) + ":" + QByteArray::number(row));
    auto drag = new QDrag(source);
    drag->setMimeData(data);
    QPixmap image(140, 32); image.fill(QColor("#285d74"));
    QPainter painter(&image); painter.setPen(Qt::white); painter.drawText(image.rect(), Qt::AlignCenter, label); painter.end();
    drag->setPixmap(image); drag->setHotSpot(QPoint(70, 16));
    drag->exec(Qt::MoveAction | Qt::CopyAction, row < 0 ? Qt::CopyAction : Qt::MoveAction);
}
class PoolTiles : public QListWidget {
  public:
    PoolTiles() { setDragEnabled(true); }
  protected:
    void startDrag(Qt::DropActions) override {
        if (currentItem()) dragPool(this, currentItem()->data(Qt::UserRole).toInt(), -1, currentItem()->text());
    }
};
class TerrainAssignments : public QTableWidget {
  public:
    std::function<void(int, int, int)> assign;
    TerrainAssignments() : QTableWidget(32, 3) {
        setDragEnabled(true); setAcceptDrops(true); setDragDropMode(QAbstractItemView::DragDrop);
        setDropIndicatorShown(false); setSortingEnabled(false);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
  protected:
    void startDrag(Qt::DropActions) override {
        auto cell = currentItem();
        if (cell && cell->column() == 2 && cell->data(Qt::UserRole).toInt())
            dragPool(this, cell->data(Qt::UserRole).toInt(), cell->row(), cell->text());
    }
    bool valid(const QMimeData *mime) const {
        return mime->hasFormat(poolMime);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (valid(event->mimeData())) event->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        target_ = rowAt(event->position().toPoint().y());
        if (valid(event->mimeData()) && target_ >= 0) event->acceptProposedAction();
        else event->ignore();
        viewport()->update();
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override { target_ = -1; viewport()->update(); event->accept(); }
    void dropEvent(QDropEvent *event) override {
        const int row = rowAt(event->position().toPoint().y());
        const auto payload = event->mimeData()->data(poolMime).split(':');
        if (valid(event->mimeData()) && row >= 0 && payload.size() == 2) {
            bool poolOk, rowOk;
            const int pool = payload[0].toInt(&poolOk), from = payload[1].toInt(&rowOk);
            if (poolOk && rowOk && assign) { assign(from, row, pool); event->acceptProposedAction(); }
        }
        target_ = -1; viewport()->update();
    }
    void paintEvent(QPaintEvent *event) override {
        QTableWidget::paintEvent(event);
        if (target_ >= 0) {
            QPainter painter(viewport()); painter.setPen(QPen(QColor("#73c9ec"), 3));
            painter.setBrush(QColor(80, 180, 210, 65));
            painter.drawRoundedRect(visualRect(model()->index(target_, 2)).adjusted(2, 2, -2, -2), 4, 4);
        }
    }
  private:
    int target_ = -1;
};
} // namespace lordly
