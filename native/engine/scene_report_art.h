#pragma once
#include "engine/dialogue_text.h"
#include <QImage>
#include <QPainter>
#include <QVector>
#include <array>

namespace lordly {
class SceneReportArt {
  public:
    explicit SceneReportArt(const Rom &);
    void frame(QPainter &, QRect box, int preset, const QByteArray &record, bool front) const;
    void line(QPainter &, const QByteArray &, QPoint) const;
    void decorations(QPainter &, QRect box, int preset, const QByteArray &record,
                     const QByteArray &payload, bool selectedList, int pageFrame) const;

  private:
    void drawPaper(QPainter &, QRect, const QByteArray &record, bool lit) const;
    QImage paper_, finger_, more_, arrows_, scroll_, labels_;
    std::array<QImage, 3> help_;
    std::array<QImage, 11> trim_;
    QImage normal_, compact_, wide_;
    QVector<quint16> wideCodes_;
    DialogueFontMetrics metrics_;
};
} // namespace lordly
