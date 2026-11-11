#include "engine/body_visual_policy.h"
#include "engine/body_markers.h"
#include "engine/direct_descriptors.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
const std::array<BodyVisualPolicyImage::Block, 5> &BodyVisualPolicyImage::blocks() {
    static const std::array<Block, 5> value{{{0x2076a8, 132, "equipment-register"},
                                             {0x205cbc, 124, "equipment-allocate"},
                                             {0x206370, 144, "equipment-image"},
                                             {0x206470, 20, "equipment-image-context"},
                                             {0x2064ec, 36, "equipment-image-kind"}}};
    return value;
}
const std::array<BodyVisualPolicyImage::Block, 4> &BodyVisualPolicyImage::constructionBlocks() {
    static const std::array<Block, 4> value{{{0x2ab884, 56, "body-construction"},
                                             {0x2abd68, 24, "body-roster-sticky"},
                                             {0x2aadc8, 40, "body-entrance-skip"},
                                             {0x2aafa4, 24, "body-entrance-sticky"}}};
    return value;
}
QByteArray BodyVisualPolicyImage::constructionCode(const Rom &rom) {
    QByteArray bytes;
    for (const auto &block : constructionBlocks())
        bytes += rom.bytes().mid(block.offset, block.size);
    return bytes;
}
QByteArray BodyVisualPolicyImage::code(const Rom &rom, bool construction) {
    QByteArray bytes;
    for (const auto &block : blocks())
        bytes += rom.bytes().mid(block.offset, block.size);
    return construction ? bytes + constructionCode(rom) : bytes;
}
QJsonObject BodyVisualPolicyImage::catalog() {
    static const auto value = [] {
        QFile f(":/game/art/body-policy-c.json");
        if (!f.open(QIODevice::ReadOnly)) throw Error("Missing body/equipment C catalog");
        return QJsonDocument::fromJson(f.readAll()).object();
    }();
    return value;
}
QJsonObject BodyVisualPolicyImage::owner(const QString &id, const QString &variant) {
    for (const auto &v : catalog()[variant.isEmpty() ? "originals" : "owners"].toArray()) {
        const auto o = v.toObject();
        if (o["id"].toString() == id && (variant.isEmpty() || o["variant"].toString() == variant))
            return o;
    }
    throw Error("Missing body/equipment C owner");
}
QVector<QJsonObject> BodyVisualPolicyImage::owners(bool construction) {
    QVector<QJsonObject> result;
    for (const auto &id : {"00205C88", "00206340", "00207658"}) result << owner(id);
    if (construction)
        for (const auto &id : {"002AB810", "002AAC60", "002ABB3C"}) result << owner(id);
    return result;
}
QByteArray BodyVisualPolicyImage::retailBytes(int offset, int size) {
    // This baseline is compiled and authenticated from the accepted C at build time.
    static const auto regions = [] {
        QFile f(":/generated/accepted-rom.bin");
        if (!f.open(QIODevice::ReadOnly)) throw Error("Missing accepted C baseline");
        const auto bytes = f.readAll();
        if (bytes.size() < 44 || !bytes.startsWith("LCCODE01")) throw Error("Invalid accepted C baseline");
        QMap<int, QByteArray> result;
        qsizetype at = 44;
        for (quint32 i = 0; i < u32(bytes, 8); ++i) {
            if (at + 16 > bytes.size()) throw Error("Truncated accepted C baseline");
            const int start = u32(bytes, at), size = u32(bytes, at + 8);
            at += 16;
            if (size <= 0 || at + size > bytes.size()) throw Error("Invalid accepted C allocation");
            result[start] = bytes.mid(at, size);
            at += size;
        }
        if (at != bytes.size()) throw Error("Unexpected accepted C baseline tail");
        return result;
    }();
    auto it = regions.upperBound(offset);
    if (it == regions.begin()) throw Error("Missing accepted C region");
    --it;
    if (offset + size > it.key() + it->size()) throw Error("Accepted C region exceeds owner");
    return it->mid(offset - it.key(), size);
}
bool BodyVisualPolicyImage::matches(const Rom &rom, const QJsonObject &o) {
    if (sha256(rom.bytes().mid(o["start"].toInt(), o["size"].toInt())) != o["sha256"].toString().toLatin1())
        return false;
    for (const auto &v : o["tables"].toArray()) {
        const auto t = v.toObject();
        if (sha256(rom.bytes().mid(t["start"].toInt(), t["size"].toInt())) != t["sha256"].toString().toLatin1())
            return false;
    }
    return true;
}
bool BodyVisualPolicyImage::usesCDirectory(const Rom &rom) {
    for (const auto &v : {"retail-narrow", "retail-wide", "policy-narrow", "policy-wide"})
        if (matches(rom, owner("00205C88", v))) return true;
    return false;
}
bool BodyVisualPolicyImage::legacyOwner(const Rom &rom, const QString &id, bool policies, bool construction) {
    const auto o = owner(id);
    const int start = o["start"].toInt(), size = o["size"].toInt();
    auto bytes = rom.bytes().mid(start, size);
    if (bytes.size() != size) return false;
    const auto restore = [&](int offset, int length) {
        if (offset >= start && offset + length <= start + size)
            bytes.replace(offset - start, length, retailBytes(offset, length));
    };
    if (policies) for (const auto &b : blocks()) restore(b.offset, b.size);
    if (construction) for (const auto &b : constructionBlocks()) restore(b.offset, b.size);
    if (id == "00205C88") {
        const auto h = sha256(rom.bytes().mid(BodyMarkerImage::offset, BodyMarkerImage::size));
        if (h != "8b5929f035c45d647fc0ee39e25de12e0d5ebed305cabd59de4b462dcdde96a9" &&
            h != "bc1d7b9897def7ca4130ddfa21b5c4cc60612583404b148497523e27c09fb0d8") return false;
        restore(BodyMarkerImage::offset, BodyMarkerImage::size);
    }
    bytes = DirectDescriptors::canonicalOwner(bytes, start);
    if (sha256(bytes) != o["sha256"].toString().toLatin1()) return false;
    for (const auto &v : o["tables"].toArray()) {
        const auto t = v.toObject();
        if (rom.bytes().mid(t["start"].toInt(), t["size"].toInt()) != retailBytes(t["start"].toInt(), t["size"].toInt()))
            return false;
    }
    return true;
}
QByteArray BodyVisualPolicyImage::ownedCode(const Rom &rom, bool construction) {
    QByteArray bytes;
    for (const auto &o : owners(construction)) {
        bytes += rom.bytes().mid(o["start"].toInt(), o["size"].toInt());
        for (const auto &v : o["tables"].toArray()) {
            const auto t = v.toObject();
            bytes += rom.bytes().mid(t["start"].toInt(), t["size"].toInt());
        }
    }
    return bytes;
}
int BodyVisualPolicyImage::count(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    const bool present = header.startsWith("LC05VPOL");
    if (!present && header != QByteArray(descriptorSize, char(-1))) return -1;
    if (present && (header.size() != descriptorSize || u32(header, 8) < 1 ||
                    u32(header, 8) > 3 || u32(header, 12) > 256)) return -1;
    const int version = present ? u32(header, 8) : 0;
    const bool bodies = version == 2 || (version == 3 && u32(header, 80));
    if (version == 3) {
        if (u32(header, 80) > 1 || header.mid(84) != QByteArray(12, 0)) return -1;
        for (const auto &o : owners(bodies)) {
            const auto id = o["id"].toString();
            const int mode = BodyMarkerImage::mode(rom);
            if (mode < 0 || !matches(rom, owner(id, id == "00205C88" ?
                    (mode ? "policy-wide" : "policy-narrow") : "policy"))) return -1;
        }
        if (!matches(rom, catalog()["query"].toObject()) ||
            u16(rom.bytes(), settingsOffset) != 512 + u32(header, 12) * 8 ||
            u16(rom.bytes(), settingsOffset + 2) ||
            header.mid(48, 32) != QByteArray::fromHex(sha256(ownedCode(rom, bodies)))) return -1;
    } else {
        if (present && (header.mid(80) != QByteArray(16, 0) ||
            header.mid(48, 32) != QByteArray::fromHex(sha256(code(rom, bodies))))) return -1;
        for (const auto &o : owners(bodies)) {
            const auto id = o["id"].toString();
            if (!present && id == "00205C88" && (matches(rom, owner(id, "retail-wide")) || matches(rom, owner(id, "retail-narrow")))) continue;
            if (!legacyOwner(rom, id, present, bodies)) return -1;
        }
    }
    return present ? int(u32(header, 12)) : 0;
}
void BodyVisualPolicyImage::validate(const Rom &rom, const AnimationRoutes &routes) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!routes.hasVisualPolicies() && !header.startsWith("LC05VPOL")) return;
    if (!routes.hasVisualPolicies() || !header.startsWith("LC05VPOL") || count(rom) < 0 ||
        (u32(header, 8) == 3 ? bool(u32(header, 80)) : u32(header, 8) == 2) != routes.hasBodyPolicies() ||
        u32(header, 12) * 8 != routes.privatePoses().size() ||
        header.mid(16, 32) != QByteArray::fromHex(sha256(routes.visualPolicies())))
        throw Error("Class visual policy data and its source-built readers disagree");
}
} // namespace lordly
