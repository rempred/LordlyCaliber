#include "rebuild/body_visual_policy.h"
#include "engine/body_visual_policy.h"
#include "engine/body_markers.h"
#include "engine/direct_descriptors.h"
#include <QJsonArray>
namespace lordly {
QByteArray rebuildBodyVisualPolicy(const Rom &target, const Rom &source,
                                   const AnimationRoutes &routes, const Compiler &compiler,
                                   const QString &directory) {
    using Policy = BodyVisualPolicyImage;
    if (!routes.hasVisualPolicies()) return target.bytes();
    const auto header = source.bytes().mid(Policy::descriptorOffset, Policy::descriptorSize);
    const bool bodies = routes.hasBodyPolicies();
    if (Policy::count(source) < 0) throw Error("Class equipment image code has an unrecognized owner");
    const bool oldBodies = header.startsWith("LC05VPOL") &&
        (u32(header, 8) == 2 || (u32(header, 8) == 3 && u32(header, 80)));
    if (bodies && !oldBodies)
        for (const auto &id : {"002AB810", "002AAC60", "002ABB3C"})
            if (!Policy::legacyOwner(source, id, false, false))
                throw Error("Class construction code has an unrecognized owner");
    const int mode = BodyMarkerImage::mode(target);
    if (mode < 0) throw Error("Equipment allocation and marker code disagree");
    if (target.bytes().mid(Policy::descriptorOffset, header.size()) != header)
        throw Error("Another export owner changed the class visual descriptor");
    const auto directoryKey = DirectDescriptors::resourceKey(target);
    auto image = target.bytes();
    for (const auto &original : Policy::owners(bodies)) {
        const auto id = original["id"].toString();
        const int start = original["start"].toInt(), size = original["size"].toInt();
        // The preceding marker pass owns the same complete allocation function.
        // Every other owner must still equal the source, including its tables.
        if (id == "00205C88") {
            const bool wasPolicy = header.startsWith("LC05VPOL");
            const auto staged = wasPolicy ? Policy::owner(id, mode ? "policy-wide" : "policy-narrow") :
                Policy::owner(id, mode ? "retail-wide" : "retail-narrow");
            if (target.bytes().mid(start, size) != source.bytes().mid(start, size) && !Policy::matches(target, staged))
                throw Error("Another export owner changed equipment allocation");
        } else {
            if (target.bytes().mid(start, size) != source.bytes().mid(start, size))
                throw Error("Another export owner changed class visual code");
            for (const auto &v : original["tables"].toArray()) {
                const auto t = v.toObject();
                if (target.bytes().mid(t["start"].toInt(), t["size"].toInt()) !=
                    source.bytes().mid(t["start"].toInt(), t["size"].toInt()))
                    throw Error("Another export owner changed a class visual table");
            }
        }
        const auto wanted = Policy::owner(id, id == "00205C88" ?
            (mode ? "policy-wide" : "policy-narrow") : "policy");
        const auto built = compiler.compileQualifiedOwner(wanted, directory, "body_" + id);
        image.replace(start, built.code.size(), built.code);
        for (const auto &v : original["tables"].toArray()) {
            const auto t = v.toObject();
            const int at = t["start"].toInt(), n = t["size"].toInt();
            image.replace(at, n, built.tables.value(at, Policy::retailBytes(at, n)));
        }
    }
    put32(image, Policy::directoryOffset, directoryKey);
    const auto query = Policy::catalog()["query"].toObject();
    const auto helper = compiler.compileQualifiedOwner(query, directory, "body_policy_query");
    image.replace(query["start"].toInt(), helper.code.size(), helper.code);
    put16(image, Policy::settingsOffset, quint16(512 + routes.privatePoses().size()));
    put16(image, Policy::settingsOffset + 2, 0);
    QByteArray next(Policy::descriptorSize, 0);
    next.replace(0, 8, "LC05VPOL");
    put32(next, 8, 3);
    put32(next, 12, quint32(routes.privatePoses().size() / 8));
    put32(next, 80, bodies ? 1 : 0);
    next.replace(16, 32, QByteArray::fromHex(sha256(routes.visualPolicies())));
    next.replace(48, 32, QByteArray::fromHex(sha256(Policy::ownedCode(Rom::fromBytes(image), bodies))));
    image.replace(Policy::descriptorOffset, next.size(), next);
    Policy::validate(Rom::fromBytes(image), routes);
    return image;
}
} // namespace lordly
