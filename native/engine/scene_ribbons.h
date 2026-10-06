#pragma once
#include "engine/scene_camera.h"
#include "game/director/path_curve.h"
#include <QImage>
#include <QMap>
#include <optional>
namespace lordly {
struct SceneRibbon {
    quint32 key = 0;
    QVector<QPointF> points, samples;
    QByteArray record;
};
// ROM-sourced path geometry and reconstructed native reveal/lifetime state.
// The desktop strip painter approximates the game's texture/vertex effects.
class SceneRibbons {
  public:
    explicit SceneRibbons(const Rom &);
    SceneRibbon load(const QVector<quint32> &args, const QMap<int, QPointF> &markers = {}) const;
    void validateRotationSource(qint32 group, qint32 entry) const;
    int sourcePointCount(qint32 group, qint32 entry) const;
    qint16 rotation(qint32 group, qint32 entry, const QMap<int, QPointF> &markers = {}) const;
    bool service(quint32 opcode, const QVector<quint32> &args, int &result,
                 const QMap<int, QPointF> &markers = {});
    void advance();
    QImage render(const SceneCamera &, const QByteArray &imageContext) const;
    const std::array<std::optional<SceneRibbon>, 20> &ribbons() const {
        return ribbons_;
    }
    bool empty() const;

  private:
    Rom rom_;
    double timeScale_;
    LordlyPathAngleConstants angles_{};
    std::array<std::array<double, 2>, 2> headingConstants_{};
    qint16 heading(QPointF from, QPointF to, bool resourcePath) const;
    std::array<int16_t, 5> margins_;
    std::array<std::optional<SceneRibbon>, 20> ribbons_;
};
} // namespace lordly
