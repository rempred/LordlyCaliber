#include "engine/actor_art.h"
#include "engine/animation_routes.h"
#include "engine/project.h"
#include "engine/scene_actors.h"
#include "engine/stage_placements.h"
#include "rebuild/compiler.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AnimationRoutesTests : public QObject {
    Q_OBJECT
  private slots:
    void linkedClassInheritance() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QMap<int, int> owners;
            for (int source : {10, 25, 123}) {
                const int id = project.appendClass(source, QString("Linked %1").arg(source));
                owners[id] = source;
                QCOMPARE(u8(project.record(Table::Classes, id).bytes, 69), quint8(source));
            }
            const int inherited = project.appendClass(165, "Linked descendant");
            owners[inherited] = 10;
            QCOMPARE(u8(project.record(Table::Classes, inherited).bytes, 69), quint8(10));
            // User-selected owner values are inherited as supplied.
            auto edited = project.record(Table::Classes, 10).bytes; put8(edited, 69, 3);
            project.replaceBytes(Table::Classes, 10, edited);
            const int explicitOwner = project.appendClass(10, "Edited owner");
            QCOMPARE(u8(project.record(Table::Classes, explicitOwner).bytes, 69), quint8(3));
            const auto check = [&](Project &p) {
                const auto data = p.bodyAnimationData();
                for (auto it = owners.cbegin(); it != owners.cend(); ++it) {
                    const int id = it.key(), owner = it.value();
                    QCOMPARE(u8(p.record(Table::Classes, id).bytes, 69), quint8(owner));
                    for (int flags = 0; flags < 4; ++flags)
                        QCOMPARE(ActorArt::loadBody(p.rom(), id, owner, flags >> 1, flags & 1, data).bodyHandle(),
                                 data->routes().row(id)[flags]);
                    QVector<QByteArray> rows(20, QByteArray(248, 0));
                    put32(rows[3], 0x48, id); put32(rows[3], 0x4c, owner); put16(rows[3], 0x36, 1);
                    SceneActors scene(p.rom(), 2, 0x3c, {}, data); scene.supplyBodyRows(rows);
                    for (int slot : {4, 8}) {
                        QByteArray actor(336, 0);
                        put32(actor, 0xe4, slot); put32(actor, 0xe8, id); put32(actor, 0xec, owner);
                        put8(actor, 0x13d, 1);
                        put8(actor, 0x147, 3); put8(actor, 0x149, slot == 8 ? 1 : 0);
                        put32(actor, 0x130, 0x3f800000); scene.supplyActor(slot, actor);
                    }
                    scene.selectBody({4, id, owner, 40, 0, 0, 0, 0, 0x3c});
                    QCOMPARE(u16(scene.actor(4)->record(), 0x138), quint16(40));
                    QCOMPARE(u16(scene.actor(8)->record(), 0x138), quint16(90));
                    QCOMPARE(scene.bodyRegistrations().size(), 2);
                }
            };
            check(project);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom); restored.applyManifest(Project::readProject(work.filePath("project")));
            check(restored);
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(QFileInfo(path).dir().filePath("Test ROMs/native-linked-class-clones-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output)); check(reopened);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            const int later = reopened.appendClass(inherited, "Later descendant");
            QCOMPARE(u8(reopened.record(Table::Classes, later).bytes, 69), quint8(10));
            qInfo().noquote() << "Linked class inheritance candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void inheritanceAndLegacyProjects() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto original = AnimationRoutes::load(rom);
            QCOMPARE(original.rowCount(), 172);
            QCOMPARE(original.decodedResource().size(), 11008);
            auto project = Project::fromRom(rom);
            QCOMPARE(project.appendClass(2, "New fighter"), 165);
            QVERIFY(project.animationRoutes());
            QVERIFY(project.animationRoutes()->entry(165) == original.entry(2));
            // Older project documents carried protected template records but no animation model.
            auto older = project.manifest();
            older.remove("animationRoutes");
            auto migrated = Project::fromRom(rom);
            migrated.applyManifest(older);
            QVERIFY(migrated.animationRoutes()->entry(165) == original.entry(2));
            QVERIFY(migrated.animationRoutes()->pending().isEmpty());
            // An edited template with no exact source match is retained for explicit assignment.
            auto additions = older["addedClasses"].toArray();
            auto row = additions[0].toObject();
            auto bytes = QByteArray::fromHex(row["record"].toString().toLatin1());
            put16(bytes, 8, 3333);
            row["record"] = QString::fromLatin1(bytes.toHex());
            additions[0] = row;
            older["addedClasses"] = additions;
            migrated = Project::fromRom(rom);
            migrated.applyManifest(older);
            QVERIFY(migrated.animationRoutes()->pending().contains(165));
            QVERIFY_THROWS_EXCEPTION(Error, migrated.animationRoutes()->decodedResource());
            auto resolved = *migrated.animationRoutes();
            resolved.setEntry(165, original.entry(2));
            migrated.setAnimationRoutes(resolved.json());
            QVERIFY(migrated.animationRoutes()->pending().isEmpty());
            auto next = *project.animationRoutes();
            next.setEntry(165, original.entry(3));
            project.setAnimationRoutes(next.json());
            QCOMPARE(project.appendClass(165, "Inherited knight"), 166);
            QVERIFY(project.animationRoutes()->entry(166) == original.entry(3));
            QTemporaryDir work;
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("project.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("project.lcpack")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(file));
                QCOMPARE(reopened.animationRoutes()->json(), project.animationRoutes()->json());
            }
            auto bad = original.row(1);
            bad[0] = 0;
            QVERIFY_THROWS_EXCEPTION(Error, next.setRow(165, bad));
            QVERIFY_THROWS_EXCEPTION(Error, next.setRow(256, original.row(1)));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void expandedSourceExportAndReopen() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM/compiler required");
            const auto rom = Rom::open(path);
            const auto original = AnimationRoutes::load(rom);
            auto project = Project::fromRom(rom);
            for (int id = 165; id <= 173; ++id)
                QCOMPARE(project.appendClass(2, QString("Fighter %1").arg(id)), id);
            QCOMPARE(project.animationRoutes()->rowCount(), 174);
            auto intended = *project.animationRoutes();
            intended.setEntry(166, original.entry(3));
            project.setAnimationRoutes(intended.json());
            const auto decoded = intended.decodedResource();
            auto prefix = decoded.left(original.decodedResource().size());
            prefix.replace(0, 36, original.decodedResource().left(36));
            QCOMPARE(prefix, original.decodedResource());
            for (int header = 0; header < 18; ++header)
                if (!QSet<int>{0, 1, 3, 7, 8, 15}.contains(header))
                    QCOMPARE(u16(decoded, header * 2), u16(original.decodedResource(), header * 2));
            QCOMPARE(u16(decoded, 30), decoded.size());
            for (int id = 0; id < 165; ++id)
                QCOMPARE(intended.row(id), original.row(id));
            QTemporaryDir work;
            const auto built = rebuild(project, tools, work.path());
            const auto output = Rom::fromBytes(built.image);
            const auto actual = AnimationRoutes::load(output);
            QCOMPARE(actual.rowCount(), 174);
            QCOMPARE(actual.decodedResource(), decoded);
            for (int id = 0; id < 174; ++id)
                QVERIFY(actual.entry(id) == intended.entry(id));
            const SceneActors beforeActors(rom, 0, 0), afterActors(output, 0, 0);
            for (int id = 165; id <= 173; ++id)
                for (int direction = 0; direction < 2; ++direction)
                    QCOMPARE(
                        afterActors.bodySpeed(id, id, direction),
                        beforeActors.bodySpeed(id == 166 ? 3 : 2, id == 166 ? 3 : 2, direction));
            for (int selector = 0; selector < 80; ++selector) {
                const auto before = StagePlacements::load(rom, selector),
                           after = StagePlacements::load(output, selector);
                QCOMPARE(after.orthographic().size(), before.orthographic().size());
                QCOMPARE(after.perspective().size(), before.perspective().size());
                for (int i = 0; i < before.orthographic().size(); ++i)
                    QCOMPARE(after.orthographic()[i].source, before.orthographic()[i].source);
                for (int i = 0; i < before.perspective().size(); ++i)
                    QCOMPARE(after.perspective()[i].source, before.perspective()[i].source);
            }
            QCOMPARE(ClassData::load(output).entries().size(), 174);
            for (int id = 165; id <= 173; ++id)
                for (int flag = 0; flag < 4; ++flag) {
                    const auto body = ActorArt::loadBody(output, id, id, flag / 2, flag % 2);
                    QCOMPARE(body.bodyHandle(), intended.row(id)[flag]);
                }
            for (const auto &[hi, lo] : AnimationRoutes::sites()) {
                QCOMPARE(u32(built.image, hi) & 0xffff0000U, u32(rom.bytes(), hi) & 0xffff0000U);
                QCOMPARE(u32(built.image, lo) & 0xffff0000U, u32(rom.bytes(), lo) & 0xffff0000U);
                const auto key =
                    ((u32(built.image, hi) & 65535) << 16) | (u32(built.image, lo) & 65535);
                QCOMPARE(key, AnimationRoutes::resourceKey(output));
                QCOMPARE(u32(built.image, key + AnimationRoutes::resourceBase),
                         quint32(decoded.size()));
            }
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-animation-routes-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            qInfo().noquote() << "Animation routes candidate:" << candidate;
            QCOMPARE(AnimationRoutes::load(Rom::open(candidate)).decodedResource(), decoded);
            const auto placement = ClassPlacement::read(output);
            QVERIFY(placement.has_value());
            QJsonArray entries;
            const auto classes = ClassData::load(output);
            for (int id = 0; id < 174; ++id) {
                const auto expected = intended.entry(id);
                entries << QJsonObject{
                    {"family", u8(classes.entries()[id].record, 69)},
                    {"selectors", QString::fromLatin1(expected.selectors.toHex())}};
            }
            QFile fixture(work.filePath("animation-routes.json"));
            QVERIFY(fixture.open(QIODevice::WriteOnly));
            fixture.write(
                QJsonDocument(QJsonObject{{"rom", candidate},
                                          {"resource", QString::fromLatin1(decoded.toHex())},
                                          {"key", double(AnimationRoutes::resourceKey(output))},
                                          {"classAddress", double(placement->layout.indexBase)},
                                          {"classOffset", double(placement->dataRomOffset)},
                                          {"classSize", double(placement->layout.end -
                                                               placement->layout.indexBase)},
                                          {"entries", entries}})
                    .toJson());
            fixture.close();
            QProcess reference;
            reference.start("node",
                            {QFINDTESTDATA("animation-routes-mips.cjs"), fixture.fileName()});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            qInfo().noquote() << reference.readAllStandardOutput();
            auto reopened = Project::fromRom(output);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QCOMPARE(reopened.appendClass(166, "Later knight"), 174);
            QVERIFY(reopened.animationRoutes()->entry(174) == original.entry(3));
            const auto second = rebuild(reopened, tools, work.path());
            const auto secondRom = Rom::fromBytes(second.image);
            const auto secondRoutes = AnimationRoutes::load(secondRom);
            QCOMPARE(secondRoutes.rowCount(), 175);
            QVERIFY(secondRoutes.entry(174) == original.entry(3));
            QCOMPARE(ClassData::load(secondRom).entries().size(), 175);
            QCOMPARE(ActorArt::loadBody(secondRom, 174, 174, 1, 1).bodyHandle(),
                     original.row(3)[3]);
            auto preserved = secondRoutes.decodedResource().left(original.decodedResource().size());
            preserved.replace(0, 36, original.decodedResource().left(36));
            QCOMPARE(preserved, original.decodedResource());
            // The one-byte class domain can use row255 without shifting unrelated sections.
            auto largest = secondRoutes;
            largest.setEntry(255, original.entry(2));
            const auto copySites = AnimationRoutes::copySites()[0];
            const auto copyLoads =
                second.image.mid(copySites.first, 4) + second.image.mid(copySites.second, 4);
            const auto maximum = largest.rebuild(secondRom, largest.decodedResource(), copyLoads);
            const auto maxRoutes = AnimationRoutes::load(Rom::fromBytes(maximum));
            QCOMPARE(maxRoutes.rowCount(), 256);
            QVERIFY(maxRoutes.entry(255) == original.entry(2));
            const auto oldOffset = u32(second.image, AnimationRoutes::descriptorOffset + 12),
                       oldSize = u32(second.image, AnimationRoutes::descriptorOffset + 16);
            auto foreign = second.image + QByteArray(4096, char(0x35));
            const auto foreignRom = Rom::fromBytes(foreign);
            auto around = AnimationRoutes::load(foreignRom);
            around.setEntry(255, original.entry(3));
            const auto moved = around.rebuild(foreignRom, around.decodedResource(), copyLoads);
            QVERIFY(u32(moved, AnimationRoutes::descriptorOffset + 12) >= quint32(foreign.size()));
            QCOMPARE(moved.mid(second.image.size(), 4096), QByteArray(4096, char(0x35)));
            QCOMPARE(moved.mid(oldOffset, oldSize), QByteArray(oldSize, char(-1)));
            auto corrupt = second.image;
            corrupt[oldOffset + 40] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, AnimationRoutes::load(Rom::fromBytes(corrupt)));
            corrupt = second.image;
            put32(corrupt, AnimationRoutes::sites()[1].second,
                  u32(corrupt, AnimationRoutes::sites()[1].second) ^ 2);
            QVERIFY_THROWS_EXCEPTION(Error, AnimationRoutes::load(Rom::fromBytes(corrupt)));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(AnimationRoutesTests)
#include "animation_routes_tests.moc"
