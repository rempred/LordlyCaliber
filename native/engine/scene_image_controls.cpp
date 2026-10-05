#include "engine/scene_image_controls.h"
namespace lordly {
QByteArray SceneImageControls::retained(int direction) const {
    if (direction < 0 || direction >= 4)
        throw Error("Image direction is outside 0..3");
    return arrows_[direction] ? arrows_[direction]->record() : QByteArray{};
}
void SceneImageControls::enable(quint32 bank, QByteArray &context, SceneSprites &sprites) {
    if (context.size() != 0x848)
        throw Error("Image controls require their launch context");
    if (u8(context, 0x83f) == 1)
        return;
    auto next = *this;
    auto pool = sprites;
    auto c = context;
    const int width = qint16(u16(c, 0x834)), height = qint16(u16(c, 0x836));
    static constexpr int x[]{0, -124, 124, 0}, y[]{-84, 0, 0, 84};
    for (int i = 0; i < 4; ++i) {
        const int slot = 29 - i;
        if ((i == 0 || i == 3) ? height >= 191 : width >= 281) {
            int ignored = 0;
            pool.service(
                0x46,
                {quint32(slot), bank, 700, quint32(i), quint32(x[i]), quint32(y[i]), 2, 100, 0},
                ignored);
        }
        if (const auto sprite = pool.sprite(slot))
            next.arrows_[i] = *sprite;
        else
            next.arrows_[i].reset();
        next.identities_[i] = pool.identity(slot);
        put32(c, 0xa8 + i * 4, next.arrows_[i] ? quint32(i + 1) : 0); // Host identity.
    }
    put8(c, 0x83f, 1);
    if (width < 281)
        put8(c, 0x83e, u8(c, 0x83e) & 0x42);
    if (height < 191)
        put8(c, 0x83e, u8(c, 0x83e) & 0x18);
    next.enabled_ = true;
    context = std::move(c);
    sprites = std::move(pool);
    *this = std::move(next);
}
void SceneImageControls::disable(QByteArray &context, SceneSprites &sprites) {
    if (context.size() != 0x848)
        throw Error("Image controls require their launch context");
    for (int i = 0; i < 4; ++i)
        sprites.supplyRecord(29 - i, {});
    put8(context, 0x83f, 0);
    enabled_ = false;
    arrows_ = {};
    identities_ = {};
    // The target leaves stale retained pointers. Keep their opaque identity
    // bytes, but release host ownership rather than retaining dangling objects.
}
void SceneImageControls::finishDraw(const QByteArray &context, SceneSprites &sprites) {
    if (context.size() != 0x848)
        throw Error("Image controls require their launch context");
    if (u8(context, 0x83f) != 1)
        return;
    auto next = *this;
    auto pool = sprites;
    std::optional<SpritePosePlayer> last;
    for (int i = 0; i < 4; ++i)
        if (const auto sprite = pool.sprite(29 - i)) {
            if (pool.identity(29 - i) == next.identities_[i])
                next.arrows_[i] = *sprite;
            last = *sprite;
        }
    // The complete114-byte constant block atROM2B8A98 uniquely matches saved
    // RAM80243C98. Its four indicator working flags start at1, then the
    // available-direction bits clear them to0 (attach); other flags detach.
    static constexpr int bits[]{2, 0x10, 8, 0x40};
    for (int i = 0; i < 4; ++i) {
        const int slot = 29 - i;
        if (u8(context, 0x83e) & bits[i]) {
            if (!pool.sprite(slot)) {
                if (!next.arrows_[i]) {
                    if (last)
                        throw Error("Image direction mask refers to an absent retained arrow");
                    continue;
                }
                if (last) {
                    next.arrows_[i]->sprite().cursor = last->sprite().cursor;
                    next.arrows_[i]->sprite().delay = last->sprite().delay;
                }
                pool.supplySprite(slot, *next.arrows_[i]);
                next.identities_[i] = pool.identity(slot);
            }
        } else
            pool.supplyRecord(slot, {});
    }
    *this = std::move(next);
    sprites = std::move(pool);
}
} // namespace lordly
