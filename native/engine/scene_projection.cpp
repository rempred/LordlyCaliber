#include "engine/scene_projection.h"
#include <bit>
#include <cstring>
namespace lordly {
SceneProjection::SceneProjection() {
    lordly_projection_initialize(&raw_);
}
SceneProjection::SceneProjection(const QByteArray &record) {
    if (record.size() != sizeof(raw_.bytes))
        throw Error("Projection record must contain 76 bytes");
    std::memcpy(raw_.bytes, record.constData(), sizeof(raw_.bytes));
}
QByteArray SceneProjection::record() const {
    return {reinterpret_cast<const char *>(raw_.bytes), sizeof(raw_.bytes)};
}
float SceneProjection::scaleX() const {
    return std::bit_cast<float>(u32(record(), 0x24));
}
float SceneProjection::scaleY() const {
    return std::bit_cast<float>(u32(record(), 0x28));
}
float SceneProjection::translateX() const {
    return std::bit_cast<float>(u32(record(), 0x30));
}
float SceneProjection::translateY() const {
    return std::bit_cast<float>(u32(record(), 0x34));
}
int SceneProjection::remaining() const {
    return qint16(u16(record(), 0x48));
}
void SceneProjection::start(float x, float y, float sx, float sy, qint32 duration) {
    if (lordly_projection_start(&raw_, x, y, sx, sy, duration))
        throw Error("Projection change requires a nonzero duration and finite coordinates");
}
void SceneProjection::advance() {
    if (lordly_projection_advance(&raw_))
        throw Error("Projection interpolation produced nonfinite coordinates");
}
bool SceneProjection::service(int mode, quint32 opcode, const QVector<quint32> &args) {
    if (opcode == 0x3d) {
        if (args.size() != 1) throw Error("Projection reset is truncated");
        if (lordly_projection_reset(&raw_, qint32(args[0])))
            throw Error("Projection reset requires a nonzero duration and finite coordinates");
        return true;
    }
    if (opcode != 0x2c)
        return false;
    if (args.size() != 5)
        throw Error("Projection command is truncated");
    if (mode == 2)
        start(float(qint32(args[0])) / 1000.f, float(qint32(args[1])) / 1000.f,
              float(qint32(args[2])) / 1000.f, float(qint32(args[3])) / 1000.f, qint32(args[4]));
    return true;
}
} // namespace lordly
