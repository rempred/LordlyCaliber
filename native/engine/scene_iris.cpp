#include "engine/scene_iris.h"
#include "game/director/iris.h"
#include <bit>
namespace lordly {
namespace {
uint8_t *raw(QByteArray &b) {
    return reinterpret_cast<uint8_t *>(b.data());
}
const uint8_t *raw(const QByteArray &b) {
    return reinterpret_cast<const uint8_t *>(b.constData());
}
void identity(SceneIrisLayer &layer) {
    for (int at = 64; at < 84; at += 4)
        put32(layer.record, at, 0);
    put32(layer.record, 84, 0x3f800000);
}
} // namespace
SceneIris::SceneIris(int count, int selected, Layers layers)
    : count_(count), selected_(selected), layers_(std::move(layers)) {
    if (count < 2 || count > 20 || selected < 0 || selected >= count)
        throw Error("Iris requires valid active and selected layers");
    for (const auto &layer : layers_)
        if (layer.record.size() != 88)
            throw Error("Iris requires twenty complete channel records");
}
void SceneIris::start(const QVector<quint32> &args, const QImage &frame, QVector<quint8> &actors,
                      QVector<quint8> &sprites) {
    QByteArray record(24, 0);
    if (args.size() != 9 || !lordly_iris_create(raw(record), args.constData(), count_))
        throw Error("Iris requires bounded geometry, a positive duration and phase zero or one");
    const bool opening = args[7] != 0;
    if (!opening && (!record_.isEmpty() || frame.isNull()))
        throw Error("Closing iris needs a fresh transition and current framebuffer");
    if (opening &&
        (record_.isEmpty() || u8(record_, 20) != 0 || active() || !saved_ || frame_.isNull()))
        throw Error("Opening iris requires a completed close and retained layers");
    auto captured = opening ? QImage{} : SceneCapture::framebuffer(frame);
    auto next = layers_;
    if (!opening) {
        const auto chosen = next[selected_];
        for (int i = selected_; i < count_ - 1; ++i)
            next[i] = next[i + 1];
        next[count_ - 1] = chosen;
        for (auto &layer : actors)
            if (layer >= selected_)
                layer = quint8(layer - 1);
        for (auto &layer : sprites)
            if (layer > selected_)
                layer = quint8(layer - 1);
        savedIndex_ = selected_;
        selected_ = count_ - 1;
    } else
        next = *saved_;
    layers_ = std::move(next);
    record_ = std::move(record);
    frame_ = std::move(captured);
}
bool SceneIris::active() const {
    return !record_.isEmpty() && lordly_iris_query(raw(record_));
}
bool SceneIris::advance() {
    if (record_.isEmpty())
        return false;
    const int event = lordly_iris_advance(raw(record_));
    if (event == 1) {
        saved_ = layers_;
        layers_[0].resource = CapturedFrame;
        identity(layers_[0]);
        layers_[1] = layers_[selected_];
        for (int i = 2; i < 20; ++i) {
            layers_[i].resource = 0;
            identity(layers_[i]);
        }
        return true;
    }
    if (event == 2) {
        selected_ = savedIndex_;
        const auto last = layers_[count_ - 1];
        for (int i = count_ - 2; i >= selected_; --i)
            layers_[i + 1] = layers_[i];
        layers_[selected_] = last;
        record_.clear();
        saved_.reset();
        savedIndex_ = -1;
    }
    return false;
}
QByteArray SceneIris::mesh(const Rom &rom, const QByteArray &record) {
    if (record.size() != 24)
        throw Error("Iris mesh requires its complete transition record");
    const int p = qint32(u32(record, 0)), d = qint32(u32(record, 4));
    if (d < 1 || d > 30000 || p < 0 || p > d + 1)
        throw Error("Iris progress is outside its interpolation domain");
    double constants[3];
    for (int i = 0; i < 3; ++i)
        constants[i] = std::bit_cast<double>((quint64(u32(rom.bytes(), 0x2b8b10 + i * 8)) << 32) |
                                             u32(rom.bytes(), 0x2b8b14 + i * 8));
    int32_t terms[5]{};
    const int quad = lordly_iris_geometry(raw(record), terms);
    QByteArray vertices(1488, 0);
    const int count = lordly_iris_mesh(raw(vertices), terms, quad, constants);
    vertices.resize(count * 16);
    return vertices;
}
} // namespace lordly
