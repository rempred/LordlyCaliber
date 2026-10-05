#include "engine/body_animation_data.h"
namespace lordly {
BodyAnimationData::BodyAnimationData(ClassData classes, AnimationRoutes routes,
                                     std::optional<AnimationOverrides> overrides)
    : classes_(std::move(classes)), routes_(std::move(routes)) {
    if (!overrides)
        return;
    if (!overrides->available()) {
        overrides_ = overrides->image().rows();
        if (!overrides->image().recognized())
            overrideError_ = overrides->image().reason();
    } else {
        const auto bytes = overrides->table(classes_);
        for (int at = 0; at < overrides->entries().size() * 8; at += 8) {
            AnimationOverrideImage::Row row;
            for (int i = 0; i < 8; ++i)
                row[i] = u8(bytes, at + i);
            overrides_ << row;
        }
    }
}
BodyAnimationData::Pointer BodyAnimationData::load(const Rom &rom) {
    auto classes = ClassData::load(rom);
    auto routes = AnimationRoutes::load(rom);
    auto overrides = AnimationOverrides::load(rom, classes, routes);
    return std::make_shared<const BodyAnimationData>(std::move(classes), std::move(routes),
                                                     std::move(overrides));
}
quint16 BodyAnimationData::handle(int source, int owner, int flagA, int flagB) const {
    if (source < 0 || source >= classes_.entries().size() || owner < 0 || owner > 255 ||
        flagA < 0 || flagA > 1 || flagB < 0 || flagB > 1)
        throw Error("Invalid class body source, owner or flags");
    const int selected = u8(classes_.entries()[source].record, 69) == owner ? source : owner;
    return routes_.row(selected)[flagA * 2 + flagB];
}
int BodyAnimationData::selector(int source, int owner, int action, int mode, int bodyFlags) const {
    if (source < 0 || source >= classes_.entries().size() || owner < 0 || owner > 255 ||
        action < 0 || action > 255 || mode < 0 || mode > 2 || bodyFlags < 0 || bodyFlags > 3)
        throw Error("Invalid attack animation context");
    if (!overrideError_.isEmpty())
        throw Error(overrideError_);
    // Original ROM201798: source's group is eligible only when its full-byte
    // class-copy match equals owner and that source group exists.
    auto row = routes_.entry(source).selectors;
    if (u8(classes_.entries()[source].record, 69) != owner || row.isEmpty())
        row = routes_.entry(owner).selectors;
    if (row.isEmpty())
        throw Error("This owner has no action selector group");
    int original = 40;
    for (int at = 0; at + 1 < row.size(); at += 4)
        if (u8(row, at) == action) {
            original = u8(row, at + 1 + mode);
            break;
        }
    // OBSO uses first match in canonical order, with exact-body rows first.
    for (const auto &entry : overrides_)
        if (entry[0] == source && entry[1] == owner && entry[2] == action &&
            (!(entry[3] & 1) || entry[4] == original) && (!(entry[3] & 2) || entry[7] == bodyFlags))
            return entry[mode == 2 ? 6 : 5];
    return original;
}
} // namespace lordly
