#include "engine/director_session.h"
#include "engine/project.h"
#include "engine/scene_actions.h"
#include "engine/scene_actors.h"
#include "engine/scene_dialogue.h"
#include "engine/dialogue_session.h"
#include "engine/scene_sprites.h"
#include "engine/scene_ribbons.h"
#include "engine/scene_image_motion.h"
#include "engine/scene_image_presentation.h"
#include "engine/scene_transforms.h"
#include "rebuild/compiler.h"
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
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
    void imageTransitionCompletionWait() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image completion wait", source, false);
            const auto base =
                program({0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                         70,          70,   130,         8,           0x50,        0,          20,
                         4,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const int point = 3;
            const auto p = SceneActions::insert(base, point, 0x54);
            const auto actions = SceneActions::inspect(p);
            const auto wait = std::find_if(actions.begin(), actions.end(),
                                           [&](const auto &a) { return a.command == point; });
            QVERIFY(wait != actions.end());
            QCOMPARE(wait->name, QString("Wait for image transition"));
            QCOMPARE(wait->commandCount, 4);
            QVERIFY(wait->fields.isEmpty());
            QCOMPARE(wait->timing, SceneActionTiming::Completion);
            QCOMPARE(p.commands[point + 3].operands, QVector<quint32>({0, 0}));
            QCOMPARE(SceneActions::remove(p, point).source, base.source);
            auto withFade = SceneActions::insert(p, point + 4, 0x1b);
            const auto moved = SceneActions::move(withFade, point, point + 5);
            QCOMPARE(SceneActions::move(moved, point + 1, point).source, withFade.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, 0, "5"));
            auto partial = p;
            partial.commands[point + 3].operands[1] = 1;
            partial = DirectorProgram::decode(partial.encode());
            for (const auto &a : SceneActions::inspect(partial))
                QVERIFY(a.command != point);
            const auto armed = program(
                {1, 0x48, 0, 255, 2, 0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(armed, 1, 0x54));
            const auto branched = program(
                {0x80000005U, 10, 0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(branched, 1, 0x54));
            auto world = base;
            put32(world.source, world.source.size() - 4, 0xff000001U);
            world = DirectorProgram::decode(world.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(world, point, 0x54));
            auto exercise = [&](const Rom &rom, ScenePrograms resources) {
                for (const auto &[duration, tail] :
                     QVector<QPair<int, int>>{{1, 0}, {20, 4}, {255, 1}})
                    for (int timed : {0, 3}) {
                        auto edited = SceneActions::edit(resources.program(id), 2, 1,
                                                         QString::number(duration));
                        edited = SceneActions::edit(edited, 2, 2, QString::number(tail));
                        if (timed)
                            edited = SceneActions::insert(edited, point + 4, 0xe, timed);
                        resources.replace(id, edited.source);
                        DirectorSession session(resources.runtimeDirectors(),
                                                resources.resource(id).selectors.first(), 0);
                        auto image = SceneImagePresentation::load(rom, 47);
                        auto context = image.initialContext();
                        auto channel = SceneTransforms{}.channel(1);
                        auto query = [&] {
                            return lordly_image_echo_query(
                                reinterpret_cast<const uint8_t *>(context.constData()));
                        };
                        bool released = false;
                        int update = 0, completed = -1;
                        bool running = false, draining = false;
                        auto service = [&](quint32 opcode, const QVector<quint32> &args,
                                           int &value) {
                            if (opcode == 0x3a)
                                image.construct(rom, args,
                                                SceneCameras::freshModeTwo(rom).registered(),
                                                context);
                            else if (opcode == 0x50)
                                image.startEcho(args, context, channel);
                            else if (opcode == 0x54)
                                value = qint32(query());
                            else if (opcode == 0x7d)
                                released = true;
                            return true;
                        };
                        QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                        for (update = 1; update <= 280 && !released; ++update) {
                            image.advanceEcho(rom, context);
                            session.advanceCounter();
                            const auto state = query();
                            running |= state == 1;
                            draining |= state == quint32(-1);
                            if (!state && completed < 0)
                                completed = update;
                            const auto result = session.dispatch(service);
                            const bool expected = completed >= 0 && update == completed + timed;
                            QCOMPARE(released, expected);
                            QCOMPARE(result,
                                     expected ? LORDLY_DIRECTOR_RELEASE : LORDLY_DIRECTOR_YIELD);
                            if (completed < 0)
                                QCOMPARE(session.state().counter, quint32(0));
                        }
                        QVERIFY(released);
                        QVERIFY(draining);
                        if (duration > 1)
                            QVERIFY(running);
                        QCOMPARE(update - 1, completed + timed);
                        QCOMPARE(session.state().counter, quint32(0));
                        for (auto live : image.echo().live)
                            QCOMPARE(live, uint8_t(0));
                        // Each scenario starts from the same authored stream, without the prior
                        // timed suffix.
                        resources.replace(id, p.source);
                    }
            };
            scenes.replace(id, p.source);
            exercise(project.rom(), scenes);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-image-completion-wait-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            exercise(reopened.rom(), *reopened.scenePrograms());
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Image completion wait export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void imageConstructionAction() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image construction action", source, false);
            const auto base = program(
                {0x80000007U, 47, 0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const int point = 1;
            auto p = SceneActions::insert(base, point, 0x3a);
            const auto defaultArgs = p.commands[point].operands;
            QCOMPARE(defaultArgs, QVector<quint32>({1, 0, 0, 0, 70, 70, 130, 8}));
            QCOMPARE(SceneActions::remove(p, point).source, base.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(p, point, 0x3a));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(base, point, 0x50));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(p, point, 0x50));
            const auto transition = SceneActions::insert(p, point + 1, 0x50);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::move(transition, point, point + 2));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::move(transition, point + 1, point));
            auto reversed = transition;
            std::swap(reversed.commands[point], reversed.commands[point + 1]);
            reversed = DirectorProgram::decode(reversed.encode());
            QCOMPARE(SceneActions::move(reversed, point + 1, point).source, transition.source);
            const auto missing = SceneActions::remove(reversed, point + 1);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(missing, point + 1, 0x3a));
            QCOMPARE(SceneActions::insert(missing, point, 0x3a).source, transition.source);
            for (const auto &[field, text] : QVector<QPair<int, QString>>{
                     {0, "2"}, {1, "20"}, {2, "-10"}, {3, "40"}, {4, "90"}, {5, "210"}, {6, "0"}})
                p = SceneActions::edit(p, point, field, text);
            for (const auto &[field, text] :
                 QVector<QPair<int, QString>>{{0, "0"}, {0, "20"}, {5, "256"}, {6, "4"}})
                QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, field, text));
            auto opaque = p;
            opaque.commands[point].operands[3] = 0x12345678;
            opaque.commands[point].operands[7] = 0x104;
            opaque = DirectorProgram::decode(opaque.encode());
            const auto retained = SceneActions::edit(opaque, point, 1, "30");
            QCOMPARE(retained.commands[point].operands[3], quint32(0x12345678));
            QCOMPARE(retained.commands[point].operands[7], quint32(0x104));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(opaque, point, 6, "8"));
            auto symbolic = p;
            symbolic.commands[point].operands[1] = 0x08880003;
            symbolic = DirectorProgram::decode(symbolic.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(symbolic, point, 1, "0"));
            QCOMPARE(SceneActions::edit(symbolic, point, 5, "100").commands[point].operands[1],
                     quint32(0x08880003));
            auto world = base;
            put32(world.source, world.source.size() - 4, 0xff000001U);
            world = DirectorProgram::decode(world.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(world, point, 0x3a));
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                auto image = SceneImagePresentation::load(rom, 47), original = image;
                const auto sourcePixels = image.source();
                auto context = image.initialContext(), old = context;
                const auto camera = SceneCameras::freshModeTwo(rom).registered();
                original.construct(rom, defaultArgs, camera, old);
                image.construct(rom, p.commands[point].operands, camera, context);
                QCOMPARE(image.slot(), 2);
                QCOMPARE(u8(context, 0x838), quint8(2));
                QCOMPARE(u8(context, 0x83d), quint8(210));
                QVERIFY(context.first(64) != old.first(64));
                QVERIFY(context.mid(0x798, 64) != old.mid(0x798, 64));
                QVERIFY(image.processed() != original.processed());
                QCOMPARE(image.source(), sourcePixels);
                int maximum = 0;
                for (int y = 0; y < image.processed().height(); ++y)
                    for (int x = 0; x < image.processed().width(); ++x)
                        maximum = std::max(maximum, image.processed().pixelColor(x, y).alpha());
                QCOMPARE(maximum, 210);
                const auto accepted = context;
                const auto processed = image.processed();
                QVERIFY_THROWS_EXCEPTION(Error, image.construct(rom, defaultArgs, camera, context));
                QCOMPARE(context, accepted);
                QCOMPARE(image.processed(), processed);
                auto mirrored = SceneImagePresentation::load(rom, 47);
                auto mirrorContext = mirrored.initialContext();
                auto args = p.commands[point].operands;
                args[4] = quint32(-40);
                mirrored.construct(rom, args, camera, mirrorContext);
                QVERIFY(mirrorContext.first(64) != context.first(64));
                QCOMPARE(mirrored.processed(), image.processed());
            };
            exercise(project.rom(), p);
            p = SceneActions::insert(p, point + 1, 0xe, 60);
            scenes.replace(id, p.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-image-construction-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Image construction Actions export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void imageTransitionAction() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image transition action", source, false);
            const auto base = program({0x80000007U, 47, 0x3a, 1, 0, 0, 0, 70, 70, 130, 8, 0x7d,
                                       0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const int point = 2;
            auto p = SceneActions::insert(base, point, 0x50);
            QCOMPARE(p.commands[point].operands, QVector<quint32>({0, 20, 4}));
            QCOMPARE(SceneActions::remove(p, point).source, base.source);
            p = SceneActions::edit(p, point, 0, "1");
            for (const auto &[field, text] : QVector<QPair<int, QString>>{
                     {0, "2"}, {1, "0"}, {1, "256"}, {2, "-1"}, {2, "256"}})
                QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, field, text));
            QCOMPARE(SceneActions::edit(p, point, 1, "255").commands[point].operands[1],
                     quint32(255));
            QCOMPARE(SceneActions::edit(p, point, 2, "255").commands[point].operands[2],
                     quint32(255));
            auto opaque = p;
            opaque.commands[point].operands[1] = 0x114;
            opaque = DirectorProgram::decode(opaque.encode());
            QCOMPARE(SceneActions::edit(opaque, point, 2, "0").commands[point].operands[1],
                     quint32(0x114));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(opaque, point, 1, "20"));
            auto symbolic = p;
            symbolic.commands[point].operands[0] = 0x08880003;
            symbolic = DirectorProgram::decode(symbolic.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(symbolic, point, 0, "0"));
            QCOMPARE(SceneActions::edit(symbolic, point, 2, "0").commands[point].operands[0],
                     quint32(0x08880003));
            auto world = base;
            put32(world.source, world.source.size() - 4, 0xff000001U);
            world = DirectorProgram::decode(world.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(world, point, 0x50));
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                for (quint32 direction : {0U, 1U})
                    for (quint32 tail : {0U, 4U}) {
                        auto image = SceneImagePresentation::load(rom, 47);
                        auto context = image.initialContext();
                        image.construct(rom, base.commands[1].operands,
                                        SceneCameras::freshModeTwo(rom).registered(), context);
                        auto channel = SceneTransforms{}.channel(1);
                        put32(channel, 0x40, std::bit_cast<quint32>(2.f));
                        put32(channel, 0x44, std::bit_cast<quint32>(3.f));
                        put32(channel, 0x54, std::bit_cast<quint32>(.75f));
                        const auto before = channel;
                        auto args = p.commands[point].operands;
                        args[0] = direction;
                        args[2] = tail;
                        image.startEcho(args, context, channel);
                        QCOMPARE(u32(context, 0x828), quint32(0));
                        QCOMPARE(u32(context, 0x82c), quint32(20));
                        QCOMPARE(u8(context, 0x83a), quint8(tail));
                        QCOMPARE(u8(context, 0x83b), quint8(direction));
                        if (direction) {
                            QCOMPARE(u32(channel, 0x40), quint32(0));
                            QCOMPARE(u32(channel, 0x44), quint32(0));
                            QCOMPARE(std::bit_cast<float>(u32(channel, 0x54)), 1.f);
                        } else
                            QCOMPARE(channel, before);
                        auto query = [&] {
                            return lordly_image_echo_query(
                                reinterpret_cast<const uint8_t *>(context.constData()));
                        };
                        for (int tick = 1; tick <= 20; ++tick) {
                            image.advanceEcho(rom, context);
                            QCOMPARE(query(), tick < 20 ? 1U : 0xffffffffU);
                        }
                        QCOMPARE(u32(context, 0x830), quint32(20 + tail * 5));
                        QVERIFY(bool(image.echo().live[0] || image.echo().live[1] ||
                                     image.echo().live[2]) == bool(tail));
                        int drain = 0;
                        while (query() != 0 && drain < 22) {
                            image.advanceEcho(rom, context);
                            ++drain;
                        }
                        QCOMPARE(query(), quint32(0));
                        QCOMPARE(u8(context, 0x83c), quint8(4));
                        for (auto live : image.echo().live)
                            QCOMPARE(live, uint8_t(0));
                        if (!tail)
                            QCOMPARE(drain, 1);
                        else
                            QVERIFY(drain > 1 && drain <= 20);
                        const auto settled = context;
                        image.advanceEcho(rom, context);
                        QCOMPARE(context, settled);
                    }
            };
            exercise(project.rom(), p);
            p = SceneActions::insert(p, point + 1, 0xe, 60);
            scenes.replace(id, p.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-image-transition-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Image transition Actions export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void imageMotionAction() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image motion action", source, false);
            const auto prelude =
                program({0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                         70,          70,   130,         8,           0x50,        0,          1,
                         0,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const auto base = SceneActions::insert(prelude, 3, 0xe, 3);
            const int point = 9;
            auto p = SceneActions::insert(base, point, 0x76);
            QCOMPARE(p.commands[point].operands,
                     QVector<quint32>({quint32(-1), quint32(-1), 0, 0, 0, 0, 30}));
            QCOMPARE(SceneActions::remove(p, point).source, base.source);
            for (const auto &[field, text] :
                 QVector<QPair<int, QString>>{{2, "12"}, {3, "6"}, {4, "1"}, {5, "3"}})
                p = SceneActions::edit(p, point, field, text);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, 5, "0"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, 4, "2"));
            auto opaque = p;
            opaque.commands[point].operands[2] = 0x12345678;
            opaque = DirectorProgram::decode(opaque.encode());
            QCOMPARE(SceneActions::edit(opaque, point, 2, "20").commands[point].operands[2],
                     quint32(0x12345678));
            auto symbolic = p;
            symbolic.commands[point].operands[0] = 0x08880003;
            symbolic = DirectorProgram::decode(symbolic.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(symbolic, point, 0, "0"));
            QCOMPARE(SceneActions::edit(symbolic, point, 3, "9").commands[point].operands[0],
                     quint32(0x08880003));
            auto world = base;
            put32(world.source, world.source.size() - 4, 0xff000001U);
            world = DirectorProgram::decode(world.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(world, point, 0x76));
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                SceneImageMotion motion(rom);
                SceneTransforms transforms;
                auto channel = transforms.channel(1);
                QByteArray context(0x848, 0);
                put8(context, 0x83c, 4);
                const auto before = channel;
                motion.start(p.commands[point].operands, context, channel);
                QCOMPARE(channel, before);
                QCOMPARE(motion.record().size(), 16);
                QCOMPARE(std::bit_cast<float>(u32(motion.record(), 0)), 4.f);
                QCOMPARE(std::bit_cast<float>(u32(motion.record(), 4)), 2.f);
                QCOMPARE(u32(motion.record(), 8), quint32(3));
                QCOMPARE(u32(motion.record(), 12), quint32(0));
                for (int i = 1; i <= 3; ++i) {
                    motion.advance(context, channel);
                    QVERIFY(motion.active());
                    QCOMPARE(std::bit_cast<float>(u32(channel, 0x40)), float(i * 4));
                    QCOMPARE(std::bit_cast<float>(u32(channel, 0x44)), float(i * 2));
                }
                motion.advance(context, channel);
                QVERIFY(!motion.active());
                QCOMPARE(std::bit_cast<float>(u32(channel, 0x54)), .7f);
                auto deadband = p.commands[point].operands;
                deadband[0] = deadband[1] = 0;
                deadband[3] = 1;
                deadband[4] = quint32(-1);
                motion.start(deadband, context, channel);
                QCOMPARE(u32(motion.record(), 0), quint32(0));
                QCOMPARE(u32(motion.record(), 4), quint32(0));
                const auto current = channel;
                motion.advance(context, channel);
                QCOMPARE(channel.mid(0x40, 8), current.mid(0x40, 8));
            };
            exercise(project.rom(), p);
            p = SceneActions::insert(p, point + 1, 0xe, 30);
            scenes.replace(id, p.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-image-motion-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Image motion Actions export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void ribbonActions() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Ribbon Actions", source, false);
            const auto base = program({0x80000007U, 47, 0x3a, 1, 0, 0, 0, 70, 70, 130, 8, 0x7d,
                                       0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const int point = 2; // Insert after the independently editable image construction.
            auto p = SceneActions::insert(base, point, 0x49);
            QCOMPARE(p.commands[point].operands,
                     QVector<quint32>({0, 3, 0, 100, quint32(-1), 3, 0, 0, 1}));
            QCOMPARE(SceneActions::remove(p, point).source, base.source);
            const auto two = SceneActions::insert(p, point + 1, 0x49);
            QCOMPARE(two.commands[point + 1].operands[0], quint32(1));
            auto symbolic = p;
            symbolic.commands[point].operands[0] = 0x08880001;
            symbolic = DirectorProgram::decode(symbolic.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(symbolic, point, 0x49));
            QCOMPARE(
                SceneActions::insert(symbolic, point + 1, 0x4a).commands[point + 1].operands[0],
                quint32(0x08880001));
            auto referenced = SceneActions::insert(base, point, SceneActions::OrientSprite);
            referenced = SceneActions::edit(referenced, point, 1, "-1");
            referenced = SceneActions::edit(referenced, point, 2, "0");
            QCOMPARE(SceneActions::insert(referenced, point, 0x49).commands[point].operands[0],
                     quint32(1));
            auto crowded = base;
            for (int i = 0; i < 20; ++i)
                crowded = SceneActions::insert(crowded, point, 0x49);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(crowded, point, 0x49));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, 5, "0"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(p, point, 6, "4"));
            p = SceneActions::edit(p, point, 3, "50");
            p = SceneActions::edit(p, point, 6, "2");
            p = SceneActions::insert(p, point + 1, 0x4a);
            p = SceneActions::edit(p, point + 1, 2, "1");
            QVERIFY(SceneActions::ribbonSource(p, point + 1, 0));
            QCOMPARE(*SceneActions::ribbonSource(p, point + 1, 0), qMakePair(3, 0));
            auto waited = SceneActions::insert(p, point + 1, 0xe, 30);
            int endpoint = -1;
            for (int i = 0; i < waited.commands.size(); ++i)
                if (waited.commands[i].opcode == 0x4a)
                    endpoint = i;
            QVERIFY(endpoint > point);
            QVERIFY(SceneActions::ribbonSource(waited, endpoint, 0));
            auto unknown = waited;
            unknown.commands.insert(endpoint, {0, 0x80000002U, {}});
            unknown = DirectorProgram::decode(unknown.encode());
            QVERIFY(!SceneActions::ribbonSource(unknown, endpoint + 1, 0));
            p = SceneActions::insert(p, point + 2, 0x4a);
            p = SceneActions::edit(p, point + 2, 1, "25");
            p = SceneActions::insert(p, point + 3, 0x4b);
            const auto reordered = SceneActions::move(p, point + 3, point + 2);
            QCOMPARE(SceneActions::move(reordered, point + 2, point + 4).source, p.source);
            auto exercise = [&](const Rom &rom, const DirectorProgram &program) {
                SceneRibbons ribbons(rom);
                int value = 0;
                for (const auto &c : program.commands) {
                    if (c.opcode < 0x49 || c.opcode > 0x4b)
                        continue;
                    const auto before = ribbons.ribbons()[0];
                    QVERIFY(ribbons.service(c.opcode, c.operands, value));
                    QVERIFY(ribbons.ribbons()[0]);
                    const auto &record = ribbons.ribbons()[0]->record;
                    if (c.opcode == 0x49) {
                        QCOMPARE(u32(record, 0x50), quint32(50));
                        QCOMPARE(u8(record, 0x83), quint8(2));
                    }
                    if (c.opcode == 0x4a) {
                        QCOMPARE(u32(record, 0x54), quint32(1));
                        QCOMPARE(u32(record, 0x68), u32(before->record, 0x68));
                        if (c.operands[2] == quint32(-1))
                            QCOMPARE(u32(record, 0x50), quint32(25));
                    }
                    if (c.opcode == 0x4b)
                        QCOMPARE(u8(record, 0x86), quint8(1));
                }
                for (int i = 0; i < 28; ++i)
                    ribbons.advance();
                QVERIFY(!ribbons.empty());
                ribbons.advance();
                QVERIFY(ribbons.empty());
                QVERIFY(ribbons.service(0x49, program.commands[point].operands, value));
                QVERIFY(ribbons.service(0x4b, {quint32(-1), 0}, value));
                QVERIFY(ribbons.empty());
            };
            exercise(project.rom(), p);
            p = SceneActions::insert(p, point + 1, 0xe, 30);
            scenes.replace(id, p.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-ribbon-actions-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Ribbon Actions export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void pathSpriteActions() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Authored path headings", source, false);
            const auto base =
                program({0x80000007U, 47,          0x3a,        1,           0,          0, 0,
                         70,          70,          130,         8,           0x49,       0, 3,
                         0,           90,          quint32(-1), 3,           0,          0, 1,
                         0x7d,        0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U});
            const int point = 3; // Insert sprites after the independently editable ribbon.
            const auto positions = SceneActions::positions(base);
            QVERIFY(std::any_of(positions.begin(), positions.end(),
                                [point](const auto &p) { return p.command == point; }));
            auto created = SceneActions::insert(base, point, SceneActions::CreatePathSprite);
            QCOMPARE(created.commands[point].opcode, quint32(0x63));
            QCOMPARE(created.commands[point].operands,
                     QVector<quint32>({0, 30, 1, 0, 0, 0, 180, 3, 0, quint32(-1)}));
            QCOMPARE(SceneActions::remove(created, point).source, base.source);
            created = SceneActions::edit(created, point, 4, "48");
            created = SceneActions::edit(created, point, 6, "4");
            auto authored = SceneActions::insert(created, point + 1, SceneActions::OrientSprite);
            QCOMPARE(authored.commands[point + 1].operands[0], quint32(0));
            QCOMPARE(authored.commands[point + 1].operands[9], quint32(0));
            authored = SceneActions::edit(authored, point + 1, 1, "-1");
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(authored, point + 1, 2, "-1"));
            auto opaque = authored;
            opaque.commands[point + 1].operands[6] = 0x12345678;
            opaque = DirectorProgram::decode(opaque.encode());
            QCOMPARE(SceneActions::edit(opaque, point + 1, 2, "1").commands[point + 1].operands[6],
                     quint32(0x12345678));
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                SceneRibbons paths(rom);
                int value = 0;
                QVERIFY(paths.service(0x49, p.commands[2].operands, value));
                paths.validateRotationSource(4, 0);
                paths.validateRotationSource(-1, 0);
                QVERIFY_THROWS_EXCEPTION(Error, paths.validateRotationSource(-1, 20));
                QVERIFY_THROWS_EXCEPTION(Error, paths.validateRotationSource(59, 0));
                QVERIFY_THROWS_EXCEPTION(Error, paths.validateRotationSource(4, 999));
                SceneSprites sprites(rom);
                const SceneSprites::PreparedRotation rotation = [&](qint32 group, qint32 entry, int,
                                                                    const SpritePosePlayer &) {
                    return paths.rotation(group, entry);
                };
                QVERIFY(sprites.service(0x63, p.commands[point].operands, value, rotation));
                QCOMPARE(qint16(u16(sprites.sprite(0)->record(), 0x40)), paths.rotation(4, 0));
                const auto identity = sprites.identity(0);
                for (int i = point + 1; i < p.commands.size(); ++i)
                    if (p.commands[i].opcode == 0x63)
                        QVERIFY(sprites.service(0x63, p.commands[i].operands, value, rotation));
                QCOMPARE(qint16(u16(sprites.sprite(0)->record(), 0x40)), paths.rotation(-1, 0));
                QCOMPARE(qint32(u32(sprites.sprite(0)->record(), 0x24)), 48);
                QCOMPARE(sprites.identity(0), identity);
            };
            exercise(project.rom(), authored);
            authored = SceneActions::insert(authored, point + 1, 0xe, 30);
            scenes.replace(id, authored.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, authored.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-path-sprite-actions-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, authored.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Path sprite Actions export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void rotatedSpriteActions() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 5 && scenes.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Rotated sprites", source, true);
            const auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            auto stream = SceneActions::insert(base, point, 0x63);
            QCOMPARE(
                stream.commands[point].operands,
                QVector<quint32>({0, 30, 1, 0, 0, 0, 180, quint32(-1), quint32(-1), quint32(-1)}));
            QCOMPARE(SceneActions::remove(stream, point).source, base.source);
            QCOMPARE(SceneActions::insert(stream, point + 1, 0x46).commands[point + 1].operands[0],
                     quint32(1));
            for (const auto &[field, text] : QVector<QPair<int, QString>>{
                     {0, "29"}, {3, "3"}, {4, "-17"}, {5, "23"}, {6, "270"}})
                stream = SceneActions::edit(stream, point, field, text);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(stream, point, 6, "32768"));
            auto reused = SceneActions::insert(stream, point + 1, SceneActions::RotateSprite);
            QCOMPARE(reused.commands[point + 1].opcode, quint32(0x63));
            QCOMPARE(reused.commands[point + 1].operands[0], quint32(29));
            QCOMPARE(reused.commands[point + 1].operands[9], quint32(0));
            reused = SceneActions::edit(reused, point + 1, 1, "90");
            auto opaque = reused;
            opaque.commands[point + 1].operands[1] = 0x12345678;
            opaque.commands[point + 1].operands[4] = 0x87654321;
            opaque.commands[point + 1].operands[7] = 0x7ffffffe;
            opaque.commands[point + 1].operands[9] = 7; // Every non-minus-one literal reuses.
            opaque = DirectorProgram::decode(opaque.encode());
            const auto edited = SceneActions::edit(opaque, point + 1, 1, "-90");
            auto expected = opaque;
            expected.commands[point + 1].operands[6] = quint32(-90);
            QCOMPARE(edited.source, expected.encode());
            auto symbolic = stream;
            symbolic.commands[point].operands[4] = 0x08880001;
            symbolic = DirectorProgram::decode(symbolic.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(symbolic, point, 4, "12"));
            QCOMPARE(SceneActions::edit(symbolic, point, 6, "90").commands[point].operands[4],
                     quint32(0x08880001));
            auto pathDriven = stream;
            pathDriven.commands[point].operands[8] = 0x08880003;
            pathDriven = DirectorProgram::decode(pathDriven.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(pathDriven, point, 6, "90"));
            const auto moved = SceneActions::move(reused, point + 1, point);
            QCOMPARE(SceneActions::move(moved, point, point + 2).source, reused.source);
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                SceneSprites sprites(rom);
                int value = 0;
                QVERIFY(sprites.service(0x63, p.commands[point].operands, value));
                const auto initial = sprites.sprite(29)->record();
                QCOMPARE(qint32(u32(initial, 0x24)), -17);
                QCOMPARE(qint32(u32(initial, 0x28)), -23);
                QCOMPARE(u8(initial, 0x4b), quint8(0));
                QCOMPARE(u8(initial, 0x4a), quint8(1));
                QCOMPARE(u16(initial, 0x48), quint16(100));
                QCOMPARE(u16(initial, 0x40), quint16(270));
                QCOMPARE(u8(initial, 0x4c), quint8(3));
                const auto identity = sprites.identity(29);
                const auto reuse = std::find_if(p.commands.begin() + point + 1, p.commands.end(),
                                                [](const auto &c) { return c.opcode == 0x63; });
                QVERIFY(reuse != p.commands.end());
                QVERIFY(sprites.service(0x63, reuse->operands, value));
                auto rotated = initial;
                put16(rotated, 0x40, 90);
                QCOMPARE(sprites.sprite(29)->record(), rotated);
                QCOMPARE(sprites.identity(29), identity);
                SceneSprites missing(rom);
                QVERIFY_THROWS_EXCEPTION(Error, missing.service(0x63, reuse->operands, value));
            };
            exercise(project.rom(), reused);
            const auto outputProgram = SceneActions::insert(reused, point + 1, 0xe, 30);
            scenes.replace(id, outputProgram.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, outputProgram.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-rotated-sprites-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, outputProgram.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rotated sprite export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneSpriteActions() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 5 && scenes.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Authored sprites", source, true);
            const auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            auto stream = SceneActions::insert(base, point, 0x46);
            QCOMPARE(stream.commands[point].operands,
                     QVector<quint32>({0, 30, 1, 2, 0, 0, 0, 100, 0}));
            QCOMPARE(SceneActions::remove(stream, point).source, base.source);
            auto withActor = SceneActions::insert(base, point, 0x14);
            auto separate = SceneActions::insert(withActor, point, 0x46);
            QCOMPARE(separate.commands[point].operands[0], quint32(0));
            QCOMPARE(separate.commands[point + 1].operands[0], quint32(0));
            for (const auto &action : SceneActions::inspect(stream))
                if (action.command == point) {
                    QCOMPARE(action.actorOperand, -1);
                    QCOMPARE(action.fields.size(), 9);
                }
            for (const auto &[field, text] : QVector<QPair<int, QString>>{
                     {0, "29"}, {4, "-17"}, {5, "23"}, {6, "2"}, {7, "125"}, {8, "1"}})
                stream = SceneActions::edit(stream, point, field, text);
            const auto authored = stream;
            auto alias = stream;
            alias.commands[point].operands[0] = 0x100; // Constructors narrow to the low byte.
            alias = DirectorProgram::decode(alias.encode());
            QCOMPARE(SceneActions::insert(alias, point, 0x46).commands[point].operands[0],
                     quint32(1));
            QCOMPARE(SceneActions::insert(alias, point + 1, 0x64).commands[point + 1].operands[0],
                     quint32(0));
            alias.commands[point].operands[0] = 0x08880001;
            alias = DirectorProgram::decode(alias.encode());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(alias, point, 0x46));
            QCOMPARE(SceneActions::insert(alias, point + 1, 0x64).commands[point + 1].operands[0],
                     quint32(0x08880001));
            QCOMPARE(SceneActions::insert(alias, point + 1, 0x66).commands[point + 1].operands[0],
                     quint32(0x08880001));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(stream, point, 0, "30"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(stream, point, 5, "1.5"));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(stream, point, 8, "256"));
            auto pose = SceneActions::insert(stream, point + 1, 0x64);
            QCOMPARE(pose.commands[point + 1].operands,
                     QVector<quint32>({29, quint32(-1), quint32(-1), 0}));
            pose = SceneActions::edit(pose, point + 1, 1, "57");
            pose = SceneActions::edit(pose, point + 1, 2, "0");
            auto released = SceneActions::insert(pose, point + 2, 0x66);
            QCOMPARE(released.commands[point + 2].operands, QVector<quint32>({29}));
            auto reordered = SceneActions::move(released, point + 1, point);
            QCOMPARE(SceneActions::move(reordered, point, point + 2).source, released.source);
            QCOMPARE(SceneActions::remove(released, point + 2).source, pose.source);
            auto exercise = [&](const Rom &rom, const DirectorProgram &p) {
                SceneSprites sprites(rom);
                int value = 0;
                QVERIFY(sprites.service(0x46, p.commands[point].operands, value));
                const auto record = sprites.sprite(29)->record();
                QCOMPARE(qint32(u32(record, 0x24)), -17);
                QCOMPARE(qint32(u32(record, 0x28)), -23);
                QCOMPARE(u8(record, 0x4a), quint8(2));
                QCOMPARE(u16(record, 0x48), quint16(125));
                QCOMPARE(u8(record, 0x4c), quint8(1));
                QVERIFY(sprites.advancePoses());
                QVector<quint32> poseArgs, releaseArgs;
                for (const auto &c : p.commands) {
                    if (c.opcode == 0x64)
                        poseArgs = c.operands;
                    if (c.opcode == 0x66)
                        releaseArgs = c.operands;
                }
                QVERIFY(!poseArgs.isEmpty() && !releaseArgs.isEmpty());
                QVERIFY(sprites.service(0x64, poseArgs, value));
                QCOMPARE(u32(sprites.sprite(29)->record(), 0x30), quint32(57));
                QCOMPARE(u16(sprites.sprite(29)->record(), 0x44), quint16(0));
                QVERIFY(sprites.service(0x66, releaseArgs, value));
                QVERIFY(!sprites.sprite(29));
            };
            exercise(project.rom(), released);
            auto crowded = base;
            for (int i = 0; i < 30; ++i)
                crowded = SceneActions::insert(crowded, point, 0x46);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(crowded, point, 0x46));
            auto opaque = authored;
            opaque.commands[point].operands[6] = 0x12345678;
            opaque = DirectorProgram::decode(opaque.encode());
            const auto retained = SceneActions::edit(opaque, point, 4, "19");
            QCOMPARE(retained.commands[point].operands[6], quint32(0x12345678));
            // Preserve a working initial presentation and delay before its native exit.
            auto outputProgram = SceneActions::insert(released, point + 1, 0xe, 30);
            scenes.replace(id, outputProgram.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, outputProgram.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-sprite-actions-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, outputProgram.source);
            exercise(reopened.rom(), reopened.scenePrograms()->program(id));
            reopened.scenePrograms()->validateReferences(reopened.rom());
            qInfo().noquote() << "Authored sprite export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
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
            if (kind == 0x2a || kind == 0x50) {
                QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 3, kind));
                continue;
            }
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
    void choiceActions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            project.loadDialogues();
            auto scenes = *project.scenePrograms();
            const auto id = scenes.createScene("Two branch choice", 0x1fa8bf4 - 0x594284, true);
            const auto blank = scenes.program(id);
            const int at = SceneActions::positions(blank).last().command;
            auto p = SceneActions::insert(blank, at, SceneActions::Choice);
            QCOMPARE(SceneActions::choices(p).size(), 1);
            QCOMPARE(SceneActions::remove(p, at).source, blank.source);
            p = SceneActions::edit(p, at, 0, "7");
            QCOMPARE(SceneActions::choices(p).size(), 1); // Every close/query follows the new ID.
            auto text = *project.dialogues();
            const auto bytes = SceneActions::choiceText("Continue", "Return");
            const int entry = text.appendEntry(Dialogues::Table::Primary, 5, bytes);
            p = SceneActions::edit(p, at, 2, QString::number(entry));
            for (int option : {1, 2}) {
                auto choice = SceneActions::choices(p).first();
                const int end = option == 1 ? choice.firstEnd : choice.secondEnd;
                p = SceneActions::insert(p, end, 0x48);
                p = SceneActions::edit(p, end, 1, QString::number(option * 70));
                p = SceneActions::insert(p, end + 1, 0xe, 3);
            }
            auto choice = SceneActions::choices(p).first();
            const auto moved = SceneActions::move(p, choice.firstBegin, choice.secondEnd);
            auto after = SceneActions::choices(moved).first();
            QCOMPARE(SceneActions::move(moved, after.secondEnd - 1, after.firstBegin).source,
                     p.source);
            const auto movedChoice = SceneActions::insert(p, choice.end, 0x48);
            const auto reordered = SceneActions::move(movedChoice, at, choice.end + 1);
            QCOMPARE(SceneActions::move(reordered, at + 1, at).source, movedChoice.source);
            QCOMPARE(SceneActions::remove(p, at).source, blank.source);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::choiceText("@1", "Return"));
            QCOMPARE(SceneActions::choiceOptions(bytes)->second, QString("Return"));
            // A foreign jump into a choice prevents treating the block as independently movable.
            auto shared = p;
            shared.commands.insert(choice.end, {0, 0x59, {p.commands[at + 7].operands[0]}});
            shared = DirectorProgram::decode(shared.encode());
            QVERIFY(SceneActions::choices(shared).isEmpty());
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(shared, at));
            scenes.replace(id, p.source);
            project.setScenePrograms(scenes.json());
            project.setDialogues(text.json());
            auto exercise = [&](const Project &current) {
                for (int option : {1, 2}) {
                    SceneResources pool({8, 0xffff, 13});
                    SceneDialogue windows(0);
                    DialoguePlayback playback(current.rom(), current.dialogues());
                    auto environment = DialoguePlayback::environment(current.rom());
                    environment.substitutions[48] = QByteArray("Joe\0", 4);
                    DirectorSession director(
                        current.scenePrograms()->runtimeDirectors(),
                        current.scenePrograms()->resource(id).selectors.first(), 0);
                    QVector<int> observed;
                    auto service = [&](quint32 op, const QVector<quint32> &args, int &value) {
                        if (op == 0xbf)
                            windows.create(args, pool);
                        else if (op == 0x25)
                            value = windows.control(args[2]);
                        else if (op == 0x10)
                            value = windows.query(args[2], pool);
                        else if (op == 5)
                            windows.close(args[0], pool);
                        else if (op == 0x48)
                            observed << int(args[1]);
                        return true;
                    };
                    bool down = false, confirmed = false, released = false;
                    for (int update = 0; update < 300 && !released; ++update) {
                        DialogueInput input{8192, 0, 0, 0};
                        if (playback.state(0, pool) == 7 && !confirmed) {
                            if (option == 2 && !down) {
                                input.repeat = 0x400;
                                down = true;
                            } else {
                                input.pressed = 0x8000;
                                confirmed = true;
                            }
                        }
                        playback.advance(pool, windows, environment, input);
                        director.advanceCounter();
                        const int result = director.dispatch(service);
                        QVERIFY2(result == LORDLY_DIRECTOR_YIELD ||
                                     result == LORDLY_DIRECTOR_RELEASE,
                                 qPrintable(director.reason()));
                        released = result == LORDLY_DIRECTOR_RELEASE;
                    }
                    QVERIFY(confirmed);
                    QVERIFY(released);
                    QCOMPARE(observed, QVector<int>{option * 70});
                }
            };
            exercise(project);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, p.source);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-choice-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            reopened.loadDialogues();
            QCOMPARE(reopened.scenePrograms()->program(id).source, p.source);
            QCOMPARE(SceneActions::choices(reopened.scenePrograms()->program(id)).size(), 1);
            QCOMPARE(reopened.dialogues()->entry(Dialogues::Table::Primary, 5, entry), bytes);
            exercise(reopened);
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote()
                << "Both real text selections, branch waits and exported/reopened choices passed:"
                << destination;
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
            const auto text =
                Dialogues::spokenText("Joe", "One\nTwo\nThree\nFour\nFive\nSix\nSeven");
            QCOMPARE(Dialogues::spokenFields(text)->second,
                     QString("One\nTwo\nThree\nFour\nFive\nSix\nSeven"));
            QVERIFY_THROWS_EXCEPTION(Error, Dialogues::spokenText("Joe", "@n"));
            QCOMPARE(
                Dialogues::spokenFields(Dialogues::spokenText("Joe", QString(61, 'x')))->second,
                QString(30, 'x') + '\n' + QString(30, 'x') + "\nx");
            const int entry = dialogues.appendEntry(Dialogues::Table::Primary, 5, text);
            project.setDialogues(dialogues.json());
            auto exerciseText = [&](const Project &current) {
                DialogueSession textPreview(current.rom(), 5, quint16(entry),
                                            DialoguePlayback::environment(current.rom()),
                                            current.dialogues());
                int confirms = 0;
                for (int tick = 0; tick < 500 && textPreview.state() != 4; ++tick) {
                    const auto state = textPreview.state();
                    const quint16 pressed = state == 3 || state == 6 ? 0x8000 : 0;
                    confirms += pressed != 0;
                    textPreview.advance({8192, pressed, 0, 0});
                }
                QCOMPARE(textPreview.state(), 4);
                QCOMPARE(confirms, 3); // Two page breaks and the final A prompt.
            };
            exerciseText(project);
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
            exerciseText(reopened);
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
    void classActorCreation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            const auto id = scenes.createScene("Class Actor scene", 0x1fa8bf4 - 0x594284, true);
            const auto blank = scenes.program(id);
            auto created = SceneActions::insert(blank, 1, 0x2a);
            QCOMPARE(created.commands[1].opcode, quint32(0x14));
            QCOMPARE(created.commands[2].opcode, quint32(0x2a));
            QCOMPARE(SceneActions::inspect(created).first().commandCount, 2);
            QCOMPARE(SceneActions::remove(created, 1).source, blank.source);
            for (const auto &point : SceneActions::positions(created))
                QVERIFY(point.command != 2);
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(created, 2, 7));
            QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(created, 2));
            const auto opaqueInitializer = created.commands[1].operands.mid(1, 2);
            created = SceneActions::edit(created, 1, 0, "5");
            created = SceneActions::edit(created, 1, 1, "84");
            created = SceneActions::edit(created, 1, 2, "85");
            created = SceneActions::edit(created, 1, 4, "12.375");
            created = SceneActions::edit(created, 1, 6, "-2.500");
            QCOMPARE(created.commands[1].operands[0], quint32(5));
            QCOMPARE(created.commands[2].operands[0], quint32(5));
            QCOMPARE(created.commands[1].operands.mid(1, 2), opaqueInitializer);
            QCOMPARE(created.commands[2].operands.mid(3, 2), (QVector<quint32>{0, 0}));
            auto unusual = created;
            unusual.commands[2].operands = {5, 0x08880001, quint32(-1), 0x12340000, 0xabcd0001, quint32(-1)};
            unusual = DirectorProgram::decode(unusual.encode());
            auto expectedUnusual = unusual.source;
            put32(expectedUnusual, (unusual.commands[1].word + 5) * 4, 2500);
            QCOMPARE(SceneActions::edit(unusual, 1, 4, "2.500").source, expectedUnusual);
            auto exercise = [&](const Rom &rom, BodyAnimationData::Pointer data,
                                const DirectorProgram &stream, int primary, int secondary) {
                SceneActors actors(rom, 2, 0, [](int, float, float) { return 0.f; }, data);
                int value = 0;
                QVERIFY(actors.service(0x14, stream.commands[1].operands, value));
                const auto before = actors.actor(5)->record();
                QVERIFY_THROWS_EXCEPTION(Error,
                                         actors.service(0x2a, stream.commands[2].operands, value));
                QCOMPARE(actors.actor(5)->record(), before);
                actors.supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
                QVERIFY(actors.service(0x2a, stream.commands[2].operands, value));
                const auto record = actors.actor(5)->record();
                QCOMPARE(u32(record, 0xe8), quint32(primary));
                QCOMPARE(u32(record, 0xec), quint32(secondary));
                QCOMPARE(u16(record, 0x138), quint16(0));
                QCOMPARE(std::bit_cast<float>(u32(record, 0x11c)), 12.375f);
                QCOMPARE(std::bit_cast<float>(u32(record, 0x124)), -2.5f);
                QVERIFY(actors.actor(5)->pose().art().isBody());
            };
            exercise(project.rom(), project.bodyAnimationData(), created, 84, 85);
            auto two = SceneActions::insert(created, 3, 0x2a);
            QCOMPARE(two.commands[3].operands[0], quint32(0));
            QCOMPARE(two.commands[4].operands[0], quint32(0));
            auto reordered = SceneActions::move(two, 1, 5);
            QCOMPARE(reordered.commands[1].operands[0], quint32(0));
            QCOMPARE(reordered.commands[3].operands[0], quint32(5));
            QCOMPARE(SceneActions::move(reordered, 3, 1).source, two.source);
            QCOMPARE(SceneActions::remove(two, 3).source, created.source);
            auto mismatch = created;
            mismatch.commands[2].operands[0] = 6;
            QCOMPARE(SceneActions::inspect(DirectorProgram::decode(mismatch.encode()))
                         .first()
                         .commandCount,
                     1);
            const int newClass = project.appendClass(2, "Scene Actor");
            created = SceneActions::edit(created, 1, 1, QString::number(newClass));
            created = SceneActions::edit(created, 1, 2, QString::number(newClass));
            exercise(project.rom(), project.bodyAnimationData(), created, newClass, newClass);
            scenes.replace(id, created.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, created.source);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-class-actor-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, created.source);
            exercise(reopened.rom(), reopened.bodyAnimationData(),
                     reopened.scenePrograms()->program(id), newClass, newClass);
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopened.rom().bytes().size()));
            auto crc = reopened.rom().bytes();
            Rom::updateChecksum(crc);
            QCOMPARE(crc, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Created class Actor candidate:" << destination;
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
