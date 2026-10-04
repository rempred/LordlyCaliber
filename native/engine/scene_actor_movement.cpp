#include "engine/scene_actors.h"
#include <cmath>
#include <limits>
namespace lordly {
namespace {
qint32 truncated(float value) {
    if (!std::isfinite(value) || double(value) < -2147483648.0 || double(value) >= 2147483648.0)
        throw Error("Actor movement coordinate exceeds native integer conversion");
    return qint32(value);
}
int separation(float a, float b) {
    const auto value = truncated(a - b);
    if (value == std::numeric_limits<qint32>::min())
        throw Error("Actor separation exceeds the supported finite range");
    return std::abs(value);
}
} // namespace
void SceneActors::supplyDepthAttachment(int slot, QByteArray record) {
    checkSlot(slot);
    if (!record.isEmpty() && record.size() != 4)
        throw Error("Actor depth attachment must contain four bytes");
    depths_[slot] = std::move(record);
}
const QByteArray &SceneActors::depthAttachment(int slot) const {
    checkSlot(slot);
    return depths_[slot];
}
// Portable reconstruction of ROM 2A0EF0..2A135C, spanning two ASM owners.
// Heights are read-only queries. Commit all three pools only after success.
void SceneActors::advanceMovement() {
    auto next = actors_;
    auto motions = motions_;
    auto depths = depths_;
    std::array<quint8, 28> flags{}, identities{};
    for (int slot = 0; slot < 28; ++slot)
        if (next[slot]) {
            const auto record = next[slot]->record();
            flags[slot] = u8(record, 0x145);
            identities[slot] = u8(record, 0x147);
        }
    auto ground = [&](int slot) {
        if (!height_)
            throw Error("Actor movement requires the scene terrain height");
        auto &position = next[slot]->pose().actor().position;
        const float x = mode_ == 2 ? float(qint16(truncated(position.x))) : position.x;
        const float z = mode_ == 2 ? float(qint16(truncated(position.z))) : position.z;
        const float height = height_(mode_, x, z);
        if (!std::isfinite(height))
            throw Error("Actor movement terrain height is nonfinite");
        position.y = height;
    };
    for (int slot = 0; slot < 28; ++slot) {
        auto &motion = motions[slot];
        if (!motion.present)
            continue;
        if (!next[slot] && (mode_ == 2 || !motion.pause))
            throw Error("Actor movement requires an occupied slot");
        if (mode_ == 2) {
            if (!sceneFlags_)
                throw Error("Actor movement requires the scene's current state flags");
            if (*sceneFlags_ & 8) {
                const auto &position = next[slot]->pose().actor().position;
                for (int other = 0; other < 28; ++other) {
                    if (!next[other] || other == slot || identities[slot] == identities[other] ||
                        !depths[other].isEmpty())
                        continue;
                    const auto &candidate = next[other]->pose().actor().position;
                    if (separation(candidate.x, position.x) < 20 &&
                        separation(candidate.z, position.z) < 20) {
                        depths[other] = QByteArray(4, 0);
                        put8(depths[other], 3, quint8(slot));
                        break;
                    }
                }
            }
        }
        if (!motion.pause) {
            auto &position = next[slot]->pose().actor().position;
            const float x = position.x + motion.vx, z = position.z + motion.vz;
            if (!std::isfinite(x) || !std::isfinite(z))
                throw Error("Actor movement produced a nonfinite coordinate");
            position.x = x;
            position.z = z;
            if ((mode_ == 0 || mode_ == 2) && (flags[slot] & 1))
                ground(slot);
            motion.remaining = quint16(motion.remaining - 1U);
        }
        if (!motion.remaining)
            motion.present = 0;
        // The second terrain read happens even for paused or just-freed motion.
        if (mode_ == 2 && (flags[slot] & 1))
            ground(slot);
    }
    for (int slot = 0; slot < 28; ++slot) {
        auto &depth = depths[slot];
        if (depth.isEmpty())
            continue;
        const int peer = u8(depth, 3);
        checkSlot(peer);
        if (!next[slot] || !next[peer]) {
            depth.clear();
            continue;
        }
        const auto &position = next[slot]->pose().actor().position;
        const auto &other = next[peer]->pose().actor().position;
        const int dx = separation(position.x, other.x), dz = separation(position.z, other.z);
        if (dx >= 20 || dz >= 20) {
            depth.clear();
            continue;
        }
        const float xx = float(dx) * float(dx), zz = float(dz) * float(dz);
        const int distance = qint16(qint32(std::sqrt(xx + zz)));
        int offset = (20 - distance) / 2;
        if (position.z - other.z < 0)
            offset = -offset;
        put16(depth, 0, quint16(offset));
    }
    actors_ = std::move(next);
    motions_ = motions;
    depths_ = std::move(depths);
}
} // namespace lordly
