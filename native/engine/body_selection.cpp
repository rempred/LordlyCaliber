#include "engine/body_selection.h"
namespace lordly {
int BodySelectionImage::count(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    const auto code = rom.bytes().mid(offset, size);
    if (header == QByteArray(descriptorSize, char(-1)))
        return sha256(code) == "a3041f08dc09df1b4afc1a769aede3cd0d9e5644d56e903c99c06396813fb1d3"
                   ? 0
                   : -1;
    if (header.size() != descriptorSize || header.left(8) != "LC05BSEL" || u32(header, 8) != 1 ||
        u32(header, 12) > 256 || header.mid(80) != QByteArray(16, 0) ||
        header.mid(48, 32) != QByteArray::fromHex(sha256(code)))
        return -1;
    return int(u32(header, 12));
}
void BodySelectionImage::validate(const Rom &rom, const QByteArray &mappings) {
    const int rows = count(rom);
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (mappings.isEmpty() && !header.startsWith("LC05BSEL"))
        return;
    if (rows < 0 || rows * 8 != mappings.size() ||
        header.mid(16, 32) != QByteArray::fromHex(sha256(mappings)))
        throw Error("Private body selection code and its animation mappings disagree");
}
} // namespace lordly
