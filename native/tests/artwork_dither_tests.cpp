#include "engine/army_artwork.h"
#include "engine/avatar_artwork.h"
#include "engine/combat_artwork.h"
#include "engine/icon_artwork.h"
#include "engine/sprite_object.h"
#include "engine/image_colors.h"
#include "engine/animation_file_import.h"
#include "engine/sprite_colors.h"
#include <QColorSpace>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
using namespace lordly;
namespace {
QImage sample(QSize size) {
    QImage image(size, QImage::Format_RGBA8888);
    for (int y = 0; y < size.height(); ++y)
        for (int x = 0; x < size.width(); ++x)
            image.setPixelColor(
                x, y,
                {x * 17 % 256, y * 29 % 256, (x * 7 + y * 13) % 256, (x * 19 + y * 23) % 256});
    return image;
}
// Independently tabulated 8-bit adjustments for the browser's native-step pattern.
QImage adjusted(const QImage &source) {
    constexpr int delta[]{-8, 1, -6, 3, 5, -4, 7, -2, -5, 4, -7, 2, 8, -1, 6, -3};
    auto result = source.copy();
    for (int y = 0; y < source.height(); ++y)
        for (int x = 0; x < source.width(); ++x) {
            auto c = source.pixelColor(x, y);
            const int d = delta[(y % 4) * 4 + x % 4];
            const auto shift = [&](int value) {
                return value == 0 || value == 255 ? value : std::clamp(value + d, 0, 255);
            };
            result.setPixelColor(x, y,
                                 {shift(c.red()), shift(c.green()), shift(c.blue()), c.alpha()});
        }
    return result;
}
} // namespace
class ArtworkDitherTests : public QObject {
    Q_OBJECT
  private slots:
    void colorProfiles() {
        QImage linear(3, 1, QImage::Format_RGBA64);
        linear.setColorSpace(QColorSpace::SRgbLinear);
        for (int x = 0; x < 3; ++x)
            linear.setPixelColor(x, 0, QColor::fromRgbF(.5, .25, .125, x / 2.));
        const auto result = imageToSrgb(linear);
        QCOMPARE(result.format(), QImage::Format_RGBA8888);
        QCOMPARE(result.colorSpace(), QColorSpace(QColorSpace::SRgb));
        for (int x = 0; x < 3; ++x) {
            const auto pixel = result.pixelColor(x, 0);
            QVERIFY(std::abs(pixel.red() - 188) <= 1);
            QVERIFY(std::abs(pixel.green() - 137) <= 1);
            QVERIFY(std::abs(pixel.blue() - 99) <= 1);
            QVERIFY(std::abs(pixel.alpha() - qRound(255 * x / 2.)) <= 1);
        }
        QCOMPARE(linear.colorSpace(), QColorSpace(QColorSpace::SRgbLinear));
        QCOMPARE(linear.format(), QImage::Format_RGBA64);
        QImage raw(2, 2, QImage::Format_RGBA8888);
        raw.fill(QColor(91, 127, 173, 129));
        const auto untagged = imageToSrgb(raw);
        QCOMPARE(untagged.pixelColor(0, 0), raw.pixelColor(0, 0));
        QCOMPARE(imageToSrgb(untagged), untagged);
        QTemporaryDir work;
        for (auto space : {QColorSpace::AdobeRgb, QColorSpace::DisplayP3}) {
            auto wide = untagged.convertedToColorSpace(QColorSpace(space), QImage::Format_RGBA64);
            const auto path = work.filePath(QString::number(int(space)) + ".png");
            QVERIFY(wide.save(path));
            const auto imported = importAnimationFiles({path}).composite(0);
            const auto pixel = imported.pixelColor(0, 0);
            QVERIFY(std::abs(pixel.red() - 91) <= 1);
            QVERIFY(std::abs(pixel.green() - 127) <= 1);
            QVERIFY(std::abs(pixel.blue() - 173) <= 1);
            QCOMPARE(pixel.alpha(), 129);
        }
        auto asset = SpriteAsset::blank("Tagged library asset", {3, 1});
        asset.frames[0].layers[0].image = linear.convertToFormat(QImage::Format_RGBA8888);
        const auto original = asset.frames[0].layers[0].image;
        const auto normalized = spriteToSrgb(asset);
        QVERIFY(normalized.composite(0).pixelColor(1, 0).red() >= 187);
        QCOMPARE(asset.frames[0].layers[0].image, original);
        QCOMPARE(SpriteAsset::fromImage("New asset", linear).composite(0).pixelColor(1, 0),
                 result.pixelColor(1, 0));
    }
    void directSprite() {
        RasterImage input{8, 8, QByteArray(8 * 8 * 4, char(-128))};
        for (int i = 0; i < 64; ++i)
            put8(input.rgba, i * 4 + 3, quint8(i * 4));
        const auto before = input.rgba;
        const auto plain = SpriteObject::fromImage(input).pixels(0);
        const auto sprite = SpriteObject::fromImage(input, true);
        const auto dithered = sprite.pixels(0);
        constexpr int expected[]{15, 16, 15, 16, 16, 15, 16, 15, 15, 16, 15, 16, 16, 15, 16, 15};
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const int at = y * 8 + x, c = expected[(y % 4) * 4 + x % 4];
                QCOMPARE(u16(dithered.color, at * 2), quint16((c << 11) | (c << 6) | (c << 1) | 1));
                QCOMPARE(u16(plain.color, at * 2), quint16(0x8421));
                QCOMPARE(u8(dithered.alpha, at), quint8(at * 4));
            }
        QCOMPARE(SpriteObject::decode(sprite.source()).pixels(0), dithered);
        QCOMPARE(input.rgba, before);
        for (const auto value : {0, 255}) {
            input.rgba.fill(char(value));
            QCOMPARE(SpriteObject::fromImage(input, true).pixels(0),
                     SpriteObject::fromImage(input).pixels(0));
        }
    }
    void portraitReduction() {
        auto image = sample({40, 48});
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 40; ++x) {
                auto c = image.pixelColor(x, y);
                c.setAlpha(255);
                image.setPixelColor(x, y, c);
            }
        const auto plain = AvatarArtwork::mapImage(image, true);
        const auto dithered = AvatarArtwork::mapImage(image, true, Qt::black, true);
        QVERIFY(plain != dithered);
        QSet<quint16> colors, plainColors;
        for (int i = 0; i < 1920; ++i) {
            colors.insert(u16(dithered, i * 2));
            plainColors.insert(u16(plain, i * 2));
            QVERIFY(u16(dithered, i * 2) & 1);
        }
        QVERIFY(colors.size() <= 80);
        QCOMPARE(colors, plainColors);
        QVERIFY(!AvatarArtwork::pack(dithered).isEmpty());
        QCOMPARE(AvatarArtwork::mapImage(image, true, Qt::black, true), dithered);
        image.fill(QColor(128, 128, 128, 128));
        // At or below the palette budget no reduction or dithering is needed.
        QCOMPARE(AvatarArtwork::mapImage(image, true, Qt::blue, true),
                 AvatarArtwork::mapImage(image, true, Qt::blue));
    }
    void fixedPalettes() {
        QString stage = "load ROM";
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            stage = "load icons";
            auto icons = IconArtwork::load(rom);
            const auto icon = sample({16, 16});
            for (const int item : {1, 0x201}) {
                const auto plain = icons.mapImage(item, icon);
                const auto mapped = icons.mapImage(item, icon, true, 128, true);
                QCOMPARE(mapped, icons.mapImage(item, adjusted(icon)));
                QVERIFY(mapped != plain);
                for (int i = 0; i < 256; ++i)
                    QCOMPARE(u16(mapped, i * 2) & 1, u16(plain, i * 2) & 1);
                icons.setWords(item, mapped);
            }
            stage = "load Army";
            const auto army = ArmyArtwork::load(rom);
            for (int atlas = 0; atlas < 6; ++atlas)
                for (int ordinal = 0; ordinal < 2; ++ordinal) {
                    stage = QString("Army %1 palette %2").arg(atlas).arg(ordinal);
                    const auto &spec = ArmyArtwork::specs()[atlas];
                    const auto image = sample({spec.width, spec.height});
                    const auto mapped = army.mapImage(atlas, ordinal, image, 128, {}, true);
                    QCOMPARE(mapped, army.mapImage(atlas, ordinal, adjusted(image)));
                    const auto original = army.image(atlas, 0, ordinal);
                    // Existing exact colors keep their original duplicate palette indices.
                    QCOMPARE(
                        army.mapImage(atlas, ordinal, original, 128, army.indices(atlas, 0), true),
                        army.indices(atlas, 0));
                }
            stage = "load combat";
            const auto combat = CombatArtwork::load(rom);
            QSet<int> formats;
            // Independently selected by the browser decoder: CI8 and direct RGB5551.
            // Other descriptor members include controls rather than sprite containers.
            for (const auto id : {QString("003bac1c:0"), QString("006ca532:56")}) {
                const auto &binding = combat.binding(id);
                stage = "combat " + id;
                const auto sprite = combat.original(id);
                const int format = sprite.colorFormat();
                const auto image = sample({sprite.width(), sprite.height()});
                const auto plain = combat.mapImage(binding.id, 0, image);
                const auto mapped = combat.mapImage(binding.id, 0, image, true);
                QCOMPARE(mapped.alpha, plain.alpha);
                if (format == 1) {
                    auto expected = combat.mapImage(binding.id, 0, adjusted(image));
                    // Unchanged source pixels preserve their exact indices before mapping.
                    const auto original = combat.image(binding.id, 0);
                    for (int y = 0; y < image.height(); ++y)
                        for (int x = 0; x < image.width(); ++x)
                            if (image.pixelColor(x, y).rgb() == original.pixelColor(x, y).rgb() &&
                                (!plain.alpha.isEmpty() || image.pixelColor(x, y).alpha() ==
                                                               original.pixelColor(x, y).alpha()))
                                expected.color[y * image.width() + x] =
                                    plain.color[y * image.width() + x];
                    QCOMPARE(mapped, expected);
                }
                QCOMPARE(combat.mapImage(binding.id, 0, combat.image(binding.id, 0), true),
                         combat.pixels(binding.id, 0));
                formats.insert(format);
            }
            QCOMPARE(formats, (QSet<int>{1, 2}));
        } catch (const Error &e) {
            QFAIL(qPrintable(stage + ": " + QString::fromUtf8(e.what())));
        }
    }
};
QTEST_GUILESS_MAIN(ArtworkDitherTests)
#include "artwork_dither_tests.moc"
