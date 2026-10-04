#include "engine/scene_images.h"
#include "engine/archive.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
namespace {
constexpr quint32 base = 0x594280;
const QJsonObject &metadata() {
    static const QJsonObject value = [] {
        QFile file(":/game/director/images.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing native scene image catalog");
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return value;
}
QByteArray resource(const Rom &rom, quint32 key) {
    if (key >= 0x10000000 || key % 2)
        throw Error("Invalid scene resource key");
    const qsizetype at = base + key;
    const auto size = u32(rom.bytes(), at);
    if (size > 64 * 1024 * 1024 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Scene resource exceeds its ROM envelope");
    return rom.bytes().mid(at + 4, size);
}
QByteArray source(const Rom &rom, const SceneImage &asset) {
    if (asset.kind == "lha") {
        const auto member = LhaMember::read(rom.bytes(), base + asset.key + 4);
        if (member.decodedSize > 64 * 1024 * 1024)
            throw Error("Image archive exceeds the decode budget");
        return member.decode(rom);
    }
    auto data = resource(rom, asset.key);
    if (asset.kind == "huff") {
        QByteArray header(4, 0);
        put32(header, 0, quint32(data.size()));
        return header + data;
    }
    if (asset.kind == "custom") {
        const auto size = u32(data, 0);
        if (size > 64 * 1024 * 1024)
            throw Error("Image resource exceeds the decode budget");
        qsizetype consumed = 0;
        const auto decoded = decodeLzss(data.mid(4), size, &consumed);
        if (consumed != data.size() - 4)
            throw Error("Image resource has unread compressed data");
        return decoded;
    }
    return data;
}
quint32 selectedKey(const Rom &rom, quint32 root, int selector) {
    const auto rows = resource(rom, root);
    if (rows.size() % 4 || selector < 0 || selector >= rows.size() / 4)
        throw Error("Background selector lies outside its current ROM table");
    return u32(rows, selector * 4);
}
QStringList knownKey(quint32 key, bool requireImage) {
    QStringList result;
    if (!key)
        return result;
    for (const auto &image : SceneImages::catalog())
        if (image.key == key)
            result << image.id;
    if (result.isEmpty() && requireImage)
        throw Error(QString("Background resource %1 has no verified image layout")
                        .arg(key, 8, 16, QChar('0')));
    return result;
}
} // namespace
const QMap<QString, SceneImage> &SceneImages::catalog() {
    static const QMap<QString, SceneImage> value = [] {
        QMap<QString, SceneImage> result;
        for (const auto &v : metadata()["images"].toArray()) {
            const auto row = v.toObject();
            SceneImage image{row["id"].toString(),
                             row["name"].toString(),
                             row["container"].toString(),
                             row["kind"].toString(),
                             quint32(row["key"].toInteger()),
                             row["layout"].toObject(),
                             {}};
            for (const auto &m : row["members"].toArray())
                image.members << m.toString();
            if (image.id.isEmpty() || result.contains(image.id))
                throw Error("Invalid scene image catalog identity");
            result.insert(image.id, std::move(image));
        }
        return result;
    }();
    return value;
}
DecodedImage SceneImages::decode(const Rom &rom, const QString &id, bool compound) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene images currently require US revision 0");
    const auto found = catalog().constFind(id);
    if (found == catalog().end())
        throw Error("Unknown scene image identity");
    const auto &asset = *found;
    auto bytes = source(rom, asset);
    if (compound && !asset.members.isEmpty()) {
        if (asset.members.first() != id || bytes.first(2) != "B5" || u8(bytes, 2) != 5 ||
            u8(bytes, 3) != asset.members.size())
            throw Error("Compound background members disagree with their root header");
        put8(bytes, 2, 1); // Decode a temporary complete B5 record sequence; ROM stays intact.
        for (int i = 1; i < asset.members.size(); ++i) {
            const auto member = catalog().constFind(asset.members[i]);
            if (member == catalog().end() || !member->layout["nativeRecordHeader"].toBool())
                throw Error("Compound background has an unmapped external record");
            const auto data = source(rom, *member);
            decodeRawImage(data, member->layout); // Authenticate framing, not catalog coordinates.
            if (bytes.size() + data.size() > 64 * 1024 * 1024)
                throw Error("Compound background exceeds the decode budget");
            bytes += data;
        }
    }
    DecodedImage result;
    if (asset.container == "64")
        result.composite = decodeN64Image(bytes);
    else if (asset.container == "K")
        result.composite = decodeKImage(bytes);
    else if (asset.container == "raw")
        result = decodeRawImage(bytes, asset.layout);
    else if (asset.container == "bg2") {
        auto bg = decodeBackgroundImage(bytes);
        result.composite = std::move(bg.composite);
        result.origin = bg.origin;
        result.reference = bg.reference;
        result.layers = std::move(bg.layers);
        result.limitations = std::move(bg.limitations);
    } else if (asset.container == "embedded-njpg" || asset.container == "section-c-njpg") {
        result.composite = decodeHuffImage(bytes, asset.container == "embedded-njpg");
        result.limitations << "HUFF fixed-point rounding is approximated.";
    } else
        throw Error("Unsupported scene image container");
    return result;
}
QStringList SceneImages::environment(const Rom &rom, int selector) {
    return knownKey(selectedKey(rom, 4, selector), true);
}
QStringList SceneImages::foreground(const Rom &rom, int selector) {
    return knownKey(selectedKey(rom, 0x2938c8, selector), true);
}
QStringList SceneImages::group(const Rom &rom, int selector) {
    const auto key = selectedKey(rom, 0x16b3d18, selector);
    if (!key)
        return {};
    const auto members = resource(rom, key);
    if (members.size() % 4 || members.size() > 1024 * 4)
        throw Error("Invalid scene resource group");
    QStringList result;
    for (int i = 0; i < members.size(); i += 4) {
        const auto memberKey = u32(members, i);
        auto images = knownKey(memberKey, false);
        if (memberKey && images.isEmpty()) {
            // Format 5 wraps the first archive in an 8-byte B5 header and a
            // 16-byte record. The group selects that wrapper, not the archive.
            const auto wrapped = resource(rom, memberKey);
            if (wrapped.size() >= 24 && wrapped.first(3) == QByteArray::fromHex("423505")) {
                images = knownKey(memberKey + 24, true);
                if (images.size() != 1 ||
                    catalog()[images.first()].members.size() != u8(wrapped, 3))
                    throw Error("Compound scene resource has inconsistent member identities");
            }
        }
        result << images; // Groups also own non-image resources.
    }
    return result;
}
SceneBackgrounds SceneImages::backgrounds(const Rom &rom, const DirectorResource &scene,
                                          int modeOverride) {
    SceneBackgrounds result;
    const int cls = scene.program.resourceClass();
    const auto context = metadata()["scenes"].toObject()[QString::number(scene.key)].toObject();
    if (cls == 4 || cls == 5) {
        result.mode = 0;
        result.modeEvidence = "Resource class 4/5 selects scene-group launch preload.";
    } else if (sha256(scene.program.encode()).toUpper() ==
                   context["sourceSha256"].toString().toLatin1().toUpper() &&
               !context["mode"].isNull()) {
        result.mode = context["mode"].toInt(-1);
        result.modeEvidence = context["modeEvidence"].toString();
    } else
        result.modeEvidence = "Launch mode requires external state or an explicit preview choice.";
    if (modeOverride != -1) {
        if (modeOverride != 0 && modeOverride != 2)
            throw Error("Invalid scene preview mode");
        result.mode = modeOverride;
        result.modeEvidence = "User-selected preview mode.";
    }
    for (const auto &command : scene.program.commands) {
        if (command.opcode != 0x80000006 || command.operands.size() != 1)
            continue;
        SceneBackgroundRequest request{command.word, qint32(command.operands[0]), {}, {}};
        try {
            if (request.selector < 0)
                request.status = "Environment is derived from external gameplay state.";
            else if (cls == 4 || cls == 5 || result.mode == 0) {
                request.images = group(rom, request.selector);
                request.status = "Scene resource group; members are shown separately.";
            } else if (result.mode == 2) {
                request.images = environment(rom, request.selector);
                request.status =
                    "Environment base. Foreground selection is independent launch state.";
            } else
                request.status = "Select a preview mode to resolve this background request.";
        } catch (const std::exception &e) {
            request.status = QString::fromUtf8(e.what());
        }
        result.requests << request;
    }
    return result;
}
} // namespace lordly
