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
            QCOMPARE(expandedImages, 480);
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
            QVERIFY_THROWS_EXCEPTION(Error, edited.setIndices(5, 26, QByteArray(896, 0)));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(ArmyArtworkTests)
#include "army_artwork_tests.moc"
