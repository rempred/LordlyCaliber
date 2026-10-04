#include "engine/sprite_object.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SpriteTests : public QObject {
    Q_OBJECT
  private slots:
    void deltaVariantsAndFallback() {
        QByteArray bytes = QByteArray::fromHex("5554020500020001");
        bytes += QByteArray::fromHex("55540003000200010100000000000000f800000000000000");
        bytes += QByteArray::fromHex("5554000300020001ff000000000000001200000000000000");
        const auto sprite = SpriteObject::decode(bytes);
        const auto lanes = sprite.materialize(1);
        QCOMPARE(lanes.color, QByteArray::fromHex("0200000000000000"));
        QCOMPARE(lanes.alpha, QByteArray::fromHex("e600000000000000"));
        QByteArray palette(512, 0);
        put16(palette, 0, 0xffff);
        put16(palette, 2, 0xf801);
        put16(palette, 4, 0x003f);
        const auto frame = sprite.frame(1, palette);
        QCOMPARE(frame.image.rgba, QByteArray::fromHex("0000ffeeffffff66"));
        const auto fallback = sprite.frame(999, palette);
        QCOMPARE(fallback.child, 0);
        QCOMPARE(fallback.requestedChild, 999);
        QCOMPARE(fallback.image.rgba, QByteArray::fromHex("ff0000ffffffff88"));
        QCOMPARE(sprite.source(), bytes);
        QVERIFY_THROWS_EXCEPTION(Error, sprite.frame(0));
        QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::decode(bytes.first(bytes.size() - 1)));
        QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::decode(bytes + char(0)));
        auto cyclic = bytes;
        put8(cyclic, 10, 1);
        QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::decode(cyclic).materialize(0));
        auto bad = bytes;
        put8(bad, 10, 2);
        QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::decode(bad));
        auto direct = QByteArray::fromHex("55540202000100015554000100010001");
        direct += QByteArray::fromHex("11223344") + QByteArray(12, 0);
        direct += QByteArray::fromHex("5554000100010001ff00aa55") + QByteArray(12, 0);
        QCOMPARE(SpriteObject::decode(direct).frame(1).image.rgba, QByteArray::fromHex("ee229911"));
    }
    void retailActorSpriteCorpus() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("sprite-fixture.cjs"),
             path, work.filePath("sprites.json")});
        QVERIFY(reference.waitForFinished(120000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("sprites.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = fixture["rows"].toArray();
        const auto palettes = fixture["palettes"].toObject();
        const auto rom = Rom::open(path);
        int count = 0;
        for (const auto &v : rows) {
            const auto row = v.toObject();
            const quint32 at = 0x594280 + quint32(row["key"].toInteger());
            const auto bytes = decodeLzss(rom.bytes().mid(at + 8, u32(rom.bytes(), at) - 4),
                                          u32(rom.bytes(), at + 4));
            const auto sprite = SpriteObject::decode(bytes);
            QCOMPARE(sprite.width(), row["width"].toInt());
            QCOMPARE(sprite.height(), row["height"].toInt());
            QCOMPARE(sprite.colorFormat(), row["colorFormat"].toInt());
            QCOMPARE(sprite.alphaFormat(), row["alphaFormat"].toInt());
            const auto variants = row["variants"].toArray();
            QCOMPARE(sprite.children().size(), variants.size());
            const auto palette =
                QByteArray::fromHex(palettes[row["paletteHash"].toString()].toString().toLatin1());
            for (int i = 0; i < variants.size(); ++i) {
                const auto expected = variants[i].toObject();
                const auto lanes = sprite.materialize(i);
                for (const auto &[name, lane] :
                     std::initializer_list<std::pair<QString, QByteArray>>{
                         {"color", lanes.color},
                         {"alpha", lanes.alpha},
                         {"palette", lanes.palette}}) {
                    if (expected[name].isNull())
                        QVERIFY(lane.isEmpty());
                    else
                        QCOMPARE(sha256(lane), expected[name].toString().toLatin1());
                }
                if (!expected["rgba"].isNull())
                    QCOMPARE(sha256(sprite.frame(i, palette).image.rgba),
                             expected["rgba"].toString().toLatin1());
                else
                    QVERIFY_THROWS_EXCEPTION(Error, sprite.frame(i, palette));
                ++count;
            }
            QCOMPARE(sprite.source(), bytes);
        }
        QCOMPARE(count, fixture["children"].toInt());
        QVERIFY(count > 1000);
        qInfo() << rows.size() << "resource/palette pairs," << count << "child variants matched";
    }
};
QTEST_GUILESS_MAIN(SpriteTests)
#include "sprite_tests.moc"
