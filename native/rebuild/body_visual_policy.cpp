#include "rebuild/body_visual_policy.h"
#include "engine/body_visual_policy.h"
#include <QFile>
namespace lordly {
QByteArray rebuildBodyVisualPolicy(const Rom &target, const Rom &source,
                                   const AnimationRoutes &routes, const Compiler &compiler,
                                   const QString &directory) {
    if (!routes.hasVisualPolicies())
        return target.bytes();
    const auto header = source.bytes().mid(BodyVisualPolicyImage::descriptorOffset,
                                           BodyVisualPolicyImage::descriptorSize);
    if (BodyVisualPolicyImage::count(source) < 0)
        throw Error("Class equipment image code has an unrecognized owner");
    if (BodyVisualPolicyImage::code(target) != BodyVisualPolicyImage::code(source) ||
        target.bytes().mid(BodyVisualPolicyImage::descriptorOffset, header.size()) != header)
        throw Error("Another export owner changed class visual policy code");
    const auto policies = routes.visualPolicies();
    if (header.startsWith("LC05VPOL") && u32(header, 12) * 8 == routes.privatePoses().size() &&
        header.mid(16, 32) == QByteArray::fromHex(sha256(policies)))
        return target.bytes();
    auto image = target.bytes();
    for (const auto &block : BodyVisualPolicyImage::blocks()) {
        QFile file(":/game/art/" + QString::fromLatin1(block.source) + ".s");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing class equipment policy source");
        const auto code = compiler.assembleOwner(
            QString(".equ LC_POLICY_BACK,%1\n").arg(512 + routes.privatePoses().size()).toLatin1() +
                file.readAll(),
            directory, QString::fromLatin1(block.source),
            quint32(0x801bdb70 + block.offset - 0x201000),
            {{"lc_image_join", 0x801c2fc4}, {"lc_kind_join", 0x801c3080}}, true);
        if (code.size() != block.size)
            throw Error("Equipment policy reader exceeds its original allocation");
        image.replace(block.offset, block.size, code);
    }
    QByteArray next(BodyVisualPolicyImage::descriptorSize, 0);
    next.replace(0, 8, "LC05VPOL");
    put32(next, 8, 1);
    put32(next, 12, quint32(routes.privatePoses().size() / 8));
    next.replace(16, 32, QByteArray::fromHex(sha256(policies)));
    next.replace(48, 32,
                 QByteArray::fromHex(sha256(BodyVisualPolicyImage::code(Rom::fromBytes(image)))));
    image.replace(BodyVisualPolicyImage::descriptorOffset, next.size(), next);
    BodyVisualPolicyImage::validate(Rom::fromBytes(image), routes);
    return image;
}
} // namespace lordly
