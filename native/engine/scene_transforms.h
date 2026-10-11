#pragma once
#include "engine/rom.h"
#include "engine/scene_camera.h"
#include <QVector>
#include <array>
#include <optional>
namespace lordly {
struct SceneTransformAsset {
    quint32 key = 0;
    QByteArray decoded;
    int frameCount = 0;
    QVector<QVector<int>> groups;
    static int selectorCount(const Rom &);
    static SceneTransformAsset load(const Rom &, int selector);
    static SceneTransformAsset parse(QByteArray, quint32 key = 0);
};
class SceneTransforms {
  public:
    SceneTransforms();
    QByteArray channel(int index) const;
    void supplyChannel(int index, const QByteArray &);
    void start(const SceneTransformAsset &, int group, qint32 duration);
    void start(const Rom &, int selector, int group, qint32 duration);
    void advance();
    // Render preparation, separate from animation cadence. Count belongs to the
    // current background/layer owner. Channel uniform scale is applied there.
    void prepareMatrices(const Rom &, const SceneCamera &, int count);
    QByteArray layerMatrix(int index) const;
    bool done() const;
    quint32 remaining() const;
    int nextKeyframe() const;
    QByteArray deltas() const;

  private:
    struct Job {
        SceneTransformAsset asset;
        QVector<int> sequence;
        int next = 0;
        qint32 duration = 0;
        quint32 remaining = 0;
        float delta[20][6]{};
    };
    void prepare();
    uint8_t channels_[20][88]{};
    std::optional<Job> job_;
};
} // namespace lordly
