#include "engine/scene_events.h"
#include "engine/battlefield_clone.h"
#include "engine/event_authoring.h"
#include "engine/scenario_seeder.h"
#include "engine/story_routes.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

using namespace lordly;

class MissionProgramCapacityTests final : public QObject {
    Q_OBJECT
  private slots:
    void exportAllBattlefieldsAndMissions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 toolchain required");
        try {
            const auto rom = Rom::open(path);
            const auto sourceDigest = sha256(rom.bytes());
            const auto prototype = [&] {
                auto project = Project::fromRom(rom);
                project.createScenarioBattlefield(MissionTables::firstKey, 1, "blank", "Blank prototype");
                return project.scenarioBattlefields().first().toObject();
            }();
            auto project = Project::fromRom(rom);
            QTemporaryDir stockWork;
            QVERIFY(stockWork.isValid());
            QCOMPARE(rebuild(project, tools, stockWork.path()).image, rom.bytes());

            constexpr int location = 6;
            constexpr int count = SceneEvents::customRowCapacity;
            static_assert(MissionTables::keyCount - MissionTables::firstKey == count);
            auto missionName = [](int index) { return QString("Capacity mission %1").arg(index + 1); };
            QJsonArray battlefields;
            for (int i = 0; i < count; ++i) {
                auto row = prototype;
                row["key"] = MissionTables::firstKey + i;
                row["name"] = missionName(i);
                auto snapshot = row["snapshot"].toObject();
                auto units = snapshot["units"].toObject();
                auto scenario = units["scenario"].toObject();
                auto layout = Eset::decode(QByteArray::fromHex(scenario["layout"].toString().toLatin1()));
                layout.setStartTime(i / 60, i % 60);
                scenario["layout"] = QString::fromLatin1(layout.encode().toHex());
                units["scenario"] = scenario;
                snapshot["units"] = units;
                row["snapshot"] = snapshot;
                battlefields.append(row);
            }
            auto manifest = project.manifest();
            manifest["scenarioBattlefields"] = battlefields;
            project.applyManifest(manifest);
            // Retail revisions plus all 64 added fields cross the archive's
            // signed-byte boundary; key132 must still load its own ESET.
            project.loadScenarios();
            auto editedScenarios = *project.scenarios();
            editedScenarios.setStartTime(1, 23, 58, project.classData());
            editedScenarios.setStartTime(2, 23, 59, project.classData());
            project.setScenarios(editedScenarios.json());
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            int prerequisite = -1;
            for (int i = 0; i < count; ++i) {
                const int row = events.createMission(location, MissionTables::firstKey + i,
                    missionName(i), -1, -1, *project.scenePrograms(), prerequisite);
                QCOMPARE(row, SceneEvents::firstCustomRow + i);
                prerequisite = events.customFlags().last().toObject()["id"].toInt();
                QCOMPARE(prerequisite, SceneEvents::firstCustomFlag + i);
                if (i + 1 < count) {
                    auto trees = events.flagEdits()[QString::number(row)].toObject()["trees"].toArray();
                    auto victory = trees[1].toObject();
                    auto nodes = victory["nodes"].toArray();
                    auto terminal = nodes.last().toObject();
                    terminal["nextMission"] = true;
                    nodes.replace(nodes.size() - 1, terminal);
                    victory["nodes"] = nodes;
                    trees.replace(1, victory);
                    events.setTrees(row, trees, *project.scenePrograms());
                }
            }
            project.setSceneEvents(events.json());

            QTemporaryDir work;
            QVERIFY(work.isValid());
            const auto built = rebuild(project, tools, work.path());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum.mid(0x10, 8), built.image.mid(0x10, 8));
            QVERIFY(built.image.size() <= 0x4000000);
            const auto output = Rom::fromBytes(built.image);
            const auto outputPath = qEnvironmentVariable("LORDLY_MISSION_CAPACITY_OUTPUT");
            if (!outputPath.isEmpty()) {
                QFile fixture(outputPath);
                QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::NewOnly));
                const auto serialized = rom.serialize(built.image);
                QCOMPARE(fixture.write(serialized), serialized.size());
            }

            auto reopened = Project::fromRom(output);
            reopened.loadSceneEvents();
            reopened.loadScenarios();
            reopened.loadWorld();
            QCOMPARE(reopened.scenarioBattlefields(), battlefields);
            QCOMPARE(reopened.sceneEvents()->json(), events.json());
            const StoryRoutes routes(output);
            const auto entries = routes.routes(location, 50);
            QMap<int, StoryRoute> authored;
            for (const auto &route : entries)
                if (SceneEvents::customMissionRow(route.event))
                    authored[route.event] = route;
            QCOMPARE(authored.size(), count);

            QSet<int> models;
            const auto resources = ScenarioResources::load(output);
            const auto worldResources = WorldResources::load(output);
            for (int i = 0; i < count; ++i) {
                const int key = MissionTables::firstKey + i;
                const int row = SceneEvents::firstCustomRow + i;
                const auto &scenario = reopened.scenarios()->scenario(key);
                QCOMPARE(scenario.name, missionName(i));
                QVERIFY(scenario.squads.isEmpty());
                QCOMPARE(reopened.world()->siteCount(key), 2);
                QCOMPARE(worldResources.treasure(output, key), QByteArray(1, 0));
                const auto layout = Eset::decode(resources.scenario(output, key));
                QCOMPARE(layout.startHour(), i / 60);
                QCOMPARE(layout.startMinute(), i % 60);
                models.insert(u8(ScenarioResources::row(output, key), 3));
                QCOMPARE(SceneEvents::readRow(output, row), events.program(row));
                QCOMPARE(routes.keys[row], QVector<int>{key});
                const int flag = SceneEvents::firstCustomFlag + i;
                QCOMPARE(authored[row].requirements.value(flag, -1), 0);
                if (i)
                    QCOMPARE(authored[row].requirements.value(flag - 1, -1), 1);
            }
            QCOMPARE(models.size(), count);
            const int lastKey = MissionTables::keyCount - 1;
            const int lastRow = SceneEvents::rowCapacity - 1;
            QVERIFY(resources.selector(lastKey) >= 128);
            BattlefieldClone::validate(output, lastKey, prototype["snapshot"].toObject()["art"].toObject());
            reopened.loadMapEdits();
            QCOMPARE(reopened.mapEdits()->source(lastKey).map.points.size(), 2);

            const ScenarioSeeder seeder(output);
            const auto &choices = seeder.choices();
            const auto last = std::find_if(choices.begin(), choices.end(),
                [=](const SeedChoice &choice) { return choice.route.event == lastRow; });
            QVERIFY(last != choices.end());
            QCOMPARE(last->scenarios, QVector<int>{lastKey});
            const auto seeded = SaveDocument::open(output, seeder.seed(int(last - choices.begin())));
            QVERIFY(seeded.battery());
            QVERIFY(seeded.saveSlots().first().valid);

            QTemporaryDir repeatWork;
            QVERIFY(repeatWork.isValid());
            const auto repeated = rebuild(reopened, tools, repeatWork.path()).image;
            if (repeated != built.image) {
                qsizetype first = 0;
                while (first < qMin(repeated.size(), built.image.size()) && repeated[first] == built.image[first]) ++first;
                qInfo() << "Repeated export differs at z64" << Qt::hex << first
                        << "sizes" << repeated.size() << built.image.size();
                if (!outputPath.isEmpty()) {
                    const QFileInfo file(outputPath);
                    const auto folder = file.absolutePath() + "/repeat";
                    QVERIFY(QDir().mkpath(folder));
                    atomicWrite(folder + "/" + file.fileName(), rom.serialize(repeated));
                }
            }
            QCOMPARE(repeated, built.image);
            QCOMPARE(sha256(Rom::open(path).bytes()), sourceDigest);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }

    void allSlotsReopen() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto scenes = ScenePrograms::load(rom);
            const auto pristine = SceneEvents::load(rom);
            QCOMPARE(SceneProgramResources::rebuild(rom, {}), rom.bytes());

            // Build a pre-expansion image with the original generated completion
            // nodes. Reopening authenticates those bytes and migrates the nodes.
            auto initial = pristine;
            const int oldFlag = initial.createFlag("Legacy A completed");
            QCOMPARE(initial.createFlag("Legacy B completed"), oldFlag + 1);
            auto legacy = initial.plan();
            for (int row : {2, 3}) {
                auto trees = EventAuthoring::missionTemplates(1, 2, oldFlag + row - 2, -1, -1);
                auto victory = trees[1].toObject();
                auto nodes = victory["nodes"].toArray();
                nodes.replace(nodes.size() - 1, QJsonObject{
                    {"type", row == 2 ? "battleResults" : "returnToMap"}, {"location", 2}});
                victory["nodes"] = nodes;
                trees.replace(1, victory);
                legacy.flags[QString::number(row)] =
                    QJsonObject{{"missionName", QString("Legacy %1").arg(row)}, {"trees", trees}};
            }
            for (const int row : SceneEvents::flagRows(legacy.flags))
                legacy.rows[row] = SceneEvents::flagRow(SceneEvents::retailRow(rom, row), row, legacy.flags);
            const auto oldRom = Rom::fromBytes(SceneProgramResources::rebuild(rom, {}, {}, &legacy));
            auto events = SceneEvents::load(oldRom);
            QVERIFY(events.modified());
            QVERIFY(events.program(2) != SceneEvents::readRow(oldRom, 2));
            QVERIFY(events.program(3) != SceneEvents::readRow(oldRom, 3));
            QCOMPARE(events.missionName(3), QString("Legacy 3"));

            QMap<int, int> completion;
            for (int n = 0; n < SceneEvents::customRowCapacity; ++n) {
                const int prerequisite = n ? completion.value(n - 1) : -1;
                const int row = events.createMission(2, 1, QString("New mission %1").arg(n + 1),
                                                     -1, -1, scenes, prerequisite);
                QCOMPARE(row, SceneEvents::firstCustomRow + n);
                completion[n] = events.customFlags().last().toObject()["id"].toInt();
            }
            QVERIFY_EXCEPTION_THROWN(events.createMission(13, 1, "Overflow", -1, -1, scenes), Error);
            const int firstRow = SceneEvents::firstCustomRow;
            events.unlockNodeAfter(firstRow, 3, true);
            const auto firstProgram = events.program(firstRow);
            const auto victoryList = u16(firstProgram, 10);
            const auto unlockHandler = u16(firstProgram, victoryList);
            const auto authored = events.flagEdits()[QString::number(firstRow)].toObject()["trees"].toArray();
            const auto positions = EventAuthoring::entries(events.authoredBase(firstRow), firstRow, authored, {});
            QCOMPARE(firstProgram.mid(unlockHandler, 12), QByteArray::fromHex("6803690271b0690181502f00"));
            QCOMPARE(int(u16(firstProgram, victoryList + 2)), positions["mission-victory"]);
            const auto plan = events.plan();
            auto image = SceneProgramResources::rebuild(oldRom, {}, {}, &plan);
            const auto reopened = Rom::fromBytes(image);
            QCOMPARE(SceneProgramResources::eventRowCount(reopened), SceneEvents::rowCapacity);
            const auto reopenedEvents = SceneEvents::load(reopened);
            QCOMPARE(reopenedEvents.json(), events.json());
            QVERIFY(!reopenedEvents.modified());
            QVERIFY(reopenedEvents.scenarioRows(1).contains(SceneEvents::rowCapacity - 1));
            for (int row : plan.rows.keys())
                QCOMPARE(SceneEvents::readRow(reopened, row), plan.rows[row]);
            QCOMPARE(SceneEvents::readRow(reopened, 114), SceneEvents::readRow(rom, 114));
            const auto repeated = reopenedEvents.plan();
            QCOMPARE(SceneProgramResources::rebuild(reopened, {}, {}, &repeated), image);

            const auto oldDirectory = SceneEvents::directory;
            const auto relocated = SceneProgramResources::expandedEventDirectory;
            constexpr int retailSize = 4 + 4 * SceneEvents::retailRowCount;
            constexpr int extendedSize = 4 + 4 * SceneEvents::rowCapacity;
            QCOMPARE(image.mid(relocated + 4, retailSize - 4), image.mid(oldDirectory + 4, retailSize - 4));
            QCOMPARE(image.mid(oldDirectory + retailSize, 64), rom.bytes().mid(oldDirectory + retailSize, 64));
            QCOMPARE(image.mid(relocated - 64, 64), rom.bytes().mid(relocated - 64, 64));
            QCOMPARE(image.mid(relocated + extendedSize, 64), rom.bytes().mid(relocated + extendedSize, 64));
            auto occupied = oldRom.bytes();
            put8(occupied, relocated + extendedSize - 1, 1);
            QVERIFY_EXCEPTION_THROWN(SceneProgramResources::rebuild(Rom::fromBytes(occupied), {}, {}, &plan), Error);
            auto damaged = image;
            put32(damaged, SceneProgramResources::eventWord(178), 0);
            QVERIFY_EXCEPTION_THROWN(SceneEvents::load(Rom::fromBytes(damaged)), Error);

            const StoryRoutes routes(reopened);
            const auto first = routes.routes(2, 50, {{completion[0], 0}});
            QVERIFY(!first.isEmpty());
            QCOMPARE(first.first().event, 115);
            QSet<int> reachable;
            for (const auto &route : routes.routes(2, 50))
                if (route.event >= SceneEvents::firstCustomRow)
                    reachable.insert(route.event);
            QCOMPARE(reachable.size(), SceneEvents::customRowCapacity);
            QMap<int, int> finalFlags;
            for (int n = 0; n < 64; ++n)
                finalFlags[completion[n]] = n != 63;
            const auto last = routes.routes(2, 50, finalFlags);
            QCOMPARE(last.size(), 1);
            QCOMPARE(last.first().event, 178);
            QCOMPARE(last.first().clearFrom, 0);
            QCOMPARE(last.first().clearTo, 0);
            finalFlags[completion[63]] = 1;
            QCOMPARE(routes.routes(2, 50, finalFlags), StoryRoutes(rom).routes(2, 50, finalFlags));

            // Removing every authored row restores the original directory and all
            // native key constructors.
            const SceneEventBuild cleared;
            QCOMPARE(SceneProgramResources::rebuild(reopened, {}, {}, &cleared), rom.bytes());

            const auto output = qEnvironmentVariable("LORDLY_MISSION_PROGRAM_OUTPUT");
            if (!output.isEmpty()) {
                Rom::updateChecksum(image);
                QFile fixture(output);
                QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::NewOnly));
                const auto serialized = rom.serialize(image);
                QCOMPARE(fixture.write(serialized), serialized.size());
            }
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
};

QTEST_APPLESS_MAIN(MissionProgramCapacityTests)
#include "mission_program_capacity_tests.moc"
