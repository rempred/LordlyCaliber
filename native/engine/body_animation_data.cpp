#include "engine/body_animation_data.h"
#include "engine/body_selection.h"
#include "engine/pose_runtime.h"
#include "engine/body_visual_policy.h"
#include "game/art/animation_readers.h"
namespace lordly {
BodyAnimationData::BodyAnimationData(ClassData classes, AnimationRoutes routes,
                                     std::optional<AnimationOverrides> overrides,
                                     QMap<int, AnimationDescriptor> privateDescriptors,
                                     std::optional<ItemData> items)
    : items_(std::move(items)), classes_(std::move(classes)), routes_(std::move(routes)),
      privateDescriptors_(std::move(privateDescriptors)) {
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
    BodyVisualPolicyImage::validate(rom, routes);
    BodySelectionImage::validate(rom, routes.privatePoses());
    PoseRuntimeImage::validate(rom, routes.privatePoses(), false, routes.hasBodyPolicies());
    auto overrides = AnimationOverrides::load(rom, classes, routes);
    return std::make_shared<const BodyAnimationData>(
        std::move(classes), std::move(routes), std::move(overrides),
        QMap<int, AnimationDescriptor>{}, ItemData::load(rom));
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
    const auto sourceRow = routes_.entry(source).selectors;
    const bool sourceMatches = u8(classes_.entries()[source].record, 69) == owner;
    // Preserve lazy owner lookup: a usable source group does not require an
    // owner group to exist or have a completed artwork assignment.
    const auto ownerRow = sourceMatches && !sourceRow.isEmpty()
                              ? QByteArray() : routes_.entry(owner).selectors;
    uint8_t original = 40;
    if (lordly_action_selector(sourceMatches,
            reinterpret_cast<const uint8_t *>(sourceRow.constData()), size_t(sourceRow.size()),
            reinterpret_cast<const uint8_t *>(ownerRow.constData()), size_t(ownerRow.size()),
            unsigned(action), unsigned(mode), &original))
        throw Error("This owner has no valid action selector group");
    // OBSO uses first match in canonical order, with exact-body rows first.
    for (const auto &entry : overrides_)
        if (entry[0] == source && entry[1] == owner && entry[2] == action &&
            (!(entry[3] & 1) || entry[4] == original) && (!(entry[3] & 2) || entry[7] == bodyFlags))
            return entry[mode == 2 ? 6 : 5];
    return original;
}
const AnimationDescriptor *BodyAnimationData::descriptor(int source,int owner,int flagA,int flagB)const {
    handle(source,owner,flagA,flagB);
    const int selected=u8(classes_.entries()[source].record,69)==owner?source:owner;
    const auto it=privateDescriptors_.constFind(selected*4+flagA*2+flagB);
    return it==privateDescriptors_.cend()?nullptr:&it.value();
}
} // namespace lordly
