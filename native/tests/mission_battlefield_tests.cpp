#include "engine/mission_battlefield.h"
#include "engine/squad_members.h"
#include <QJsonArray>
#include <QtTest>

using namespace lordly;
class MissionBattlefieldTests final : public QObject {
    Q_OBJECT
  private slots:
    void snapshotAndIndependentResources() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto classes = ClassData::load(rom);
            auto scenarios = Scenarios::load(rom);
            auto world = ScenarioWorld::load(rom);
            const auto originalScenarios = scenarios.json();
            const auto originalWorld = world.json();
            scenarios.setStartTime(2, 18, 25, classes);
            const auto snapshot = MissionBattlefield::capture(rom, scenarios, world, 2, "exact");
            scenarios.setStartTime(2, 9, 0, classes);
            const auto image = MissionBattlefield::prepare(rom, 10, snapshot);
            const auto derived = Rom::fromBytes(image);
            auto copied = Scenarios::load(derived);
            auto copiedWorld = ScenarioWorld::load(derived);
            QCOMPARE(copied.scenario(10).layout.startHour(), 18);
            QCOMPARE(copied.scenario(10).layout.startMinute(), 25);
            QCOMPARE(copied.scenario(10).json(), snapshot["scenario"].toObject());
            QCOMPARE(copied.scenario(2).json(),
                     originalScenarios["scenarios"].toObject()["2"].toObject());
            QCOMPARE(copiedWorld.scenario(10).sites, world.scenario(2).sites);
            QCOMPARE(copiedWorld.scenario(2), world.scenario(2));
            QCOMPARE(copiedWorld.json()["towns"], originalWorld["towns"]);
            QSet<int> donorFlags, cloneFlags;
            for (int i = 0; i < ScenarioWorld::treasureCount(world.scenario(2).treasures); ++i)
                donorFlags.insert(u8(world.scenario(2).treasures, 1 + i * 6));
            for (int i = 0; i < ScenarioWorld::treasureCount(copiedWorld.scenario(10).treasures); ++i)
                cloneFlags.insert(u8(copiedWorld.scenario(10).treasures, 1 + i * 6));
            QVERIFY(!cloneFlags.intersects(donorFlags));
            auto record = copied.scenario(10).squads.first();
            record[1] = char(qint8(u8(record, 1)) + 1);
            copied.setSquad(10, 0, record, classes);
            QCOMPARE(copied.scenario(2).json(),
                     originalScenarios["scenarios"].toObject()["2"].toObject());
            copiedWorld.setSiteAllegiance(10, 0, 0);
            QCOMPARE(copiedWorld.scenario(2), world.scenario(2));
            const int count = copied.scenario(10).squads.size();
            copied.setAuthoredScenario(10);
            copied.setRouteGroup(10, count - 1, 0, 0, classes);
            copied.removeSquad(10, count - 1, classes);
            QCOMPARE(copied.scenario(10).squads.size(), count - 1);
            QCOMPARE(copied.scenario(2).json(),
                     originalScenarios["scenarios"].toObject()["2"].toObject());
            QCOMPARE(MissionBattlefield::prepare(derived, 10, snapshot), image);
            QCOMPARE(sha256(rom.bytes()), sha256(Rom::open(path).bytes()));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void mapWithoutSquads() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto scenarios = Scenarios::load(rom);
            const auto world = ScenarioWorld::load(rom);
            const auto snapshot = MissionBattlefield::capture(rom, scenarios, world, 2, "mapOnly");
            auto derived = Rom::fromBytes(MissionBattlefield::prepare(rom, 10, snapshot));
            const auto copied = Scenarios::load(derived);
            QVERIFY(copied.scenario(10).squads.isEmpty());
            QVERIFY(copied.scenario(10).layout.deployments.isEmpty());
            QCOMPARE(copied.scenario(10).layout.nodes.size(), 1);
            QVERIFY(copied.scenario(10).layout.conditions.isEmpty());
            copied.validateForExport(ClassData::load(rom));
            const auto emptySites = WorldResources::load(rom).siteSelector(10);
            QCOMPARE(emptySites, 0);
            QVERIFY(WorldResources::load(derived).siteSelector(10) > 0);
            const auto copiedWorld = ScenarioWorld::load(derived);
            auto expectedSites = world.scenario(2).sites;
            // An empty mission must retain an enemy base to capture even when
            // the donor's script would normally set its allegiance later.
            for (int site = 0; site < world.siteCount(2); ++site)
                if (u8(world.town(u16(expectedSites, 1 + site * 4)), 24) == 255)
                    put16(expectedSites, 3 + site * 4, 4);
            QCOMPARE(copiedWorld.scenario(10).sites, expectedSites);
            QCOMPARE(copiedWorld.scenario(2).sites, world.scenario(2).sites);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void missingObjectiveCannotCreateCaptureMission() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenarios = Scenarios::load(rom);
        auto world = ScenarioWorld::load(rom);
        const auto sites = world.scenario(2).sites;
        for (int i = 0; i < world.siteCount(2); ++i) {
            const int id = u16(sites, 1 + i * 4);
            auto town = world.town(id);
            town[24] = 50;
            world.setTown(id, town);
        }
        QVERIFY_EXCEPTION_THROWN(MissionBattlefield::capture(rom, scenarios, world, 2, "mapOnly"), Error);
        QVERIFY_EXCEPTION_THROWN(MissionBattlefield::capture(rom, scenarios, world, 2, "blank"), Error);
        QVERIFY(!MissionBattlefield::capture(rom, scenarios, world, 2, "exact").isEmpty());
    }
};
QTEST_APPLESS_MAIN(MissionBattlefieldTests)
#include "mission_battlefield_tests.moc"
