#pragma once
#include "engine/rom.h"
#include <QPointF>
#include <QSizeF>
#include <QVector>
#include <array>
namespace lordly {
struct SceneVector {
    double x = 0, y = 0, z = 0;
    SceneVector operator+(SceneVector other) const {
        return {x + other.x, y + other.y, z + other.z};
    }
    SceneVector operator-(SceneVector other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
    SceneVector operator*(double scale) const {
        return {x * scale, y * scale, z * scale};
    }
    SceneVector operator/(double scale) const {
        return {x / scale, y / scale, z / scale};
    }
    double length() const;
};
struct CameraPoint {
    QPointF screen;
    float depth = 0, pixelsPerUnit = 0;
    bool visible = false;
};
struct CameraPlane {
    // Row-vector 3x3 homography mapping local image pixels to screen pixels.
    std::array<double, 9> mapping{};
    QVector<QPointF> clip;
};
// Floating desktop projection. Native 16.16 object matrices are decoded when
// supplied; this does not emulate RSP clipping, rasterization or depth precision.
class SceneCamera {
  public:
    explicit SceneCamera(QByteArray bank, float modelScale);
    // Affine native clip matrix for the separate screen-space drawing pass.
    // This camera has no fourteen-float perspective bank and cannot accept 35/36.
    static SceneCamera orthographic(const QByteArray &packedProjection);
    const QByteArray &bank() const {
        return bank_;
    }
    float modelScale() const {
        return modelScale_;
    }
    void select(const QVector<quint32> &operands);
    CameraPoint project(SceneVector point) const;
    CameraPoint projectPackedOrigin(const QByteArray &matrix) const;
    CameraPoint projectPackedPoint(const QByteArray &matrix, SceneVector point) const;
    CameraPlane projectPackedPlane(const QByteArray &matrix, SceneVector origin, SceneVector across,
                                   SceneVector down, QSizeF size) const;
    SceneVector unprojectAtHeight(QPointF screen, double height) const;

  private:
    SceneCamera() = default;
    void prepare();
    CameraPoint projectWorld(SceneVector world) const;
    QByteArray bank_;
    float modelScale_ = 1;
    bool orthographic_ = false;
    std::array<float, 16> clipMatrix_{};
    SceneVector eye_, back_, right_, up_;
    double aspect_ = 0;
    float near_ = 0, far_ = 0, focal_ = 0;
};
class SceneCameras {
  public:
    // Fresh mode-two initializer copies seven current-ROM shared defaults;
    // aspect, clipping planes, up and homogeneous scale remain bank-owned.
    static SceneCameras freshModeTwo(const Rom &rom);
    SceneCameras(SceneCamera actor, SceneCamera registered, SceneCamera shared);
    const SceneCamera &actor() const {
        return actor_;
    }
    const SceneCamera &registered() const {
        return registered_;
    }
    const SceneCamera &shared() const {
        return shared_;
    }
    bool service(int mode, quint32 opcode, const QVector<quint32> &operands);

  private:
    SceneCamera actor_, registered_, shared_;
};
} // namespace lordly
