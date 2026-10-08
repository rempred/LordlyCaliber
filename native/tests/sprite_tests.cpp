#include "engine/actor_art.h"
#include "engine/sprite_object.h"
#include "game/director/actor_state.h"
#include "game/images/sepia.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SpriteTests : public QObject {
    Q_OBJECT
  private slots:
    void rgba32ChildOffsets() {
        // Accepted 00203414 assigns zero secondary bytes to RGBA32. A set
        // secondary flag must not consume a nonexistent plane or the next header.
        QByteArray bytes = QByteArray::fromHex("5554020200010001");
        bytes += QByteArray::fromHex("555400030001000112345678000000000000000000000000");
        bytes += QByteArray::fromHex("5554010100010001abcdefef000000000000000000000000");
        const auto sprite = SpriteObject::decode(bytes);
        QCOMPARE(sprite.children().size(), 2);
        QCOMPARE(sprite.children()[1].colorOffset, qsizetype(40));
        QVERIFY(sprite.materialize(0).alpha.isEmpty());
        QCOMPARE(sprite.frame(0).image.rgba, QByteArray::fromHex("12345678"));
        QCOMPARE(sprite.frame(1).image.rgba, QByteArray::fromHex("abcdefef"));
        auto pixels = sprite.pixels(0);
        pixels.color = QByteArray::fromHex("10203040");
        const auto edited = SpriteObject::decode(sprite.rebuild({{0, pixels}}));
        QCOMPARE(edited.frame(0).image.rgba, pixels.color);
        QCOMPARE(edited.frame(1).image.rgba, sprite.frame(1).image.rgba);
        QCOMPARE(u8(edited.source(), 11), quint8(3)); // Preserve the source flags.
        QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::decode(bytes.chopped(1)));
    }
    void repeatedSepiaKeepsAlpha() {
        const auto bytes=QByteArray::fromHex("55540101000200015554000300020001f800003f000000008f00000000000000");
        const auto sprite=SpriteObject::decode(bytes);
        auto red=quint16(0xf800),blue=quint16(0x003f);
        for(int count=0;count<=64;++count) {
            const auto frame=sprite.frame(0,{},count);
            const quint16 colors[]{red,blue};
            for(int x=0;x<2;++x) for(int c=0;c<3;++c)
                QCOMPARE(u8(frame.image.rgba,x*4+c),quint8((((colors[x]>>(11-c*5))&31)*255+15)/31));
            QCOMPARE(u8(frame.image.rgba,3),136); QCOMPARE(u8(frame.image.rgba,7),255);
            red=lordly_sepia_rgba16(red); blue=lordly_sepia_rgba16(blue);
        }
        quint32 white=0xffffffffU; quint16 shortWhite=0xffff;
        for(int i=0;i<64;++i) { white=lordly_sepia_rgba32(white); shortWhite=lordly_sepia_rgba16(shortWhite); }
        QCOMPARE(white,255U); QCOMPARE(shortWhite,quint16(1));
        QVERIFY_THROWS_EXCEPTION(Error,sprite.frame(0,{},65));
    }
    void separateTextureAlphaAndOrientation() {
        // Two direct RGBA16 pixels have alpha0/1, independently of mask8/15.
        const auto bytes = QByteArray::fromHex(
            "55540101000200015554000300020001f800003f000000008f00000000000000");
        const auto frame = SpriteObject::decode(bytes).frame(0);
        QCOMPARE(frame.image.rgba, QByteArray::fromHex("ff0000880000ffff"));
        QCOMPARE(frame.colorAlpha, QByteArray::fromHex("00ff"));
        QVERIFY(frame.dualTexture);
        const auto flipped = frame.oriented(2, 1, true, false);
        QCOMPARE(flipped.image.rgba, QByteArray::fromHex("0000ffffff000088"));
        QCOMPARE(flipped.colorAlpha, QByteArray::fromHex("ff00"));
        auto padded = frame;
        padded.image = {3, 2, QByteArray::fromHex(
            "ff0000880000ffff12345600abcdef002233440099887700")};
        padded.colorAlpha = QByteArray::fromHex("00ff11aa55ee");
        const auto cropped = padded.oriented(2, 2, true, true);
        QCOMPARE(cropped.image.rgba, QByteArray::fromHex("22334400abcdef000000ffffff000088"));
        QCOMPARE(cropped.colorAlpha, QByteArray::fromHex("55aaff00"));
        QVERIFY_THROWS_EXCEPTION(Error, padded.oriented(1, 2, false, false));
        padded.colorAlpha.clear();
        QVERIFY_THROWS_EXCEPTION(Error, padded.oriented(2, 2, false, false));
        QCOMPARE(frame.colorAlpha, QByteArray::fromHex("00ff"));
    }
    void animationResolverAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-state-mips.cjs"),
             path, work.filePath("states.json")});
        QVERIFY(reference.waitForFinished(60000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("states.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 4620);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> banks;
        for (const auto &v : rows) {
            const auto row = v.toObject();
            const int bank = row["bank"].toInt();
            if (!banks.contains(bank))
                banks.insert(bank, ActorArt::load(rom, bank));
            QCOMPARE(banks[bank].resolve(row["key"].toInt(), row["facing"].toInt(),
                                         row["option"].toInt()),
                     row["result"].toInt());
        }
        const short invalid[] = {3, 0, 123};
        int output = 42;
        QCOMPARE(lordly_actor_resolve_state(invalid, 3, 1, 1, 0, 0, &output), 1);
        QCOMPARE(output, 42);
        const short unterminated[] = {2, 0, 123};
        QCOMPARE(lordly_actor_resolve_state(unterminated, 3, 1, 1, 0, 0, &output), 1);
        QCOMPARE(lordly_actor_resolve_state(unterminated, 3, 0, 1, 0, 0, &output), 1);
        QCOMPARE(output, 42);
    }
    void actorBanksPosesAndFrames() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-art-fixture.cjs"),
             path, work.filePath("actors.json")});
        QVERIFY(reference.waitForFinished(120000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("actors.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto banks = fixture["banks"].toArray();
        QCOMPARE(banks.size(), 68);
        const auto rom = Rom::open(path);
        int programCount = 0, frameCount = 0, layerCount = 0;
        for (const auto &value : banks) {
            const auto row = value.toObject();
            const auto bank = ActorArt::load(rom, row["bank"].toInt());
            QCOMPARE(bank.descriptorKey(), quint32(row["descriptorKey"].toInteger()));
            const auto keys = row["artKeys"].toArray();
            QCOMPARE(bank.artKeys().size(), keys.size());
            for (int i = 0; i < keys.size(); ++i)
                QCOMPARE(bank.artKeys()[i], quint32(keys[i].toInteger()));
            const auto poses = row["poses"].toArray();
            QCOMPARE(bank.poses().size(), poses.size());
            for (int i = 0; i < poses.size(); ++i) {
                const auto expected = poses[i].toObject();
                const auto &pose = bank.poses()[i];
                QCOMPARE(sha256(pose.source), expected["sha256"].toString().toLatin1());
                const auto records = expected["records"].toArray();
                QCOMPARE(pose.records.size(), records.size());
                for (int j = 0; j < records.size(); ++j) {
                    const auto record = records[j].toArray();
                    QCOMPARE(pose.records[j].opcode, record[0].toInt());
                    QCOMPARE(pose.records[j].operands.size(), record.size() - 1);
                    for (int k = 1; k < record.size(); ++k)
                        QCOMPARE(u8(pose.records[j].operands, k - 1), record[k].toInt());
                }
                ++programCount;
            }
            for (const auto &f : row["frames"].toArray()) {
                const auto frame = f.toObject();
                const auto actual = bank.frame(rom, frame["token"].toInt(), 0);
                const auto expected = frame["layers"].toArray();
                QCOMPARE(actual.size(), expected.size());
                for (int i = 0; i < actual.size(); ++i) {
                    const auto layer = expected[i].toObject();
                    QCOMPARE(actual[i].metadata.origin,
                             QPoint(layer["x"].toInt(), layer["y"].toInt()));
                    QCOMPARE(actual[i].metadata.flags, layer["flags"].toInt());
                    QCOMPARE(actual[i].metadata.scaleX / 1024., layer["scaleX"].toDouble());
                    QCOMPARE(actual[i].metadata.scaleY / 1024., layer["scaleY"].toDouble());
                    QCOMPARE(actual[i].pixels.image.width, layer["width"].toInt());
                    QCOMPARE(actual[i].pixels.image.height, layer["height"].toInt());
                    QCOMPARE(sha256(actual[i].pixels.image.rgba),
                             layer["sha256"].toString().toLatin1());
                    ++layerCount;
                }
                ++frameCount;
            }
        }
        QCOMPARE(programCount, fixture["programs"].toInt());
        QCOMPARE(frameCount, fixture["frames"].toInt());
        QCOMPARE(layerCount, fixture["layers"].toInt());
        auto changed = rom.bytes();
        constexpr int root = 0x1633be2;
        put32(changed, root, u32(changed, root + 4));
        QCOMPARE(ActorArt::load(Rom::fromBytes(changed), 1).descriptorKey(),
                 ActorArt::load(rom, 2).descriptorKey());
        QVERIFY_THROWS_EXCEPTION(Error, ActorArt::load(rom, 0));
        QVERIFY_THROWS_EXCEPTION(Error, ActorArt::load(rom, 69));
        // Native rendering artifact for the documented Bank 30 / state 12 walk.
        const auto magnus = ActorArt::load(rom, 30);
        QVector<QVector<ActorFrameLayer>> walk;
        QRect bounds;
        for (const auto &record : magnus.poses()[12].records)
            if (record.opcode == 1) {
                auto layers = magnus.frame(rom, u8(record.operands, 0), 0);
                for (const auto &layer : layers)
                    bounds = bounds.united(QRect(
                        layer.metadata.origin, QSize(layer.metadata.width, layer.metadata.height)));
                walk << std::move(layers);
            }
        QCOMPARE(walk.size(), 6);
        QImage contact((bounds.width() + 4) * int(walk.size()) * 4, bounds.height() * 4,
                       QImage::Format_RGBA8888);
        contact.fill(Qt::transparent);
        {
            QPainter painter(&contact);
            painter.scale(4, 4);
            for (int i = 0; i < walk.size(); ++i)
                for (const auto &layer : walk[i]) {
                    const auto &r = layer.pixels.image;
                    QImage image(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                 r.height, r.width * 4, QImage::Format_RGBA8888);
                    painter.drawImage(layer.metadata.origin - bounds.topLeft() +
                                          QPoint(i * (bounds.width() + 4), 0),
                                      image);
                }
        }
        QVERIFY(contact.save("native-actor-walk.png"));
        qInfo() << programCount << "pose programs," << frameCount << "frames," << layerCount
                << "layers matched";
    }
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
