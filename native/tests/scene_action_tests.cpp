#include "engine/director_session.h"
#include "engine/project.h"
#include "engine/scene_actions.h"
#include "engine/scene_actors.h"
#include "engine/scene_dialogue.h"
#include "rebuild/compiler.h"
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SceneActionTests : public QObject {
    Q_OBJECT
  private slots:
    static DirectorProgram program(std::initializer_list<quint32> words) {
        QByteArray bytes;
        for (auto word : words) {
            auto at = bytes.size();
            bytes.resize(at + 4);
            put32(bytes, at, word);
        }
        return DirectorProgram::decode(bytes);
    }
    void authoringBoundaries() {
        const auto source =
            program({0x80000006, 1,     0x36, 0,          0,          0,          0,         0,
                     100000,     45000, 0x35, 0,          0,          0,          0,         0,
                     100000,     45000, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        const auto points = SceneActions::positions(source);
        QCOMPARE(points.size(), 1);
        QCOMPARE(points[0].command, 3);
        QVERIFY(points[0].canWait);
        for (auto kind : SceneActions::kinds().keys()) {
            const auto inserted = SceneActions::insert(source, 3, kind);
            QCOMPARE(inserted.source.first(72), source.source.first(72));
            QCOMPARE(inserted.source.last(20), source.source.last(20));
            QCOMPARE(SceneActions::remove(inserted, 3).source, source.source);
        }
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 0, 7));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 1, 7));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(source, 1));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 3, 0xe, 0));
        const auto noBarrier = program({0x7d, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(noBarrier).first().canWait);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(noBarrier, 0, 0xe));
        const auto operandYield = program({7, 0, 0x80000000, 0, 0, 0, 1, 0, 0x7d, 0x80000002,
                                           0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(operandYield).first().canWait);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(operandYield, 0));
        const auto branch =
            program({0x80000005, 10, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(branch).first().canWait);
        // Shared waits cannot shift deadlines past signed32 or cross unknown control.
        const auto overflow = program(
            {1,   0, 0x80000002, 0x80000000, 0xe,  2,          10,         7,          0,
             0,   0, 0,          0,          1,    0,          0,          0x80000002, 0x80000000,
             0xe, 2, 0x7fffffff, 2,          0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(overflow, 5, 0xe, 1));
        const auto noReset = program(
            {1, 7, 0, 0, 0, 0, 0, 1, 0, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(noReset).first().canWait);
        auto branchThenArm = noReset;
        branchThenArm.commands.insert(2, {0, 2, {}});
        branchThenArm = DirectorProgram::decode(branchThenArm.encode());
        QVERIFY(SceneActions::positions(branchThenArm).first().canWait);
        branchThenArm.commands.prepend({0, 0x80000005, {10}});
        branchThenArm = DirectorProgram::decode(branchThenArm.encode());
        QVERIFY(!SceneActions::positions(branchThenArm).first().canWait);
        const auto extraYield = program({1, 0, 0x80000002, 0x80000000, 7, 0, 0, 0, 0, 0, 1, 0, 2,
                                         0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(extraYield).first().canWait);
    }
    void insertedWaitTimingAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId original;
            for (auto id : scenes.scenes())
                if (scenes.canCreateBlank(id)) {
                    original = id;
                    break;
                }
            QVERIFY(original);
            const auto id = scenes.createScene("Action authoring", original, true);
            const auto blank = scenes.program(id);
            auto standalone = SceneActions::insert(blank, 3, 0x14);
            standalone = SceneActions::insert(standalone, 4, 0xe, 3);
            standalone = SceneActions::insert(standalone, 10, 0xb5, 4);
            const auto span =
                program({1, 0,    0x80000002,     0x80000000,     0xe,        2,         10,
                         7, 0,    quint32(-1000), quint32(-1000), 0,          0,         1,
                         0, 0,    0x80000002,     0x80000000,     0xe,        2,         20,
                         2, 0x7d, 0x80000002,     0x80000000,     0x80000001, 0xff000005});
            auto run = [&](const DirectorProgram &p, int moveAt, int releaseAt) {
                auto current = scenes;
                current.replace(id, p.source);
                DirectorSession session(current.runtimeDirectors(),
                                        current.resource(id).selectors.first(), 0);
                QVector<int> moves;
                int released = -1, update = 0;
                auto service = [&](quint32 opcode, const QVector<quint32> &, int &) {
                    if (opcode == 7)
                        moves << update;
                    if (opcode == 0x7d)
                        released = update;
                    return true;
                };
                QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                for (update = 1; update <= releaseAt; ++update) {
                    session.advanceCounter();
                    QCOMPARE(session.dispatch(service),
                             update == releaseAt ? LORDLY_DIRECTOR_RELEASE : LORDLY_DIRECTOR_YIELD);
                }
                QCOMPARE(released, releaseAt);
                QCOMPARE(session.state().counter, quint32(0));
                QCOMPARE(moves, moveAt < 0 ? QVector<int>{} : QVector<int>{moveAt});
            };
            run(standalone, -1, 7);
            run(span, 10, 20);
            const auto inserted = SceneActions::insert(span, 5, 0xe, 30);
            QCOMPARE(inserted.commands[8].operands[1], quint32(40));
            QCOMPARE(inserted.commands[13].operands[1], quint32(50));
            run(inserted, 40, 50);
            const auto edited = SceneActions::edit(inserted, 5, 0, "7");
            QCOMPARE(edited.commands[8].operands[1], quint32(17));
            QCOMPARE(edited.commands[13].operands[1], quint32(27));
            run(edited, 17, 27);
            QCOMPARE(SceneActions::remove(edited, 5).source, span.source);
            // Keep real class-five setup, authored creation, sequential waits and native exit.
            scenes.replace(id, standalone.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, standalone.source);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-action-insert-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            const auto rom = Rom::open(destination);
            auto reopened = Project::fromRom(rom);
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, standalone.source);
            const auto placement = SceneProgramResources::placement(rom);
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(rom.bytes().size()));
            const auto directory = Directors::load(rom);
            const int selector = scenes.resource(id).selectors.first();
            QCOMPARE(directory.entries()[directory.selectors()[selector]].program.source,
                     standalone.source);
            auto checksum = rom.bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, rom.bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt and reopened" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void dialogueActions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            project.loadDialogues();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (auto id : scenes.scenes())
                if (scenes.canCreateBlank(id)) {
                    source = id;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Authored dialogue", source, true);
            const auto blank = scenes.program(id);
            auto authored = SceneActions::insert(blank, 3, 0xbf);
            auto action = SceneActions::inspect(authored).last();
            QCOMPARE(action.commandCount, 7);
            QCOMPARE(action.command, 3);
            auto expected = authored.source;
            for (auto target : {QPair{3, 0}, QPair{7, 2}, QPair{8, 0}, QPair{9, 0}})
                put32(expected, (authored.commands[target.first].word + target.second + 1) * 4, 7);
            authored = SceneActions::edit(authored, 3, 0, "7");
            QCOMPARE(authored.source, expected);
            QCOMPARE(SceneActions::remove(authored, 3).source, blank.source);
            auto separate = authored;
            separate.commands[7].operands[2] = 6;
            separate = DirectorProgram::decode(separate.encode());
            const auto partial = SceneActions::inspect(separate).last();
            QCOMPARE(partial.commandCount, 1);
            QVERIFY(partial.fields.first().name != "Window ID");
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(separate, 3));
            // The unused word is preserved even when all linked identity fields change.
            auto reserved = authored;
            reserved.commands[3].operands[6] = 0x12345678;
            reserved = DirectorProgram::decode(reserved.encode());
            QCOMPARE(SceneActions::edit(reserved, 3, 0, "9").commands[3].operands[6],
                     quint32(0x12345678));
            QVERIFY_THROWS_EXCEPTION(
                Error, SceneActions::insert(program({1, 7, 0, 0, 0, 0, 0, 1, 0, 2, 0x7d, 0x80000002,
                                                     0x80000000, 0x80000001, 0xff000005}),
                                            1, 0xbf));
            const auto nested =
                program({1,  0,    0x80000002, 0x80000000, 0xe,        2,         10, 0xe, 2,
                         20, 7,    0,          0,          0,          0,         0,  1,   0,
                         2,  0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
            for (const auto &point : SceneActions::positions(nested))
                if (point.command == 6)
                    QVERIFY(!point.canWait);
            auto dialogues = *project.dialogues();
            const QByteArray text("Native authored dialogue.@nOne connected window.");
            const int entry = dialogues.appendEntry(Dialogues::Table::Primary, 5, text);
            project.setDialogues(dialogues.json());
            authored = SceneActions::edit(authored, 3, 2, QString::number(entry));
            authored = SceneActions::insert(authored, 10, 0xe, 3);
            scenes.replace(id, authored.source);
            project.setScenePrograms(scenes.json());
            SceneResources resources({8, 0xffff, 13});
            SceneDialogue windows(0);
            DirectorSession session(scenes.runtimeDirectors(),
                                    scenes.resource(id).selectors.first(), 0);
            int created = 0, resumed = 0, closed = 0;
            auto service = [&](quint32 opcode, const QVector<quint32> &args, int &value) {
                if (opcode == 0xbf) {
                    if (windows.create(args, resources) != 0)
                        throw Error("Unexpected window pool slot");
                    ++created;
                }
                if (opcode == 0x10)
                    value = windows.query(args[2], resources);
                if (opcode == 6) {
                    windows.resume(args[0], resources);
                    ++resumed;
                }
                if (opcode == 5) {
                    windows.close(qint32(args[0]), resources);
                    ++closed;
                }
                return true;
            };
            QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
            QCOMPARE(created, 1);
            QCOMPARE(windows.query(7, resources), 0);
            for (int i = 0; i < 3; ++i) {
                QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                QCOMPARE(created, 1);
                QCOMPARE(resumed, 0);
                QCOMPARE(closed, 0);
            }
            auto record = resources.record(0);
            put8(record, 0x8a, u8(record, 0x8a) | 1);
            resources.supplyRecord(
                0, record); // Controlled ready-state input, not simulated text playback.
            QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
            QCOMPARE(created, 1);
            QCOMPARE(resumed, 1);
            QCOMPARE(closed, 1);
            QVERIFY(u8(resources.record(0), 0x8a) & 2);
            QVERIFY(u16(resources.record(0), 0) & 0x1000);
            for (int update = 1; update <= 3; ++update) {
                session.advanceCounter();
                QCOMPARE(session.dispatch(service),
                         update == 3 ? LORDLY_DIRECTOR_RELEASE : LORDLY_DIRECTOR_YIELD);
            }
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, authored.source);
            QCOMPARE(restored.dialogues()->entry(Dialogues::Table::Primary, 5, entry), text);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-dialogue-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            reopened.loadDialogues();
            QCOMPARE(reopened.scenePrograms()->program(id).source, authored.source);
            QCOMPARE(reopened.dialogues()->entry(Dialogues::Table::Primary, 5, entry), text);
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt dialogue action and text" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void actionOrdering() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId templateId;
            for (auto id : scenes.scenes())
                if (scenes.canCreateBlank(id)) {
                    templateId = id;
                    break;
                }
            QVERIFY(templateId);
            const auto id = scenes.createScene("Action order", templateId, true);
            auto source = SceneActions::insert(scenes.program(id), 3, 0x14);
            source = SceneActions::insert(source, 4, 7);
            source = SceneActions::insert(source, 5, 0xe, 3);
            source = SceneActions::insert(source, 11, 0x1d);
            source = SceneActions::insert(source, 12, 0xb5, 4);
            auto expected = source;
            const auto movement = expected.commands.takeAt(4);
            expected.commands.insert(10, movement);
            const auto moved = SceneActions::move(source, 4, 11);
            QCOMPARE(moved.source, expected.encode());
            QCOMPARE(SceneActions::move(moved, 10, 4).source, source.source);
            QCOMPARE(SceneActions::move(source, 4, 5).source, source.source);
            const auto waitMoved = SceneActions::move(source, 5, 12);
            QCOMPARE(waitMoved.commands[10].opcode, quint32(0xe));
            QCOMPARE(waitMoved.commands[10].operands, QVector<quint32>({2, 3}));
            auto run = [&](const DirectorProgram &p, int moveAt, int scaleAt, int releaseAt) {
                auto current = scenes;
                current.replace(id, p.source);
                DirectorSession session(current.runtimeDirectors(),
                                        current.resource(id).selectors.first(), 0);
                QVector<int> moves, scales;
                int released = -1, tick = 0;
                auto service = [&](quint32 op, const QVector<quint32> &, int &) {
                    if (op == 7)
                        moves << tick;
                    if (op == 0x1d)
                        scales << tick;
                    if (op == 0x7d)
                        released = tick;
                    return true;
                };
                QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                for (tick = 1; tick <= releaseAt; ++tick) {
                    session.advanceCounter();
                    QCOMPARE(session.dispatch(service),
                             tick == releaseAt ? LORDLY_DIRECTOR_RELEASE : LORDLY_DIRECTOR_YIELD);
                }
                QCOMPARE(moves, QVector<int>{moveAt});
                QCOMPARE(scales, QVector<int>{scaleAt});
                QCOMPARE(released, releaseAt);
            };
            run(source, 0, 3, 7);
            run(moved, 3, 3, 7);
            run(waitMoved, 0, 0, 7);
            const auto shared = program(
                {1, 0,    0x80000002, 0x80000000, 0xe,        0,          10,  7, 0,  0,    0, 0,
                 0, 1,    0,          0,          0x80000002, 0x80000000, 0xe, 2, 20, 0x1d, 0, 100,
                 2, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
            const auto sharedMoved = SceneActions::move(shared, 1, 10);
            run(shared, 10, 20, 20);
            run(sharedMoved, 0, 20, 20);
            QCOMPARE(sharedMoved.commands[9].operands, QVector<quint32>({0, 20}));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::move(shared, 1, 12));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::move(source, 1, 11));
            auto opaque = source;
            opaque.commands.insert(5, {0, 0x12, {}});
            opaque = DirectorProgram::decode(opaque.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::move(opaque, 4, 12));
            scenes.replace(id, moved.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, moved.source);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-action-order-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, moved.source);
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt reordered actions" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void actorActionServices() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId templateId;
            for (auto id : scenes.scenes())
                if (scenes.canCreateBlank(id)) {
                    templateId = id;
                    break;
                }
            QVERIFY(templateId);
            const auto id = scenes.createScene("Actor action controls", templateId, true);
            auto source = SceneActions::insert(scenes.program(id), 3, 0x14);
            source = SceneActions::insert(source, 4, 0x1c);
            source = SceneActions::edit(source, 4, 1, "3");
            source = SceneActions::insert(source, 5, 0x15);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, 5, 3, "0"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, 5, 1, "4"));
            source = SceneActions::edit(source, 5, 3, "-1");
            source = SceneActions::insert(source, 6, 0xe, 30);
            source = SceneActions::insert(source, 12, 0x13);
            SceneActors actors(project.rom(), 0, 0, [](int, float, float) { return 0.f; });
            int value = 0;
            const auto create = source.commands[3], channel = source.commands[4],
                       turn = source.commands[5], release = source.commands[12];
            QVERIFY(actors.service(create.opcode, create.operands, value));
            QVERIFY(actors.actor(0));
            auto expected = actors.actor(0)->record();
            put8(expected, 0x13e, 3);
            QVERIFY(actors.service(channel.opcode, channel.operands, value));
            QCOMPARE(actors.actor(0)->record(), expected);
            QVERIFY(actors.service(turn.opcode, turn.operands, value));
            int updates = 0;
            while (actors.turnJob(0).present() && updates < 100) {
                actors.advanceTurns();
                ++updates;
            }
            // ROM phases [0,9,6,3]: 0->1 with direction -1 has nine steps at cadence3.
            QCOMPARE(updates, 25);
            QVERIFY(!actors.turnJob(0).present());
            QCOMPARE(u8(actors.actor(0)->record(), 0x13f), quint8(1));
            const auto pose = SceneActions::insert(source, 5, 3).commands[5];
            const auto priorPose = actors.actor(0)->record();
            QVERIFY(actors.service(pose.opcode, pose.operands, value));
            QCOMPARE(actors.actor(0)->record().mid(0x11c, 12), priorPose.mid(0x11c, 12));
            QCOMPARE(u32(actors.actor(0)->record(), 0xe8), u32(priorPose, 0xe8));
            QCOMPARE(u8(actors.actor(0)->record(), 0x13f), quint8(1));
            auto second = create.operands;
            second[0] = 1;
            QVERIFY(actors.service(0x14, second, value));
            QVERIFY(actors.service(0x1c, {quint32(-1), 4}, value));
            QCOMPARE(u8(actors.actor(0)->record(), 0x13e), quint8(4));
            QCOMPARE(u8(actors.actor(1)->record(), 0x13e), quint8(4));
            actors.setMode(2);
            const auto retained = actors.actor(0)->record();
            QVERIFY(actors.service(0x1c, {0, 7}, value));
            QCOMPARE(actors.actor(0)->record(), retained);
            QVERIFY(actors.service(release.opcode, release.operands, value));
            QVERIFY(!actors.actor(0));
            QVERIFY(actors.actor(1));
            QVERIFY(actors.service(0x13, {quint32(-1)}, value));
            QVERIFY(!actors.actor(1));
            auto unusual = source;
            unusual.commands[4].operands[1] = 0x10003;
            unusual = DirectorProgram::decode(unusual.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(unusual, 4, 1, "3"));
            QCOMPARE(SceneActions::edit(unusual, 4, 0, "1").commands[4].operands[1],
                     quint32(0x10003));
            scenes.replace(id, source.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, source.source);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-actor-actions-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, source.source);
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt Actor controls" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void bodyActionFields() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            auto source = program({0x2a, 0, 0x58, 0, 0, 0, 0x58, 0x7d, 0x80000002, 0x80000000,
                                   0x80000001, 0xff000008});
            auto edited = SceneActions::edit(source, 0, 1, "84");
            edited = SceneActions::edit(edited, 0, 2, "85");
            auto expected = source.source;
            put32(expected, 8, 0x54);
            put32(expected, 24, 0x55);
            QCOMPARE(edited.source, expected);
            QCOMPARE(SceneActions::inspect(edited).first().actorOperand, 0);
            SceneActors actors(project.rom(), 2, 0, [](int, float, float) { return 0.f; });
            actors.supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
            int value = 0;
            QVERIFY(actors.service(0x14, {0, 1, 0, 0, 12375, 0, quint32(-2500), 0, 0}, value));
            const auto position = actors.actor(0)->record().mid(0x11c, 12);
            QVERIFY(actors.service(0x2a, edited.commands[0].operands, value));
            QCOMPARE(u32(actors.actor(0)->record(), 0xe8), quint32(0x54));
            QCOMPARE(u32(actors.actor(0)->record(), 0xec), quint32(0x55));
            QCOMPARE(u16(actors.actor(0)->record(), 0x138), quint16(0));
            QCOMPARE(actors.actor(0)->record().mid(0x11c, 12), position);
            auto retained = edited;
            for (int field : {1, 2, 3})
                retained = SceneActions::edit(retained, 0, field, "-1");
            QVERIFY(actors.service(0x2a, retained.commands[0].operands, value));
            QCOMPARE(u32(actors.actor(0)->record(), 0xe8), quint32(0x54));
            QCOMPARE(u32(actors.actor(0)->record(), 0xec), quint32(0x55));
            const auto before = actors.actor(0)->record();
            QVERIFY(actors.service(
                0x2a, SceneActions::edit(edited, 0, 0, "-1").commands[0].operands, value));
            QCOMPARE(actors.actor(0)->record(), before);
            actors.setMode(0);
            QVERIFY(actors.service(0x2a, source.commands[0].operands, value));
            QCOMPARE(actors.actor(0)->record(), before);
            auto opaque = source;
            opaque.commands[0].operands[3] = 0x10000;
            opaque.commands[0].operands[4] = 0x20000;
            opaque.commands[0].operands[1] = 0x08880001;
            opaque = DirectorProgram::decode(opaque.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(opaque, 0, 1, "84"));
            const auto changed = SceneActions::edit(opaque, 0, 3, "1");
            auto preserved = opaque.source;
            put32(preserved, 12, 1);
            QCOMPARE(changed.source, preserved);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, 0, 2, "256"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, 0, 3, "32768"));
            auto referenced = source;
            referenced.commands[0].operands[0] = 5;
            referenced = DirectorProgram::decode(referenced.encode());
            QCOMPARE(SceneActions::insert(referenced, 1, 7).commands[1].operands[0], quint32(5));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            const SceneId id(0x1fa8bf4 - 0x594284);
            const auto native = scenes.program(id);
            int command = -1;
            for (int i = 0; i < native.commands.size(); ++i)
                if (native.commands[i].opcode == 0x2a) {
                    command = i;
                    break;
                }
            QVERIFY(command >= 0);
            const int pose = native.commands[command].operands[2] == 0 ? 1 : 0;
            const auto authored = SceneActions::edit(native, command, 3, QString::number(pose));
            auto expectedNative = native.source;
            put32(expectedNative, (native.commands[command].word + 3) * 4, quint32(pose));
            QCOMPARE(authored.source, expectedNative);
            scenes.replace(id, authored.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, expectedNative);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-body-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            const auto &directors = reopened.scenePrograms()->original();
            for (int selector : scenes.resource(id).selectors)
                QCOMPARE(directors.entries()[directors.selectors()[selector]].program.source,
                         expectedNative);
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt body action" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void exactFields() {
        SceneActionField coordinate{"X", 0, 0, 1000};
        for (const qint32 value : {qint32(-2147483647 - 1), -12375, -1, 0, 1, 12375, 2147483647})
            QCOMPARE(coordinate.parse(coordinate.text(quint32(value))), quint32(value));
        QCOMPARE(coordinate.parse("-0.001"), quint32(-1));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("2147483.648"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("-2147483.649"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("0.0001"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("999999999999999999999.1"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("-"));
        QByteArray bytes;
        for (quint32 word : {7U,          0U,          1000U,       quint32(-2500), 4500U,
                             7000U,       30U,         0U,          0x1bU,          30U,
                             12U,         34U,         56U,         0xffffffffU,    255U,
                             1U,          0U,          0x80000002U, 0x80000000U,    0xeU,
                             2U,          30U,         2U,          0x7dU,          0x80000002U,
                             0x80000000U, 0x80000001U, 0xff000005U}) {
            const auto at = bytes.size();
            bytes.resize(at + 4);
            put32(bytes, at, word);
        }
        const auto source = DirectorProgram::decode(bytes);
        const auto actions = SceneActions::inspect(source);
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions[0].name, QString("Move Actor"));
        QCOMPARE(actions[2].commandCount, 6);
        auto expected = bytes;
        put32(expected, 16, quint32(-12375));
        QCOMPARE(SceneActions::edit(source, 0, 3, "-12.375").source, expected);
        expected = bytes;
        put32(expected, 21 * 4, 91);
        QCOMPARE(SceneActions::edit(source, actions[2].command, 0, "91").source, expected);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, actions[2].command, 0, "0"));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, actions[1].command, 1, "256"));
        auto unknown = bytes;
        put32(unknown, 10 * 4, 0x1234000c); // Noncanonical byte encoding stays intact.
        const auto odd = DirectorProgram::decode(unknown);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(odd, 1, 1, "12"));
        auto preserved = unknown;
        put32(preserved, 9 * 4, 50);
        QCOMPARE(SceneActions::edit(odd, 1, 0, "50").source, preserved);
    }
    void waitAndRebuild() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenePrograms();
            const auto base = *project.scenePrograms();
            quint32 key = 0;
            for (const auto &r : base.original().entries())
                if (r.program.resourceClass() == 5) {
                    key = r.key;
                    break;
                }
            QVERIFY(key != 0);
            for (const auto opcode : {0xeU, 0xb5U})
                for (const int compare : {0, 2}) {
                    QByteArray bytes;
                    for (const auto word :
                         {1U, 0U, 0x80000002U, 0x80000000U, opcode, quint32(compare), 7U, 2U, 0x7dU,
                          0x80000002U, 0x80000000U, 0x80000001U, 0xff000005U}) {
                        const auto at = bytes.size();
                        bytes.resize(at + 4);
                        put32(bytes, at, word);
                    }
                    const auto source = DirectorProgram::decode(bytes);
                    for (const int duration : {1, 2, 30, 120}) {
                        const auto edited =
                            SceneActions::edit(source, 0, 0, QString::number(duration));
                        auto expected = bytes;
                        put32(expected, 24, duration);
                        QCOMPARE(edited.source, expected);
                        auto scenes = base;
                        scenes.replace(key, edited.source);
                        DirectorSession session(scenes.runtimeDirectors(),
                                                scenes.resource(key).selectors.first(), 0);
                        bool released = false;
                        auto service = [&](quint32 op, const QVector<quint32> &, int &) {
                            released |= op == 0x7d;
                            return true;
                        };
                        QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                        for (int update = 1; update <= duration; ++update) {
                            session.advanceCounter();
                            QCOMPARE(session.dispatch(service), update == duration
                                                                    ? LORDLY_DIRECTOR_RELEASE
                                                                    : LORDLY_DIRECTOR_YIELD);
                            QCOMPARE(released, update == duration);
                        }
                        QCOMPARE(session.state().counter, quint32(0));
                        if (opcode == 0xb5) {
                            DirectorSession skip(scenes.runtimeDirectors(),
                                                 scenes.resource(key).selectors.first(), 0);
                            QCOMPARE(skip.dispatch(service), LORDLY_DIRECTOR_YIELD);
                            skip.setController(0x8000);
                            QCOMPARE(skip.dispatch(service), LORDLY_DIRECTOR_RELEASE);
                            QCOMPARE(skip.state().counter, quint32(0));
                        }
                    }
                }
            // Edit a real movement, preserving every unrelated word in its actual scene.
            auto scenes = base;
            key = 0x1a14970;
            const auto original = scenes.program(key);
            int movement = -1;
            for (const auto &action : SceneActions::inspect(original))
                if (action.name == "Move Actor") {
                    movement = action.command;
                    break;
                }
            QVERIFY(movement >= 0);
            const auto edited = SceneActions::edit(original, movement, 3, "-12.375");
            auto expected = original.source;
            put32(expected, original.commands[movement].word * 4 + 16, quint32(-12375));
            QCOMPARE(edited.source, expected);
            scenes.replace(key, edited.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(key).source, expected);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-scene-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            const auto reopenedRom = Rom::open(destination), sourceAgain = Rom::open(path);
            const auto reopened = Directors::load(reopenedRom);
            for (const auto selector : scenes.resource(key).selectors)
                QCOMPARE(reopened.entries()[reopened.selectors()[selector]].program.source,
                         expected);
            const auto placement = SceneProgramResources::placement(reopenedRom);
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopenedRom.bytes().size()));
            auto checksum = reopenedRom.bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, reopenedRom.bytes());
            QCOMPARE(sourceAgain.bytes(), rom.bytes());
            qInfo().noquote() << "Rebuilt and reopened" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(SceneActionTests)
#include "scene_action_tests.moc"
