#include "engine/scene_image_cache.h"
namespace lordly {
quint8 SceneImageCache::touch(qint32 source, quint8 variant, int art, bool sepia) {
    if (art < 0 || art >= 65536)
        throw Error("Actor image index exceeds its descriptor");
    const auto owner = key(source, variant);
    if (!owners_.contains(owner) && owners_.size() >= 40)
        throw Error("Scene Actor image cache exceeds forty source variants");
    auto &images = owners_[owner];
    if (!images.contains(art))
        images.insert(art, {quint8(sepia ? 1 : 0), 0});
    auto &entry = images[art];
    entry.age = 0;
    return entry.conversions;
}
void SceneImageCache::invalidate(qint32 source, quint8 variant) {
    owners_.remove(key(source, variant));
}
void SceneImageCache::convertAll() {
    for (auto &images : owners_)
        for (auto &entry : images)
            // Both native conversions are monotone and reach black from white
            // before64 applications. Further conversion cannot alter any RGB value.
            entry.conversions = qMin<quint8>(64, quint8(entry.conversions + 1));
}
void SceneImageCache::finishDraw() {
    // Original2A2AD0 increments loaded-image ages, frees at age3. Access resets0.
    for (auto owner = owners_.begin(); owner != owners_.end();) {
        auto &images = owner.value();
        for (auto image = images.begin(); image != images.end();)
            if (++image.value().age >= 3)
                image = images.erase(image);
            else
                ++image;
        if (images.isEmpty())
            owner = owners_.erase(owner);
        else
            ++owner;
    }
}
} // namespace lordly
