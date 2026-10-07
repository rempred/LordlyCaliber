#include "engine/art_resource_links.h"
#include "engine/direct_descriptors.h"
#include "engine/animation_routes.h"
#include "engine/animation_override_image.h"
#include "engine/army_artwork.h"
#include "engine/avatar_library.h"
#include "engine/icon_artwork.h"
namespace lordly {
QByteArray canonicalArtOwner(QByteArray code, qsizetype start) {
    code = DirectDescriptors::canonicalOwner(std::move(code),start);
    code = AnimationOverrideImage::canonicalOwner(std::move(code),start);
    code = AnimationRoutes::canonicalOwner(std::move(code), start);
    return ArmyArtwork::canonicalOwner(
        AvatarLibrary::canonicalOwner(IconArtwork::canonicalOwner(std::move(code), start), start),
        start);
}
QByteArray retainArtReferences(const Rom &rom, QByteArray code, qsizetype start) {
    validateArtOwnerLinks(rom, start, code.size());
    const auto current = rom.bytes().mid(start, code.size());
    const auto canonical = canonicalArtOwner(current, start);
    const auto compiled = canonicalArtOwner(code, start);
    for (qsizetype at = 0; at + 4 <= code.size(); at += 4)
        if (u32(current, at) != u32(canonical, at)) {
            if (u32(compiled, at) != u32(canonical, at))
                throw Error("Compiled owner changed an artwork reference instruction");
            put32(code, at, u32(current, at));
        }
    return code;
}
void validateArtResourceLinks(const Rom &rom) {
    AnimationOverrideImage::validateOwner(rom,0,rom.bytes().size());
    DirectDescriptors::validateRelocations(rom);
    IconArtwork::validateRelocations(rom);
    AvatarLibrary::validateRelocations(rom);
    ArmyArtwork::validateRelocations(rom);
    AnimationRoutes::validateRelocations(rom);
}
void validateArtOwnerLinks(const Rom &rom, qsizetype start, qsizetype size) {
    AnimationOverrideImage::validateOwner(rom,start,size);
    auto contained = [&](QPair<int, int> site) {
        return site.first >= start && site.second >= start && site.first + 4 <= start + size &&
               site.second + 4 <= start + size;
    };
    for (const auto &site : DirectDescriptors::sites())
        if (contained(site)) { DirectDescriptors::validateRelocations(rom); break; }
    bool animations = false;
    for (const auto &site : AnimationRoutes::sites())
        animations |= contained(site);
    if (animations)
        AnimationRoutes::validateRelocations(rom);
    bool icons = false;
    for (const auto &spec : IconArtwork::specs())
        for (const auto &site : spec.sites)
            icons |= contained(site);
    if (icons)
        IconArtwork::validateRelocations(rom);
    bool avatars = false;
    for (const auto &site : AvatarLibrary::sites())
        avatars |= contained(site);
    if (avatars)
        AvatarLibrary::validateRelocations(rom);
    bool army = false;
    for (const auto &spec : ArmyArtwork::specs())
        for (const auto &site : spec.sites)
            army |= contained(site);
    if (army)
        ArmyArtwork::validateRelocations(rom);
}
} // namespace lordly
