#include "engine/body_visual_policy.h"
namespace lordly {
const std::array<BodyVisualPolicyImage::Block, 5> &BodyVisualPolicyImage::blocks() {
    static const std::array<Block, 5> value{{{0x2076a8, 132, "equipment-register"},
                                             {0x205cbc, 124, "equipment-allocate"},
                                             {0x206370, 144, "equipment-image"},
                                             {0x206470, 20, "equipment-image-context"},
                                             {0x2064ec, 36, "equipment-image-kind"}}};
    return value;
}
QByteArray BodyVisualPolicyImage::code(const Rom &rom) {
    QByteArray bytes;
    for (const auto &block : blocks())
        bytes += rom.bytes().mid(block.offset, block.size);
    return bytes;
}
int BodyVisualPolicyImage::count(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (header == QByteArray(descriptorSize, char(-1)))
        return sha256(code(rom)) ==
                       "94997e3973da0468090be53d86708ae4f2dde29183c055ab052f7f3a8071867e"
                   ? 0
                   : -1;
    if (header.size() != descriptorSize || !header.startsWith("LC05VPOL") || u32(header, 8) != 1 ||
        u32(header, 12) > 256 || header.mid(80) != QByteArray(16, 0) ||
        header.mid(48, 32) != QByteArray::fromHex(sha256(code(rom))))
        return -1;
    return int(u32(header, 12));
}
void BodyVisualPolicyImage::validate(const Rom &rom, const AnimationRoutes &routes) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!routes.hasVisualPolicies() && !header.startsWith("LC05VPOL"))
        return;
    if (!routes.hasVisualPolicies() || !header.startsWith("LC05VPOL") || count(rom) < 0 ||
        u32(header, 12) * 8 != routes.privatePoses().size() ||
        header.mid(16, 32) != QByteArray::fromHex(sha256(routes.visualPolicies())))
        throw Error("Class visual policy data and its source-built readers disagree");
}
} // namespace lordly
