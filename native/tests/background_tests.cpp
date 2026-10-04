#include "engine/archive.h"
#include "engine/background_image.h"
#include "engine/scene_images.h"
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class BackgroundTests : public QObject {
    Q_OBJECT
  private slots:
    void rawPaletteBanks() {
        const QJsonObject layout{
            {"width", 2},        {"height", 1},           {"pixelFormat", "ci4"},
            {"paletteCount", 2}, {"paletteBankCount", 2}, {"dataOffset", 8}};
        const auto bytes = QByteArray::fromHex("f801003f07c1ffff01");
        const auto image = decodeRawImage(bytes, layout);
        QCOMPARE(image.frames.size(), 2);
        QCOMPARE(image.frames[0].rgba, QByteArray::fromHex("ff0000ff0000ffff"));
        QCOMPARE(image.frames[1].rgba, QByteArray::fromHex("00ff00ffffffffff"));
        QCOMPARE(image.composite.width, 4);
        QVERIFY_THROWS_EXCEPTION(Error, decodeRawImage(bytes.first(8), layout));
        auto bad = bytes;
        put8(bad, 8, 0x21);
        QVERIFY_THROWS_EXCEPTION(Error, decodeRawImage(bad, layout));
        auto invalid = layout;
        invalid["paletteBankCount"] = 1000000;
        QVERIFY_THROWS_EXCEPTION(Error, decodeRawImage(bytes, invalid));
        invalid = layout;
        invalid["rowStride"] = 2147483647;
        QVERIFY_THROWS_EXCEPTION(Error, decodeRawImage(bytes, invalid));
    }
    void currentRomSelectors() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private source ROM required");
        const auto rom = Rom::open(path);
        QCOMPARE(SceneImages::environment(rom, 34), QStringList{"mode2-environment:000D62F6"});
        QCOMPARE(SceneImages::environment(rom, 57), QStringList{"mode2-environment:00236B58"});
        QVERIFY(SceneImages::foreground(rom, 34).isEmpty());
        QCOMPARE(SceneImages::foreground(rom, 57), QStringList{"archive:49"});
        QCOMPARE(SceneImages::group(rom, 0).size(), 5);
        QVERIFY_THROWS_EXCEPTION(Error, SceneImages::environment(rom, 80));
        auto changed = rom.bytes();
        put32(changed, 0x594288 + 34 * 4, u32(changed, 0x594288 + 57 * 4));
        QCOMPARE(SceneImages::environment(Rom::fromBytes(changed), 34),
                 QStringList{"mode2-environment:00236B58"});
        const auto directors = Directors::load(rom);
        const auto &scene = directors.entries()[0x1a14970];
        const auto bg = SceneImages::backgrounds(rom, scene);
        QCOMPARE(bg.mode, 2);
        QCOMPARE(bg.requests.size(), 1);
        QCOMPARE(bg.requests[0].selector, 34);
        QCOMPARE(bg.requests[0].images, SceneImages::environment(rom, 34));
        auto edited = scene;
        edited.program.commands[0].operands[0] = 57;
        QCOMPARE(SceneImages::backgrounds(rom, edited).mode, -1);
        const auto override = SceneImages::backgrounds(rom, edited, 2);
        QCOMPARE(override.requests[0].images, SceneImages::environment(rom, 57));
    }
    void signedLayersAndAlpha() {
        QByteArray bytes = QByteArray::fromHex("4235000200070009");
        auto layer = [](int x, int width, quint16 color, quint8 alpha) {
            QByteArray r(32, 0);
            put16(r, 0, quint16(x));
            put16(r, 2, quint16(-3));
            put16(r, 4, quint16(width));
            put16(r, 6, 1);
            put32(r, 8, 16);
            for (int i = 0; i < width; ++i) {
                put16(r, 16 + i * 2, color);
                put8(r, 24 + i, alpha);
            }
            return r;
        };
        bytes += layer(-2, 2, 0xf801, 255);
        bytes += layer(-1, 1, 0x003f, 128);
        const auto decoded = decodeBackgroundImage(bytes);
        QCOMPARE(decoded.origin, QPoint(-2, -3));
        QCOMPARE(decoded.reference, QPoint(7, 9));
        QCOMPARE(decoded.layers.size(), 2);
        QCOMPARE(decoded.composite.width, 2);
        QCOMPARE(decoded.composite.height, 1);
        QCOMPARE(decoded.composite.rgba, QByteArray::fromHex("ff0000ff7f0080ff"));
        QVERIFY(!decoded.limitations.isEmpty());
        QVERIFY_THROWS_EXCEPTION(Error, decodeBackgroundImage(bytes.first(bytes.size() - 1)));
        auto invalid = bytes;
        put32(invalid, 20, 1);
        QVERIFY_THROWS_EXCEPTION(Error, decodeBackgroundImage(invalid));
        QVERIFY_THROWS_EXCEPTION(Error, decodeBackgroundImage(bytes + char(0)));
        const auto k = QByteArray::fromHex("4b0102000002000110f801003f");
        QCOMPARE(decodeKImage(k).rgba, QByteArray::fromHex("0000ffffff0000ff"));
        invalid = k;
        put8(invalid, 8, 0x20);
        QVERIFY_THROWS_EXCEPTION(Error, decodeKImage(invalid));
    }
    void huffConstantBlock() {
        // Six DC-only blocks. Y/Cb/Cr DC=256 => each plane=128 with the native
        // scale. Standard JPEG category/EOB codes written independently.
        QString bits = "11111101000000001010"; // Y DC category 9, +256, EOB.
        bits += "001010001010001010";          // Three more luma blocks, zero DC delta.
        bits += "11111111010000000000";        // Cb category 9, +256, EOB.
        bits += "11111111010000000000";        // Cr category 9, +256, EOB.
        QByteArray source = QByteArray::fromHex("4855fe0000100010485546460001");
        for (int i = 0; i < bits.size(); i += 8) {
            int byte = 0;
            for (int j = 0; j < 8; ++j)
                byte = byte * 2 + (i + j < bits.size() && bits[i + j] == '1');
            source += char(byte);
        }
        const auto image = decodeHuffImage(source, true);
        QCOMPARE(image.width, 16);
        QCOMPARE(image.height, 16);
        QCOMPARE(image.rgba, QByteArray::fromHex("808080ff").repeated(256));
        QVERIFY_THROWS_EXCEPTION(Error, decodeHuffImage(source.first(source.size() - 1), true));
        auto bad = source;
        put16(bad, 12, 2);
        QVERIFY_THROWS_EXCEPTION(Error, decodeHuffImage(bad, true));
        QByteArray outer(4, 0);
        put32(outer, 0, quint32(source.size()));
        outer += source;
        QCOMPARE(decodeHuffImage(outer, false).rgba, image.rgba);
    }
    void realRomImages() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private source ROM required");
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("background-fixture.cjs"),
                                 path, work.path()});
        QVERIFY(generator.waitForFinished(60000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(work.filePath("backgrounds.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = fixture["rows"].toArray();
        QCOMPARE(rows.size(), 367);
        const auto rom = Rom::open(path);
        for (const auto &v : fixture["selectors"].toArray()) {
            const auto table = v.toObject();
            const auto id = table["id"].toString();
            for (const auto &e : table["entries"].toArray()) {
                const auto entry = e.toObject();
                const int selector = entry["selector"].toInt();
                QStringList expected;
                for (const auto &i : entry["images"].toArray())
                    expected << i.toString();
                const auto actual =
                    id.contains("mode2-environment") ? SceneImages::environment(rom, selector)
                    : id.contains("mode2-overlay")   ? SceneImages::foreground(rom, selector)
                                                     : SceneImages::group(rom, selector);
                QCOMPARE(actual, expected);
            }
        }
        int exact = 0, rounded = 0;
        qsizetype roundedChannels = 0;
        for (const auto &v : rows) {
            const auto r = v.toObject(), source = r["source"].toObject();
            if (r["compound"].toBool()) {
                const auto full = SceneImages::decode(rom, r["id"].toString());
                QCOMPARE(full.composite.width, r["width"].toInt());
                QCOMPARE(full.composite.height, r["height"].toInt());
                QCOMPARE(full.origin, QPoint(r["originX"].toInt(), r["originY"].toInt()));
                QCOMPARE(full.layers.size(), 6);
                QCOMPARE(sha256(full.composite.rgba), r["sha256"].toString().toLatin1());
                continue;
            }
            const int offset = source["offset"].toInt();
            const auto kind = source["kind"].toString();
            QByteArray data;
            if (kind == "lha")
                data = LhaMember::read(rom.bytes(), offset).decode(rom);
            else if (kind == "custom")
                data = decodeLzss(rom.bytes().mid(offset + 8, u32(rom.bytes(), offset) - 4),
                                  u32(rom.bytes(), offset + 4));
            else if (kind == "stored")
                data = rom.bytes().mid(offset + 4, u32(rom.bytes(), offset));
            else
                data = rom.bytes().mid(offset, source["size"].toInt());
            RasterImage image;
            const auto container = r["container"].toString();
            if (container == "64")
                image = decodeN64Image(data);
            else if (container == "K")
                image = decodeKImage(data);
            else if (container == "bg2") {
                const auto bg = decodeBackgroundImage(data);
                image = bg.composite;
                QCOMPARE(bg.origin, QPoint(r["originX"].toInt(), r["originY"].toInt()));
                const auto layers = r["layers"].toArray();
                QCOMPARE(bg.layers.size(), layers.size());
                for (int i = 0; i < layers.size(); ++i) {
                    const auto l = layers[i].toObject();
                    QCOMPARE(bg.layers[i].origin, QPoint(l["x"].toInt(), l["y"].toInt()));
                    QCOMPARE(bg.layers[i].image.width, l["width"].toInt());
                    QCOMPARE(bg.layers[i].image.height, l["height"].toInt());
                }
            } else if (container == "raw")
                image = decodeRawImage(data, r["layout"].toObject()).composite;
            else
                image = decodeHuffImage(data, container == "embedded-njpg");
            const auto catalogImage = SceneImages::decode(rom, r["id"].toString(), false);
            QCOMPARE(catalogImage.composite.rgba, image.rgba);
            QCOMPARE(image.width, r["width"].toInt());
            QCOMPARE(image.height, r["height"].toInt());
            if (!r["pixels"].isNull()) {
                QFile pixels(work.filePath(r["pixels"].toString()));
                QVERIFY(pixels.open(QIODevice::ReadOnly));
                const auto expected = pixels.readAll();
                QCOMPARE(expected.size(), image.rgba.size());
                int maximum = 0;
                for (qsizetype i = 0; i < expected.size(); ++i) {
                    const int delta = std::abs(int(u8(expected, i)) - u8(image.rgba, i));
                    maximum = std::max(maximum, delta);
                    roundedChannels += delta != 0;
                }
                QVERIFY2(maximum <= 1,
                         qPrintable(r["id"].toString() +
                                    QString(" maximum channel difference %1").arg(maximum)));
                ++rounded;
            } else {
                QVERIFY2(sha256(image.rgba) == r["sha256"].toString().toLatin1(),
                         qPrintable(r["id"].toString()));
                ++exact;
            }
            if (r["id"].toString() == "mode2-environment:00236B58") {
                QImage view(reinterpret_cast<const uchar *>(image.rgba.constData()), image.width,
                            image.height, image.width * 4, QImage::Format_RGBA8888);
                QVERIFY(view.save("native-graduation-background.png"));
            }
        }
        qInfo() << exact << "exact images," << rounded << "HUFF previews," << roundedChannels
                << "channels differing by one";
    }
};
QTEST_GUILESS_MAIN(BackgroundTests)
#include "background_tests.moc"
