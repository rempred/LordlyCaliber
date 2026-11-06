#include "engine/project.h"
#include "engine/scenario_battlefields.h"
#include "engine/battlefield_clone.h"
#include "engine/capacity_placement.h"
#include "engine/event_authoring.h"
#include "rebuild/compiler.h"
#include <QtTest>

using namespace lordly;

class ScenarioBattlefieldTests : public QObject {
    Q_OBJECT
  private slots:
    void independentPresetsAndProjectRoundtrip() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private US revision 0 ROM required");
        try {
            const auto source = Rom::open(path);
            for (const auto &mode : {QString("exact"), QString("mapOnly"), QString("blank")}) {
                auto project = Project::fromRom(source);
                project.loadScenarios(); project.loadWorld(); project.loadMapEdits();
                auto scenarios = *project.scenarios();
                scenarios.setBaseLevel(2, 17, project.classData());
                project.setScenarios(scenarios.json());
                const auto sourceScenario = project.scenarios()->scenario(2).json();
                const auto sourceSites = project.world()->scenario(2);
                const auto sourceModel = project.mapEdits()->model(2);
                const auto sourceTerrain = project.mapEdits()->fineTerrain(2);
                QVERIFY(project.availableScenarioBattlefields().contains(10));
                project.createScenarioBattlefield(10, 2, mode, "Volmus experiment");
                QCOMPARE(project.scenarioName(10), QString("Volmus experiment"));
                QVERIFY(!project.availableScenarioBattlefields().contains(10));
                QCOMPARE(project.scenarios()->scenario(2).json(), sourceScenario);
                QCOMPARE(project.world()->scenario(2), sourceSites);
                QCOMPARE(project.mapEdits()->model(2), sourceModel);
                QCOMPARE(project.mapEdits()->fineTerrain(2), sourceTerrain);
                QVERIFY(project.mapEdits()->sharedScenarios(10) == QVector<int>{10});
                const auto &copy = project.scenarios()->scenario(10);
                if (mode == "exact") {
                    QCOMPARE(copy.json(), sourceScenario);
                    QCOMPARE(project.world()->siteCount(10), project.world()->siteCount(2));
                } else {
                    QVERIFY(copy.squads.isEmpty());
                    QVERIFY(copy.layout.deployments.isEmpty());
                    if (mode == "blank") QCOMPARE(project.world()->siteCount(10), 2);
                }
                auto map = *project.mapEdits();
                const auto beforeTerrain = map.fineTerrain(10);
                int cell = 0;
                while (cell < beforeTerrain.size() && (u8(beforeTerrain, cell) < 1 || u8(beforeTerrain, cell) >= 15)) ++cell;
                QVERIFY(cell < beforeTerrain.size());
                map.paint(10, {{double(cell % 256), double(cell / 256)}}, u8(beforeTerrain, cell) == 5 ? 3 : 5, 2);
                QVERIFY(map.fineTerrain(10) != beforeTerrain);
                project.setMapEdits(map.json());
                QCOMPARE(project.mapEdits()->model(2), sourceModel);
                QCOMPARE(project.mapEdits()->fineTerrain(2), sourceTerrain);
                const auto state = project.manifest();
                QCOMPARE(state["sourceSha256"].toString().toLatin1(), sha256(source.bytes()));
                auto reopened = Project::fromRom(source);
                reopened.applyManifest(state);
                reopened.loadScenarios(); reopened.loadWorld(); reopened.loadMapEdits();
                QCOMPARE(reopened.scenarioBattlefields(), project.scenarioBattlefields());
                QCOMPARE(reopened.scenarios()->json(), project.scenarios()->json());
                QCOMPARE(reopened.world()->json(), project.world()->json());
                QCOMPARE(reopened.mapEdits()->json(), project.mapEdits()->json());
                QCOMPARE(reopened.mapEdits()->model(10), project.mapEdits()->model(10));
                QCOMPARE(reopened.mapEdits()->fineTerrain(10), project.mapEdits()->fineTerrain(10));
            }
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void metadataRoundtripAndCorruption() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private US revision 0 ROM required");
        try {
            const auto rom = Rom::open(path);
            QCOMPARE(writeScenarioBattlefields(rom, {}), rom.bytes());
            auto project = Project::fromRom(rom);
            project.createScenarioBattlefield(10, 2, "mapOnly", "Metadata fixture");
            auto image = writeScenarioBattlefields(project.rom(), project.scenarioBattlefields());
            const auto reopened = Rom::fromBytes(image);
            QCOMPARE(readScenarioBattlefields(reopened), project.scenarioBattlefields());
            QCOMPARE(writeScenarioBattlefields(reopened, project.scenarioBattlefields()), image);
            auto ordinary = Project::fromRom(reopened);
            QCOMPARE(ordinary.scenarioName(10), QString("Metadata fixture"));
            const auto at = image.indexOf("LC08MSSN", 0x2800000);
            QVERIFY(at >= 0);
            image[at + 65] ^= 1;
            QVERIFY_EXCEPTION_THROWN(readScenarioBattlefields(Rom::fromBytes(image)), Error);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void independentCapacity() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private US revision 0 ROM required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            const QVector<int> keys{10, 22, 35, 62};
            QVector<int> available;
            for (int key = MissionTables::firstKey; key < MissionTables::keyCount; ++key) available << key;
            available += keys;
            QCOMPARE(project.availableScenarioBattlefields(), available);
            for (int i = 0; i < keys.size(); ++i) {
                project.createScenarioBattlefield(keys[i], 2, i % 2 ? "blank" : "mapOnly",
                                                  QString("Independent map %1").arg(i + 1));
                QCOMPARE(project.availableScenarioBattlefields().size(), available.size() - i - 1);
                for (int j = 0; j <= i; ++j)
                    QCOMPARE(project.mapEdits()->sharedScenarios(keys[j]), QVector<int>{keys[j]});
            }
            QVERIFY_EXCEPTION_THROWN(project.createScenarioBattlefield(10, 2, "exact", "Occupied"), Error);
            auto reopened = Project::fromRom(source);
            reopened.applyManifest(project.manifest());
            reopened.loadScenarios(); reopened.loadWorld(); reopened.loadMapEdits();
            QCOMPARE(reopened.scenarioBattlefields(), project.scenarioBattlefields());
            QCOMPARE(reopened.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(reopened.world()->json(), project.world()->json());
            for (int key : keys) {
                QCOMPARE(reopened.mapEdits()->model(key), project.mapEdits()->model(key));
                QCOMPARE(reopened.mapEdits()->fineTerrain(key), project.mapEdits()->fineTerrain(key));
            }
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void nonTailCapacityRecordReuse() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private US revision 0 ROM required");
        try {
            const auto rom = Rom::open(path);
            auto patched = rom.bytes();
            put32(patched, 0x1000, u32(patched, 0x1000) ^ 1);
            auto original = CapacityPlacement::record(rom.bytes(), patched, 0x80250000, 0x8025d6f0);
            original += QByteArray(32, char(0x5a));
            const auto prior = CapacityPlacement::read(Rom::fromBytes(original));
            QVERIFY(prior.has_value());
            const auto canonical = prior->canonical(Rom::fromBytes(original));
            auto repatched = canonical.bytes();
            put32(repatched, 0x1000, u32(patched, 0x1000));
            QCOMPARE(CapacityPlacement::record(canonical.bytes(), repatched, prior->base, prior->codeAddress, prior), original);
            auto occupied = canonical.bytes();
            put8(occupied, prior->offset + 64, 7);
            auto occupiedAfter = occupied;
            put32(occupiedAfter, 0x1000, u32(patched, 0x1000));
            const auto relocated = CapacityPlacement::record(occupied, occupiedAfter, prior->base, prior->codeAddress, prior);
            QCOMPARE(relocated.first(occupied.size()), occupiedAfter);
            QVERIFY(CapacityPlacement::read(Rom::fromBytes(relocated))->offset >= occupied.size());
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void exportWithCustomFlagsAndConsumableTree() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto toolchain = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || toolchain.isEmpty()) QSKIP("Private ROM and N64 toolchain required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.createScenarioBattlefield(10, 2, "mapOnly", "Playable test battlefield");
            auto map = *project.mapEdits();
            const auto beforeTerrain = map.fineTerrain(10);
            int cell = 0;
            while (cell < beforeTerrain.size() && (u8(beforeTerrain, cell) < 1 || u8(beforeTerrain, cell) >= 15)) ++cell;
            QVERIFY(cell < beforeTerrain.size());
            map.paint(10, {{double(cell % 256), double(cell / 256)}}, u8(beforeTerrain, cell) == 5 ? 3 : 5, 2);
            QVERIFY(map.fineTerrain(10) != beforeTerrain);
            map.moveSite(10, 0, map.source(10).map.bounds.center());
            project.setMapEdits(map.json());
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            const int flag = events.createFlag("Playable test battlefield completed");
            events.setTrees(2, EventAuthoring::missionTemplates(10, 6, flag), *project.scenePrograms());
            events.setRetailGate(10, 0x7e,
                {{"mode", "if"}, {"condition", QJsonObject{{"type", "flag"}, {"flag", flag}, {"value", 1}}},
                 {"yes", "original"}, {"no", "no"}}, *project.scenePrograms());
            project.setSceneEvents(events.json());
            project.loadConsumableEffects();
            auto effects = *project.consumableEffects();
            effects.setTree(3, {QJsonObject{{"type", "heal"}, {"minimum", 90}, {"maximum", 90}}});
            project.setConsumableEffects(effects.json());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            const auto built = rebuild(project, toolchain, work.path());
            const auto exported = Rom::fromBytes(built.image);
            const auto placement = ClassPlacement::read(exported);
            QVERIFY(placement.has_value());
            QVERIFY(placement->layout.customFlags);
            QVERIFY(placement->customFlagsGetter);
            QVERIFY(placement->customFlagsSetter);
            auto reopened = Project::fromRom(exported);
            reopened.loadSceneEvents(); reopened.loadScenarios(); reopened.loadWorld();
            reopened.loadMapEdits(); reopened.loadConsumableEffects();
            QCOMPARE(reopened.scenarioBattlefields(), project.scenarioBattlefields());
            QCOMPARE(reopened.sceneEvents()->program(2), project.sceneEvents()->program(2));
            QCOMPARE(reopened.sceneEvents()->program(10), project.sceneEvents()->program(10));
            QCOMPARE(reopened.consumableEffects()->tree(3), project.consumableEffects()->tree(3));
            QCOMPARE(reopened.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(reopened.world()->json(), project.world()->json());
            QCOMPARE(reopened.mapEdits()->model(10), project.mapEdits()->model(10));
            QCOMPARE(reopened.mapEdits()->fineTerrain(10), project.mapEdits()->fineTerrain(10));
            QCOMPARE(reopened.mapEdits()->points(10), project.mapEdits()->points(10));
            if (const auto destination = qEnvironmentVariable("LORDLY_SCENARIO_EXPORT_FIXTURE"); !destination.isEmpty())
                atomicWrite(destination, project.rom().serialize(built.image));
            auto restoredEvents = *reopened.sceneEvents();
            restoredEvents.setRetailGate(10, 0x7e, {}, *reopened.scenePrograms());
            reopened.setSceneEvents(restoredEvents.json());
            QTemporaryDir restoreWork;
            QVERIFY(restoreWork.isValid());
            const auto restored = rebuild(reopened, toolchain, restoreWork.path());
            QCOMPARE(u32(restored.image, 0x66654), quint32(0x80171b58));
            auto restoredProject = Project::fromRom(Rom::fromBytes(restored.image));
            restoredProject.loadSceneEvents();
            QCOMPARE(restoredProject.sceneEvents()->program(10), SceneEvents::retailRow(Rom::open(path), 10));
            QCOMPARE(restoredProject.sceneEvents()->program(2), project.sceneEvents()->program(2));
            auto detached = restoredProject.manifest();
            detached["scenarioBattlefields"] = QJsonArray{};
            restoredProject.applyManifest(detached);
            QTemporaryDir detachedWork;
            QVERIFY(detachedWork.isValid());
            const auto detachedBuild = rebuild(restoredProject, toolchain, detachedWork.path());
            const auto detachedRom = Rom::fromBytes(detachedBuild.image);
            const auto detachedPlacement = ClassPlacement::read(detachedRom);
            QVERIFY(detachedPlacement.has_value());
            const auto resident = detachedRom.bytes().mid(detachedPlacement->residentAddress - ClassPlacement::residentDelta,
                                                          detachedPlacement->residentBytes);
            QVERIFY(!resident.contains("LCMISN01"));
            QVERIFY(readScenarioBattlefields(detachedRom).isEmpty());
            QCOMPARE(BattlefieldClone::modelTable(detachedRom), BattlefieldClone::modelTable(exported));
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void exportWithGothAndExpandedArmies_data() {
        QTest::addColumn<int>("key");
        QTest::newRow("legacy-slot") << 10;
        QTest::newRow("extended-slot") << MissionTables::keyCount - 1;
    }
    void exportWithGothAndExpandedArmies() {
        QFETCH(int, key);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto toolchain = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || toolchain.isEmpty()) QSKIP("Private ROM and N64 toolchain required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.createScenarioBattlefield(key, 2, "mapOnly", "Expanded army test battlefield");
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            const int flag = events.createFlag("Expanded army test battlefield completed");
            events.setTrees(2, EventAuthoring::missionTemplates(key, 6, flag), *project.scenePrograms());
            project.setSceneEvents(events.json());
            auto scenarios = *project.scenarios();
            while (scenarios.scenario(key).squads.size() < 21)
                scenarios.addSquad(key, -1, project.classData());
            project.setScenarios(scenarios.json());
            QVERIFY(project.expandedArmies());
            project.loadTools();
            auto settings = project.tools()->json(), rules = settings["gameRules"].toObject();
            rules["gothLowDivisor"] = 10;
            rules["gothHighDivisor"] = 100;
            rules["reserveXp"] = 25;
            settings["gameRules"] = rules;
            project.setTools(settings);
            project.loadShops();
            auto shops = *project.shops();
            shops.setStock(1, {{1, 277}, {1, 44}});
            project.setShops(shops.json());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            const auto built = rebuild(project, toolchain, work.path());
            auto reopened = Project::fromRom(Rom::fromBytes(built.image));
            reopened.loadScenarios(); reopened.loadTools(); reopened.loadShops();
            QVERIFY(reopened.expandedArmies());
            QCOMPARE(reopened.shops()->json(), project.shops()->json());
            QCOMPARE(reopened.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(reopened.scenarioBattlefields(), project.scenarioBattlefields());
            QCOMPARE(reopened.tools()->gameRules().value("gothLowDivisor"), 10);
            QCOMPARE(reopened.tools()->gameRules().value("reserveXp"), 25);
            if (const auto destination = qEnvironmentVariable("LORDLY_SCENARIO_EXPORT_FIXTURE"); !destination.isEmpty()) {
                const QFileInfo file(destination);
                const auto folder = file.absolutePath() + QString("/expanded-%1").arg(key);
                QVERIFY(QDir().mkpath(folder));
                const auto expanded = folder + "/" + file.fileName();
                atomicWrite(expanded, project.rom().serialize(built.image));
                qInfo().noquote() << "Expanded mission candidate:" << expanded;
            }
            QTemporaryDir again;
            QVERIFY(again.isValid());
            const auto rebuilt = rebuild(reopened, toolchain, again.path());
            if (rebuilt.image != built.image) {
                int first = 0;
                while (first < qMin(rebuilt.image.size(), built.image.size()) && rebuilt.image[first] == built.image[first]) ++first;
                qInfo() << "First repeated-export difference at z64" << Qt::hex << first
                        << "sizes" << rebuilt.image.size() << built.image.size();
                if (const auto destination = qEnvironmentVariable("LORDLY_SCENARIO_EXPORT_FIXTURE"); !destination.isEmpty()) {
                    const QFileInfo file(destination);
                    const auto folder = file.absolutePath() + QString("/expanded-again-%1").arg(key);
                    QVERIFY(QDir().mkpath(folder));
                    atomicWrite(folder + "/" + file.fileName(), reopened.rom().serialize(rebuilt.image));
                }
            }
            QCOMPARE(rebuilt.image, built.image);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void exportExactDonorVictory() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto toolchain = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || toolchain.isEmpty()) QSKIP("Private ROM and N64 toolchain required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.createScenarioBattlefield(10, 8, "exact", "Independent donor victory test");
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            const int flag = events.createFlag("Independent donor victory completed");
            events.setTrees(2, EventAuthoring::missionTemplates(10, 6, flag), *project.scenePrograms());
            project.setSceneEvents(events.json());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            const auto built = rebuild(project, toolchain, work.path());
            auto reopened = Project::fromRom(Rom::fromBytes(built.image));
            reopened.loadScenarios(); reopened.loadSceneEvents();
            QCOMPARE(reopened.scenarioBattlefields(), project.scenarioBattlefields());
            QCOMPARE(reopened.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(reopened.sceneEvents()->program(2), project.sceneEvents()->program(2));
            if (const auto destination = qEnvironmentVariable("LORDLY_SCENARIO_EXPORT_FIXTURE"); !destination.isEmpty()) {
                const QFileInfo file(destination);
                const auto folder = file.absolutePath() + "/exact";
                QVERIFY(QDir().mkpath(folder));
                const auto candidate = folder + "/" + file.fileName();
                atomicWrite(candidate, project.rom().serialize(built.image));
                qInfo().noquote() << "Exact mission candidate:" << candidate;
            }
            QTemporaryDir again;
            QVERIFY(again.isValid());
            QCOMPARE(rebuild(reopened, toolchain, again.path()).image, built.image);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
};
QTEST_APPLESS_MAIN(ScenarioBattlefieldTests)
#include "scenario_battlefield_tests.moc"
