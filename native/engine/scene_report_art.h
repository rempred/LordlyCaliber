#pragma once
#include "engine/dialogue_text.h"
#include <QImage>
#include <QPainter>
#include <QVector>
#include <array>

namespace lordly {
class ScenePanelFont {
  public:
    explicit ScenePanelFont(const Rom &, int palette);
    void line(QPainter &, const QByteArray &, QPoint) const;
  private:
    QImage normal_, compact_, wide_;
    QVector<quint16> wideCodes_;
    DialogueFontMetrics metrics_;
};
class SceneReportArt {
  public:
    explicit SceneReportArt(const Rom &);
    void frame(QPainter &, QRect box, int preset, const QByteArray &record, bool front) const;
    void line(QPainter &p, const QByteArray &bytes, QPoint pos) const { font_.line(p, bytes, pos); }
    void decorations(QPainter &, QRect box, int preset, const QByteArray &record,
                     const QByteArray &payload, bool selectedList, int pageFrame) const;

  private:
    void drawPaper(QPainter &, QRect, const QByteArray &record, bool lit) const;
    QImage paper_, finger_, more_, arrows_, scroll_, labels_;
    std::array<QImage, 3> help_;
    std::array<QImage, 11> trim_;
    ScenePanelFont font_;
};
} // namespace lordly
