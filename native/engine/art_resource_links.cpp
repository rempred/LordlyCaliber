#include "engine/art_resource_links.h"
#include "engine/animation_routes.h"
#include "engine/army_artwork.h"
#include "engine/avatar_library.h"
#include "engine/icon_artwork.h"
namespace lordly {
QByteArray canonicalArtOwner(QByteArray code, qsizetype start) {
    code = AnimationRoutes::canonicalOwner(std::move(code), start);
    return ArmyArtwork::canonicalOwner(
        AvatarLibrary::canonicalOwner(IconArtwork::canonicalOwner(std::move(code), start), start),
        start);
}
void validateArtResourceLinks(const Rom &rom) {
    IconArtwork::validateRelocations(rom);
    AvatarLibrary::validateRelocations(rom);
    ArmyArtwork::validateRelocations(rom);
    AnimationRoutes::validateRelocations(rom);
}
void validateArtOwnerLinks(const Rom &rom, qsizetype start, qsizetype size) {
    auto contained = [&](QPair<int, int> site) {
        return site.first >= start && site.second >= start && site.first + 4 <= start + size &&
               site.second + 4 <= start + size;
    };
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
