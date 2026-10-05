#pragma once
#include "engine/scene_capture.h"
#include <optional>
namespace lordly {
struct SceneIrisLayer {
    quint32 resource = 0; // Opaque host resource identity, not a RAM pointer.
    QByteArray record;
};
class SceneIris {
  public:
    static constexpr quint32 CapturedFrame = 0xfffffffeU;
    using Layers = std::array<SceneIrisLayer, 20>;
    SceneIris(int count, int selected, Layers);
    void start(const QVector<quint32> &, const QImage &, QVector<quint8> &actors,
               QVector<quint8> &sprites);
    // True requests release of all current Actors/sprites at close completion.
    bool advance();
    bool active() const;
    int selected() const {
        return selected_;
    }
    int count() const {
        return count_;
    }
    const QByteArray &record() const {
        return record_;
    }
    const QImage &frame() const {
        return frame_;
    }
    Layers &layers() {
        return layers_;
    }
    const Layers &layers() const {
        return layers_;
    }
    static QByteArray mesh(const Rom &, const QByteArray &record);

  private:
    int count_, selected_, savedIndex_ = -1;
    Layers layers_;
    std::optional<Layers> saved_;
    QByteArray record_;
    QImage frame_;
};
} // namespace lordly
