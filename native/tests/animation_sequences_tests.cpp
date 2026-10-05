#include "engine/actor_pose.h"
#include "engine/animation_sequences.h"
#include "engine/body_markers.h"
#include "engine/body_selection.h"
#include "engine/pose_runtime.h"
#include "engine/scene_actors.h"
#include "engine/project.h"
#include "engine/sprite_object.h"
#include "rebuild/body_markers.h"
#include "rebuild/body_selection.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AnimationSequencesTests : public QObject {
    Q_OBJECT
  private slots:
    void browserSequenceImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path); QTemporaryDir work;
            QProcess generator; generator.setProcessChannelMode(QProcess::MergedChannels);
            generator.start("node", {QFINDTESTDATA("browser-sequence-fixture.cjs"), path, work.filePath("fixture.json")});
            QVERIFY(generator.waitForFinished(60000));
            const auto log = generator.readAll(); QVERIFY2(generator.exitCode() == 0, log.constData()); qInfo().noquote() << log;
            QFile file(work.filePath("fixture.json")); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto fixture = QJsonDocument::fromJson(file.readAll()).object(), input = fixture["project"].toObject();
            auto project = Project::fromRom(rom); project.importLegacy(input);
            QVERIFY2(project.pendingLegacySections().isEmpty(), qPrintable(project.pendingLegacySections().join(',')));
            const auto check = [&](Project &p) {
                const auto body = p.bodyAnimationData();
                for (const auto &value : fixture["expected"].toArray()) {
                    const auto group = value.toObject(); const auto parts = group["id"].toString().split(':');
                    const int id = parts[0].toInt(), flags = parts[1].toInt();
                    const auto *descriptor = p.animationSequences()->find(id, flags); QVERIFY(descriptor);
                    const auto bank = descriptor->poses();
                    const auto values = group["controls"].toArray();
                    std::array<QByteArray, 4> controls;
                    for (int i = 0; i < 4; ++i) controls[i] = QByteArray::fromBase64(values[i].toString().toLatin1());
                    const auto expected = PoseBank::parse(controls[1]);
                    const auto original = AnimationDescriptor::load(rom, descriptor->key());
                    const int originalArtCount = int(original.members().size()) - 4;
                    QMap<int,int> states{{0,0},{1,1},{2,2},{3,3},{4,4},{8,8},{18,18}};
                    for (const auto &v : group["routes"].toArray()) {
                        const auto row = v.toObject();
                        if (row["actionId"].toInt() >= 0) {
                            const int field = u8(p.record(Table::Classes,id).bytes,69), owner = field ? field : id;
                            states[row["selector"].toInt()] = body->selector(id,owner,row["actionId"].toInt(),row["laneKey"]=="blocked"?2:0,flags);
                        }
                    }
                    QSet<QPair<int,int>> compared;
                    for (auto state = states.begin(); state != states.end(); ++state) {
                        const auto want = expected.effective(state.key()).records, actual = bank.effective(state.value()).records;
                        QCOMPARE(actual.size(),want.size());
                        for (int instruction = 0; instruction < want.size(); ++instruction) {
                            const auto &a = actual[instruction], &b = want[instruction];
                            if (b.opcode != 1) { QCOMPARE(a.opcode,b.opcode); QCOMPARE(a.operands,b.operands); continue; }
                            QVERIFY(a.opcode==1||a.opcode==21); QCOMPARE(a.operands.back(),b.operands.back());
                            const int actualToken = u8(a.operands,0)+(a.opcode==21?256*u8(a.operands,1):0), expectedToken=u8(b.operands,0);
                            const auto frame = descriptor->frame(actualToken);
                            const int offset = int(u32(controls[0],expectedToken*4));
                            const auto reference = controls[0].mid(offset,2+16*u16(controls[0],offset));
                            QCOMPARE(frame.size(),reference.size());
                            for(int layer=0;layer<u16(frame,0);++layer) {
                                const int at=2+layer*16, actualArt=u16(frame,at), expectedArt=u16(reference,at);
                                QCOMPARE(frame.mid(at+2,14),reference.mid(at+2,14));
                                const QPair<int,int> identity{actualArt,expectedArt}; if(compared.contains(identity)) continue; compared.insert(identity);
                                const auto &config=descriptor->controls()[2];
                                QCOMPARE(u8(config,32+actualArt),u8(controls[2],32+expectedArt));
                                const auto palette=descriptor->controls()[3].mid(u8(config,u32(config,28)+actualArt)*512,512),
                                    referencePalette=controls[3].mid(u8(controls[2],u32(controls[2],28)+expectedArt)*512,512);
                                QCOMPARE(palette,referencePalette);
                                QByteArray resource;
                                if(expectedArt<originalArtCount) resource=original.sprite(expectedArt);
                                else resource=QByteArray::fromBase64(group["sources"].toArray()[expectedArt-originalArtCount].toObject()["data"].toString().toLatin1());
                                const auto sprite=SpriteObject::decode(descriptor->sprite(actualArt)), source=SpriteObject::decode(resource);
                                QCOMPARE(sprite.width(),source.width()); QCOMPARE(sprite.height(),source.height()); QCOMPARE(sprite.children().size(),source.children().size());
                                for(int child=0;child<source.children().size();++child) {
                                    QCOMPARE(sprite.pixels(child),source.pixels(child));
                                    QCOMPARE(sprite.frame(child,palette).image.rgba,source.frame(child,referencePalette).image.rgba);
                                }
                            }
                        }
                    }
                }
            };
            check(project); QVERIFY(!QTest::currentTestFailed());
            const auto saved=project.manifest(); project.saveFolder(work.filePath("project"));
            auto restored=Project::fromRom(rom); restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationSequences()->json(),project.animationSequences()->json()); check(restored); QVERIFY(!QTest::currentTestFailed());
            // Older native projects retained this browser section without converting it.
            auto legacyManifest=Project::fromRom(rom).manifest();legacyManifest["legacyImport"]=input;
            auto migrated=Project::fromRom(rom);migrated.applyManifest(legacyManifest);check(migrated);QVERIFY(!QTest::currentTestFailed());
            auto malformed=input;auto patches=malformed["patches"].toObject(),art=patches["art"].toObject(),separations=art["separations"].toObject(),entries=separations["entries"].toObject();
            auto row=entries["2:-1:0:idle"].toObject();row["poseProgramBase64"]="AA==";entries["2:-1:0:idle"]=row;
            separations["entries"]=entries;art["separations"]=separations;patches["art"]=art;malformed["patches"]=patches;
            QVERIFY_THROWS_EXCEPTION(Error,project.importLegacy(malformed));QCOMPARE(project.manifest(),saved);
            const auto built=rebuild(project,tools,work.path());
            QTemporaryDir candidates(QFileInfo(path).dir().filePath("Test ROMs/native-browser-sequences-XXXXXX"));QVERIFY(candidates.isValid());candidates.setAutoRemove(false);
            const auto output=candidates.filePath(QFileInfo(path).fileName());exportRom(project,built,output);
            auto reopened=Project::fromRom(Rom::open(output));
            // Exported descriptors have new resource identities; compare programs and rendered assets through the staged baseline.
            const auto reloaded=reopened.bodyAnimationData();QVERIFY(reopened.animationSequences()->find(2,0));
            const auto &before=*project.animationSequences()->find(2,0),&after=*reopened.animationSequences()->find(2,0);
            QCOMPARE(after.controls(),before.controls());
            for(int artId=0;artId<before.members().size()-4;++artId)QCOMPARE(after.sprite(artId),before.sprite(artId));
            QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
            restored.removePrivateAnimation(2,0);restored.saveFolder(work.filePath("removed"));
            auto removed=Project::fromRom(rom);removed.applyManifest(Project::readProject(work.filePath("removed")));
            QVERIFY(removed.animationSequences()->groups().isEmpty());QVERIFY(removed.pendingLegacySections().isEmpty());
            QCOMPARE(removed.bodyAnimationData()->selector(2,2,4,0,0),40);
            qInfo().noquote()<<"Browser sequence candidate:"<<output;
        }catch(const Error &e){QFAIL(e.what());}
    }
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
            QCOMPARE(u32(built.image, AnimationSequences::descriptorOffset + 8), quint32(3));
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
    void linkedPrivateAttacks() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAnimationRoutes();
            auto copies = *project.animationSequences();
            copies.create(10, 0, *project.animationRoutes());
            project.setAnimationSequences(copies.json());
            QCOMPARE(project.assignPrivateAttack(10, 0, 4, false, 40), 100);
            QCOMPARE(project.assignPrivateAttack(10, 0, 4, true, 40), 101);
            const auto group = project.animationSequences()->groups().value(40);
            QCOMPARE(group.reservedAttackStart, 101);
            QCOMPARE(group.pairedPoses.value(100), 256);
            QCOMPARE(group.pairedPoses.value(101), 257);
            QCOMPARE(group.descriptor.poses().programs().size(), 258);
            QCOMPARE(group.descriptor.poses().effective(256).source,
                     copies.find(10, 0)->poses().effective(90).source);
            const auto body = project.bodyAnimationData();
            QCOMPARE(body->routes().privatePose(10, 10, 0, 100), 256);
            QCOMPARE(body->routes().privatePose(10, 10, 1, 100), -2);
            QCOMPARE(body->routes().privatePose(10, 10, 0, 102), -2);
            QVector<QByteArray> rows(28, QByteArray(248, 0));
            put32(rows[3], 0x48, 10); put32(rows[3], 0x4c, 10); put16(rows[3], 0x36, 1);
            SceneActors scene(rom, 2, 0x3c, {}, body);
            scene.supplyBodyRows(rows);
            for (int slot : {4, 8}) {
                QByteArray actor(336, 0);
                put32(actor, 0xe4, quint32(slot)); put32(actor, 0xe8, 10); put32(actor, 0xec, 10);
                put8(actor, 0x147, 3); put8(actor, 0x149, slot == 8 ? 1 : 0);
                put32(actor, 0x130, 0x3f800000);
                scene.supplyActor(slot, actor);
            }
            scene.selectBody({4, 10, 10, 100, 0, 0, 0, 0, 0x3c});
            QCOMPARE(u16(scene.actor(4)->record(), 0x134), quint16(100));
            QCOMPARE(u16(scene.actor(8)->record(), 0x134), quint16(256));
            QCOMPARE(scene.bodyRegistrations().size(), 2);
            const auto secondary = scene.actor(8)->record();
            // Unregistered high states retain the original skip policy.
            scene.selectBody({8, 10, 10, 102, 0, 0, 0, 0, 0x3c});
            QCOMPARE(scene.actor(8)->record(), secondary);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationSequences()->json(), project.animationSequences()->json());
            const auto built = rebuild(project, tools, work.path());
            QCOMPARE(u32(built.image, AnimationSequences::descriptorOffset + 8), quint32(3));
            QCOMPARE(BodySelectionImage::count(Rom::fromBytes(built.image)), 2);
            QCOMPARE(built.image.mid(BodySelectionImage::offset - 16, 16), rom.bytes().mid(BodySelectionImage::offset - 16, 16));
            QCOMPARE(built.image.mid(BodySelectionImage::offset + BodySelectionImage::size, 16),
                     rom.bytes().mid(BodySelectionImage::offset + BodySelectionImage::size, 16));
            QTemporaryDir candidates(QFileInfo(path).dir().filePath("Test ROMs/native-linked-attacks-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.bodyAnimationData()->routes().privatePose(10, 10, 0, 101), 257);
            QCOMPARE(reopened.animationSequences()->groups().value(40).pairedPoses, group.pairedPoses);
            QCOMPARE(reopened.changedRecords(), 0);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            const auto run = [&](const QString &script, const QStringList &args) {
                QProcess process;
                process.setProcessChannelMode(QProcess::MergedChannels);
                process.start("node", QStringList{script} + args);
                if (!process.waitForFinished(60000) || process.exitCode())
                    throw Error(QString::fromLocal8Bit(process.readAll()));
                qInfo().noquote() << process.readAll();
            };
            run(QFINDTESTDATA("actor-body-mips.cjs"), {path, work.filePath("original.json")});
            atomicWrite(work.filePath("body.bin"), built.image.mid(BodySelectionImage::offset, BodySelectionImage::size));
            run(QFINDTESTDATA("body-selection-mips.cjs"),
                {path, work.filePath("paired.json"), work.filePath("body.bin"), work.filePath("original.json")});
            const auto leaderChecks = [&](Project &p, const QByteArray &image, const QString &candidate, bool baseline) {
                QCOMPARE(LeaderSelectionImage::count(Rom::fromBytes(image)), baseline ? 0 : 2);
                QCOMPARE(image.mid(LeaderSelectionImage::offset - 16, 16), rom.bytes().mid(LeaderSelectionImage::offset - 16, 16));
                QCOMPARE(image.mid(LeaderSelectionImage::offset + LeaderSelectionImage::size, 16),
                         rom.bytes().mid(LeaderSelectionImage::offset + LeaderSelectionImage::size, 16));
                atomicWrite(work.filePath("leader-map.bin"), p.privatePoseMappings());
                run(QFINDTESTDATA("leader-slots-mips.cjs"), {path, candidate, work.filePath("leader-map.bin"),
                    baseline ? "baseline" : "private", work.filePath("leaders.json")});
                QFile file(work.filePath("leaders.json")); QVERIFY(file.open(QIODevice::ReadOnly));
                const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
                QCOMPARE(cases.size(), 64);
                for (const auto &value : cases) {
                    const auto c = value.toObject();
                    SceneActors actors(p.rom(), 2, 0, {}, p.bodyAnimationData());
                    QVector<QByteArray> appearance;
                    for (const auto &r : c["rows"].toArray()) appearance << QByteArray::fromHex(r.toString().toLatin1());
                    actors.supplyBodyRows(appearance); actors.supplyRosterLaunch({});
                    const auto before = c["before"].toArray(), after = c["after"].toArray();
                    for (int i = 0; i < 28; ++i)
                        if (!before[i].isNull()) actors.supplyActor(i, QByteArray::fromHex(before[i].toString().toLatin1()));
                    actors.materializeRoster(0, 0, 0);
                    for (int i = 0; i < 28; ++i) {
                        QCOMPARE(actors.actor(i).has_value(), !after[i].isNull());
                        if (!after[i].isNull()) QCOMPARE(actors.actor(i)->record(), QByteArray::fromHex(after[i].toString().toLatin1()));
                    }
                }
            };
            leaderChecks(reopened, built.image, output, false);
            auto leaderCorrupt = built.image; leaderCorrupt[LeaderSelectionImage::offset + 20] ^= 1;
            auto badLeader = Project::fromRom(Rom::fromBytes(leaderCorrupt));
            QVERIFY_THROWS_EXCEPTION(Error, badLeader.loadAnimationRoutes());
            auto corrupt = built.image;
            corrupt[BodySelectionImage::offset + 20] ^= 1;
            auto invalid = Project::fromRom(Rom::fromBytes(corrupt));
            QVERIFY_THROWS_EXCEPTION(Error, invalid.loadAnimationRoutes());
            auto foreign = rom.bytes(); foreign[BodySelectionImage::offset] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, rebuildBodySelection(Rom::fromBytes(foreign), Rom::fromBytes(foreign),
                                                                 project.privatePoseMappings(), Compiler(tools), work.path()));
            reopened.restorePrivateAttack(10, 0, 4);
            QVERIFY(reopened.privatePoseMappings().isEmpty());
            const auto reset = rebuild(reopened, tools, work.path());
            auto clean = Project::fromRom(Rom::fromBytes(reset.image));
            QVERIFY(QDir(candidates.path()).mkpath("restored"));
            const auto resetPath = candidates.filePath("restored/" + QFileInfo(path).fileName());
            exportRom(reopened, reset, resetPath);
            leaderChecks(clean, reset.image, resetPath, true);
            QCOMPARE(BodySelectionImage::count(clean.rom()), 0);
            atomicWrite(work.filePath("baseline.bin"), reset.image.mid(BodySelectionImage::offset, BodySelectionImage::size));
            run(QFINDTESTDATA("body-selection-mips.cjs"),
                {path, work.filePath("baseline.json"), work.filePath("baseline.bin"), work.filePath("original.json"), "legacy"});
            QCOMPARE(clean.bodyAnimationData()->selector(10, 10, 4, 0, 0), 40);
            QVERIFY(clean.privatePoseMappings().isEmpty());
            QCOMPARE(rebuild(clean, tools, work.path()).image, reset.image);
            // Add another paired copy after reopening a padded bank.
            QCOMPARE(clean.assignPrivateAttack(10, 0, 4, false, 40), 102);
            QCOMPARE(clean.bodyAnimationData()->routes().privatePose(10, 10, 0, 102), 258);
            clean.removePrivateAnimation(10, 0);
            const auto removed = rebuild(clean, tools, work.path());
            auto empty = Project::fromRom(Rom::fromBytes(removed.image));
            empty.loadAnimationRoutes(); QVERIFY(empty.animationSequences()->groups().isEmpty());
            QCOMPARE(BodySelectionImage::count(empty.rom()), 0);
            qInfo().noquote() << "Linked attack candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void fixedPosesAndWideTransfers() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAnimationRoutes();
            auto copies = *project.animationSequences();
            for (int id : {10, 135, 136, 161}) copies.create(id, 0, *project.animationRoutes());
            // A delayed secondary loop with an explicit self-state transfer.
            auto descriptor = *copies.find(10, 0);
            auto poses = descriptor.poses();
            poses.setProgram(90, QByteArray::fromHex("02010002055a"));
            descriptor.setPoses(poses); copies.setDescriptor(10, 0, descriptor);
            project.setAnimationSequences(copies.json());
            QCOMPARE(project.assignPrivateAttack(10, 0, 4, false, 40), 100);
            const auto companion = project.animationSequences()->find(10, 0)->poses().effective(256);
            QCOMPARE(companion.records.last().opcode, 22);
            QCOMPARE(companion.records.last().operands, QByteArray::fromHex("0001"));
            for (int id : {135, 136, 161}) {
                const int selected = project.assignPrivateAttack(id, 0, 4, false, 0);
                QCOMPARE(selected, 100);
                const int field = u8(project.record(Table::Classes, id).bytes, 69), owner = field ? field : id;
                const auto data = project.bodyAnimationData();
                QCOMPARE(data->routes().privatePose(id, owner, 0, selected), -1);
                QCOMPARE(data->routes().privatePose(id, owner, 0, 40), -2);
                ActorPosePlayer player(rom, ActorArt::loadBody(rom, id, owner, 0, 0, data), selected);
                player.setLinkedObjectKnownAbsent(true);
                QCOMPARE(player.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(player.actor().state, qint16(selected));
                ActorPosePlayer fallback(rom, ActorArt::loadBody(rom, id, owner, 0, 0, data), 40);
                fallback.setLinkedObjectKnownAbsent(true);
                QCOMPARE(fallback.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(fallback.actor().state, qint16(0));
            }
            QVERIFY(project.animationSequences()->requiresWideStates());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationSequences()->json(), project.animationSequences()->json());
            const auto built = rebuild(project, tools, work.path());
            QCOMPARE(u8(built.image, PoseRuntimeImage::widthsOffset + 22), quint8(3));
            for (const auto &[offset, size] : {QPair<int,int>{PoseRuntimeImage::poseOffset, PoseRuntimeImage::poseSize},
                                              {PoseRuntimeImage::readerOffset, PoseRuntimeImage::readerSize}}) {
                QCOMPARE(built.image.mid(offset - 8, 8), rom.bytes().mid(offset - 8, 8));
                QCOMPARE(built.image.mid(offset + size, 8), rom.bytes().mid(offset + size, 8));
            }
            QCOMPARE(built.image.mid(PoseRuntimeImage::widthsOffset + 23, 25), rom.bytes().mid(PoseRuntimeImage::widthsOffset + 23, 25));
            QTemporaryDir candidates(QFileInfo(path).dir().filePath("Test ROMs/native-private-pose-runtime-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.bodyAnimationData()->routes().privatePoses(), project.privatePoseMappings());
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            ActorPosePlayer wide(reopened.rom(), ActorArt::loadBody(reopened.rom(), 10, 10, 0, 0, reopened.bodyAnimationData()), 256);
            wide.setLinkedObjectKnownAbsent(true);
            for (int tick = 0; tick < 8; ++tick) { QCOMPARE(wide.advance(), int(LORDLY_POSE_OK)); QCOMPARE(wide.actor().state, qint16(256)); }
            QCOMPARE(wide.actor().previous_state, qint16(256));
            const auto run = [&](const QString &script, const QStringList &args) {
                QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
                process.start("node", QStringList{script} + args);
                if (!process.waitForFinished(60000) || process.exitCode()) throw Error(QString::fromLocal8Bit(process.readAll()));
                qInfo().noquote() << process.readAll();
            };
            run(QFINDTESTDATA("private-pose-mips.cjs"), {path, output});
            atomicWrite(work.filePath("pose.bin"), built.image.mid(PoseRuntimeImage::poseOffset, PoseRuntimeImage::poseSize));
            atomicWrite(work.filePath("reader.bin"), built.image.mid(PoseRuntimeImage::readerOffset, PoseRuntimeImage::readerSize));
            atomicWrite(work.filePath("widths.bin"), built.image.mid(PoseRuntimeImage::widthsOffset, PoseRuntimeImage::widthsSize));
            const auto rows = [&](const QString &file) {
                QFile input(file); if (!input.open(QIODevice::ReadOnly)) throw Error("Missing pose fixture");
                return QJsonDocument::fromJson(input.readAll()).object()["rows"].toArray();
            };
            for (const auto *kind : {"actor", "body"}) {
                const auto script = QFINDTESTDATA("actor-pose-mips.cjs").replace("actor-pose", QString(kind) + "-pose");
                const auto baseline = work.filePath(QString(kind) + "-original.json"), changed = work.filePath(QString(kind) + "-rebuilt.json");
                run(script, {path, baseline, "linked"});
                run(script, {path, changed, "linked", work.filePath("pose.bin"), work.filePath("reader.bin"), work.filePath("widths.bin")});
                QVERIFY2(rows(baseline) == rows(changed), kind);
            }
            for (int offset : {PoseRuntimeImage::poseOffset, PoseRuntimeImage::readerOffset, PoseRuntimeImage::widthsOffset + 22}) {
                auto bytes = built.image; bytes[offset] ^= 1;
                auto bad = Project::fromRom(Rom::fromBytes(bytes));
                QVERIFY_THROWS_EXCEPTION(Error, bad.loadAnimationRoutes());
            }
            for (int id : {10, 135, 136, 161}) reopened.removePrivateAnimation(id, 0);
            const auto removed = rebuild(reopened, tools, work.path());
            auto empty = Project::fromRom(Rom::fromBytes(removed.image));
            empty.loadAnimationRoutes();
            QVERIFY(empty.animationSequences()->groups().isEmpty());
            QCOMPARE(PoseRuntimeImage::count(empty.rom()), 0);
            QCOMPARE(u8(removed.image, PoseRuntimeImage::widthsOffset + 22), quint8(0));
            QCOMPARE(rebuild(empty, tools, work.path()).image, removed.image);
            QVERIFY(QDir(candidates.path()).mkpath("restored"));
            const auto resetOutput = candidates.filePath("restored/" + QFileInfo(path).fileName());
            exportRom(reopened, removed, resetOutput);
            run(QFINDTESTDATA("private-pose-mips.cjs"), {path, resetOutput, "baseline"});
            // Class edits can combine a fixed source ID with a linked owner.
            auto mixed = Project::fromRom(rom);
            auto record = mixed.record(Table::Classes, 135).bytes;
            put8(record, 69, 10); mixed.replaceBytes(Table::Classes, 135, record);
            mixed.loadAnimationRoutes();
            auto routes = *mixed.animationRoutes(); routes.cloneClass(10, 135);
            mixed.setAnimationRoutes(routes.json());
            auto privateRows = *mixed.animationSequences(); privateRows.create(135, 0, *mixed.animationRoutes());
            mixed.setAnimationSequences(privateRows.json());
            QCOMPARE(mixed.assignPrivateAttack(135, 0, 4, false, 40), 100);
            QCOMPARE(mixed.assignPrivateAttack(135, 0, 4, true, 40), 101);
            const auto mixedBody = mixed.bodyAnimationData();
            QCOMPARE(mixedBody->routes().privatePose(135, 10, 0, 100), 256);
            QVERIFY(mixedBody->routes().registeredPose(135, 10, 0, 256));
            QVERIFY(!mixedBody->routes().registeredPose(135, 10, 0, -1));
            QVERIFY(!mixedBody->routes().registeredPose(135, 10, 0, 65535));
            for (int state : {100, 256}) {
                ActorPosePlayer player(rom, ActorArt::loadBody(rom, 135, 10, 0, 0, mixedBody), state);
                player.setLinkedObjectKnownAbsent(true);
                QCOMPARE(player.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(player.actor().state, qint16(state));
            }
            const auto combined = rebuild(mixed, tools, work.path());
            QCOMPARE(PoseRuntimeImage::count(Rom::fromBytes(combined.image)), 2);
            QVERIFY(QDir(candidates.path()).mkpath("combined"));
            const auto combinedOutput = candidates.filePath("combined/" + QFileInfo(path).fileName());
            exportRom(mixed, combined, combinedOutput);
            run(QFINDTESTDATA("private-pose-mips.cjs"), {path, combinedOutput});
            auto mixedReopened = Project::fromRom(Rom::open(combinedOutput));
            QVERIFY(mixedReopened.bodyAnimationData()->routes().registeredPose(135, 10, 0, 256));
            QCOMPARE(rebuild(mixedReopened, tools, work.path()).image, combined.image);
            qInfo().noquote() << "Private pose runtime candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
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
