#include "engine/actor_art.h"
#include "engine/animation_override_image.h"
#include "engine/animation_overrides.h"
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
    void sourceExportImportAndReset() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QJsonArray entries{QJsonObject{{"classId", 2},
                                           {"actionId", 5},
                                           {"normalSelector", 40},
                                           {"blockedSelector", 41}},
                               QJsonObject{{"classId", 85},
                                           {"actionId", 30},
                                           {"normalSelector", 42},
                                           {"blockedSelector", 43},
                                           {"bodyFlags", 1}}};
            project.importLegacy(QJsonObject{
                {"format", "ob64-patch"},
                {"version", 15},
                {"patches",
                 QJsonObject{{"combatAnimationOverrides", QJsonObject{{"entries", entries}}}}}});
            QVERIFY(project.pendingLegacySections().isEmpty());
            QCOMPARE(project.animationOverrides()->entries(), entries);
            QTemporaryDir work;
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("overrides.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("overrides.lcpack")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(file));
                QCOMPARE(restored.animationOverrides()->entries(), entries);
            }
            auto reset = project;
            auto empty = reset.animationOverrides()->json();
            empty["entries"] = QJsonArray{};
            reset.setAnimationOverrides(empty);
            auto resetRestored = Project::fromRom(rom);
            resetRestored.applyManifest(reset.manifest());
            QVERIFY(resetRestored.animationOverrides()->entries().isEmpty());
            QVERIFY(resetRestored.pendingLegacySections().isEmpty());
            const auto built = rebuild(project, tools, work.path());
            const auto outputRom = Rom::fromBytes(built.image);
            const auto inspected = AnimationOverrideImage::inspect(outputRom);
            QVERIFY2(inspected.recognized(), qPrintable(inspected.reason()));
            QCOMPARE(inspected.version(), 3);
            QCOMPARE(inspected.rows().size(), 2);
            const auto definition = AnimationOverrideImage::definition(),
                       layout = definition["layout"].toObject();
            auto expected = rom.bytes();
            for (const auto &value : definition["sites"].toArray()) {
                const auto site = value.toObject();
                const int at = site["offset"].toInt(), size = site["size"].toInt();
                expected.replace(at, size, built.image.mid(at, size));
            }
            for (const auto &[at, size] : std::array<QPair<int, int>, 2>{
                     {{layout["moduleZ64"].toInt(), layout["moduleSize"].toInt()},
                      {layout["bootstrapZ64"].toInt(), layout["bootstrapSize"].toInt()}}})
                expected.replace(at, size, built.image.mid(at, size));
            Rom::updateChecksum(expected);
            QCOMPARE(built.image, expected);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-override-export-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            QProcess browser;
            browser.start("node", {QFINDTESTDATA("animation-override-readback.cjs"), candidate});
            QVERIFY(browser.waitForFinished(30000));
            QVERIFY2(browser.exitCode() == 0, browser.readAllStandardError().constData());
            const auto compatibility =
                QJsonDocument::fromJson(browser.readAllStandardOutput()).object();
            QCOMPARE(compatibility["kind"].toString(), "owned-v3");
            QVERIFY(!compatibility["advanced"].toBool());
            QCOMPARE(compatibility["count"].toInt(), 2);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadAnimationOverrides();
            QCOMPARE(reopened.animationOverrides()->entries(), entries);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QCOMPARE(reopened.appendClass(2, "Copied action overrides"), 165);
            QCOMPARE(reopened.animationOverrides()->entries().size(), 3);
            const auto cloned = rebuild(reopened, tools, work.path());
            const auto clonedRom = Rom::fromBytes(cloned.image);
            const auto clonedOwner = AnimationOverrideImage::inspect(clonedRom);
            QCOMPARE(clonedOwner.rows().size(), 3);
            QCOMPARE(clonedOwner.rows().last()[0], 165);
            QCOMPARE(clonedOwner.rows().last()[1], 165);
            QVERIFY(ActorArt::loadBody(clonedRom, 165, 165, 0, 0).bodyHandle() > 0);
            auto removal =
                Project::fromRom(Rom::fromBytes(cloned.image + QByteArray(512, char(0x37))));
            removal.loadAnimationOverrides();
            auto removed = removal.animationOverrides()->json();
            removed["entries"] = QJsonArray{};
            removal.setAnimationOverrides(removed);
            const auto final = rebuild(removal, tools, work.path());
            const auto finalRom = Rom::fromBytes(final.image);
            QCOMPARE(AnimationOverrideImage::inspect(finalRom).version(), 0);
            QVERIFY(AnimationOverrideImage::inspect(finalRom).recognized());
            QCOMPARE(final.image.mid(cloned.image.size(), 512), QByteArray(512, char(0x37)));
            QCOMPARE(ClassData::load(finalRom).entries().size(), 166);
            qInfo().noquote() << "Source-built animation override candidate:" << candidate;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void logicalEntriesAndCurrentSelectors() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto classes = ClassData::load(rom);
            auto routes = AnimationRoutes::load(rom);
            QTemporaryDir work;
            const auto fileName = work.filePath("selectors.json");
            QProcess node;
            node.start("node", {QFINDTESTDATA("animation-override-fixture.cjs"), fileName, path});
            QVERIFY(node.waitForFinished(30000));
            QVERIFY2(node.exitCode() == 0, node.readAllStandardError().constData());
            QFile file(fileName);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto reference = QJsonDocument::fromJson(file.readAll()).object();
            const auto candidates = reference["candidates"].toArray();
            QCOMPARE(candidates.size(), 164);
            for (const auto &value : candidates) {
                const auto row = value.toObject();
                QVector<int> expected;
                for (const auto &selector : row["currentSelectors"].toArray())
                    expected << selector.toInt();
                QCOMPARE(AnimationOverrides::selectors(rom, routes, row["id"].toInt()), expected);
                for (const auto &old : row["selectors"].toArray())
                    QVERIFY(expected.contains(old.toInt()));
            }
            auto model = AnimationOverrides::load(rom, classes, routes);
            QVERIFY2(model.available(), qPrintable(model.unavailableReason()));
            QVERIFY(!model.needsRebuild(classes));
            QJsonObject ordinary{
                {"classId", 2}, {"actionId", 5}, {"normalSelector", 40}, {"blockedSelector", 41}};
            QJsonObject exact = ordinary;
            exact["bodyFlags"] = 1;
            exact["normalSelector"] = 255;
            exact["blockedSelector"] = 44;
            model.setEntries(QJsonArray{ordinary, exact}, rom, classes, routes);
            QVERIFY(model.needsRebuild(classes));
            const auto table = model.table(classes);
            QCOMPARE(table.left(16), QByteArray::fromHex("0202050200ff2c010202050000282900"));
            const auto saved = model.json();
            auto restored = AnimationOverrides::load(rom, classes, routes);
            restored.apply(saved, rom, classes, routes);
            QCOMPARE(restored.json(), saved);
            auto bad = ordinary;
            bad["normalSelector"] = 44;
            QVERIFY_THROWS_EXCEPTION(Error,
                                     restored.setEntries(QJsonArray{bad}, rom, classes, routes));
            QCOMPARE(restored.json(), saved);
            QVERIFY_THROWS_EXCEPTION(
                Error, restored.setEntries(QJsonArray{ordinary, ordinary}, rom, classes, routes));
            QJsonArray tooMany;
            for (int i = 0; i < 129; ++i)
                tooMany << ordinary;
            QVERIFY_THROWS_EXCEPTION(Error, restored.setEntries(tooMany, rom, classes, routes));
            auto legacy = AnimationOverrides::load(rom, classes, routes);
            legacy.importLegacy(
                QJsonObject{{"entries", QJsonArray{QJsonObject{
                                            {"classId", 2}, {"actionId", 5}, {"selector", 40}}}}},
                14, rom, classes, routes);
            QCOMPARE(legacy.entries()[0].toObject()["blockedSelector"].toInt(), 40);
            legacy.importLegacy(QJsonObject{{"entries", QJsonArray{ordinary, exact}}}, 15, rom,
                                classes, routes);
            QCOMPARE(legacy.entries(), model.entries());
            QCOMPARE(classes.append(2, "New fighter"), 165);
            routes.cloneClass(2, 165);
            auto newRow = ordinary;
            newRow["classId"] = 165;
            model.setEntries(QJsonArray{newRow}, rom, classes, routes);
            QCOMPARE(model.table(classes).left(8), QByteArray::fromHex("a5a5050000282900"));
            auto asset = classes.entries()[165];
            put8(asset.record, 69, 2);
            classes.set(165, asset);
            QCOMPARE(model.table(classes).left(8), QByteArray::fromHex("a502050000282900"));
            put8(asset.record, 69, 0);
            classes.set(165, asset);
            QCOMPARE(model.table(classes).left(8), QByteArray::fromHex("a5a5050000282900"));
            const auto moduleData = model.moduleData(classes);
            QCOMPARE(u16(moduleData, 12), 1);
            QCOMPARE(moduleData.mid(0x400), model.table(classes));
            QCOMPARE(moduleData.mid(0x20, 32), QByteArray::fromHex(sha256(moduleData.mid(0x400))));
            routes.cloneClass(1, 2);
            QVERIFY(AnimationOverrides::selectors(rom, routes, 2).contains(44));
            restored.setEntries(QJsonArray{bad}, rom, classes, routes);
            for (const auto &value : reference["versions"].toArray()) {
                auto bytes = rom.bytes();
                for (const auto &part : value.toObject()["regions"].toArray()) {
                    const auto region = part.toObject();
                    const auto data = QByteArray::fromHex(region["bytes"].toString().toLatin1());
                    bytes.replace(region["offset"].toInt(), data.size(), data);
                }
                const auto patched = Rom::fromBytes(bytes);
                auto loaded = AnimationOverrides::load(patched, ClassData::load(patched),
                                                       AnimationRoutes::load(patched));
                QVERIFY2(loaded.available(), qPrintable(loaded.unavailableReason()));
                QVERIFY(!loaded.needsRebuild(ClassData::load(patched)));
                QCOMPARE(loaded.entries().size(), 2);
            }
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
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
                    const auto inherited = AnimationOverrideImage::inspect(rebuilt);
                    QVERIFY(inherited.recognized());
                    QCOMPARE(inherited.rows().size(), 3);
                    for (int i = 0; i < 2; ++i)
                        QCOMPARE(inherited.rows()[i], owned.rows()[i]);
                    QCOMPARE(inherited.rows()[2][0], 165);
                    QCOMPARE(inherited.rows()[2][1], 165);
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
                if (owned.version() == 3) {
                    auto advancedBytes = bytes;
                    put8(advancedBytes, module + 0x403, 1);
                    put8(advancedBytes, module + 0x404, 40);
                    updateHash(advancedBytes);
                    const auto advancedRom = Rom::fromBytes(advancedBytes);
                    QVERIFY(AnimationOverrideImage::inspect(advancedRom).recognized());
                    auto advancedProject = Project::fromRom(advancedRom);
                    advancedProject.loadAnimationOverrides();
                    QVERIFY(!advancedProject.animationOverrides()->available());
                    const auto before = advancedProject.manifest();
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             advancedProject.appendClass(2, "Advanced template"));
                    QCOMPARE(advancedProject.manifest(), before);
                    QCOMPARE(advancedProject.appendClass(1, "Unrelated template"), 165);
                }
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
