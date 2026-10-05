#include "engine/actor_pose.h"
#include "engine/animation_sequences.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AnimationSequencesTests : public QObject {
    Q_OBJECT
  private slots:
    void unchangedClassRouteCoverage() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto routes = AnimationRoutes::load(rom);
        const auto classes = ClassData::load(rom);
        int copied = 0;
        for (int id = 1; id < classes.entries().size(); ++id)
            for (int flags = 0; flags < 4; ++flags) {
                try {
                    auto sequences = AnimationSequences::load(rom);
                    sequences.create(id, flags, routes);
                    sequences.validate(routes, classes.entries().size());
                    const auto source = ActorArt::loadBody(rom, id, id, flags >> 1, flags & 1);
                    QCOMPARE(
                        sequences.find(id, flags)->poses().encoded(),
                        AnimationDescriptor::load(rom, source.descriptorKey()).poses().encoded());
                    ++copied;
                } catch (const Error &e) {
                    QFAIL(qPrintable(QString("Class %1 route %2: %3")
                                         .arg(id)
                                         .arg(flags)
                                         .arg(QString::fromUtf8(e.what()))));
                }
            }
        QCOMPARE(copied, 656);
    }
    void projectExportAndOwnership() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAnimationRoutes();
            const auto originalRoutes = *project.animationRoutes();
            QCOMPARE(project.changedRecords(), 0);
            auto sequences = *project.animationSequences();
            sequences.create(2, 0, *project.animationRoutes());
            auto descriptor = *sequences.find(2, 0);
            auto frame = descriptor.frame(17);
            put16(frame, 4, quint16(qint16(u16(frame, 4)) + 5));
            const int privateFrame = descriptor.appendFrame(frame);
            QCOMPARE(privateFrame, 32);
            auto poses = descriptor.poses();
            poses.setFrame(40, 0, privateFrame, 2);
            descriptor.setPoses(poses);
            sequences.setDescriptor(2, 0, descriptor);
            project.setAnimationSequences(sequences.json());
            const auto snapshot = project.bodyAnimationData();
            ActorPosePlayer edited(rom, ActorArt::loadBody(rom, 2, 2, 0, 0, snapshot), 40);
            ActorPosePlayer shared(rom, ActorArt::loadBody(rom, 2, 2, 0, 1, snapshot), 40);
            ActorPosePlayer baseline(rom, ActorArt::loadBody(rom, 2, 2, 0, 1), 40);
            edited.setLinkedObjectKnownAbsent(true);
            shared.setLinkedObjectKnownAbsent(true);
            baseline.setLinkedObjectKnownAbsent(true);
            for (int i = 0; i < 2; ++i) {
                QCOMPARE(edited.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(shared.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(baseline.advance(), int(LORDLY_POSE_OK));
            }
            QCOMPARE(edited.actor().frame, 18);
            QCOMPARE(shared.actor().frame, baseline.actor().frame);
            auto routes = *project.animationRoutes();
            routes.setEntry(2, originalRoutes.entry(3));
            QVERIFY_THROWS_EXCEPTION(Error, project.setAnimationRoutes(routes.json()));
            QCOMPARE(project.animationRoutes()->json(), originalRoutes.json());
            auto invalidMarkers = sequences;
            auto marker = *invalidMarkers.find(2, 0);
            auto markerPoses = marker.poses();
            markerPoses.setProgram(
                38, PoseBank::encodeProgram({PoseRecord{21, QByteArray::fromHex("200002")}}));
            marker.setPoses(markerPoses);
            invalidMarkers.setDescriptor(2, 0, marker);
            QVERIFY_THROWS_EXCEPTION(Error, project.setAnimationSequences(invalidMarkers.json()));
            QCOMPARE(project.animationSequences()->json(), sequences.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("project.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("project.lcpack")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(file));
                QCOMPARE(restored.animationSequences()->json(), sequences.json());
                QCOMPARE(ActorArt::loadBody(rom, 2, 2, 0, 0, restored.bodyAnimationData())
                             .poseRecord(40, 0)
                             .operands,
                         QByteArray::fromHex("2002"));
            }
            const auto built = rebuild(project, tools, work.path());
            const auto exported = Rom::fromBytes(built.image);
            const auto loaded = AnimationSequences::load(exported);
            QCOMPARE(loaded.groups().size(), 1);
            QVERIFY(!loaded.modified());
            QCOMPARE(loaded.find(2, 0)->controls(), descriptor.controls());
            for (int art = 0; art < descriptor.members().size() - 4; ++art)
                QCOMPARE(loaded.find(2, 0)->sprite(art), descriptor.sprite(art));
            const auto realRoutes = AnimationRoutes::load(exported);
            QCOMPARE(realRoutes.row(2)[0] & 4095, 209);
            QCOMPARE(realRoutes.row(2)[1], originalRoutes.row(2)[1]);
            QCOMPARE(ActorArt::loadBody(exported, 2, 2, 0, 0).poseRecord(40, 0).operands,
                     QByteArray::fromHex("2002"));
            QCOMPARE(CombatArtwork::load(exported).bindings().size(),
                     CombatArtwork::load(rom).bindings().size());
            auto conservation = built.image.left(rom.bytes().size());
            for (int at : {AnimationSequences::descriptorOffset, AnimationRoutes::descriptorOffset,
                           DirectDescriptors::descriptorOffset})
                conservation.replace(at, 96, rom.bytes().mid(at, 96));
            for (const auto &[hi, lo] : AnimationRoutes::sites()) {
                conservation.replace(hi, 4, rom.bytes().mid(hi, 4));
                conservation.replace(lo, 4, rom.bytes().mid(lo, 4));
            }
            for (const auto &[a, b] : AnimationRoutes::copySites()) {
                conservation.replace(a, 4, rom.bytes().mid(a, 4));
                conservation.replace(b, 4, rom.bytes().mid(b, 4));
            }
            for (const auto &[hi, lo] : DirectDescriptors::sites()) {
                conservation.replace(hi, 4, rom.bytes().mid(hi, 4));
                conservation.replace(lo, 4, rom.bytes().mid(lo, 4));
            }
            conservation.replace(0x10, 8, rom.bytes().mid(0x10, 8));
            QCOMPARE(conservation, rom.bytes());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-private-animations-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            reopened.loadAnimationRoutes();
            QCOMPARE(reopened.changedRecords(), 0);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QCOMPARE(reopened.appendClass(2, "Private Fighter"), 165);
            QVERIFY(reopened.animationSequences()->find(165, 0));
            QCOMPARE(reopened.animationSequences()->find(165, 0)->controls(),
                     descriptor.controls());
            const auto cloned = rebuild(reopened, tools, work.path());
            const auto cloneRom = Rom::fromBytes(cloned.image);
            QCOMPARE(ClassData::load(cloneRom).entries().size(), 166);
            QCOMPARE(ActorArt::loadBody(cloneRom, 165, 165, 0, 0).poseRecord(40, 0).operands,
                     QByteArray::fromHex("2002"));
            QVERIFY(AnimationRoutes::load(cloneRom).row(165)[0] !=
                    AnimationRoutes::load(cloneRom).row(2)[0]);

            auto later = built.image;
            later += QByteArray(512, char(0x35));
            auto moving = Project::fromRom(Rom::fromBytes(later));
            moving.loadAnimationRoutes();
            auto changed = *moving.animationSequences();
            changed.create(3, 0, *moving.animationRoutes());
            moving.setAnimationSequences(changed.json());
            const auto moved = rebuild(moving, tools, work.path());
            QCOMPARE(moved.image.mid(built.image.size(), 512), QByteArray(512, char(0x35)));
            const auto oldAt = u32(built.image, AnimationSequences::descriptorOffset + 12),
                       oldSize = u32(built.image, AnimationSequences::descriptorOffset + 16);
            QVERIFY(u32(moved.image, AnimationSequences::descriptorOffset + 12) != oldAt);
            QCOMPARE(moved.image.mid(oldAt, oldSize), QByteArray(oldSize, char(-1)));
            auto removing = Project::fromRom(Rom::fromBytes(moved.image));
            removing.loadAnimationRoutes();
            auto empty = *removing.animationSequences();
            empty.remove(2, 0);
            empty.remove(3, 0);
            removing.setAnimationSequences(empty.json());
            const auto removed = rebuild(removing, tools, work.path());
            QCOMPARE(removed.image.mid(built.image.size(), 512), QByteArray(512, char(0x35)));
            QCOMPARE(removed.image.mid(AnimationSequences::descriptorOffset, 96),
                     QByteArray(96, char(-1)));
            QCOMPARE(AnimationRoutes::load(Rom::fromBytes(removed.image)).row(2),
                     originalRoutes.row(2));
            QCOMPARE(AnimationRoutes::load(Rom::fromBytes(removed.image)).row(3),
                     originalRoutes.row(3));
            QVERIFY(AnimationSequences::load(Rom::fromBytes(removed.image)).groups().isEmpty());
            auto recycling = Project::fromRom(Rom::fromBytes(removed.image));
            recycling.loadAnimationRoutes();
            auto recycled = *recycling.animationSequences();
            recycled.create(2, 0, *recycling.animationRoutes());
            const auto reused = recycled.plan(recycling.rom());
            QCOMPARE(reused.handles[8] & 4095, 209);
            QCOMPARE(reused.directory.keys().size(),
                     DirectDescriptors::load(recycling.rom()).keys().size());
            auto bad = built.image;
            bad[oldAt + 20] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, AnimationSequences::load(Rom::fromBytes(bad)));
            QVERIFY_THROWS_EXCEPTION(Error, loaded.plan(Rom::fromBytes(bad)));
            bad = rom.bytes();
            bad[AnimationSequences::descriptorOffset] = 0;
            QVERIFY_THROWS_EXCEPTION(Error, sequences.plan(Rom::fromBytes(bad)));
            auto unsafe = sequences.plan(rom);
            unsafe.offset += 16;
            QVERIFY_THROWS_EXCEPTION(Error, sequences.materialize(rom, unsafe, unsafe.payload));
            const auto safe = sequences.json();
            auto corrupt = safe;
            auto rows = corrupt["groups"].toArray();
            rows << rows.first();
            corrupt["groups"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, sequences.apply(corrupt));
            QCOMPARE(sequences.json(), safe);
            qInfo().noquote() << "Private animation candidate:" << output;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_MAIN(AnimationSequencesTests)
#include "animation_sequences_tests.moc"
