#include "engine/pose_runtime.h"
namespace lordly {
int PoseRuntimeImage::count(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    const auto pose = rom.bytes().mid(poseOffset, poseSize),
               reader = rom.bytes().mid(readerOffset, readerSize),
               widths = rom.bytes().mid(widthsOffset, widthsSize);
    if (widths.size() != widthsSize ||
        widths.left(22) != QByteArray::fromHex("01030302020201010101010104030302030303030304"))
        return -1;
    if (header == QByteArray(descriptorSize, char(-1)))
        return sha256(pose) == "c6d8a4a4028fdbc57a8a89823e33705b1382a5ca28d00f6b3705e461c975940c" &&
                       sha256(reader) ==
                           "34b6830bc258827623bd48614d41685a76486a9fd8b50de76f52596119279d8c" &&
                       !u8(widths, 22)
                   ? 0
                   : -1;
    if (header.size() != descriptorSize || !header.startsWith("LC05POSE") || u32(header, 8) < 1 ||
        u32(header, 8) > 2 || u32(header, 12) > 256 || u32(header, 16) > 1 ||
        u32(header, 20) < readerAddress || u32(header, 20) >= readerAddress + readerSize ||
        u32(header, 20) % 4 || u32(header, 24) > (u32(header, 8) == 2 ? 1U : 0U) ||
        u32(header, 28) || u8(widths, 22) != (u32(header, 16) ? 3 : 0) ||
        header.mid(64, 32) != QByteArray::fromHex(sha256(pose)) ||
        header.mid(96, 32) != QByteArray::fromHex(sha256(reader)) ||
        header.mid(128, 32) != QByteArray::fromHex(sha256(widths)))
        return -1;
    return int(u32(header, 12));
}
void PoseRuntimeImage::validate(const Rom &rom, const QByteArray &mappings, bool wideRequired,
                                bool bodyPolicies) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    // Paired mappings alone remain compatible with earlier LC05BSEL exports.
    bool fixed = false;
    for (int at = 0; at < mappings.size(); at += 8) {
        const auto source = u8(mappings, at);
        fixed |=
            source == 135 || source == 136 || source == 161 || qint16(u16(mappings, at + 4)) == -1;
    }
    if (!fixed && !wideRequired && !bodyPolicies && !header.startsWith("LC05POSE"))
        return;
    if (count(rom) < 0 || u32(header, 12) * 8 != mappings.size() ||
        u32(header, 24) != quint32(bodyPolicies) ||
        header.mid(32, 32) != QByteArray::fromHex(sha256(mappings)) ||
        (wideRequired && !u32(header, 16)))
        throw Error("Private pose programs need matching source-built execution and width data");
}
} // namespace lordly
