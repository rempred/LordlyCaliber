#include "engine/stage_perspective.h"
namespace lordly {
StagePerspective StagePerspective::load(const Rom &rom, const StagePlacements &placements,
                                        const std::array<quint8, 3> &rgb,
                                        const std::function<quint32()> &random) {
    StagePerspective result;
    for (const auto &source : placements.perspective()) {
        const auto row = source.resolve();
        const int count = u16(row, 22);
        if (count > 4096 - result.props_.size())
            throw Error("Stage object pool exceeds its record budget");
        for (int i = 0; i < count; ++i) {
            const auto handle = u16(row, 0), variant = u16(row, 2);
            quint16 state = u16(row, 4);
            QByteArray owner(248, 0), drawing(192, 0);
            put32(owner, 0x48, 0x100);
            put32(owner, 0x4c, variant);
            put32(owner, 0x50, handle);
            std::array<int, 3> xyz;
            auto choose = [&](int low, int high) {
                if (!random || high <= low)
                    throw Error(
                        "Scattered Stage object needs valid ranges and explicit randomness");
                // The game PRNG returns a nonnegative signed word. Reject an
                // incompatible source instead of silently changing signed DIV.
                const auto value = random();
                if (value > 0x7fffffffU)
                    throw Error("Stage random value exceeds the native signed range");
                return low + int(value % quint32(high - low));
            };
            if (source.randomized()) {
                for (int j = 0; j < 6; ++j)
                    put16(owner, 0x24 + j * 2, u16(row, 24 + j * 2));
                put16(owner, 0x36, state);
                put16(owner, 0x38, quint16(u16(row, 18) + 1));
                put16(owner, 0x3a, u16(row, 20));
                xyz[0] = choose(qint16(u16(owner, 0x24)), qint16(u16(owner, 0x2a)));
                xyz[2] = choose(qint16(u16(owner, 0x26)), qint16(u16(owner, 0x28)));
                xyz[1] = choose(qint16(u16(owner, 0x2e)), qint16(u16(owner, 0x2c)));
                state = quint16(choose(state, int(u16(row, 18)) + 1));
                put8(drawing, 0x14, 2);
                put32(drawing, 0x94, 0x10);
            } else {
                for (int j = 0; j < 3; ++j)
                    xyz[j] = qint16(u16(row, 8 + j * 2));
                put8(drawing, 0x14, u16(row, 6) & 1 ? 3 : 2);
                put32(drawing, 0x94, u16(row, 6));
            }
            for (int j = 0; j < 3; ++j) {
                put32(owner, 0x5c + j * 4, quint32(xyz[j]));
                put32(drawing, 4 + j * 4, quint32(xyz[j] * 30));
                put16(drawing, 0x1c + j * 2, quint16(xyz[j]));
                put16(drawing, 0x28 + j * 2, quint16(xyz[j]));
                put8(drawing, 0x38 + j, rgb[j]);
                put8(drawing, 0x3b + j, 255);
            }
            put32(drawing, 0x18, handle);
            put16(drawing, 0x34, 255);
            put8(owner, 0xab, 255);
            // Slots are host identities, never native pointers or ROM addresses.
            const auto identity = quint32(result.props_.size() + 1);
            put32(owner, 0, identity);
            put32(drawing, 0x40, identity);
            put32(drawing, 0x90, identity);
            if (u8(drawing, 0x14) == 3) {
                put16(drawing, 0x1c, u16(row, 14));
                put16(drawing, 0x1e, u16(row, 16));
                put16(drawing, 0x20, 0);
            }
            put32(drawing, 0x44, handle);
            DirectPosePlayer pose(rom, drawing.mid(0x44, 80));
            pose.initialize(state);
            result.props_ << StageWorldProp{owner, drawing, std::move(pose)};
        }
    }
    return result;
}
StagePerspective StagePerspective::fromRecords(const Rom &rom, const QVector<QByteArray> &owners,
                                               const QVector<QByteArray> &drawings) {
    if (owners.size() != drawings.size() || owners.size() > 4096)
        throw Error("Invalid Stage object pools");
    StagePerspective result;
    for (int i = 0; i < owners.size(); ++i) {
        if (owners[i].size() != 248 || drawings[i].size() != 192 || u32(owners[i], 0x48) != 0x100)
            throw Error("Invalid direct Stage object record");
        result.props_ << StageWorldProp{owners[i], drawings[i],
                                        DirectPosePlayer(rom, drawings[i].mid(0x44, 80))};
    }
    return result;
}
void StagePerspective::advance() {
    auto next = props_;
    for (auto &prop : next) {
        const auto priorState = u16(prop.pose.record(), 4);
        if (prop.pose.advance(10000) != LORDLY_POSE_OK)
            throw Error(prop.pose.error());
        const auto &pose = prop.pose.record();
        const int height = qint16(u16(prop.drawing, 0x1e));
        const auto y = qint16(u16(pose, 14));
        if (!(u32(prop.drawing, 0x94) & 0x10) || height + y >= 0)
            continue;
        const auto state = qint16(u16(pose, 4));
        if (state != priorState)
            prop.pose.setOffsets(0, qint16(u16(prop.owner, 0x2c) - height), 0);
        else if (state < u16(prop.owner, 0x38)) {
            const auto x = qint16(u16(pose, 12)), z = qint16(u16(pose, 16));
            prop.pose.initialize(quint32(state + u16(prop.owner, 0x3a)));
            prop.pose.setOffsets(x, y, z);
        }
    }
    props_ = std::move(next);
}
QVector<QByteArray> StagePerspective::records() const {
    QVector<QByteArray> result;
    for (const auto &prop : props_)
        result << prop.record();
    return result;
}
} // namespace lordly
