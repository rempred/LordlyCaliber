#pragma once
#include "engine/scene_camera.h"
#include <QImage>
namespace lordly {
struct SceneVignetteVertex {
    qint16 x, y;
    quint8 alpha;
};
// State owned by root+1CA4, separate from the persistent image-processing state.
class SceneCapture {
  public:
    void start(quint32 selector, quint32 duration, const QImage &frame = {});
    void advance();
    void clear();
    bool present() const {
        return !record_.isEmpty();
    }
    int selector() const {
        return present() ? u8(record_, 0) : -1;
    }
    const QByteArray &record() const {
        return record_;
    }
    void supplyRecord(QByteArray record);
    quint8 alpha() const;
    const QImage &frame() const {
        return frame_;
    }
    static QImage framebuffer(const QImage &);
    static std::array<SceneVignetteVertex, 10> vignetteVertices();
    static QImage vignette(const SceneCamera &, const QByteArray &parent);

  private:
    QByteArray record_;
    QImage frame_;
};
} // namespace lordly
