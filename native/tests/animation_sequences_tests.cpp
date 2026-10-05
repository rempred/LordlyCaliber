#include "engine/actor_pose.h"
#include "engine/animation_sequences.h"
#include "engine/body_markers.h"
#include "engine/project.h"
#include "engine/sprite_object.h"
#include "rebuild/body_markers.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QJsonArray>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AnimationSequencesTests : public QObject {
    Q_OBJECT
  private slots:
    void privateAttackAssignments() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAnimationRoutes();
            project.loadAnimationOverrides();
            auto overrides = project.animationOverrides()->json();
            overrides["entries"] = QJsonArray{QJsonObject{{"classId", 2},
                                                          {"bodyFlags", 0},
                                                          {"actionId", 4},
                                                          {"normalSelector", 40},
                                                          {"blockedSelector", 40}}};
            project.setAnimationOverrides(overrides);
            auto copies = *project.animationSequences();
            copies.create(2, 0, *project.animationRoutes());
            project.setAnimationSequences(copies.json());
            const auto oldPoses = copies.find(2, 0)->poses();
            QCOMPARE(project.assignPrivateAttack(2, 0, 4, false, 40), 100);
            QCOMPARE(project.assignPrivateAttack(2, 0, 4, true, 40), 101);
            QCOMPARE(project.assignPrivateAttack(2, 0, 1, false, 40), 102);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 0, 0), 100);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 1, 0), 100);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 2, 0), 101);
            const auto descriptor = *project.animationSequences()->find(2, 0);
            for (int i = 0; i < oldPoses.programs().size(); ++i)
                QCOMPARE(descriptor.poses().programs()[i].source, oldPoses.programs()[i].source);
            const auto safe = project.animationSequences()->json();
            auto erased = *project.animationSequences();
            erased.remove(2, 0);
            QVERIFY_THROWS_EXCEPTION(Error, project.setAnimationSequences(erased.json()));
            QCOMPARE(project.animationSequences()->json(), safe);
            auto badOverrides = project.animationOverrides()->json();
            badOverrides["entries"] = QJsonArray{};
            QVERIFY_THROWS_EXCEPTION(Error, project.setAnimationOverrides(badOverrides));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationSequences()->json(), project.animationSequences()->json());
            QCOMPARE(restored.animationOverrides()->json(), project.animationOverrides()->json());
            const auto built = rebuild(project, tools, work.path());
            QCOMPARE(u32(built.image, AnimationSequences::descriptorOffset + 8), quint32(2));
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-private-attacks-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            const auto body = reopened.bodyAnimationData();
            QCOMPARE(body->selector(2, 2, 4, 0, 0), 100);
            QCOMPARE(body->selector(2, 2, 4, 2, 0), 101);
            ActorPosePlayer player(reopened.rom(),
                                   ActorArt::loadBody(reopened.rom(), 2, 2, 0, 0, body), 100);
            player.setLinkedObjectKnownAbsent(true);
            QCOMPARE(player.advance(), int(LORDLY_POSE_OK));
            QCOMPARE(player.actor().frame, 17);
            QCOMPARE(reopened.changedRecords(), 0);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            auto cloned = reopened;
            QCOMPARE(cloned.appendClass(2, "Private attack clone"), 165);
            QCOMPARE(cloned.bodyAnimationData()->selector(165, 165, 4, 0, 0), 100);
            const auto cloneBuild = rebuild(cloned, tools, work.path());
            auto cloneReopened = Project::fromRom(Rom::fromBytes(cloneBuild.image));
            QCOMPARE(cloneReopened.bodyAnimationData()->selector(165, 165, 4, 0, 0), 100);
            QCOMPARE(cloneReopened.animationSequences()->groups().value(165 * 4).attackBaselines,
                     cloned.animationSequences()->groups().value(165 * 4).attackBaselines);
            cloned.removePrivateAnimation(165, 0);
            QCOMPARE(cloned.bodyAnimationData()->selector(165, 165, 4, 0, 0), 40);
            reopened.restorePrivateAttack(2, 0, 4);
            QCOMPARE(reopened.bodyAnimationData()->selector(2, 2, 4, 0, 0), 40);
            QVERIFY(reopened.animationSequences()->groups().value(8).attackBaselines.contains("1"));
            reopened.removePrivateAnimation(2, 0);
            QCOMPARE(reopened.animationOverrides()->entries(), overrides["entries"].toArray());
            const auto reset = rebuild(reopened, tools, work.path());
            auto resetProject = Project::fromRom(Rom::fromBytes(reset.image));
            QCOMPARE(resetProject.bodyAnimationData()->selector(2, 2, 4, 0, 0), 40);
            QVERIFY(resetProject.animationSequences()->groups().isEmpty());
            qInfo().noquote() << "Private attack candidate:" << output;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void wideEquipmentMarkers() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            QCOMPARE(BodyMarkerImage::mode(rom), 0);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("body-markers-mips.cjs"), path,
                                     work.filePath("markers.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            qInfo().noquote() << reference.readAllStandardOutput();
            auto project = Project::fromRom(rom);
            project.loadAnimationRoutes();
            auto sequences = *project.animationSequences();
            sequences.create(2, 0, *project.animationRoutes());
            auto descriptor = *sequences.find(2, 0);
            const auto row = descriptor.frame(0);
            while (descriptor.frameCount() <= 256)
                descriptor.appendFrame(row);
            auto bank = descriptor.poses();
            QByteArray extended(88 * 4, 0);
            for (int state = 0; state < 88; ++state) {
                put32(extended, state * 4, quint32(extended.size()));
                extended += state < bank.programs().size() ? bank.programs()[state].source
                                                           : QByteArray(1, 0);
            }
            bank = PoseBank::parse(extended);
            bank.setProgram(
                38, PoseBank::encodeProgram({PoseRecord{1, QByteArray::fromHex("0002")},
                                             PoseRecord{21, QByteArray::fromHex("000102")}}));
            descriptor.setPoses(bank);
            sequences.setDescriptor(2, 0, descriptor);
            QVERIFY(sequences.requiresMarkerRuntime());
            project.setAnimationSequences(sequences.json());
            const auto preview = ActorArt::loadBody(rom, 2, 2, 0, 0, project.bodyAnimationData());
            const auto layers = preview.bodyFrame(rom, 256, preview.defaultAppearance(rom));
            QVERIFY(!layers.isEmpty());
            QCOMPARE(layers.first().metadata.art, preview.layers(0).first().art);
            QVERIFY(layers.first().metadata.flags & 0x40);
            const auto built = rebuild(project, tools, work.path());
            const auto expanded = Rom::fromBytes(built.image);
            QCOMPARE(BodyMarkerImage::mode(expanded), 1);
            const auto body = ActorArt::loadBody(expanded, 2, 2, 0, 0);
            const auto actual = body.bodyFrame(expanded, 256, body.defaultAppearance(expanded));
            QCOMPARE(actual.size(), layers.size());
            for (int i = 0; i < layers.size(); ++i) {
                QCOMPARE(actual[i].metadata.flags, layers[i].metadata.flags);
                QCOMPARE(actual[i].pixels.image.rgba, layers[i].pixels.image.rgba);
            }
            auto reopened = Project::fromRom(expanded);
            reopened.loadAnimationRoutes();
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QCOMPARE(reopened.appendClass(2, "Wide marker clone"), 165);
            const auto clone = rebuild(reopened, tools, work.path());
            const auto cloneRom = Rom::fromBytes(clone.image);
            QCOMPARE(BodyMarkerImage::mode(cloneRom), 1);
            const auto cloned = ActorArt::loadBody(cloneRom, 165, 165, 0, 0);
            QCOMPARE(cloned.bodyFrame(cloneRom, 256, cloned.defaultAppearance(cloneRom))
                         .first()
                         .pixels.image.rgba,
                     layers.first().pixels.image.rgba);
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-wide-markers-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            QCOMPARE(BodyMarkerImage::mode(Rom::open(output)), 1);
            qInfo().noquote() << "Wide marker candidate:" << output;
            auto reset = Project::fromRom(expanded);
            reset.loadAnimationRoutes();
            auto empty = *reset.animationSequences();
            empty.remove(2, 0);
            reset.setAnimationSequences(empty.json());
            const auto restored = rebuild(reset, tools, work.path());
            QCOMPARE(BodyMarkerImage::mode(Rom::fromBytes(restored.image)), 0);
            QCOMPARE(restored.image.mid(BodyMarkerImage::offset, BodyMarkerImage::size),
                     rom.bytes().mid(BodyMarkerImage::offset, BodyMarkerImage::size));
            auto foreign = rom.bytes();
            foreign[BodyMarkerImage::offset] ^= 1;
            auto blocked = Project::fromRom(Rom::fromBytes(foreign));
            blocked.loadAnimationRoutes();
            QVERIFY_THROWS_EXCEPTION(Error, blocked.setAnimationSequences(sequences.json()));
            auto broken = built.image;
            broken[BodyMarkerImage::offset] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, AnimationSequences::load(Rom::fromBytes(broken)));
            Compiler compiler(tools);
            QVERIFY_THROWS_EXCEPTION(Error, rebuildBodyMarkers(Rom::fromBytes(foreign), rom, true,
                                                               compiler, work.path()));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
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
            const auto importedSprite = SpriteObject::fromImage(
                {3, 2, QByteArray::fromHex("ff0000ff00ff00800000ff0001020344abcdefc010203001")});
            const int importedArt = descriptor.appendSprite(importedSprite.source(), 0,
                                                            descriptor.controls()[3].left(512));
            QByteArray importedRow(18, 0);
            put16(importedRow, 0, 1);
            put16(importedRow, 2, quint16(importedArt));
            put16(importedRow, 4, quint16(-5));
            put16(importedRow, 6, quint16(-7));
            put16(importedRow, 8, 3);
            put16(importedRow, 10, 2);
            put16(importedRow, 14, 1024);
            put16(importedRow, 16, 1024);
            const int importedFrame = descriptor.appendFrame(importedRow);
            auto poses = descriptor.poses();
            poses.setFrame(40, 0, privateFrame, 2);
            poses.setFrame(40, 1, importedFrame, 6);
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
            QCOMPARE(edited.actor().frame, importedFrame);
            QCOMPARE(shared.actor().frame, baseline.actor().frame);
            auto routes = *project.animationRoutes();
            routes.setEntry(2, originalRoutes.entry(3));
            QVERIFY_THROWS_EXCEPTION(Error, project.setAnimationRoutes(routes.json()));
            QCOMPARE(project.animationRoutes()->json(), originalRoutes.json());
            auto wideMarkers = sequences;
            auto marker = *wideMarkers.find(2, 0);
            auto markerPoses = marker.poses();
            markerPoses.setProgram(
                38, PoseBank::encodeProgram({PoseRecord{1, QByteArray::fromHex("0002")},
                                             PoseRecord{21, QByteArray::fromHex("200002")}}));
            marker.setPoses(markerPoses);
            wideMarkers.setDescriptor(2, 0, marker);
            auto compatible = project;
            compatible.setAnimationSequences(wideMarkers.json());
            QVERIFY(compatible.animationSequences()->requiresMarkerRuntime());
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
            const auto importedBody = ActorArt::loadBody(exported, 2, 2, 0, 0);
            QCOMPARE(
                importedBody
                    .bodyFrame(exported, importedFrame, importedBody.defaultAppearance(exported))
                    .first()
                    .pixels.image.rgba,
                importedSprite.frame(0).image.rgba);
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
            QProcess probe;
            probe.start("node", {QFINDTESTDATA("private-sprite-mips.cjs"), output,
                                 QString::number(importedFrame), QString::number(importedArt)});
            QVERIFY(probe.waitForFinished(60000));
            QVERIFY2(probe.exitCode() == 0, probe.readAllStandardError().constData());
            qInfo().noquote() << probe.readAllStandardOutput();
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
