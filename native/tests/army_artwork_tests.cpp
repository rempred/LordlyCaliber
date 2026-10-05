#include "engine/army_artwork.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ArmyArtworkTests : public QObject {
    Q_OBJECT
    static QJsonArray reference(const QString &path, const QString &output) {
        QProcess node;
        node.start("node", {QFINDTESTDATA("army-artwork-reference.cjs"), path, output});
        if (!node.waitForFinished(30000) || node.exitCode() != 0)
            throw Error("Army reference failed: " + QString::fromUtf8(node.readAllStandardError()));
        QFile file(output);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Cannot open Army reference output");
        return QJsonDocument::fromJson(file.readAll()).object()["atlases"].toArray();
    }
  private slots:
    void changedClassSizeNeedsValidModels() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadArmyArtwork();
            const auto original = project.classData();
            int selected = -1;
            for (int id = 1; id < original.entries().size(); ++id)
                if (ArmyArtwork::classLane(original.entries()[id]) != 2 &&
                    ArmyArtwork::classModel(original.entries()[id]) >=
                        project.armyArtwork()->sourceCount(4)) {
                    selected = id;
                    break;
                }
            QVERIFY(selected > 0);
            const auto templateId = selected;
            selected = project.appendClass(templateId, "Size validation");
            auto record = project.record(Table::Classes, selected).bytes;
            put8(record, ClassData::unitSizeOffset, 2);
            project.replaceBytes(Table::Classes, selected, record);
            QTemporaryDir work;
            QString reason;
            try {
                rebuild(project, tools, work.path());
            } catch (const Error &e) {
                reason = QString::fromUtf8(e.what());
            }
            QVERIFY2(reason.contains("new unit size selects a missing"), qPrintable(reason));
            auto army = *project.armyArtwork();
            army.setRoute(project.classData(), selected, 2, 16);
            project.setArmyArtwork(army.json());
            const auto result = rebuild(project, tools, work.path());
            const auto exported = ClassData::load(Rom::fromBytes(result.image));
            QCOMPARE(ArmyArtwork::classLane(exported.entries()[selected]), 2);
            QCOMPARE(ArmyArtwork::classModel(exported.entries()[selected]), 16);
            QCOMPARE(exported.entries()[selected].appearance.first(4),
                     original.entries()[templateId].appearance.first(4));
            QCOMPARE(exported.entries()[selected].appearance[5],
                     original.entries()[templateId].appearance[5]);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void dynamicClassRoutesAndPortraitClone() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            const auto original = ArmyArtwork::load(rom);
            auto routeOnly = Project::fromRom(rom);
            routeOnly.loadArmyArtwork();
            auto routed = *routeOnly.armyArtwork();
            routed.setRoute(routeOnly.classData(), 1, 1, 0);
            routeOnly.setArmyArtwork(routed.json());
            QVERIFY(!routed.pixelsModified());
            const auto first = rebuild(routeOnly, tools, work.path());
            QCOMPARE(first.image.mid(ArmyArtwork::descriptorOffset, ArmyArtwork::descriptorSize),
                     rom.bytes().mid(ArmyArtwork::descriptorOffset, ArmyArtwork::descriptorSize));
            const auto firstRom = Rom::fromBytes(first.image);
            QVERIFY(ClassPlacement::read(firstRom));
            QCOMPARE(ArmyArtwork::load(firstRom).decodedPacks(), original.decodedPacks());
            QCOMPARE(ArmyArtwork::classLane(ClassData::load(firstRom).entries()[1]), 1);
            auto project = Project::fromRom(rom);
            project.loadArmyArtwork();
            QCOMPARE(project.appendClass(1, "Independent model"), 165);
            QCOMPARE(project.appendClass(71, "Independent large"), 166);
            auto army = *project.armyArtwork();
            army.setRoute(project.classData(), 165, 0, 56);
            army.setRoute(project.classData(), 166, 2, 63);
            project.setArmyArtwork(army.json());
            QVERIFY_THROWS_EXCEPTION(Error, army.validateRoutes(project.classData()));
            for (int atlas = 0; atlas < 2; ++atlas)
                army.setIndices(atlas, 56, original.indices(atlas, 1));
            for (int atlas = 4; atlas < 6; ++atlas)
                army.setIndices(atlas, 63, original.indices(atlas, 16));
            project.setArmyArtwork(army.json());
            army.validateRoutes(project.classData());
            auto preserved = project.classData();
            auto asset = preserved.entries()[165];
            const auto firstWords = asset.appearance.first(4);
            put8(asset.appearance, 4, u8(asset.appearance, 4) | 0x40);
            put8(asset.appearance, 5, 0xa7);
            preserved.set(165, asset);
            army.applyRoutes(preserved);
            QCOMPARE(preserved.entries()[165].appearance.first(4), firstWords);
            QCOMPARE(u8(preserved.entries()[165].appearance, 4) & 0x40, 0x40);
            QCOMPARE(u8(preserved.entries()[165].appearance, 5), quint8(0xa7));
            const auto packs = army.decodedPacks();
            QCOMPARE(packs[0].size(), 1024 + 57 * 384);
            QCOMPARE(packs[1].size(), 1024 + 57 * 384 + 512);
            QCOMPARE(packs[4].size(), 1024 + 64 * 896);
            QCOMPARE(packs[5].size(), 1024 + 64 * 896);
            project.loadAvatarArtwork();
            auto portraits = *project.avatarArtwork();
            const auto words = portraits.words(project.classData(), 2, 0);
            portraits.setWords(project.classData(), 165, 0, words);
            project.setAvatarArtwork(portraits.json());
            QCOMPARE(project.appendClass(165, "Inherited artwork"), 167);
            QCOMPARE(project.armyArtwork()->routes()[167].model, 56);
            QCOMPARE(project.avatarArtwork()->words(project.classData(), 167, 0), words);
            auto roundtrip = Project::fromRom(rom);
            roundtrip.applyManifest(project.manifest());
            QCOMPARE(roundtrip.armyArtwork()->json(), project.armyArtwork()->json());
            QCOMPARE(roundtrip.avatarArtwork()->json(), project.avatarArtwork()->json());
            const auto built = rebuild(project, tools, work.path());
            const auto actualRom = Rom::fromBytes(built.image);
            const auto actual = ArmyArtwork::load(actualRom);
            const auto classes = ClassData::load(actualRom);
            QCOMPARE(actual.sourceCount(0), 57);
            QCOMPARE(actual.sourceCount(1), 57);
            QCOMPARE(actual.sourceCount(4), 64);
            QCOMPARE(actual.sourceCount(5), 64);
            QCOMPARE(ArmyArtwork::classModel(classes.entries()[165]), 56);
            QCOMPARE(ArmyArtwork::classModel(classes.entries()[166]), 63);
            QCOMPARE(ArmyArtwork::classModel(classes.entries()[167]), 56);
            QCOMPARE(
                AvatarLibrary::load(actualRom).words(u16(classes.entries()[167].appearance, 0)),
                words);
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            auto reopened = Project::fromRom(actualRom);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-army-routes-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            qInfo().noquote() << "Army routes candidate:" << candidate;
            const auto checked = reference(candidate, work.filePath("routed-reference.json"));
            for (int atlas = 0; atlas < 6; ++atlas)
                QCOMPARE(QByteArray::fromBase64(
                             checked[atlas].toObject()["decoded"].toString().toLatin1()),
                         actual.decodedPacks()[atlas]);
            // Maximum-size, high-entropy planes exercise the shared-upper-operand bound.
            auto dense = original;
            quint32 random = 0x93af04d9;
            for (int atlas = 4; atlas < 6; ++atlas)
                for (int model = 0; model < 64; ++model) {
                    QByteArray pixels(896, 0);
                    for (auto &pixel : pixels) {
                        random = random * 1664525U + 1013904223U;
                        pixel = char(random >> 24);
                    }
                    dense.setIndices(atlas, model, pixels);
                }
            const auto large = dense.decodedPacks();
            for (int atlas = 4; atlas < 6; ++atlas)
                QVERIFY(encodeLzss(large[atlas]).size() + 8 < 65536);
            auto bad = project.classData();
            asset = bad.entries()[165];
            put8(asset.record, ClassData::unitSizeOffset, 2);
            bad.set(165, asset);
            QVERIFY_THROWS_EXCEPTION(Error, project.armyArtwork()->validateRoutes(bad));
            QVERIFY_THROWS_EXCEPTION(Error, army.setRoute(project.classData(), 165, 2, 10));
            QVERIFY_THROWS_EXCEPTION(Error, army.setRoute(project.classData(), 165, 0, 64));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void pairedAtlasExportAndGrowth() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            const auto original = ArmyArtwork::load(rom);
            auto edited = original;
            QTemporaryDir work;
            const auto baseline = reference(path, work.filePath("baseline.json"));
            QCOMPARE(baseline.size(), 6);
            int images = 0;
            for (int atlas = 0; atlas < 6; ++atlas) {
                const auto row = baseline[atlas].toObject();
                QCOMPARE(original.sourceCount(atlas), ArmyArtwork::specs()[atlas].retailCount);
                for (const auto &value : row["images"].toArray()) {
                    const auto entry = value.toObject();
                    const auto model = entry["model"].toInt(), palette = entry["palette"].toInt();
                    QCOMPARE(original.indices(atlas, model).toBase64(),
                             entry["indices"].toString().toLatin1());
                    QCOMPARE(original.words(atlas, model, palette).toBase64(),
                             entry["words"].toString().toLatin1());
                    const auto image = original.image(atlas, model, palette);
                    QByteArray rgba;
                    for (int y = 0; y < image.height(); ++y)
                        rgba.append(reinterpret_cast<const char *>(image.constScanLine(y)),
                                    image.width() * 4);
                    QCOMPARE(rgba.toBase64(), entry["rgba"].toString().toLatin1());
                    QCOMPARE(original.mapImage(atlas, palette, image, 128,
                                               original.indices(atlas, model)),
                             original.indices(atlas, model));
                    ++images;
                }
                edited.setIndices(atlas, 0, original.indices(atlas, 1));
            }
            QCOMPARE(images, 450);
            QVERIFY(!original.present(3, 24));
            QVERIFY(!original.present(5, 25));
            edited.setIndices(3, 24, original.indices(2, 24));
            edited.setIndices(5, 25, original.indices(4, 25));
            QVERIFY(edited.present(3, 24));
            QVERIFY(edited.present(5, 25));
            QFile legacyFile(work.filePath("baseline.json"));
            QVERIFY(legacyFile.open(QIODevice::ReadOnly));
            const auto sourceReference = QJsonDocument::fromJson(legacyFile.readAll()).object();
            const auto legacyRows = sourceReference["legacy"].toObject();
            const auto classData = ClassData::load(rom);
            for (const auto &value : sourceReference["classRoutes"].toArray()) {
                const auto row = value.toObject();
                const auto id = row["classId"].toInt();
                QCOMPARE(
                    ArmyArtwork::classLane(classData.entries()[id]),
                    (QStringList{"ordinary", "special", "large"}).indexOf(row["lane"].toString()));
                QCOMPARE(ArmyArtwork::classModel(classData.entries()[id]), row["modelId"].toInt());
                QVERIFY(original
                            .consumers(classData,
                                       ArmyArtwork::classLane(classData.entries()[id]) * 2,
                                       ArmyArtwork::classModel(classData.entries()[id]))
                            .contains(id));
            }
            QCOMPARE(legacyRows.size(), 8);
            QJsonObject importedArt{{"schemaVersion", 5}, {"armySprites", legacyRows}};
            auto legacy = Project::fromRom(rom);
            legacy.importLegacy({{"format", "ob64-patch"},
                                 {"version", 36},
                                 {"patches", QJsonObject{{"art", importedArt}}}});
            QCOMPARE(legacy.armyArtwork()->json(), edited.json());
            QVERIFY(legacy.pendingLegacySections().isEmpty());
            legacy.setArmyArtwork(original.json());
            auto reset = Project::fromRom(rom);
            reset.applyManifest(legacy.manifest());
            QVERIFY(!reset.armyArtwork()->modified());
            const auto decoded = edited.decodedPacks();
            for (int atlas = 0; atlas < 6; ++atlas) {
                QCOMPARE(decoded[atlas].first(1024), original.decodedPacks()[atlas].first(1024));
                for (int model = 1; model < original.sourceCount(atlas); ++model)
                    QCOMPARE(edited.indices(atlas, model), original.indices(atlas, model));
            }
            QCOMPARE(decoded[1].last(512), original.decodedPacks()[1].last(512));
            auto project = Project::fromRom(rom);
            project.setArmyArtwork(edited.json());
            auto persisted = Project::fromRom(rom);
            persisted.applyManifest(project.manifest());
            QCOMPARE(persisted.armyArtwork()->json(), edited.json());
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("archive.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("archive.lcpack")}) {
                auto saved = Project::fromRom(rom);
                saved.applyManifest(Project::readProject(file));
                QCOMPARE(saved.armyArtwork()->json(), edited.json());
            }
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-army-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            qInfo().noquote() << "Army candidate:" << candidate;
            const auto loaded = Rom::open(candidate);
            const auto actual = ArmyArtwork::load(loaded);
            QCOMPARE(actual.decodedPacks(), decoded);
            const auto expected = reference(candidate, work.filePath("exported.json"));
            int expandedImages = 0;
            for (int atlas = 0; atlas < 6; ++atlas) {
                const auto row = expected[atlas].toObject();
                QCOMPARE(QByteArray::fromBase64(row["decoded"].toString().toLatin1()),
                         decoded[atlas]);
                if (atlas % 2)
                    QCOMPARE(row["key"].toInteger() >> 16,
                             expected[atlas - 1].toObject()["key"].toInteger() >> 16);
                expandedImages += row["images"].toArray().size();
            }
            QCOMPARE(expandedImages, 454);
            auto preserved = rom.bytes();
            preserved.replace(
                ArmyArtwork::descriptorOffset, ArmyArtwork::descriptorSize,
                built.image.mid(ArmyArtwork::descriptorOffset, ArmyArtwork::descriptorSize));
            for (const auto &spec : ArmyArtwork::specs())
                for (const auto &[hi, lo] : spec.sites) {
                    put32(preserved, hi, u32(built.image, hi));
                    put32(preserved, lo, u32(built.image, lo));
                }
            Rom::updateChecksum(preserved);
            QCOMPARE(built.image.first(rom.bytes().size()), preserved);
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            QCOMPARE(rebuild(project, tools, work.path()).image, built.image);
            auto reopened = Project::fromRom(loaded);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QCOMPARE(reopened.appendClass(1, "Army class"), 165);
            const auto withClass = Rom::fromBytes(rebuild(reopened, tools, work.path()).image);
            QCOMPARE(ArmyArtwork::load(withClass).decodedPacks(), decoded);
            auto later = Project::fromRom(withClass);
            later.loadArmyArtwork();
            auto laterArt = *later.armyArtwork();
            laterArt.setIndices(0, 2, original.indices(0, 3));
            QByteArray noisy(896, 0);
            quint32 random = 0x74123;
            for (auto &pixel : noisy) {
                random = random * 1664525U + 1013904223U;
                pixel = char(random >> 24);
            }
            laterArt.setIndices(5, 25, noisy);
            later.setArmyArtwork(laterArt.json());
            later.appendClass(2, "Second Army class");
            const auto combined = Rom::fromBytes(rebuild(later, tools, work.path()).image);
            QCOMPARE(ArmyArtwork::load(combined).indices(0, 2), original.indices(0, 3));
            QCOMPARE(ClassData::load(combined).entries().size(), 167);
            QCOMPARE(ArmyArtwork::load(combined).decodedPacks(), laterArt.decodedPacks());
            const auto oldOffset = u32(loaded.bytes(), ArmyArtwork::descriptorOffset + 12),
                       oldSize = u32(loaded.bytes(), ArmyArtwork::descriptorOffset + 16);
            QVERIFY(u32(combined.bytes(), ArmyArtwork::descriptorOffset + 16) > oldSize);
            QVERIFY(u32(combined.bytes(), ArmyArtwork::descriptorOffset + 12) != oldOffset);
            QCOMPARE(combined.bytes().mid(oldOffset, oldSize), QByteArray(oldSize, char(-1)));
            auto tailProject = Project::fromRom(loaded);
            tailProject.loadArmyArtwork();
            auto tailArt = *tailProject.armyArtwork();
            tailArt.setIndices(5, 25, noisy);
            tailProject.setArmyArtwork(tailArt.json());
            const auto tail = rebuild(tailProject, tools, work.path());
            QCOMPARE(u32(tail.image, ArmyArtwork::descriptorOffset + 12), oldOffset);
            QVERIFY(u32(tail.image, ArmyArtwork::descriptorOffset + 16) > oldSize);
            QCOMPARE(ArmyArtwork::load(Rom::fromBytes(tail.image)).decodedPacks(),
                     tailArt.decodedPacks());
            auto classesFirst = Project::fromRom(rom);
            classesFirst.appendClass(1, "First class");
            classesFirst =
                Project::fromRom(Rom::fromBytes(rebuild(classesFirst, tools, work.path()).image));
            classesFirst.setArmyArtwork(edited.json());
            QCOMPARE(
                ArmyArtwork::load(Rom::fromBytes(rebuild(classesFirst, tools, work.path()).image))
                    .decodedPacks(),
                decoded);
            auto bad = built.image;
            bad[u32(bad, ArmyArtwork::descriptorOffset + 12) + 10] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, ArmyArtwork::load(Rom::fromBytes(bad)));
            for (const auto &spec : ArmyArtwork::specs())
                for (const auto &[hi, lo] : spec.sites) {
                    bad = rom.bytes();
                    bad[lo + 3] ^= 2;
                    QVERIFY_THROWS_EXCEPTION(Error, ArmyArtwork::load(Rom::fromBytes(bad)));
                }
            bad = rom.bytes();
            bad[ArmyArtwork::descriptorOffset] = 0;
            auto held = ArmyArtwork::load(Rom::fromBytes(bad));
            held.setIndices(0, 0, original.indices(0, 1));
            QVERIFY_THROWS_EXCEPTION(Error, held.rebuild(Rom::fromBytes(bad), held.decodedPacks()));
            bad = rom.bytes();
            put32(bad, 0x83800, 0x3c0501db);
            put32(bad, 0x83810, 0x34a5b2ac);
            QVERIFY_THROWS_EXCEPTION(Error, ArmyArtwork::load(Rom::fromBytes(bad)));
            QVERIFY_THROWS_EXCEPTION(Error, original.indices(0, -1));
            QVERIFY_THROWS_EXCEPTION(Error, edited.setIndices(5, 64, QByteArray(896, 0)));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(ArmyArtworkTests)
#include "army_artwork_tests.moc"
