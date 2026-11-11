#include "rebuild/pose_runtime.h"
#include "engine/body_visual_policy.h"
#include "engine/pose_runtime.h"
#include <QFile>
namespace lordly {
namespace {
constexpr int heightOffset = 0x20d590, heightSize = 196;
constexpr auto heightOriginal = "38270a9b760cbe658a0af98d94d8dff3fec5fb70baf3260f766f696defd91baa";
constexpr auto heightOriginalTail = "675a225a66eee4a191a31886c68d3166a97cb3dd7e42f62dd3107f4499449816";
QByteArray source(const QString &name, const QString &extension = ".c") {
    QFile file(":/game/art/" + name + extension);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing pose execution source");
    return file.readAll();
}
QByteArray fit(const CompiledModule &module, const char *symbol, int size) {
    const auto entry = module.entries.value(symbol, 0), padding = entry - module.address;
    if (entry < module.address || padding > 12 || padding % 4 ||
        module.bytes.left(padding) != QByteArray(padding, 0))
        throw Error("Pose function entry moved beyond its leading NOP alignment");
    return module.fitOwner(size);
}
} // namespace
quint32 poseHeightRuntimeAddress(const Rom &rom, const std::optional<ClassPlacement> &placement) {
    if (!placement) return 0;
    const auto function = rom.bytes().mid(heightOffset, heightSize);
    if (u32(function, 0) >> 26 != 2 || u32(function, 4) ||
        sha256(function.sliced(8)) != heightOriginalTail) return 0;
    const quint32 address = 0x80000000U | ((u32(function, 0) & 0x03ffffffU) << 2);
    const auto &layout = placement->layout;
    if (address % 16 || address < layout.discovery + 32 + poseHeightHeaderSize || address >= layout.end)
        return 0;
    const auto at = placement->dataRomOffset + address - layout.indexBase - poseHeightHeaderSize;
    const auto header = rom.bytes().mid(at, poseHeightHeaderSize);
    if (!header.startsWith("LCHGHT01") || u32(header, 8) != address || !u32(header, 12) ||
        u32(header, 12) % 16 || quint64(address) + u32(header, 12) > layout.end)
        return 0;
    return address;
}
QByteArray buildPoseHeightRuntime(quint32 address, const Compiler &compiler, const QString &directory) {
    const auto module = compiler.compileModule(source("body-height"), directory, "body_height", address, {},
                                               {{"lc_body_handle", 0x801be138},
                                                {"lc_pose_records", 0x801c1a50},
                                                {"lc_pose_widths", 0x801ceee0},
                                                {"lc_frame_bounds", 0x801c1ee8}});
    if (module.entries.value("lc_body_height") != address || module.bytes.isEmpty())
        throw Error("The compiled body-height function has an invalid entry");
    QByteArray header(poseHeightHeaderSize, 0);
    header.replace(0, 8, "LCHGHT01");
    put32(header, 8, address);
    const auto size = (module.bytes.size() + 15) & ~qsizetype(15);
    put32(header, 12, quint32(size));
    return header + module.fitOwner(size);
}
void installPoseHeightRuntime(QByteArray &image, const Rom &original, quint32 address,
                              const Compiler &compiler, const QString &directory) {
    const auto function = original.bytes().mid(heightOffset, heightSize);
    if (image.mid(heightOffset, heightSize) != function)
        throw Error("Another export owner changed the ROM body-height function");
    if (sha256(function) != heightOriginal) {
        const auto prior = ClassPlacement::read(original);
        const quint32 previous = poseHeightRuntimeAddress(original, prior);
        if (!previous) throw Error("The ROM body-height function has an unknown owner");
        const auto expected = buildPoseHeightRuntime(previous, compiler, directory);
        const auto at = prior->dataRomOffset + previous - prior->layout.indexBase - poseHeightHeaderSize;
        if (original.bytes().mid(at, expected.size()) != expected)
            throw Error("The prior ROM body-height helper differs from its compiled C owner");
    }
    if (address < 0x80000000U || address >= 0x80400000U || address % 16)
        throw Error("The compiled body-height function has an invalid game address");
    put32(image, heightOffset, 0x08000000U | ((address >> 2) & 0x03ffffffU));
    put32(image, heightOffset + 4, 0);
    const auto placed = Rom::fromBytes(image);
    if (poseHeightRuntimeAddress(placed, ClassPlacement::read(placed)) != address)
        throw Error("The body-height jump does not reach its linked class allocation");
}
QByteArray rebuildPoseRuntime(const Rom &target, const Rom &original, const QByteArray &mappings,
                              bool wide, bool bodyPolicies, const Compiler &compiler,
                              const QString &directory) {
    const auto header =
        original.bytes().mid(PoseRuntimeImage::descriptorOffset, PoseRuntimeImage::descriptorSize);
    bool fixed = false;
    for (int at = 0; at < mappings.size(); at += 8) {
        const auto source = u8(mappings, at);
        fixed |=
            source == 135 || source == 136 || source == 161 || qint16(u16(mappings, at + 4)) == -1;
    }
    if (!fixed && !wide && !bodyPolicies && !header.startsWith("LC05POSE"))
        return target.bytes();
    const int before = PoseRuntimeImage::count(original);
    if (before < 0)
        throw Error("The pose interpreter, reader or width table has an unknown owner");
    for (const auto &[offset, size] :
         {QPair<int, int>{PoseRuntimeImage::poseOffset, PoseRuntimeImage::poseSize},
          {PoseRuntimeImage::readerOffset, PoseRuntimeImage::readerSize},
          {PoseRuntimeImage::widthsOffset, PoseRuntimeImage::widthsSize},
          {PoseRuntimeImage::descriptorOffset, PoseRuntimeImage::descriptorSize}})
        if (target.bytes().mid(offset, size) != original.bytes().mid(offset, size))
            throw Error("Another export owner changed pose execution code or data");
    if (header.startsWith("LC05POSE") && (!bodyPolicies || u32(header, 8) == 3) && before * 8 == mappings.size() &&
        u32(header, 16) == quint32(wide) && u32(header, 24) == quint32(bodyPolicies) &&
        header.mid(32, 32) == QByteArray::fromHex(sha256(mappings)))
        return target.bytes();
    if (mappings.size() % 8 || mappings.size() > 2048)
        throw Error("Invalid private pose mapping extent");
    const auto defines = QString("#define LC_PRIVATE_POSE_COUNT %1\n#define LC_WIDE_STATE "
                                 "%2\n#define LC_BODY_POLICY %3\n")
                             .arg(mappings.size() / 8)
                             .arg(int(wide))
                             .arg(int(bodyPolicies))
                             .toLatin1();
    const auto reader = compiler.compileModule(defines + source("body-record"), directory,
                                               "body_record", PoseRuntimeImage::readerAddress,
                                               {{"pose_operands.h", source("pose_operands", ".h")}},
                                               {{"lc_body_handle", 0x801be138},
                                                {"lc_pose_program", 0x801c2ce4},
                                                {"lc_widths", 0x801ceee0},
                                                {"lc_class_handles", 0x801d0690}});
    const auto readerBytes = fit(reader, "lc_body_record", PoseRuntimeImage::readerSize);
    const auto query = reader.entries.value("lc_registered_pose", 0);
    if (!query)
        throw Error("Compiled pose registration query is missing");
    const auto pose =
        compiler.compileModule(defines + source("pose-advance"), directory, "pose_advance",
                               PoseRuntimeImage::poseAddress, {},
                               {{"lc_registered_pose", query},
                                {"lc_visual_policy", BodyVisualPolicyImage::queryAddress},
                                {"lc_plain_record", 0x80233968},
                                {"lc_body_record", PoseRuntimeImage::readerAddress},
                                {"lc_fill", 0x8009c970},
                                {"lc_sounds", 0x801cf3f0},
                                {"lc_sound_pairs", 0x801cef10},
                                {"lc_roster", 0x801ce8bc},
                                {"lc_sound_origin", 0x80249240},
                                {"lc_project", 0x8022d534},
                                {"lc_sound", 0x801c5938},
                                {"lc_linked_sound", 0x8022e72c}});
    const auto poseBytes = fit(pose, "lc_advance_pose", PoseRuntimeImage::poseSize);
    auto widths =
        original.bytes().mid(PoseRuntimeImage::widthsOffset, PoseRuntimeImage::widthsSize);
    put8(widths, 22, wide ? 3 : 0);
    const auto compiledWidths = compiler.compileData(widths, directory, "pose_widths");
    if (compiledWidths != widths)
        throw Error("Compiled pose width data differ");
    auto image = target.bytes();
    image.replace(PoseRuntimeImage::poseOffset, poseBytes.size(), poseBytes);
    image.replace(PoseRuntimeImage::readerOffset, readerBytes.size(), readerBytes);
    image.replace(PoseRuntimeImage::widthsOffset, widths.size(), compiledWidths);
    QByteArray next(PoseRuntimeImage::descriptorSize, 0);
    next.replace(0, 8, "LC05POSE");
    put32(next, 8, bodyPolicies ? 3 : 1);
    put32(next, 12, quint32(mappings.size() / 8));
    put32(next, 16, quint32(wide));
    put32(next, 20, query);
    put32(next, 24, quint32(bodyPolicies));
    next.replace(32, 32, QByteArray::fromHex(sha256(mappings)));
    next.replace(64, 32, QByteArray::fromHex(sha256(poseBytes)));
    next.replace(96, 32, QByteArray::fromHex(sha256(readerBytes)));
    next.replace(128, 32, QByteArray::fromHex(sha256(widths)));
    image.replace(PoseRuntimeImage::descriptorOffset, next.size(), next);
    PoseRuntimeImage::validate(Rom::fromBytes(image), mappings, wide, bodyPolicies);
    return image;
}
} // namespace lordly
