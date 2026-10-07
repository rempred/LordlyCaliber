#pragma once
#include <QImage>
#include <QPolygonF>
#include <QPushButton>
#include <QWidget>
#include <array>
namespace lordly {
class Project;
// One ROM-backed Army board, shared by the three squad editing workflows.
class FormationBoard final : public QWidget {
    Q_OBJECT
  public:
    explicit FormationBoard(Project *, const QString &cellPrefix, bool playerFormation = false,
                            QWidget *parent = nullptr, int firstCellNumber = 0);
    QPushButton *cell(int id) const { return cells_.at(id); }
    QPolygonF cellPolygon(int id) const;
    QPoint cellCenter(int id) const;
    int cellAt(QPoint position) const;
    void clearUnits();
    void setUnit(int cell, int classId, const QString &label, bool selected = false,
                 bool conflict = false);
    void setCellHint(int cell, const QString &label);
    void finishUpdate();
    QSize sizeHint() const override { return {416, 360}; }
  signals:
    void cellPressed(int cell);
    void cellDragStarted(int cell);
    void cellMoved(int from, int to);
  protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool event(QEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
  private:
    QPointF foot(int cell) const;
    QPointF origin() const;
    QRectF spriteRect(int cell) const;
    int memberAt(QPoint) const;
    bool pointerEvent(QEvent *, QWidget *source);
    void cancelDrag();
    void arrangeCells();
    Project *project_;
    bool playerFormation_;
    QImage board_;
    QString artworkError_;
    std::array<QPushButton *, 9> cells_{};
    std::array<QImage, 9> sprites_{};
    std::array<bool, 9> occupied_{}, conflicts_{};
    std::array<QString, 9> hints_{};
    int pressed_ = -1, target_ = -1;
    QPoint pressPosition_, pointerPosition_;
    bool dragging_ = false;
};
}
