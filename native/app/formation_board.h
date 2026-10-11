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
    QImage overviewImage();
    QImage overviewHitMap() const;
    QPolygonF overviewCellPolygon(int cell) const;
    void setPixelScale(int scale);
    void setExternalDragEnabled(bool enabled) { externalDragEnabled_ = enabled; }
    void setDropCell(int cell) { target_ = cell; update(); }
    QSize sizeHint() const override { return {104 * zoom_, 90 * zoom_}; }
  signals:
    void cellPressed(int cell);
    void cellDragStarted(int cell);
    void cellDraggedOutside(int cell);
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
    bool externalDragEnabled_ = false;
    bool overview_ = false;
    int zoom_ = 4;
};
}
