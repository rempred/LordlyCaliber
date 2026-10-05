#include "engine/scene_image_presentation.h"
#include "game/director/image_presentation.h"
#include "game/director/scene_matrix.h"
#include <bit>
namespace lordly {
SceneImagePresentation SceneImagePresentation::load(const Rom &rom, int row) {
    SceneImagePresentation result;
    result.imageId_ = SceneImages::presentationImage(rom, row);
    const auto decoded = SceneImages::decode(rom, result.imageId_);
    const auto &r = decoded.composite;
    if (r.width < 2 || r.height < 4 || r.width > 4096 || r.height > 4096)
        throw Error("Scene presentation image exceeds its supported dimensions");
    result.source_ = QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                            r.width * 4, QImage::Format_RGBA8888)
                         .copy();
    if (result.source_.isNull())
        throw Error("Unable to allocate scene presentation image");
    return result;
}
QByteArray SceneImagePresentation::initialContext() const {
    QByteArray context(0x848, 0);
    put32(context, 0x818, 0x3f800000);
    put32(context, 0x81c, 0x3f800000);
    put16(context, 0x834, quint16(source_.width()));
    put16(context, 0x836, quint16(source_.height()));
    put32(context, 0x4c, 1); // Host resource identity; never a target RAM pointer.
    return context;
}
QImage SceneImagePresentation::vignette(const QImage &source, int alphaCap) {
    if (source.isNull() || source.width() < 2 || source.height() < 4 || source.width() > 4096 ||
        source.height() > 4096)
        throw Error("Vignette requires a bounded source image");
    const int width = source.width() / 2, height = source.height() / 2, storage = (width + 3) & ~3;
    QImage result(storage, height, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    const auto rgba = source.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < height; ++y) {
        auto out = result.scanLine(y);
        const auto in = rgba.constScanLine(y * 2);
        for (int x = 0; x < width; ++x)
            for (int c = 0; c < 3; ++c)
                out[x * 4 + c] = in[x * 8 + c] & 0xf8;
    }
    if (!lordly_image_vignette(result.bits(), storage, height, alphaCap))
        throw Error("Vignette alpha or dimensions are outside the native domain");
    return result.copy(0, 0, width, height);
}
void SceneImagePresentation::construct(const Rom &rom, const QVector<quint32> &args,
                                       const SceneCamera &camera, QByteArray &context) {
    if (args.size() != 8 || context.size() != 0x848 || camera.bank().size() != 56)
        throw Error("Scene vignette requires eight operands, its image owner, and a camera");
    if (slot_ >= 0)
        throw Error("Scene vignette can be constructed only once per image launch");
    if (args[0] >= 20 || args[6] > 255)
        throw Error("Scene vignette slot or alpha is outside its native range");
    auto next = context;
    float values[14], placement[4]{float(qint32(args[1])), float(qint32(args[2])),
                                   float(qint32(args[4])), float(qint32(args[5]))};
    for (int i = 0; i < 14; ++i)
        values[i] = std::bit_cast<float>(u32(camera.bank(), i * 4));
    double constants[5];
    // Qualified complete constant block ROM2B8A98 -> retained RAM80243C98.
    for (int i = 0; i < 5; ++i)
        constants[i] = std::bit_cast<double>(quint64(u32(rom.bytes(), 0x2b8a98 + i * 8)) << 32 |
                                             u32(rom.bytes(), 0x2b8a9c + i * 8));
    if (!lordly_image_matrices(reinterpret_cast<uint8_t *>(next.data()), values, placement, args[7],
                               constants, std::bit_cast<float>(u32(rom.bytes(), 0x3ab50))))
        throw Error("Scene vignette transform exceeds finite fixed-point bounds");
    auto processed = vignette(source_, int(args[6]));
    put8(next, 0x838, quint8(args[0]));
    put8(next, 0x83d, quint8(args[6]));
    put32(next, 0x50, 1);
    put32(next, 0x4c, 2);
    context = std::move(next);
    processed_ = std::move(processed);
    slot_ = int(args[0]);
}
QVector<ActorRasterLayer> SceneImagePresentation::project(const QByteArray &context,
                                                          const SceneCamera &camera,
                                                          const QByteArray &parent) const {
    if (slot_ < 0)
        return {};
    if (context.size() != 0x848 || parent.size() != 64)
        throw Error("Scene image drawing lost its matrices");
    if (u32(context, 0x824) || u32(context, 0x82c))
        throw Error("Scene image transition requires its echo drawing state");
    if (u32(context, 0x4c) != 2)
        throw Error("Scene image drawing requires its selected image resource");
    float model[16], outer[16], scale[16];
    uint8_t packed[64];
    lordly_matrix_unpack(model, reinterpret_cast<const uint8_t *>(context.constData()));
    lordly_matrix_unpack(outer, reinterpret_cast<const uint8_t *>(parent.constData()));
    lordly_matrix_scale(scale, 2, 2,
                        2); // Current processed image compensates its half-size pixels.
    lordly_matrix_multiply(model, scale, model);
    lordly_matrix_multiply(model, model, outer);
    if (!lordly_matrix_pack(packed, model))
        throw Error("Scene image drawing exceeds fixed-point bounds");
    ActorRasterLayer layer;
    layer.slot = slot_;
    layer.image = processed_;
    layer.primitive = {255, 255, 255, 1};
    // Original current-image combiner saturates texture alpha + primitive alpha1.
    for (int y = 0; y < layer.image.height(); ++y) {
        auto p = layer.image.scanLine(y);
        for (int x = 0; x < layer.image.width(); ++x)
            p[x * 4 + 3] = quint8(qMin(255, int(p[x * 4 + 3]) + 1));
    }
    const int width = layer.image.width(), height = layer.image.height();
    layer.plane = camera.projectPackedPlane(QByteArray(reinterpret_cast<const char *>(packed), 64),
                                            {double(-(width / 2)), double(height / 2), 0},
                                            {1, 0, 0}, {0, -1, 0}, QSizeF(width, height));
    return {layer};
}
} // namespace lordly
