#include "engine/scene_crop.h"
#include "game/director/screen_crop.h"
namespace lordly {
void SceneCrop::start(const QVector<quint32> &args) {
    if (args.size() != 6)
        throw Error("Screen crop requires six operands");
    QByteArray record(14, 0);
    lordly_crop_start(reinterpret_cast<uint8_t *>(record.data()), args.constData());
    record_ = std::move(record);
}
bool SceneCrop::active() const {
    return !record_.isEmpty() &&
           lordly_crop_active(reinterpret_cast<const uint8_t *>(record_.constData()));
}
void SceneCrop::draw() {
    if (record_.isEmpty() ||
        !lordly_crop_draw(reinterpret_cast<uint8_t *>(record_.data()), scissor_.data())) {
        record_.clear();
        scissor_ = {0xed000060U, 0x004fc35cU};
    }
}
QRect SceneCrop::clip() const {
    const int top = int(scissor_[0] & 0xfff) / 4, bottom = int(scissor_[1] & 0xfff) / 4;
    return QRect(0, top, 319, qMax(0, bottom - top)).intersected(QRect(0, 0, 320, 240));
}
} // namespace lordly
