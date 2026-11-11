#include "rebuild/body_markers.h"
#include "engine/body_markers.h"
#include "engine/body_visual_policy.h"
#include "engine/direct_descriptors.h"
#include <QJsonArray>
namespace lordly {
QByteArray rebuildBodyMarkers(const Rom &target, const Rom &source, bool wide,
                              const Compiler &compiler, const QString &directory) {
    using Policy = BodyVisualPolicyImage;
    const int before = BodyMarkerImage::mode(source);
    if (before < 0) {
        if (!wide) return target.bytes();
        throw Error("The equipment marker code is not a recognized source-built owner");
    }
    const auto original = Policy::owner("00205C88");
    const int start = original["start"].toInt(), size = original["size"].toInt();
    if (target.bytes().mid(start, size) != source.bytes().mid(start, size))
        throw Error("Another export owner changed the equipment marker owner");
    for (const auto &v : original["tables"].toArray()) {
        const auto t = v.toObject();
        if (target.bytes().mid(t["start"].toInt(), t["size"].toInt()) !=
            source.bytes().mid(t["start"].toInt(), t["size"].toInt()))
            throw Error("Another export owner changed the equipment marker table");
    }
    const bool policies = source.bytes().mid(Policy::descriptorOffset, 8) == "LC05VPOL";
    if (policies && Policy::count(source) < 0) throw Error("Invalid equipment policy owner");
    const auto wanted = policies ? Policy::owner("00205C88", wide ? "policy-wide" : "policy-narrow") :
                                  Policy::owner("00205C88", wide ? "retail-wide" : "retail-narrow");
    auto image = target.bytes();
    const auto built = compiler.compileQualifiedOwner(wanted, directory, "body_markers");
    image.replace(start, built.code.size(), built.code);
    put32(image, Policy::directoryOffset, DirectDescriptors::resourceKey(source));
    // Switch tables no longer used by policy variants are restored, not orphaned.
    for (const auto &v : original["tables"].toArray()) {
        const auto t = v.toObject();
        const int at = t["start"].toInt(), n = t["size"].toInt();
        image.replace(at, n, built.tables.value(at, Policy::retailBytes(at, n)));
    }
    if (BodyMarkerImage::mode(Rom::fromBytes(image)) != int(wide))
        throw Error("Compiled equipment marker owner differs from its checked C");
    return image;
}
} // namespace lordly
