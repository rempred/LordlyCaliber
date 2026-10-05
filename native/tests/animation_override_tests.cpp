#include "engine/animation_override_image.h"
#include "engine/class_data.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AnimationOverrideTests : public QObject {
    Q_OBJECT
  private slots:
    void historicalOwnershipAndClassReaders() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto retail = AnimationOverrideImage::inspect(rom);
            QVERIFY2(retail.recognized(), qPrintable(retail.reason()));
            QCOMPARE(retail.version(), 0);
            QVERIFY(retail.rows().isEmpty());
            const auto classes = ClassData::load(rom);
            QTemporaryDir work;
            const auto fileName = work.filePath("versions.json");
            QProcess node;
            node.start("node", {QFINDTESTDATA("animation-override-fixture.cjs"), fileName});
            QVERIFY(node.waitForFinished(30000));
            QVERIFY2(node.exitCode() == 0, node.readAllStandardError().constData());
            QFile file(fileName);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto versions =
                QJsonDocument::fromJson(file.readAll()).object()["versions"].toArray();
            QCOMPARE(versions.size(), 3);
            const auto definition = AnimationOverrideImage::definition();
            const auto layout = definition["layout"].toObject();
            const int module = layout["moduleZ64"].toInt();
            auto updateHash = [&](QByteArray &bytes) {
                bytes.replace(module + 0x20, 32,
                              QByteArray::fromHex(sha256(bytes.mid(module + 0x400, 1024))));
            };
            for (const auto &value : versions) {
                const auto version = value.toObject();
                auto bytes = rom.bytes();
                for (const auto &part : version["regions"].toArray()) {
                    const auto region = part.toObject();
                    const auto data = QByteArray::fromHex(region["bytes"].toString().toLatin1());
                    bytes.replace(region["offset"].toInt(), data.size(), data);
                }
                const auto ownedRom = Rom::fromBytes(bytes);
                const auto owned = AnimationOverrideImage::inspect(ownedRom);
                QVERIFY2(owned.recognized(), qPrintable(owned.reason()));
                QCOMPARE(owned.version(), version["version"].toInt());
                QCOMPARE(owned.rows().size(), 2);
                QCOMPARE(owned.rows()[0][5], 40);
                QCOMPARE(owned.rows()[0][6], owned.version() == 1 ? 40 : 41);
                QCOMPARE(owned.rows()[1][7], owned.version() == 3 ? 1 : 0);
                QVERIFY(owned.identity() != retail.identity());
                owned.requireUnchanged(ownedRom);
                // A valid selector-return hook must not invalidate its class-data owner.
                const auto reopened = ClassData::load(ownedRom);
                QCOMPARE(reopened.entries().size(), classes.entries().size());
                for (int id = 0; id < classes.entries().size(); ++id)
                    QCOMPARE(reopened.entries()[id].record, classes.entries()[id].record);
                if (owned.version() == 3 &&
                    !qEnvironmentVariable("LORDLY_N64_TOOLCHAIN").isEmpty()) {
                    auto project = Project::fromRom(ownedRom);
                    QCOMPARE(project.appendClass(2, "Override coexistence"), 165);
                    const auto built =
                        rebuild(project, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), work.path());
                    const auto rebuilt = Rom::fromBytes(built.image);
                    QCOMPARE(ClassData::load(rebuilt).entries().size(), 166);
                    owned.requireUnchanged(rebuilt);
                    QVERIFY(AnimationOverrideImage::inspect(rebuilt).recognized());
                    QTemporaryDir candidates(QFileInfo(path).dir().filePath(
                        "Test ROMs/native-animation-override-XXXXXX"));
                    QVERIFY(candidates.isValid());
                    candidates.setAutoRemove(false);
                    const auto output = candidates.filePath(QFileInfo(path).fileName());
                    exportRom(project, built, output);
                    QCOMPARE(ClassData::load(Rom::open(output)).entries().size(), 166);
                    qInfo().noquote() << "Override/class coexistence candidate:" << output;
                }
                for (const auto &siteValue : definition["sites"].toArray()) {
                    const auto site = siteValue.toObject();
                    const int start = site["offset"].toInt(), size = site["size"].toInt();
                    QCOMPARE(AnimationOverrideImage::canonicalOwner(bytes.mid(start, size), start),
                             rom.bytes().mid(start, size));
                }
                auto bad = bytes;
                bad[module + 0x405] ^= 1;
                QVERIFY(!AnimationOverrideImage::inspect(Rom::fromBytes(bad)).recognized());
                QVERIFY_THROWS_EXCEPTION(Error, owned.requireUnchanged(Rom::fromBytes(bad)));
                bad = bytes;
                bad[module + 0x201] ^= 1;
                QVERIFY(!AnimationOverrideImage::inspect(Rom::fromBytes(bad)).recognized());
                bad = bytes;
                bad.replace(module + 0x408, 8, bad.mid(module + 0x400, 8));
                updateHash(bad);
                QVERIFY(AnimationOverrideImage::inspect(Rom::fromBytes(bad))
                            .reason()
                            .contains("Duplicate"));
                bad = bytes;
                bad[module + 0x410] = 1;
                updateHash(bad);
                QVERIFY(AnimationOverrideImage::inspect(Rom::fromBytes(bad))
                            .reason()
                            .contains("undeclared"));
                bad = bytes;
                bad[module + 0x407] = 4;
                updateHash(bad);
                QVERIFY(AnimationOverrideImage::inspect(Rom::fromBytes(bad))
                            .reason()
                            .contains("Malformed"));
                bad = bytes;
                bad[layout["selectorReturnZ64"].toInt()] ^= 1;
                QVERIFY(!AnimationOverrideImage::inspect(Rom::fromBytes(bad)).recognized());
            }
            auto occupied = rom.bytes();
            occupied[module] = 0;
            QVERIFY(!AnimationOverrideImage::inspect(Rom::fromBytes(occupied)).recognized());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(AnimationOverrideTests)
#include "animation_override_tests.moc"
