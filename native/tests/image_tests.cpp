#include "engine/archive.h"
#include "engine/image.h"
#include "engine/scene_images.h"
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ImageTests : public QObject {
    Q_OBJECT
  private slots:
    void imageLaunchAgainstMips() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QTemporaryDir work;QProcess reference;
        reference.start("node",{QFINDTESTDATA("image-launch-mips.cjs"),path,work.filePath("launch.json")});
        QVERIFY(reference.waitForFinished(60000));QVERIFY2(reference.exitCode()==0,reference.readAllStandardError().constData());
        QFile file(work.filePath("launch.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(data["cases"].toArray().size(),127);
        const auto rom=Rom::open(path);const auto directors=Directors::load(rom);
        for(const auto &value:data["cases"].toArray()) {
            const auto row=value.toObject();QVector<uint32_t> words;
            for(const auto &word:row["words"].toArray())words<<quint32(word.toInteger());
            LordlySceneLaunch launch{0,-1,-1,0};QVERIFY(lordly_scene_launch_scan(words.constData(),int(words.size()),&launch));
            const auto expected=QByteArray::fromHex(row["record"].toString().toLatin1());
            QCOMPARE(launch.resource_class,qint16(u16(expected,0)));QCOMPARE(launch.environment,qint16(u16(expected,2)));
            QCOMPARE(launch.image_row,qint16(u16(expected,4)));QCOMPARE(launch.capture,qint16(u16(expected,6)));
            const auto key=quint32(row["key"].toInteger());
            if(key) {
                QVERIFY(directors.entries().contains(key));
                const auto actual=SceneImages::launch(directors.entries()[key].program);
                QCOMPARE(actual.resource_class,launch.resource_class);QCOMPARE(actual.environment,launch.environment);
                QCOMPARE(actual.image_row,launch.image_row);QCOMPARE(actual.capture,launch.capture);
            }
        }
        QCOMPARE(data["rows"].toArray().size(),69);
        for(const auto &value:data["rows"].toArray()) {
            const auto row=value.toObject();
            if(row["image"].isNull())QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,row["row"].toInt()));
            else QCOMPARE(SceneImages::presentationImage(rom,row["row"].toInt()),row["image"].toString());
        }
        auto changed=rom.bytes();put8(changed,0x654a0+9+3,40);
        QCOMPARE(SceneImages::presentationImage(Rom::fromBytes(changed),1),QString("archive:154"));
        auto pointerChange=rom.bytes();const auto root=0x18bd022+0x594280+4;
        put32(pointerChange,root+5*4,u32(pointerChange,root+36*4));
        QCOMPARE(SceneImages::presentationImage(Rom::fromBytes(pointerChange),1),QString("archive:154"));
        QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,-1));
        QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,69));
        QByteArray script(16,0);put32(script,0,0x80000007);put32(script,4,65538);
        put32(script,8,0x80000001);put32(script,12,0xff000004);
        const auto edited=SceneImages::launch(DirectorProgram::decode(script));QCOMPARE(edited.image_row,2);
        put32(script,4,65535);QCOMPARE(SceneImages::launch(DirectorProgram::decode(script)).image_row,-1);
        LordlySceneLaunch preserved{4,5,6,7};const uint32_t invalid=0x80000007;
        QVERIFY(!lordly_scene_launch_scan(&invalid,1,&preserved));QCOMPARE(preserved.image_row,6);QCOMPARE(preserved.resource_class,4);
    }
    void indexedPaddingAndBounds() {
        // 3x2 CI4, 8-byte storage rows. Padding deliberately selects another
        // palette entry; expected visible pixels are specified independently.
        auto bytes = QByteArray::fromHex("3634020000030002") + QByteArray(16, char(-18)) +
                     QByteArray(32, 0);
        put8(bytes, 8, 0x12);
        put8(bytes, 9, 0x3e);
        put8(bytes, 16, 0x32);
        put8(bytes, 17, 0x1e);
        put16(bytes, 26, 0xf801);
        put16(bytes, 28, 0x07c1);
        put16(bytes, 30, 0x003f);
        const auto decoded = decodeN64Image(bytes);
        QCOMPARE(decoded.width, 3);
        QCOMPARE(decoded.height, 2);
        QCOMPARE(decoded.rgba,
                 QByteArray::fromHex("ff0000ff00ff00ff0000ffff0000ffff00ff00ffff0000ff"));
        QCOMPARE(decodeN64Image(bytes + QByteArray("retained tail")).rgba, decoded.rgba);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bytes.first(bytes.size() - 1)));
        auto bad = bytes;
        put16(bad, 4, 0);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        bad = bytes;
        put16(bad, 4, 8192);
        put16(bad, 6, 8192);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        bad = bytes;
        put8(bad, 2, 7);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bytes.first(7)));
    }
    void browserPixelAgreement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for real image comparisons");
        QTemporaryDir work;
        const auto fixture = work.filePath("images.json");
        QProcess generator;
        generator.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("image-fixture.cjs"),
             path, fixture});
        QVERIFY(generator.waitForFinished(30000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(fixture);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 104);
        QCOMPARE(data["types"].toObject().size(), 6);
        const auto rom = Rom::open(path);
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const auto source = LhaMember::read(rom.bytes(), row["offset"].toInt()).decode(rom);
            const auto image = decodeN64Image(source);
            QCOMPARE(image.width, row["width"].toInt());
            QCOMPARE(image.height, row["height"].toInt());
            QCOMPARE(sha256(image.rgba), row["sha256"].toString().toLatin1());
        }
        const auto image = decodeN64Image(LhaMember::find(rom, "s_map04.n64").decode(rom));
        QImage view(reinterpret_cast<const uchar *>(image.rgba.constData()), image.width,
                    image.height, image.width * 4, QImage::Format_RGBA8888);
        QVERIFY(view.scaled(image.width * 3, image.height * 3, Qt::KeepAspectRatio,
                            Qt::FastTransformation)
                    .save("native-map-image.png"));
    }
};
QTEST_GUILESS_MAIN(ImageTests)
#include "image_tests.moc"
