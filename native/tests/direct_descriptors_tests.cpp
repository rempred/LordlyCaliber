#include "engine/actor_art.h"
#include "engine/combat_artwork.h"
#include "engine/direct_descriptors.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class DirectDescriptorTests : public QObject {
    Q_OBJECT
  private slots:
    void expandedDirectoryAndClassExport() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            auto root = DirectDescriptors::load(rom);
            QCOMPARE(root.keys().size(), 208);
            const auto routes = AnimationRoutes::load(rom);
            const auto oldHandle = routes.row(2)[0];
            const auto key = root.keys()[(oldHandle & 4095) - 1];
            const auto baseline = root.decoded();
            QCOMPARE(root.rebuild(rom, baseline), rom.bytes());
            while (root.keys().size() < 257)
                root.append(key);
            const auto intended = root.decoded();
            QTemporaryDir work;
            Compiler compiler(tools);
            const auto compiled = compiler.compileData(intended, work.path(), "direct_descriptors");
            auto image = root.rebuild(rom, compiled);
            Rom::updateChecksum(image);
            const auto expanded = Rom::fromBytes(image);
            QCOMPARE(DirectDescriptors::directory(expanded), intended);
            QCOMPARE(intended.left(baseline.size()), baseline);
            const auto offset = u32(image, DirectDescriptors::descriptorOffset + 12),
                       size = u32(image, DirectDescriptors::descriptorOffset + 16);
            auto conserved = image.left(rom.bytes().size());
            conserved.replace(DirectDescriptors::descriptorOffset,
                              DirectDescriptors::descriptorSize,
                              rom.bytes().mid(DirectDescriptors::descriptorOffset,
                                              DirectDescriptors::descriptorSize));
            for (const auto &[hi, lo] : DirectDescriptors::sites()) {
                conserved.replace(hi, 4, rom.bytes().mid(hi, 4));
                conserved.replace(lo, 4, rom.bytes().mid(lo, 4));
            }
            conserved.replace(0x10, 8, rom.bytes().mid(0x10, 8));
            QCOMPARE(conserved, rom.bytes());
            QCOMPARE(ActorArt::loadDirect(expanded, 257).descriptorKey(), key);
            QVERIFY(CombatArtwork::load(expanded).bindings().first().descriptorKey > 0);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-descriptor-directory-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(Project::fromRom(rom), {image, {}}, output);
            QFile fixture(work.filePath("directory.json"));
            QVERIFY(fixture.open(QIODevice::WriteOnly));
            fixture.write(
                QJsonDocument(QJsonObject{{"rom", output},
                                          {"directory", QString::fromLatin1(intended.toHex())},
                                          {"key", double(DirectDescriptors::resourceKey(expanded))},
                                          {"count", root.keys().size()}})
                    .toJson());
            fixture.close();
            QProcess reference;
            reference.start("node",
                            {QFINDTESTDATA("direct-descriptors-mips.cjs"), fixture.fileName()});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            qInfo().noquote() << reference.readAllStandardOutput();
            auto project = Project::fromRom(expanded);
            const int id = project.appendClass(2, "Expanded descriptor");
            auto assigned = *project.animationRoutes();
            auto row = assigned.row(id);
            row[0] = quint16((oldHandle & 0xf000) | 257);
            assigned.setRow(id, row);
            project.setAnimationRoutes(assigned.json());
            const auto built = rebuild(project, tools, work.path());
            exportRom(project, built, output);
            const auto reopened = Rom::open(output);
            QCOMPARE(ActorArt::loadBody(reopened, id, id, 0, 0).bodyHandle(), row[0]);
            QCOMPARE(ActorArt::loadBody(reopened, id, id, 0, 0).descriptorKey(), key);
            QCOMPARE(DirectDescriptors::directory(reopened), intended);
            QCOMPARE(ClassData::load(reopened).entries().size(), 166);
            QCOMPARE(rebuild(Project::fromRom(reopened), tools, work.path()).image,
                     reopened.bytes());
            qInfo().noquote() << "Expanded descriptor/class candidate:" << output;
            auto rootAgain = DirectDescriptors::load(expanded);
            QCOMPARE(rootAgain.rebuild(expanded, intended), image);
            rootAgain.append(key);
            const auto tail = rootAgain.rebuild(expanded, rootAgain.decoded());
            QCOMPARE(u32(tail, DirectDescriptors::descriptorOffset + 12), offset);
            auto separated = Rom::fromBytes(image + QByteArray(512, char(0x35)));
            auto growing = DirectDescriptors::load(separated);
            while (growing.keys().size() < 4095)
                growing.append(key);
            QVERIFY_THROWS_EXCEPTION(Error, growing.append(key));
            const auto moved = growing.rebuild(separated, compiler.compileData(growing.decoded(), work.path(), "largest_directory"));
            QVERIFY(u32(moved, DirectDescriptors::descriptorOffset + 12) >=
                    quint32(separated.bytes().size()));
            QCOMPARE(moved.mid(image.size(), 512), QByteArray(512, char(0x35)));
            QCOMPARE(moved.mid(offset, size), QByteArray(size, char(-1)));
            QCOMPARE(DirectDescriptors::load(Rom::fromBytes(moved)).keys().size(), 4095);
            QCOMPARE(ActorArt::loadDirect(Rom::fromBytes(moved), 0xffff).descriptorKey(), key);
            for (int at : {int(offset + 4), DirectDescriptors::descriptorOffset + 64,
                           DirectDescriptors::sites()[0].second}) {
                auto corrupt = image;
                corrupt[at] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, DirectDescriptors::load(Rom::fromBytes(corrupt)));
            }
            auto bad = root;
            bad.set(1, 0x3a00000);
            QVERIFY_THROWS_EXCEPTION(Error, bad.rebuild(rom, bad.decoded()));
            QVERIFY_THROWS_EXCEPTION(Error, root.rebuild(rom, compiled + QByteArray(4, 0)));
            auto competing = rom.bytes();
            put32(competing, DirectDescriptors::retailKey + DirectDescriptors::resourceBase + 4, key);
            QVERIFY_THROWS_EXCEPTION(Error, root.rebuild(Rom::fromBytes(competing), compiled));
            auto occupied = rom.bytes();
            occupied[DirectDescriptors::descriptorOffset] = 1;
            QVERIFY_THROWS_EXCEPTION(Error, root.rebuild(Rom::fromBytes(occupied), compiled));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(DirectDescriptorTests)
#include "direct_descriptors_tests.moc"
