#include "engine/consumable_visual.h"
#include "engine/campaign_map.h"
#include "engine/image_colors.h"
#include <QPainter>
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key) {
    const auto offset = 0x594280U + key;
    const auto count = u32(rom.bytes(), offset), size = u32(rom.bytes(), offset + 4);
    if (count < 4 || count > 8192 || size > 8192 ||
        quint64(offset) + count + 4 > quint64(rom.bytes().size()))
        throw Error("Invalid item visual resource");
    return decodeLzss(rom.bytes().mid(offset + 8, count - 4), size);
}
} // namespace
QByteArray ConsumableVisual::retailPixels(const Rom &rom) {
    const auto bytes = resource(rom, 0x1de9bc0);
    if (bytes.size() != 2872)
        throw Error("Unrecognized item artwork layout");
    return bytes.first(2048);
}
QByteArray ConsumableVisual::retailPalettes(const Rom &rom) {
    const auto bytes = resource(rom, 0x1de9fa4);
    if (bytes.size() != 128)
        throw Error("Unrecognized item palette layout");
    return bytes;
}
ConsumableVisual ConsumableVisual::retail(const Rom &rom, int item) {
    return {"Original sparkle effect", retailPixels(rom),
            retailPalettes(rom).mid(item == 6 || item == 7 ? 96 : 0, 32)};
}
void ConsumableVisual::validate() const {
    if (name.trimmed().isEmpty() || name.size() > 80 || pixels.size() != 2048 ||
        palette.size() != 32)
        throw Error("Item visuals require a name, four 32 × 32 frames and a 16-colour palette");
}
QJsonObject ConsumableVisual::json() const {
    validate();
    return {{"name", name},
            {"pixels", QString::fromLatin1(pixels.toBase64())},
            {"palette", QString::fromLatin1(palette.toBase64())}};
}
ConsumableVisual ConsumableVisual::fromJson(const QJsonObject &o) {
    const auto pixels = o["pixels"].toString().toLatin1(),
               palette = o["palette"].toString().toLatin1();
    ConsumableVisual result{o["name"].toString(), QByteArray::fromBase64(pixels),
                            QByteArray::fromBase64(palette)};
    result.validate();
    if (result.pixels.toBase64() != pixels || result.palette.toBase64() != palette)
        throw Error("Invalid item artwork encoding");
    return result;
}
QImage ConsumableVisual::fit(const QImage &input, bool smooth) {
    if (input.isNull() || input.width() > 4096 || input.height() > 4096)
        throw Error("Choose an image no larger than 4096 × 4096");
    const auto normalized = imageToSrgb(input);
    const auto scaled = normalized.scaled(
        32, 32, Qt::KeepAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
    QImage result(32, 32, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawImage((32 - scaled.width()) / 2, (32 - scaled.height()) / 2, scaled);
    return result;
}
ConsumableVisual ConsumableVisual::fromImages(const QString &name, const QVector<QImage> &frames,
                                              bool smooth) {
    if (frames.size() != 4)
        throw Error("Choose four frames for the item effect");
    QImage sheet(32, 128, QImage::Format_RGBA8888);
    sheet.fill(Qt::transparent);
    {
        QPainter painter(&sheet);
        for (int i = 0; i < 4; ++i)
            painter.drawImage(0, i * 32, fit(frames[i], smooth));
    }
    // Reuse the existing indexed-art converter: a common palette across all frames,
    // native 5-bit channels and a reserved transparent entry.
    const auto data = CampaignMap::prepareArtwork(sheet, 16, false, false);
    ConsumableVisual result{name, data.mid(32), data.first(32)};
    result.validate();
    return result;
}
QImage ConsumableVisual::frame(int id) const {
    validate();
    if (id < 0 || id >= 4)
        throw Error("Invalid item effect frame");
    return CampaignMap::artworkImage(palette + pixels.mid(id * 512, 512), {32, 32}, 16);
}
} // namespace lordly
