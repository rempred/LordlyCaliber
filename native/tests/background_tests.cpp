#include "engine/archive.h"
#include "engine/background_image.h"
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
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 289);
        const auto rom = Rom::open(path);
        int exact = 0, rounded = 0;
        qsizetype roundedChannels = 0;
        for (const auto &v : rows) {
            const auto r = v.toObject(), source = r["source"].toObject();
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
            } else
                image = decodeHuffImage(data, container == "embedded-njpg");
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
                QCOMPARE(sha256(image.rgba), r["sha256"].toString().toLatin1());
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
