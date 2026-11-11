#include "engine/body_markers.h"
#include "engine/body_visual_policy.h"
namespace lordly {
int BodyMarkerImage::mode(const Rom &rom) {
    for (const auto &variant : {"retail-wide", "retail-narrow", "policy-wide", "policy-narrow"})
        if (BodyVisualPolicyImage::matches(rom, BodyVisualPolicyImage::owner("00205C88", variant)))
            return QString::fromLatin1(variant).endsWith("wide") ? 1 : 0;
    const auto header = rom.bytes().mid(BodyVisualPolicyImage::descriptorOffset, BodyVisualPolicyImage::descriptorSize);
    const bool policies = header.startsWith("LC05VPOL") && u32(header, 8) <= 2;
    if (!BodyVisualPolicyImage::legacyOwner(rom, "00205C88", policies, false)) return -1;
    const auto bytes = rom.bytes().mid(offset, size);
    if (bytes.size() != size)
        return -1;
    const auto hash = sha256(bytes);
    if (hash == "8b5929f035c45d647fc0ee39e25de12e0d5ebed305cabd59de4b462dcdde96a9")
        return 0;
    if (hash == "bc1d7b9897def7ca4130ddfa21b5c4cc60612583404b148497523e27c09fb0d8")
        return 1;
    return -1;
}
} // namespace lordly
