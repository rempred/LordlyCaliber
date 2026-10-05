#include "rebuild/body_selection.h"
#include "engine/body_selection.h"
#include <QFile>
namespace lordly {
QByteArray rebuildBodySelection(const Rom &target, const Rom &source, const QByteArray &mappings,
                                const Compiler &compiler, const QString &directory) {
    const int before = BodySelectionImage::count(source);
    const auto header = source.bytes().mid(BodySelectionImage::descriptorOffset,
                                           BodySelectionImage::descriptorSize);
    if (mappings.isEmpty() && !header.startsWith("LC05BSEL"))
        return target.bytes();
    if (before < 0)
        throw Error("The body selection code has an unrecognized owner");
    if (target.bytes().mid(BodySelectionImage::offset, BodySelectionImage::size) !=
            source.bytes().mid(BodySelectionImage::offset, BodySelectionImage::size) ||
        target.bytes().mid(BodySelectionImage::descriptorOffset,
                           BodySelectionImage::descriptorSize) != header)
        throw Error("Another export owner changed body selection code");
    if (before * 8 == mappings.size() && header.startsWith("LC05BSEL") &&
        header.mid(16, 32) == QByteArray::fromHex(sha256(mappings)))
        return target.bytes();
    if (mappings.size() % 8 || mappings.size() > 256 * 8)
        throw Error("Private body selection mapping exceeds its code bounds");
    QFile file(":/game/art/body-selection.c");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing body selection source");
    const auto module = compiler.compileModule(
        QString("#define LC_PRIVATE_POSE_COUNT %1\n").arg(mappings.size() / 8).toLatin1() +
            file.readAll(),
        directory, "body_selection", BodySelectionImage::address, {},
        {{"lc_class_handles", 0x801d0690},
         {"lc_scene", 0x8022a974},
         {"lc_roster", 0x801ce8bc},
         {"lc_presentation", 0x801ce8fc},
         {"lc_appearance0", 0x8016df88},
         {"lc_appearance1", 0x8016dfdc},
         {"lc_appearance2", 0x8016e030},
         {"lc_appearance3", 0x8016e084},
         {"lc_equipment", 0x8016fa34},
         {"lc_fill", 0x8009c970},
         {"lc_zero", 0x80093380},
         {"lc_prepare", 0x801c41c8},
         {"lc_pose", 0x8022e9e8}});
    const auto entry = module.entries.value("lc_body_select", 0);
    const auto padding = entry - BodySelectionImage::address;
    if (entry < BodySelectionImage::address || padding > 12 || padding % 4 ||
        module.bytes.left(padding) != QByteArray(padding, 0))
        throw Error("Body selection entry point moved beyond its leading NOP alignment");
    const auto code = module.fitOwner(BodySelectionImage::size);
    auto image = target.bytes();
    image.replace(BodySelectionImage::offset, BodySelectionImage::size, code);
    QByteArray next(BodySelectionImage::descriptorSize, 0);
    next.replace(0, 8, "LC05BSEL");
    put32(next, 8, 1);
    put32(next, 12, quint32(mappings.size() / 8));
    next.replace(16, 32, QByteArray::fromHex(sha256(mappings)));
    next.replace(48, 32, QByteArray::fromHex(sha256(code)));
    image.replace(BodySelectionImage::descriptorOffset, next.size(), next);
    BodySelectionImage::validate(Rom::fromBytes(image), mappings);
    return image;
}
} // namespace lordly
