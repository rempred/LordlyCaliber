#pragma once
#include "engine/scene_camera.h"
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
    std::array<int16_t, 5> margins_;
    std::array<std::optional<SceneRibbon>, 20> ribbons_;
};
} // namespace lordly
