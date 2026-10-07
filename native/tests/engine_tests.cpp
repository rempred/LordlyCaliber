#include "engine/archive.h"
#include "engine/class_data.h"
#include "engine/class_runtime_inventory.h"
#include "engine/class_text.h"
#include "engine/consumable_effects.h"
#include "engine/custom_squads.h"
#include "engine/eset.h"
#include "engine/expanded_promotion_resource.h"
#include "engine/project.h"
#include "engine/resource.h"
#include "engine/scenario_resources.h"
#include "engine/shop_runtime.h"
#include "engine/world_resources.h"
#include "rebuild/class_menu_layout.h"
#include "rebuild/class_runtime.h"
#include "rebuild/compiler.h"
#include "rebuild/creature_rewards.h"
#include "rebuild/custom_encounters.h"
#include "rebuild/elf_references.h"
#include "rebuild/promotion_runtime.h"
#include "rebuild/scenario_rates.h"
#include <QJsonArray>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace lordly;
static QByteArray syntheticRom() {
    QByteArray data(0x2800000, char(0));
    put32(data, 0, 0x80371240);
    data.replace(0x20, 12, "OgreBattle64");
    data.replace(0x3b, 2, "NO");
    data[0x3e] = 'E';
    return data;
}
static Promotion syntheticPromotion() {
    Promotion base;
    QJsonArray families, assignments;
    for (int id = 0; id <= 10; ++id)
        families << QJsonObject{
            {"id", id}, {"name", id ? QString("Family %1").arg(id) : "None"}, {"behavior", id}};
    for (int id = 0; id < 81; ++id) {
        assignments << id % 11;
        base.masks += char(id * 7);
        base.flags += char((id * 16) & 0xf0);
        for (int field = 0; field < 8; ++field)
            base.gates += char(id * 3 + field);
    }
    base.families = {{"schemaVersion", 1}, {"families", families}, {"assignments", assignments}};
    base.originalFamilies = base.families;
    base.originalGates = base.gates;
    base.originalMasks = base.masks;
    base.originalFlags = base.flags;
    return base;
}
class EngineTests : public QObject {
    Q_OBJECT
  private slots:
    void browserScenarioImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            QTemporaryDir work;
            const auto fixturePath = work.filePath("browser.json");
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("browser-scenario-fixture.cjs"),
                                     path, fixturePath});
            QVERIFY2(generator.waitForFinished(30000), qPrintable(generator.errorString()));
            QCOMPARE(generator.exitCode(), 0);
            QFile fixtureFile(fixturePath);
            QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
            const auto fixture = QJsonDocument::fromJson(fixtureFile.readAll()).object();
            const auto patch = fixture["patch"].toObject(),
                       expected = fixture["expected"].toObject();
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            const auto classData = project.classData();
            const auto classCapacities = fixture["classCapacities"].toArray();
            QCOMPARE(classCapacities.size(), 165);
            for (int id = 0; id < classCapacities.size(); ++id)
                QCOMPARE(u8(classData.entries()[id].record, ClassData::itemCapacityOffset),
                         classCapacities[id].toInt());
            const auto capacities = fixture["capacities"].toObject();
            QVERIFY(capacities.size() > 100);
            for (auto it = capacities.begin(); it != capacities.end(); ++it)
                QCOMPARE(Scenarios::carryingCapacity(QByteArray::fromHex(it.key().toLatin1()),
                                                     classData),
                         it.value().toInt());
            project.importLegacy(patch);
            QVERIFY(project.pendingLegacySections().isEmpty());
            QCOMPARE(project.scenarios()->changedScenarios(), 7);
            for (auto it = expected.begin(); it != expected.end(); ++it)
                QCOMPARE(project.scenarios()->scenario(it.key().toInt()).json(),
                         it.value().toObject());
            const auto authored = project.scenarios()->json();
            project.saveFolder(work.path());
            project.saveArchive(work.filePath("import.lcproject"));
            for (const auto &location : {work.path(), work.filePath("import.lcproject")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.scenarios()->json(), authored);
                QVERIFY(reopened.pendingLegacySections().isEmpty());
            }
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-scenario-import-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadScenarios();
            QCOMPARE(reopened.scenarios()->json(), authored);
            auto fresh = Project::fromRom(rom);
            auto patches = patch["patches"].toObject();
            auto scenario = patches["scenario"].toObject();
            const auto sourceManifest = fresh.manifest();
            auto invalid = scenario;
            auto added = invalid["addedSquads"].toArray();
            auto row = added[0].toObject();
            row.remove("compRecHex");
            added[0] = row;
            invalid["addedSquads"] = added;
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), sourceManifest);
            invalid = scenario;
            auto layouts = invalid["modifiedEsets"].toObject();
            auto layout = layouts["10"].toObject();
            layout["sourceSha256"] = QString(64, '0');
            layouts["10"] = layout;
            invalid["modifiedEsets"] = layouts;
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), sourceManifest);
            invalid = scenario;
            layouts = invalid["modifiedEsets"].toObject();
            layout = layouts["10"].toObject();
            layout["unrecognizedEdit"] = 1;
            layouts["10"] = layout;
            invalid["modifiedEsets"] = layouts;
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), sourceManifest);
            // Without explicit level intent, older projects preserve the source base.
            auto implicit = scenario;
            implicit["levelBaseEdits"] = QJsonArray{};
            fresh.importLegacy(implicit);
            QCOMPARE(fresh.scenarios()->scenario(10).layout.baseLevel(), 2);
            // Historical physical paths canonicalize only under the matching source hash.
            auto migrated = scenario;
            layouts = migrated["modifiedEsets"].toObject();
            for (const auto key : {"10", "22", "35", "62"}) {
                layout = layouts[key].toObject();
                layout["resourcePath"] = "historical/equal-payload-resource";
                layout["filename"] = "historical.bin";
                layouts[key] = layout;
            }
            migrated["modifiedEsets"] = layouts;
            auto bases = migrated["levelBaseEdits"].toArray();
            auto base = bases[0].toObject();
            base["resourcePath"] = "historical/equal-payload-resource";
            bases[0] = base;
            migrated["levelBaseEdits"] = bases;
            auto migratedProject = Project::fromRom(rom);
            migratedProject.importLegacy(migrated);
            QCOMPARE(migratedProject.scenarios()->scenario(10).layout.baseLevel(), 12);
            QCOMPARE(migratedProject.scenarios()->scenario(22).layout.baseLevel(), 12);
            // Clearing imported edits remains authoritative over retained browser content.
            auto cleared = *project.scenarios();
            const auto keys = cleared.entries().keys();
            for (const auto key : keys)
                cleared.restore(key);
            project.setScenarios(cleared.json());
            auto restored = Project::fromRom(rom);
            restored.applyManifest(project.manifest());
            QVERIFY(!restored.scenarios()->modified());
            QCOMPARE(restored.manifest()["legacyImport"], project.manifest()["legacyImport"]);
            // Invalid world data rejects atomically; genuinely unknown sections remain pending.
            auto retained = scenario;
            retained["modifiedTreasures"] = QJsonObject{{"750", QJsonObject{{"rawHex", "00"}}}};
            auto pending = Project::fromRom(rom);
            QVERIFY_THROWS_EXCEPTION(Error, pending.importLegacy(retained));
            retained = scenario;
            retained["futureWorldFeature"] = QJsonObject{{"enabled", true}};
            pending.importLegacy(retained);
            QVERIFY(pending.pendingLegacySections().contains("scenario.futureWorldFeature"));
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(pending, tools, work.path()));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Browser scenario import export:" << candidate;
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void scenarioRoutes() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for route authoring");
        try {
            const auto rom = Rom::open(path);
            const auto classes = ClassData::load(rom);
            auto model = Scenarios::load(rom);
            const auto original = model.json();
            QCOMPARE(model.addCondition(10, 9, classes), 0);
            QCOMPARE(model.addNode(10, 1, classes), 1);
            QCOMPARE(model.addNode(10, 0, classes), 2);
            auto node = model.scenario(10).layout.nodes[1];
            node[4] = 120;
            put8(node, 5, 160);
            node[10] = 1;
            node[17] = 6;
            model.setNode(10, 1, node, classes);
            auto deployment = model.scenario(10).layout.deployments[0];
            deployment[6] = 5;
            model.setDeployment(10, 0, deployment, classes);
            const auto authored = model.json();
            QVERIFY_THROWS_EXCEPTION(Error, model.removeNode(10, 1, classes));
            QVERIFY_THROWS_EXCEPTION(Error, model.removeNode(10, 2, classes));
            QVERIFY_THROWS_EXCEPTION(Error, model.removeCondition(10, 0, classes));
            QCOMPARE(model.json(), authored);
            auto invalid = node;
            invalid[17] = 19;
            QVERIFY_THROWS_EXCEPTION(Error, model.setNode(10, 1, invalid, classes));
            invalid = node;
            invalid[10] = 16;
            QVERIFY_THROWS_EXCEPTION(Error, model.setNode(10, 1, invalid, classes));
            auto condition = model.scenario(10).layout.conditions[0];
            condition[6] = 31;
            QVERIFY_THROWS_EXCEPTION(Error, model.setCondition(10, 0, condition, classes));
            QCOMPARE(model.json(), authored);
            node[10] = 0;
            node[17] = 1;
            model.setNode(10, 1, node, classes);
            model.removeCondition(10, 0, classes);
            QVERIFY(!model.scenario(10).layout.conditionsPresent);
            model.removeNode(10, 2, classes);
            deployment[6] = 1;
            model.setDeployment(10, 0, deployment, classes);
            model.removeNode(10, 1, classes);
            model.restore(10);
            QCOMPARE(model.json(), original);
            // Renumbering an unreferenced middle node updates later initial orders.
            model.addNode(10, 0, classes);
            model.addNode(10, 0, classes);
            deployment = model.scenario(10).layout.deployments[0];
            deployment[6] = 6;
            model.setDeployment(10, 0, deployment, classes);
            model.removeNode(10, 1, classes);
            QCOMPARE(u8(model.scenario(10).layout.nodes[1], 0), 5);
            QCOMPARE(u8(model.scenario(10).layout.deployments[0], 6), 5);
            // A deleted added condition renumbers references to surviving added conditions.
            model.addCondition(10, 9, classes);
            model.addCondition(10, 9, classes);
            node = model.scenario(10).layout.nodes[1];
            node[10] = 2;
            model.setNode(10, 1, node, classes);
            model.removeCondition(10, 0, classes);
            QCOMPARE(u8(model.scenario(10).layout.nodes[1], 10), 1);
            model.setRouteGroup(10, 1, 7, 5, classes);
            model.setRouteGroup(10, 2, 7, 5, classes);
            QCOMPARE(model.scenario(10).layout.deployments[0].right(2),
                     QByteArray::fromHex("0705"));
            const auto valid = model.json();
            QVERIFY_THROWS_EXCEPTION(Error, model.setRouteGroup(10, 0, 1, 5, classes));
            QVERIFY_THROWS_EXCEPTION(Error, model.setRouteGroup(10, 1, 1, 255, classes));
            QCOMPARE(model.json(), valid);
            auto restored = Scenarios::load(rom);
            restored.apply(valid, classes);
            QCOMPARE(restored.json(), valid);
            const auto plan = model.plan(classes);
            const auto built =
                Rom::fromBytes(ScenarioResources::load(rom).rebuild(rom, plan.enemies, plan.esets));
            QCOMPARE(Scenarios::load(built).json(), valid);
            model.restore(10);
            while (model.scenario(10).layout.nodes.size() < 16)
                model.addNode(10, 0, classes);
            while (model.scenario(10).layout.conditions.size() < 16)
                model.addCondition(10, 9, classes);
            const auto full = model.json();
            QVERIFY_THROWS_EXCEPTION(Error, model.addNode(10, 0, classes));
            QVERIFY_THROWS_EXCEPTION(Error, model.addCondition(10, 9, classes));
            QCOMPARE(model.json(), full);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void scenarioAuthoring() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for scenario authoring");
        try {
            const auto rom = Rom::open(path);
            const auto resources = ScenarioResources::load(rom);
            const auto classes = ClassData::load(rom);
            auto model = Scenarios::load(rom);
            QCOMPARE(model.entries().size(), 63);
            const auto original = model.json();
            model.apply(original, classes);
            QCOMPARE(model.json(), original);
            QVERIFY(!model.modified());
            QVERIFY(model.plan(classes).esets.isEmpty());
            QCOMPARE(model.plan(classes).enemies, resources.enemyData());
            auto changedClasses = classes;
            auto asset = classes.entries()[u8(model.scenario(10).squads[0], 0)];
            asset.record[ClassData::itemCapacityOffset] = 11;
            changedClasses.set(u8(model.scenario(10).squads[0], 0), asset);
            QVERIFY_THROWS_EXCEPTION(Error, model.validateForExport(changedClasses));
            changedClasses = classes;
            asset = classes.entries()[u8(model.scenario(10).squads[0], 0)];
            asset.record[4] = 3;
            changedClasses.set(u8(model.scenario(10).squads[0], 0), asset);
            QVERIFY_THROWS_EXCEPTION(Error, model.validateForExport(changedClasses));
            changedClasses = classes;
            asset = classes.entries()[5];
            asset.record[59] ^= char(1); // Rear action, not carried-item capacity.
            changedClasses.set(5, asset);
            model.validateForExport(changedClasses);
            // Keys 10 and 22 select the same original ESET; local edits split them.
            QCOMPARE(model.scenario(10).json(), model.scenario(22).json());
            auto record = model.scenario(10).squads[0];
            put16(record, 2, 277);
            model.setSquad(10, 0, record, classes);
            QCOMPARE(model.scenario(22).json(), model.original(22).json());
            QCOMPARE(model.changedScenarios(), 1);
            auto plan = model.plan(classes);
            QCOMPARE(plan.enemies.first(resources.enemyData().size()), resources.enemyData());
            QCOMPARE(plan.enemies.size(), resources.enemyData().size() + 35);
            QCOMPARE(Eset::decode(plan.esets[10]).deployments[0].mid(1, 2),
                     QByteArray::fromHex("022d")); // Appended one-based ID 557.
            const auto before = model.json();
            auto invalid = record;
            put16(invalid, 2, 278);
            QVERIFY_THROWS_EXCEPTION(Error, model.setSquad(10, 0, invalid, classes));
            QCOMPARE(model.json(), before);
            QVERIFY_THROWS_EXCEPTION(Error, model.setBaseLevel(10, 0, classes));
            QCOMPARE(model.json(), before);
            invalid = record;
            invalid[8] = char(127);
            invalid[13] = invalid[6];
            QVERIFY_THROWS_EXCEPTION(Error, model.setSquad(10, 0, invalid, classes));
            QCOMPARE(model.json(), before);
            int ambiguous = -1;
            for (int i = 0; i < model.scenario(25).squads.size(); ++i)
                if ((u8(model.scenario(25).layout.deployments[i], 0) & 127) == 40)
                    ambiguous = i;
            QVERIFY(ambiguous >= 0);
            invalid = model.scenario(25).squads[ambiguous];
            invalid[8] ^= char(1);
            QVERIFY_THROWS_EXCEPTION(Error, model.setSquad(25, ambiguous, invalid, classes));
            QCOMPARE(model.json(), before);
            // A simple authored squad gives add/remove and supply checks an explicit formation.
            record = QByteArray(35, 0);
            record[0] = 5;
            record[6] = 9;
            auto deployment = model.scenario(10).layout.deployments[0];
            deployment.replace(13, 3, QByteArray(3, 0));
            model.setDeployment(10, 0, deployment, classes);
            model.setSquad(10, 0, record, classes);
            const auto beforeAdd = model.json();
            const int row = model.addSquad(10, 0, classes);
            QCOMPARE(model.scenario(10).layout.deployments[row - 1].right(2), QByteArray(2, 0));
            QCOMPARE(model.scenario(10).squads[row], record);
            model.removeSquad(10, row, classes);
            QCOMPARE(model.json(), beforeAdd);
            QVERIFY_THROWS_EXCEPTION(Error, model.removeSquad(10, 0, classes));
            deployment = model.scenario(10).layout.deployments[0];
            deployment[13] = 3;
            deployment[14] = 3;
            deployment[15] = 2;
            QVERIFY_THROWS_EXCEPTION(Error, model.setDeployment(10, 0, deployment, classes));
            QCOMPARE(model.json(), beforeAdd);
            auto saved = Scenarios::load(rom);
            saved.apply(model.json(), classes);
            QCOMPARE(saved.json(), model.json());
            model.restore(10);
            QCOMPARE(model.json(), original);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void scenarioProjectExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and authenticated compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            auto unsafe = project;
            const int leader = u8(project.scenarios()->scenario(10).squads[0], 0);
            auto unsafeClass = unsafe.record(Table::Classes, leader).bytes;
            unsafeClass[ClassData::itemCapacityOffset] = 11;
            unsafe.replaceBytes(Table::Classes, leader, unsafeClass);
            QTemporaryDir rejectedWork;
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(unsafe, tools, rejectedWork.path()));
            QCOMPARE(project.scenarios()->json(), unsafe.scenarios()->json());
            QCOMPARE(project.appendClass(5, "Scenario class"), 165);
            auto model = *project.scenarios();
            auto deploymentInput = model.scenario(10).layout.deployments[0];
            deploymentInput.replace(13, 3, QByteArray(3, 0));
            model.setDeployment(10, 0, deploymentInput, project.classData());
            QByteArray record(35, 0);
            put8(record, 0, 165);
            record[6] = 9;
            put16(record, 2, 277);
            model.setSquad(10, 0, record, project.classData());
            model.setBaseLevel(22, 12, project.classData());
            // Editing an automatically allied row must retain its effective flag after remap.
            int ally = -1;
            for (int i = 0; i < model.scenario(4).squads.size(); ++i)
                if (u8(model.scenario(4).layout.deployments[i], 0) & 128)
                    ally = i;
            QVERIFY(ally >= 0);
            auto allyRecord = model.scenario(4).squads[ally];
            put16(allyRecord, 2, 277);
            model.setSquad(4, ally, allyRecord, project.classData());
            project.setScenarios(model.json());
            QTemporaryDir folder, work;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("scenarios.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("scenarios.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.scenarios()->json(), model.json());
            }
            const auto result = rebuild(project, tools, work.path());
            const auto built = Rom::fromBytes(result.image);
            const auto resources = ScenarioResources::load(built);
            QCOMPARE(resources.enemyData().first(556 * 35),
                     ScenarioResources::load(rom).enemyData());
            const auto deployment = Eset::decode(resources.scenario(built, 4)).deployments[ally];
            QVERIFY(u8(deployment, 0) & 128);
            QVERIFY(u16(deployment, 1) > 556);
            QVERIFY(resources.selector(10) != resources.selector(22));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-scenario-project-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadScenarios();
            QCOMPARE(reopened.scenarios()->json(), model.json());
            QCOMPARE(reopened.records(Table::Classes).size(), 166);
            const auto repeated = rebuild(reopened, tools, work.path());
            QCOMPARE(repeated.image, result.image);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Scenario project export:" << candidate;
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void browserWorldImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            QTemporaryDir work;
            const auto fixturePath = work.filePath("world.json");
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("browser-world-fixture.cjs"),
                                     path, fixturePath});
            QVERIFY(generator.waitForFinished(30000));
            const auto generatorErrors = generator.readAllStandardError();
            QVERIFY2(generator.exitCode() == 0, generatorErrors.constData());
            QFile file(fixturePath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto fixture = QJsonDocument::fromJson(file.readAll()).object(),
                       patch = fixture["patch"].toObject(),
                       expected = fixture["expected"].toObject();
            const int archive = fixture["treasureArchive"].toInt();
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.importLegacy(patch);
            QVERIFY(project.world());
            QVERIFY(project.pendingLegacySections().isEmpty());
            const auto authored = project.world()->json();
            auto independent = *project.world();
            const int last = ScenarioWorld::treasureCount(independent.scenario(52).treasures) - 1;
            auto moved = ScenarioWorld::treasureRecord(independent.scenario(52).treasures, last);
            moved[1] ^= char(1);
            independent.setTreasure(52, last, moved);
            auto independentProject = project;
            independentProject.setWorld(independent.json());
            QCOMPARE(independentProject.world()->scenario(19), project.world()->scenario(19));
            QCOMPARE(authored["towns"].toString(), expected["691"].toString());
            for (const int k : {19, 52, 53, 54})
                QCOMPARE(QString::fromLatin1(project.world()->scenario(k).treasures.toHex()),
                         expected[QString::number(archive)].toString());
            for (const int k : {52, 53, 54})
                QCOMPARE(QString::fromLatin1(project.world()->scenario(k).sites.toHex()),
                         expected["725"].toString());
            QCOMPARE(project.world()->scenario(19).sites, project.world()->original(19).sites);
            project.saveFolder(work.path());
            project.saveArchive(work.filePath("world.lcproject"));
            for (const auto &location : {work.path(), work.filePath("world.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.world()->json(), authored);
                QVERIFY(restored.pendingLegacySections().isEmpty());
            }
            const auto result = rebuild(project, tools, work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-world-import-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadWorld();
            QCOMPARE(reopened.world()->json(), authored);
            auto fresh = Project::fromRom(rom);
            const auto original = fresh.manifest();
            const auto scenario = patch["patches"].toObject()["scenario"].toObject();
            auto noOpIntent = scenario;
            noOpIntent["siteAllegiances"] = QJsonObject{{"52", QJsonObject{{"1", "enemy"}}},
                                                        {"53", QJsonObject{{"1", "allied"}}}};
            auto noOpProject = Project::fromRom(rom);
            noOpProject.importLegacy(noOpIntent);
            QCOMPARE(noOpProject.world()->json(), authored);
            auto invalid = scenario;
            invalid["siteAllegiances"] = QJsonObject{{"52", QJsonObject{{"1", "enemy"}}},
                                                     {"53", QJsonObject{{"1", "neutral"}}}};
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), original);
            invalid = scenario;
            auto treasures = invalid["modifiedTreasures"].toObject();
            auto row = treasures[QString::number(archive)].toObject();
            row["filename"] = "different.bin";
            treasures[QString::number(archive)] = row;
            invalid["modifiedTreasures"] = treasures;
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), original);
            invalid = scenario;
            invalid["siteAllegiances"] = QJsonObject{{"10", QJsonObject{{"1", "enemy"}}}};
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), original);
            invalid = scenario;
            invalid["strongholdFields"] = QJsonObject{{"185", QJsonObject{{"population", 1.5}}}};
            QVERIFY_THROWS_EXCEPTION(Error, fresh.importLegacy(invalid));
            QCOMPARE(fresh.manifest(), original);
            auto changedSource = Project::fromRom(Rom::fromBytes(result.image));
            QVERIFY_THROWS_EXCEPTION(Error, changedSource.importLegacy(patch));
            auto cleared = *project.world();
            for (int k = 1; k < 65; ++k)
                cleared.restoreScenario(k);
            for (int id = 1; id <= ScenarioWorld::townCount; ++id)
                cleared.restoreTown(id);
            project.setWorld(cleared.json());
            auto restored = Project::fromRom(rom);
            restored.applyManifest(project.manifest());
            QVERIFY(restored.world());
            QVERIFY(!restored.world()->modified());
            QVERIFY(restored.pendingLegacySections().isEmpty());
            QCOMPARE(restored.manifest()["legacyImport"], project.manifest()["legacyImport"]);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Browser world export:" << candidate;
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void worldAuthoringExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and authenticated compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadWorld();
            auto model = *project.world();
            const auto base = model.json();
            model.apply(base);
            QCOMPARE(model.json(), base);
            for (int id = 1; id <= ScenarioWorld::townCount; ++id)
                model.setTown(id, model.town(id));
            QCOMPARE(model.json(), base);
            const int key = 52;
            const int townId = u16(model.scenario(key).sites, 1);
            auto town = model.town(townId);
            put16(town, 22, 4321);
            put8(town, 24, 99);
            put8(town, 27, 35);
            town.replace(1, 20, QByteArray("Native Test") + QByteArray(9, 0));
            model.setTown(townId, town);
            model.setSiteAllegiance(key, 0, 2);
            const int added = model.addTreasure(key);
            auto treasure = ScenarioWorld::treasureRecord(model.scenario(key).treasures, added);
            QVERIFY(u8(treasure, 0) > 230);
            put8(treasure, 1, 255);
            put8(treasure, 2, 0);
            put8(treasure, 3, 1);
            put16(treasure, 4, 277);
            model.setTreasure(key, added, treasure);
            QCOMPARE(model.scenario(53), model.original(53));
            auto rejected = model;
            auto invalid = treasure;
            invalid[0] ^= char(1);
            QVERIFY_THROWS_EXCEPTION(Error, rejected.setTreasure(key, added, invalid));
            invalid = treasure;
            put16(invalid, 4, 278);
            QVERIFY_THROWS_EXCEPTION(Error, rejected.setTreasure(key, added, invalid));
            auto badTown = town;
            badTown[21] ^= char(1);
            QVERIFY_THROWS_EXCEPTION(Error, rejected.setTown(townId, badTown));
            QVERIFY_THROWS_EXCEPTION(Error, rejected.setSiteAllegiance(10, 0, 1));
            QCOMPARE(rejected.json(), model.json());
            auto full = model;
            while (ScenarioWorld::treasureCount(full.scenario(key).treasures) < 16)
                full.addTreasure(key);
            QVERIFY_THROWS_EXCEPTION(Error, full.addTreasure(key));
            auto exhausted = model;
            int freeKey = 1;
            for (int id = 232; id <= 255; ++id) {
                while (ScenarioWorld::treasureCount(exhausted.scenario(freeKey).treasures) == 16)
                    ++freeKey;
                const int row = exhausted.addTreasure(freeKey);
                QCOMPARE(
                    u8(ScenarioWorld::treasureRecord(exhausted.scenario(freeKey).treasures, row),
                       0),
                    quint8(id));
            }
            while (ScenarioWorld::treasureCount(exhausted.scenario(freeKey).treasures) == 16)
                ++freeKey;
            QVERIFY_THROWS_EXCEPTION(Error, exhausted.addTreasure(freeKey));
            auto malformed = model.json();
            auto rows = malformed["scenarios"].toObject();
            auto row = rows["1"].toObject();
            row["treasures"] = QString::fromLatin1(model.scenario(key).treasures.toHex());
            rows["1"] = row;
            malformed["scenarios"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, rejected.apply(malformed));
            QCOMPARE(rejected.json(), model.json());
            project.setWorld(model.json());
            project.loadShops();
            auto shops = *project.shops();
            shops.setStock(35, {{1, 277}, {1, 44}});
            project.setShops(shops.json());
            project.loadScenarios();
            auto squads = *project.scenarios();
            squads.setBaseLevel(10, 12, project.classData());
            project.setScenarios(squads.json());
            QTemporaryDir folder, work;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("world.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("world.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.world()->json(), model.json());
            }
            const auto result = rebuild(project, tools, work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-world-project-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadWorld();
            reopened.loadShops();
            reopened.loadScenarios();
            QCOMPARE(reopened.world()->json(), model.json());
            QCOMPARE(reopened.shops()->json(), shops.json());
            QCOMPARE(reopened.scenarios()->json(), squads.json());
            QCOMPARE(rebuild(reopened, tools, work.path()).image, result.image);
            model.removeTreasure(key, added);
            model.restoreScenario(key);
            model.restoreTown(townId);
            QCOMPARE(model.json(), base);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "World project export:" << candidate;
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void worldResources() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for world resource routing");
        try {
            const auto rom = Rom::open(path);
            const auto resources = WorldResources::load(rom);
            const auto originalShops = Shops::load(rom);
            QCOMPARE(resources.treasureMembers().size(), 40);
            QCOMPARE(resources.siteMembers().size(), 61);
            QCOMPARE(resources.towns(rom).size(), 316 * 28);
            QCOMPARE(resources.rebuild(rom, resources.towns(rom), {}, {}), rom.bytes());
            QCOMPARE(resources.treasureSelector(52), resources.treasureSelector(53));
            QCOMPARE(resources.siteSelector(52), resources.siteSelector(53));
            auto treasure = resources.treasure(rom, 52), sites = resources.sites(rom, 52);
            QVERIFY(u8(treasure, 0) > 0);
            QVERIFY(u8(sites, 0) > 0);
            put16(treasure, 5, 277); // First treasure retains its ID/coordinates/table.
            put16(sites, 3, u16(sites, 3) == 0x2012 ? 4 : 0x2012);
            auto towns = resources.towns(rom);
            const int town = (u16(sites, 1) - 1) * 28;
            put16(towns, town + 22, 4567);
            towns.replace(town + 1, 20, QByteArray("Native Site") + QByteArray(9, 0));
            auto image = resources.rebuild(rom, towns, {{52, treasure}}, {{52, sites}});
            Rom::updateChecksum(image);
            const auto reopened = Rom::fromBytes(image);
            const auto built = WorldResources::load(reopened);
            QVERIFY(built.placement());
            QCOMPARE(built.treasureMembers().size(), 41);
            QCOMPARE(built.siteMembers().size(), 62);
            QCOMPARE(built.treasureSelector(52), 40);
            QCOMPARE(built.siteSelector(52), 61);
            QCOMPARE(built.towns(reopened), towns);
            QCOMPARE(built.shopInventory(reopened), resources.shopInventory(rom));
            const auto shops = Shops::load(reopened);
            for (int id = 0; id < Shops::count; ++id)
                QCOMPARE(shops.stock(id), originalShops.stock(id));
            QCOMPARE(built.rebuild(reopened, towns, {}, {}), image);
            QCOMPARE(image.mid(WorldResources::root + 8, 4),
                     rom.bytes().mid(WorldResources::root + 8, 4));
            QCOMPARE(image.mid(0x2744dcc, 0x3dd), rom.bytes().mid(0x2744dcc, 0x3dd));
            for (int key = 0; key < 65; ++key) {
                QCOMPARE(built.treasure(reopened, key),
                         key == 52 ? treasure : resources.treasure(rom, key));
                QCOMPARE(built.sites(reopened, key), key == 52 ? sites : resources.sites(rom, key));
            }
            // Unchanged compressed members and inactive SCINCSV tails survive exactly.
            for (int i = 1; i < 61; ++i) {
                const auto &a = resources.siteMembers()[i], &b = built.siteMembers()[i];
                QCOMPARE(image.mid(b.offset, b.headerSize + b.storedSize),
                         rom.bytes().mid(a.offset, a.headerSize + a.storedSize));
            }
            auto nextTreasure = treasure;
            nextTreasure[2] ^= char(1);
            const auto nextImage = built.rebuild(reopened, towns, {{52, nextTreasure}}, {});
            const auto nextRom = Rom::fromBytes(nextImage);
            const auto next = WorldResources::load(nextRom);
            QCOMPARE(next.treasureMembers().size(), 41);
            QCOMPARE(next.siteMembers().size(), 62);
            const auto restoredRom = Rom::fromBytes(
                next.rebuild(nextRom, resources.towns(rom), {{52, resources.treasure(rom, 52)}},
                             {{52, resources.sites(rom, 52)}}));
            const auto restored = WorldResources::load(restoredRom);
            QCOMPARE(restored.treasureMembers().size(), 40);
            QCOMPARE(restored.siteMembers().size(), 61);
            auto corrupt = image;
            corrupt[built.placement()->offset + 100] ^= char(1);
            QVERIFY_THROWS_EXCEPTION(Error, WorldResources::load(Rom::fromBytes(corrupt)));
            auto selector = image;
            put8(selector, WorldResources::selectorTable + 52 * 12 + 5, 60);
            QVERIFY_THROWS_EXCEPTION(Error, WorldResources::load(Rom::fromBytes(selector)));
            auto redirected = rom.bytes();
            put32(redirected, 0x1a380, 0x08000000);
            QVERIFY_THROWS_EXCEPTION(Error, WorldResources::load(Rom::fromBytes(redirected)));
            auto collision = rom.bytes();
            collision[WorldResources::descriptorOffset] = 'X';
            QVERIFY_THROWS_EXCEPTION(Error, WorldResources::load(Rom::fromBytes(collision))
                                                .rebuild(Rom::fromBytes(collision), towns, {}, {}));
            QVERIFY_THROWS_EXCEPTION(Error, resources.rebuild(rom, towns + char(0), {}, {}));
            QVERIFY_THROWS_EXCEPTION(Error, resources.rebuild(rom, towns, {{0, treasure}}, {}));
            QVERIFY_THROWS_EXCEPTION(Error, resources.rebuild(reopened, towns, {}, {}));
            const auto foreignRom = Rom::fromBytes(image + QByteArray(4096, 'Z'));
            auto enlarged = treasure;
            for (int i = 0; i < 20000; ++i)
                enlarged += char((i * 3253) ^ (i >> 2));
            const auto grownRom =
                Rom::fromBytes(built.rebuild(foreignRom, towns, {{52, enlarged}}, {}));
            const auto grown = WorldResources::load(grownRom);
            QVERIFY(grown.placement()->offset >= foreignRom.bytes().size());
            QCOMPARE(grownRom.bytes().mid(image.size(), 4096), QByteArray(4096, 'Z'));
            QCOMPARE(grownRom.bytes().mid(built.placement()->offset, built.placement()->allocation),
                     QByteArray(built.placement()->allocation, char(-1)));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-world-resources-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            atomicWrite(candidate, rom.serialize(image));
            QJsonArray rows;
            for (int type = 0; type < 2; ++type) {
                const auto &members = type ? built.siteMembers() : built.treasureMembers();
                for (int i = 0; i < members.size(); ++i)
                    rows << QJsonObject{
                        {"slot", type ? 2 : 0},
                        {"index", i},
                        {"data", QString::fromLatin1(members[i].decode(reopened).toHex())}};
            }
            QJsonArray selectors;
            for (int key = 0; key < 65; ++key)
                selectors << QJsonObject{{"key", key},
                                         {"treasure", built.treasureSelector(key)},
                                         {"sites", built.siteSelector(key)}};
            const QJsonObject fixture{
                {"rom", candidate}, {"sourceRom", path}, {"rows", rows}, {"selectors", selectors}};
            atomicWrite(QCoreApplication::applicationDirPath() + "/world-resources.json",
                        QJsonDocument(fixture).toJson());
            output.setAutoRemove(false);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void scenarioResources() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for scenario resource routing");
        try {
            const auto rom = Rom::open(path);
            const auto before = sha256(rom.bytes());
            const auto resources = ScenarioResources::load(rom);
            QCOMPARE(resources.members().size(), 63);
            QCOMPARE(resources.enemyData().size(), 556 * 35);
            QCOMPARE(resources.selector(10), resources.selector(22));
            QCOMPARE(resources.members()[resources.selector(42)].name, "eset4_06a.bin");
            QCOMPARE(resources.rebuild(rom, resources.enemyData(), {}), rom.bytes());
            QMap<int, QByteArray> edits;
            for (const auto key : {6, 10, 22}) {
                auto eset = Eset::decode(resources.scenario(rom, key));
                eset.setBaseLevel(key == 10 ? 7 : key == 22 ? 8 : 9);
                if (key == 6) {
                    put16(eset.deployments[0], 1, 557);
                    for (int i = 0; i < 8192; ++i)
                        eset.tail += char((i * 7919) ^ (i >> 5));
                }
                edits[key] = eset.encode();
            }
            const auto enemies = resources.enemyData() + resources.enemyData().first(35);
            auto image = resources.rebuild(rom, enemies, edits);
            Rom::updateChecksum(image);
            const auto reopened = Rom::fromBytes(image);
            const auto built = ScenarioResources::load(reopened);
            QVERIFY(built.placement());
            QCOMPARE(built.members().size(), 66);
            QCOMPARE(built.enemyData(), enemies);
            QVERIFY(built.selector(10) != built.selector(22));
            for (int key = 0; key < 65; ++key)
                QCOMPARE(built.scenario(reopened, key),
                         edits.contains(key) ? edits[key] : resources.scenario(rom, key));
            for (int i = 0; i < 63; ++i) {
                const auto &a = resources.members()[i], &b = built.members()[i];
                QCOMPARE(reopened.bytes().mid(b.offset, b.headerSize + b.storedSize),
                         rom.bytes().mid(a.offset, a.headerSize + a.storedSize));
            }
            QCOMPARE(built.rebuild(reopened, enemies, {}), image);
            auto indexed = image;
            indexed[ScenarioResources::selectorTable + 10 * 9 + 8] = 0;
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioResources::load(Rom::fromBytes(indexed)));
            auto corrupted = image;
            corrupted[built.placement()->offset + 80] ^= char(1);
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioResources::load(Rom::fromBytes(corrupted)));
            auto collision = rom.bytes();
            collision[ScenarioResources::descriptorOffset] = 'X';
            QVERIFY_THROWS_EXCEPTION(Error,
                                     resources.rebuild(Rom::fromBytes(collision), enemies, edits));
            QVERIFY_THROWS_EXCEPTION(Error, resources.rebuild(rom, enemies, {{54, edits[10]}}));
            QVERIFY_THROWS_EXCEPTION(Error, resources.rebuild(rom, enemies + char(0), edits));
            auto redirected = rom.bytes();
            put32(redirected, 0x1a380, 0x08000000);
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioResources::load(Rom::fromBytes(redirected)));
            // Repeated changes compact appended ordinals; original alias keys stay untouched.
            auto again = Eset::decode(edits[10]);
            again.setBaseLevel(12);
            const auto secondImage = built.rebuild(reopened, enemies, {{10, again.encode()}});
            const auto secondRom = Rom::fromBytes(secondImage);
            const auto second = ScenarioResources::load(secondRom);
            QCOMPARE(second.members().size(), 66);
            QCOMPARE(second.scenario(secondRom, 22), edits[22]);
            const auto restoredImage = second.rebuild(secondRom, resources.enemyData(),
                                                      {{6, resources.scenario(rom, 6)},
                                                       {10, resources.scenario(rom, 10)},
                                                       {22, resources.scenario(rom, 22)}});
            const auto restoredRom = Rom::fromBytes(restoredImage);
            const auto restored = ScenarioResources::load(restoredRom);
            QCOMPARE(restored.members().size(), 63);
            QCOMPARE(restored.selector(10), resources.selector(10));
            // Growth behind unrelated allocations must relocate without consuming their bytes.
            auto foreign = image + QByteArray(4096, 'Z');
            const auto foreignRom = Rom::fromBytes(foreign);
            auto enlarged = Eset::decode(edits[6]);
            for (int i = 0; i < 20000; ++i)
                enlarged.tail += char((i * 3253) ^ (i >> 2));
            const auto grownImage = built.rebuild(foreignRom, enemies, {{6, enlarged.encode()}});
            const auto grownRom = Rom::fromBytes(grownImage);
            const auto grown = ScenarioResources::load(grownRom);
            QVERIFY(grown.placement()->offset >= foreign.size());
            QCOMPARE(grownImage.mid(image.size(), 4096), QByteArray(4096, 'Z'));
            QCOMPARE(grownImage.mid(built.placement()->offset, built.placement()->allocation),
                     QByteArray(built.placement()->allocation, char(-1)));
            QCOMPARE(sha256(rom.bytes()), before);
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-scenario-resources-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            atomicWrite(candidate, rom.serialize(image));
            QJsonArray rows;
            for (int key = 0; key < 65; ++key)
                rows << QJsonObject{
                    {"key", key},
                    {"originalIndex", resources.selector(key)},
                    {"index", built.selector(key)},
                    {"name", built.members()[built.selector(key)].name},
                    {"data", QString::fromLatin1(built.scenario(reopened, key).toHex())}};
            const QJsonObject fixture{{"rom", candidate},
                                      {"sourceRom", path},
                                      {"rows", rows},
                                      {"enemyData", QString::fromLatin1(enemies.toHex())}};
            atomicWrite(output.filePath("scenarios.json"), QJsonDocument(fixture).toJson());
            atomicWrite(QCoreApplication::applicationDirPath() + "/scenario-resources.json",
                        QJsonDocument(fixture).toJson());
            output.setAutoRemove(false);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void customSquadProjectExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and authenticated compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadEncounters();
            auto model = *project.encounters();
            CustomSquad row;
            row.scenarioKey = 33;
            row.terrainSlot = 9;
            row.label = "Custom squad";
            row.members[0].equipment = {277, 1};
            row.members[1].classId = 16;
            row.members[1].levelOffset = -1;
            row.bonuses = {{5, 25}};
            row.retreatHp = 25;
            row.rewards = {{44, 0x8115, 0}, {2, 3, 1}};
            model.setCustomSquad(row, 165);
            row.scenarioKey = 255;
            row.terrainSlot = 0;
            row.enabled = false;
            row.members[0].levelOffset = -128;
            row.members[1].levelOffset = 127;
            model.setCustomSquad(row, 165);
            model.setScenarioBranch(33, true, ScenarioRoll{7, 2});
            project.setEncounters(model.json());
            QTemporaryDir folder, work;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("squads.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("squads.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.encounters()->json(), model.json());
            }
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-custom-project-XXXXXX");
            QVERIFY(candidate.isValid());
            auto save = [&](const QString &kind, const BuildResult &build) {
                const auto directory = candidate.filePath(kind);
                QDir().mkpath(directory);
                const auto out = QDir(directory).filePath(QFileInfo(path).fileName());
                exportRom(project, build, out);
                return out;
            };
            const auto first = rebuild(project, tools, work.path());
            const auto firstPath = save("initial", first);
            const auto firstPlacement = CustomPlacement::read(Rom::fromBytes(first.image));
            QVERIFY(firstPlacement.has_value());
            auto opened = Project::fromRom(Rom::open(firstPath));
            opened.loadEncounters();
            QCOMPARE(opened.encounters()->json(), project.encounters()->json());
            const auto unchanged =
                rebuild(Project::fromRom(Rom::fromBytes(first.image)), tools, work.path());
            QCOMPARE(unchanged.image, first.image);
            const QByteArray marker(32, char(0x5a));
            const auto later = first.image + marker;
            auto growing = Project::fromRom(Rom::fromBytes(later));
            growing.loadEncounters();
            QCOMPARE(growing.appendClass(5, "New squad class"), 165);
            auto grownModel = *growing.encounters();
            for (int key = 1; key <= 64; ++key) {
                row = CustomSquad{};
                row.scenarioKey = key;
                row.terrainSlot = 9;
                row.members[0].classId = 165;
                row.members[1].equipment = {1, 277};
                grownModel.setCustomSquad(row, 166);
            }
            growing.setEncounters(grownModel.json());
            const auto grown = rebuild(growing, tools, work.path());
            const auto grownPath = save("grown", grown);
            const auto grownRom = Rom::fromBytes(grown.image);
            const auto grownPlacement = CustomPlacement::read(grownRom);
            QVERIFY(grownPlacement.has_value());
            QVERIFY(grownPlacement->profileCount > 56);
            QVERIFY(grownPlacement->dataOffset >= quint64(later.size()));
            QVERIFY(grownPlacement->codeAddress != firstPlacement->codeAddress);
            QCOMPARE(grown.image.mid(first.image.size(), marker.size()), marker);
            QCOMPARE(grown.image.mid(firstPlacement->dataOffset, firstPlacement->dataSize),
                     QByteArray(firstPlacement->dataSize, char(-1)));
            auto reopened = Project::fromRom(grownRom);
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->json(), grownModel.json());
            QCOMPARE(reopened.records(Table::Classes).size(), 166);
            auto removedModel = *reopened.encounters();
            const auto all = removedModel.customSquads().profiles();
            for (const auto &profile : all)
                removedModel.removeCustomSquad(profile.scenarioKey, profile.terrainSlot);
            reopened.setEncounters(removedModel.json());
            const auto removed = rebuild(reopened, tools, work.path());
            save("removed", removed);
            const auto removedRom = Rom::fromBytes(removed.image);
            QVERIFY(!CustomPlacement::read(removedRom));
            QCOMPARE(ClassPlacement::read(removedRom)->residentBytes, 0U);
            QVERIFY(ClassPlacement::read(removedRom)->saves.installed());
            QCOMPARE(removed.image.mid(first.image.size(), marker.size()), marker);
            auto final = Project::fromRom(removedRom);
            final.loadEncounters();
            QCOMPARE(final.encounters()->json(), removedModel.json());
            // Native empty state must override retained browser edits on the next open.
            auto imported = Project::fromRom(rom);
            const auto legacy = QJsonDocument::fromJson(R"({
  "format": "ob64-patch",
  "version": 36,
  "patches": {
    "neutral_encounters": {
      "custom_squads": {
        "33:9": {
          "runtime_key": 33,
          "terrain_slot": 9,
          "label": "Imported",
          "members": [
            {
              "class_id": 5,
              "level_offset_raw": 0,
              "cell": 8,
              "cohort": "A"
            },
            {
              "class_id": 5,
              "level_offset_raw": 255,
              "cell": 6,
              "cohort": "B"
            }
          ]
        }
      }
    }
  }
})")
                                    .object();
            imported.importLegacy(legacy);
            QVERIFY(imported.pendingLegacySections().isEmpty());
            QVERIFY(imported.encounters()->customSquads().hasProfiles());
            auto cleared = *imported.encounters();
            cleared.removeCustomSquad(33, 9);
            imported.setEncounters(cleared.json());
            auto importRestored = Project::fromRom(rom);
            importRestored.applyManifest(imported.manifest());
            QVERIFY(!importRestored.encounters()->customSquads().hasProfiles());
            candidate.setAutoRemove(false);
            qInfo().noquote() << "Public custom squad exports:" << candidate.path();
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void customSquadModel() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for class size/reference checks");
        try {
            const auto rom = Rom::open(path);
            auto classes = ClassData::load(rom);
            auto squads = CustomSquads::load(rom, 165);
            QVERIFY(!squads.modified());
            const auto empty = squads.json();
            CustomSquad row;
            row.scenarioKey = 255;
            row.terrainSlot = 9;
            while (classes.entries().size() < 256)
                classes.append(5, "Custom class");
            row.members[0].classId = 255;
            row.members[0].levelOffset = -128;
            row.members[0].equipment = {277, 1};
            row.members[1].classId = 165;
            row.members[1].levelOffset = 127;
            row.members[1].equipment = {1, 277};
            row.label = "123456789012345678901234";
            row.bonuses = {{255, 100}, {165, 1}};
            row.retreatHp = 100;
            row.rewards = {{0, 44, 0x8115}, {65535, 0, 65535}};
            squads.set(row, 256);
            squads.validateFormations(classes);
            QCOMPARE(squads.changedRows(), 1);
            const auto saved = squads.json();
            CustomSquads restored;
            restored.apply(saved, 256);
            QCOMPARE(restored.json(), saved);
            const auto data = squads.bytes(0x2800000), profile = row.bytes();
            QCOMPARE(u32(data, 2559 * 4), 0x2800000U + CustomPlacement::indexBytes);
            QCOMPARE(data.mid(CustomPlacement::indexBytes), profile);
            QCOMPARE(u8(profile, 9), 128);
            QCOMPARE(u8(profile, 17), 127);
            QCOMPARE(profile.mid(68, 38),
                     QByteArray("@b@%0@n\"123456789012345678901234\"@a@c\0", 38));
            for (const auto &field : {"classId", "levelOffset", "cell", "equipment"}) {
                auto bad = row.json();
                auto members = bad["members"].toArray();
                auto member = members[0].toObject();
                member[field] = field == QString("equipment")
                                    ? QJsonValue(QJsonArray{0, 278})
                                    : QJsonValue(field == QString("classId")       ? 256
                                                 : field == QString("levelOffset") ? 128
                                                                                   : 9);
                members[0] = member;
                bad["members"] = members;
                QVERIFY_THROWS_EXCEPTION(Error, CustomSquad::parse(bad, 256));
            }
            QVERIFY_THROWS_EXCEPTION(Error, squads.set(row, 165));
            auto bad = row;
            bad.members[1].cell = bad.members[0].cell;
            QVERIFY_THROWS_EXCEPTION(Error, squads.set(bad, 256));
            bad = row;
            bad.bonuses.append({255, 1});
            QVERIFY_THROWS_EXCEPTION(Error, squads.set(bad, 256));
            bad = row;
            bad.rewards.weights = {0, 0, 0};
            QVERIFY_THROWS_EXCEPTION(Error, squads.set(bad, 256));
            for (const auto &label :
                 {QString(), QString(25, 'A'), QString("Bad@text"), QString("Bad\"text"),
                  QString("Bad\\text"), QString::fromUtf8("é")}) {
                bad = row;
                bad.label = label;
                QVERIFY_THROWS_EXCEPTION(Error, squads.set(bad, 256));
            }
            auto large = classes.entries()[255];
            large.record[4] = 2;
            classes.set(255, large);
            squads.validateFormations(classes);
            bad = row;
            bad.members[1].cell = 7;
            squads.set(bad, 256);
            QVERIFY_THROWS_EXCEPTION(Error, squads.validateFormations(classes));
            bad = row;
            bad.members = {{255, 0, 8, {0, 0}}, {255, 0, 6, {0, 0}}, {255, 0, 0, {0, 0}}};
            squads.set(bad, 256);
            QVERIFY_THROWS_EXCEPTION(Error, squads.validateFormations(classes));
            squads.apply(saved, 256);
            const auto legacyRow = QJsonObject{
                {"profile_id", 1},
                {"runtime_key", 33},
                {"terrain_slot", 9},
                {"slice", 40},
                {"label", "Bandits!"},
                {"members", QJsonArray{QJsonObject{{"class_id", 5},
                                                   {"level_offset_raw", 255},
                                                   {"cell", 8},
                                                   {"cohort", "A"}},
                                       QJsonObject{{"class_id", 16},
                                                   {"level_offset_raw", 127},
                                                   {"cell", 6},
                                                   {"cohort", "B"}}}},
                {"equipment", QJsonObject{{"A", QJsonArray{277, 1}}, {"B", QJsonArray{0, 277}}}},
                {"persuasion",
                 QJsonObject{{"mode", "fixed"},
                             {"chance", 37},
                             {"class_bonuses",
                              QJsonArray{QJsonObject{{"class_id", 5}, {"bonus_percent", 10}}}}}},
                {"retreat", QJsonObject{{"hp_threshold", 25}}},
                {"rewards",
                 QJsonObject{{"slots", QJsonArray{QJsonObject{{"raw", 44}, {"weight", 2}},
                                                  QJsonObject{{"raw", 0x8115}, {"weight", 3}},
                                                  QJsonObject{{"raw", 0}, {"weight", 1}}}}}}};
            CustomSquads imported;
            imported.importLegacy({{"33:9", legacyRow}}, 165);
            const auto &legacy = imported.profiles()[339];
            QCOMPARE(legacy.members[0].levelOffset, -1);
            QCOMPARE(legacy.members[1].equipment[1], 277);
            QCOMPARE(legacy.chance, 37);
            QCOMPARE(legacy.rewards.outcomes[0], 44);
            const auto before = imported.json();
            QVERIFY_THROWS_EXCEPTION(
                Error, imported.importLegacy({{"33:9", QJsonValue::Null}, {"34:0", 7}}, 165));
            QCOMPARE(imported.json(), before);
            imported.importLegacy({{"33:9", QJsonValue::Null}}, 165);
            QVERIFY(!imported.hasProfiles());
            restored.apply(empty, 256);
            QVERIFY(!restored.modified());
            row = CustomSquad{};
            for (int key = 0; key < 256; ++key)
                for (int slot = 0; slot < 10; ++slot) {
                    row.scenarioKey = key;
                    row.terrainSlot = slot;
                    row.enabled = (slot % 2) == 0;
                    restored.set(row, 165);
                }
            QCOMPARE(restored.profiles().size(), 2560);
            const auto expanded = restored.bytes(0x2800000);
            QCOMPARE(expanded.size(), CustomPlacement::indexBytes + 2560 * 128);
            for (int i = 0; i < 2560; ++i)
                QCOMPARE(u32(expanded, i * 4),
                         quint32(0x2800000 + CustomPlacement::indexBytes + i * 128));
            QVERIFY_THROWS_EXCEPTION(Error, restored.bytes(0x3fffff0));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void customCallerInstallation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and authenticated compiler required");
        try {
            const auto rom = Rom::open(path);
            Compiler compiler(tools);
            compiler.authenticate();
            QTemporaryDir work;
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-custom-callers-XXXXXX");
            QVERIFY(candidate.isValid());
            QJsonArray probes;
            for (const bool scenario : {false, true}) {
                CustomRuntimeBuild custom;
                const auto runtime = buildClassRuntime(
                    ClassData::load(rom), compiler, work.path(), 0x2800000, scenario,
                    [&](quint32 at, quint32 size) {
                        custom = buildCustomRuntime(compiler, work.path(), at, size, 0x2900000);
                        return custom.bytes;
                    });
                auto image = linkClassRuntime(rom, runtime);
                auto rewards = CreatureRewards::load(rom);
                rewards.setRow(5, CreatureReward{{0x8115, 44, 0}, {2, 3, 1}});
                image =
                    rebuildCreatureRewards(Rom::fromBytes(image), rewards, compiler, work.path());
                auto rate = EncounterRate::load(rom);
                rate.apply({{"divisor", 1000}, {"normalPassCount", 20}, {"alternatePassCount", 0}});
                auto rates = ScenarioRates::load(rom);
                if (scenario)
                    rates.setBranch(33, true, ScenarioRoll{7, 2});
                image =
                    rebuildScenarioRates(Rom::fromBytes(image), rates, rate, compiler, work.path());
                Rom::updateChecksum(image);
                const auto base = image;
                CustomPlacement placed;
                placed.dataOffset = 0x2900000;
                placed.profileCount = 1;
                placed.dataSize = CustomPlacement::indexBytes + 128;
                placed.codeAddress = custom.code.address;
                placed.codeBytes = quint32(custom.code.bytes.size());
                placed.residentBytes = quint32(custom.bytes.size());
                placed.stateAddress = custom.stateAddress;
                for (const auto &name : customRuntimeEntries())
                    placed.entries[name] = custom.code.entries[name];
                QByteArray data(placed.dataSize, 0);
                put32(data, (33 * 10 + 9) * 4, placed.dataOffset + CustomPlacement::indexBytes);
                const auto at = CustomPlacement::indexBytes;
                put8(data, at, 1);
                put8(data, at + 1, 2);
                put8(data, at + 2, 1);
                put8(data, at + 3, 100);
                put8(data, at + 4, 30);
                put8(data, at + 5, 33);
                put8(data, at + 6, 9);
                put8(data, at + 8, 5);
                put8(data, at + 10, 8);
                put16(data, at + 12, 277);
                put8(data, at + 16, 71);
                put8(data, at + 17, 255);
                put8(data, at + 18, 6);
                put16(data, at + 56, 44);
                put16(data, at + 58, 1);
                const QByteArray message("@b@%0@n\"Custom squad\"@a@c");
                data.replace(at + 68, message.size(), message);
                QCOMPARE(compiler.compileData(data, work.path(), "custom_profiles"), data);
                image = rebuildCustomCallers(Rom::fromBytes(base), compiler, work.path(),
                                             placed.entries);
                image.append(
                    QByteArray(placed.dataOffset + placed.dataSize - image.size(), char(-1)));
                image.replace(placed.dataOffset, data.size(), data);
                const auto header = placed.descriptor(data, custom.bytes);
                QCOMPARE(
                    image.mid(CustomPlacement::descriptorOffset, CustomPlacement::descriptorSize),
                    QByteArray(CustomPlacement::descriptorSize, char(-1)));
                image.replace(CustomPlacement::descriptorOffset, header.size(), header);
                Rom::updateChecksum(image);
                const auto rebuilt = Rom::fromBytes(image);
                const auto actual = CustomPlacement::read(rebuilt);
                QVERIFY(actual.has_value());
                QCOMPARE(actual->entries, placed.entries);
                verifyCustomCallers(rebuilt, actual);
                QCOMPARE(EncounterRate::load(rebuilt).json(), rate.json());
                QCOMPARE(ScenarioRates::load(rebuilt).json(), rates.json());
                QCOMPARE(CreatureRewards::load(rebuilt).json(), rewards.json());
                const auto squads = CustomSquads::load(rebuilt, 165);
                squads.validateFormations(ClassData::load(rebuilt));
                QCOMPARE(squads.profiles().size(), 1);
                QCOMPARE(squads.profiles()[339].members[1].levelOffset, -1);
                QCOMPARE(squads.bytes(placed.dataOffset), data);
                QVERIFY(!squads.modified());
                auto corrupt = image;
                corrupt[0x214a08] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error,
                                         verifyCustomCallers(Rom::fromBytes(corrupt), actual));
                corrupt = image;
                corrupt[placed.codeAddress - ClassPlacement::residentDelta] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, CustomPlacement::read(Rom::fromBytes(corrupt)));
                auto badIndex = data;
                put32(badIndex, (33 * 10 + 9) * 4,
                      placed.dataOffset + CustomPlacement::indexBytes + 128);
                corrupt = image;
                corrupt.replace(placed.dataOffset, badIndex.size(), badIndex);
                corrupt.replace(CustomPlacement::descriptorOffset, header.size(),
                                placed.descriptor(badIndex, custom.bytes));
                QVERIFY_THROWS_EXCEPTION(Error, CustomPlacement::read(Rom::fromBytes(corrupt)));
                auto removed = rebuildCustomCallers(rebuilt, compiler, work.path(), {}, actual);
                removed.replace(CustomPlacement::descriptorOffset, CustomPlacement::descriptorSize,
                                QByteArray(CustomPlacement::descriptorSize, char(-1)));
                removed.truncate(base.size());
                Rom::updateChecksum(removed);
                QCOMPARE(removed, base);
                verifyCustomCallers(Rom::fromBytes(removed), std::nullopt);
                const auto folder = candidate.filePath(scenario ? "scenario" : "global");
                QVERIFY(QDir().mkpath(folder));
                const auto output = QDir(folder).filePath(QFileInfo(path).fileName());
                atomicWrite(output, rom.serialize(image));
                QJsonObject entries, symbols;
                const auto services = customEncounterServices();
                for (auto it = custom.code.entries.begin(); it != custom.code.entries.end(); ++it)
                    entries[it.key()] = double(it.value());
                for (auto it = services.begin(); it != services.end(); ++it)
                    symbols[it.key()] = double(it.value());
                symbols["lc_custom_index"] = double(placed.dataOffset);
                probes << QJsonObject{{"rom", output},
                                      {"state", double(custom.stateAddress)},
                                      {"entries", entries},
                                      {"symbols", symbols},
                                      {"expandedSaves", scenario},
                                      {"installed", true}};
            }
            const auto fixture = candidate.filePath("custom-callers.json");
            atomicWrite(fixture, QJsonDocument(probes).toJson());
            candidate.setAutoRemove(false);
            qInfo().noquote() << "Custom caller installation probes:" << fixture;
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void residentCustomAllocation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and authenticated compiler required");
        try {
            const auto rom = Rom::open(path);
            const auto classes = ClassData::load(rom);
            QTemporaryDir build;
            Compiler compiler(tools);
            compiler.authenticate();
            CustomRuntimeBuild custom;
            quint32 state = 0;
            bool fillAllocation = false;
            auto resident = [&](quint32 begin, quint32 capacity) {
                custom = buildCustomRuntime(compiler, build.path(), begin, capacity, 0x2900000);
                state = custom.stateAddress;
                return fillAllocation ? custom.bytes + QByteArray(capacity - custom.bytes.size(), 0)
                                      : custom.bytes;
            };
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(candidates));
            QTemporaryDir candidate(candidates + "/native-resident-XXXXXX");
            QVERIFY(candidate.isValid());
            QJsonArray probes;
            auto previous = rom;
            for (int mode = 0; mode < 3; ++mode) {
                const bool saves = mode != 0;
                fillAllocation = mode == 2;
                const auto runtime =
                    buildClassRuntime(classes, compiler, build.path(), 0x2800000, saves, resident);
                const auto linked = linkClassRuntime(previous, runtime);
                const auto reopened = Rom::fromBytes(linked);
                const auto placement = ClassPlacement::read(reopened);
                QVERIFY(placement.has_value());
                QCOMPARE(u32(linked, ClassPlacement::descriptorRomOffset + 8), 4U);
                QCOMPARE(placement->residentAddress,
                         saves ? placement->saves.endAddress() : ClassPlacement::residentBegin);
                QCOMPARE(placement->residentBytes, quint32(runtime.resident.size()));
                QCOMPARE(placement->residentChecksum, crc32(runtime.resident));
                QCOMPARE(ClassData::load(reopened).build(0x80243db0).bytes, runtime.compiledData);
                QCOMPARE(linked.mid(placement->residentAddress - ClassPlacement::residentDelta,
                                    placement->residentBytes),
                         runtime.resident);
                const auto cacheEnd = quint32(((u32(linked, 0x2438) & 65535) << 16) +
                                              qint16(u32(linked, 0x243c) & 65535));
                QCOMPARE(cacheEnd, placement->residentAddress + placement->residentBytes);
                auto corrupt = linked;
                corrupt[placement->residentAddress - ClassPlacement::residentDelta] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
                auto removed = buildClassRuntime(classes, compiler, build.path(), 0x2800000, saves);
                QVERIFY_THROWS_EXCEPTION(Error, linkClassRuntime(reopened, removed));
                removed.removeResident = true;
                const auto without = linkClassRuntime(reopened, removed);
                QCOMPARE(ClassPlacement::read(Rom::fromBytes(without))->residentBytes, 0U);
                QCOMPARE(without.mid(placement->residentAddress - ClassPlacement::residentDelta,
                                     placement->residentBytes),
                         QByteArray(runtime.resident.size(), 0));
                auto crc = linked;
                Rom::updateChecksum(crc);
                QCOMPARE(crc, linked);
                const auto directory = candidate.filePath(fillAllocation ? "full-allocation"
                                                          : saves        ? "with-saves"
                                                                         : "without-saves");
                QVERIFY(QDir().mkpath(directory));
                const auto output = QDir(directory).filePath(QFileInfo(path).fileName());
                atomicWrite(output, rom.serialize(linked));
                QJsonObject entries;
                for (auto it = custom.code.entries.begin(); it != custom.code.entries.end(); ++it)
                    entries[it.key()] = double(it.value());
                QJsonObject symbols;
                const auto services = customEncounterServices();
                for (auto it = services.begin(); it != services.end(); ++it)
                    symbols[it.key()] = double(it.value());
                symbols["lc_custom_index"] = double(0x2900000);
                probes << QJsonObject{{"rom", output},
                                      {"state", double(state)},
                                      {"entries", entries},
                                      {"symbols", symbols},
                                      {"codeBytes", custom.code.bytes.size()},
                                      {"expandedSaves", saves}};
                previous = reopened;
            }
            QVERIFY_THROWS_EXCEPTION(Error,
                                     buildClassRuntime(classes, compiler, build.path(), 0x2800000,
                                                       false, [](quint32, quint32 bytes) {
                                                           return QByteArray(bytes + 16, 0);
                                                       }));
            QVERIFY_THROWS_EXCEPTION(
                Error, buildClassRuntime(classes, compiler, build.path(), 0x2800000, false,
                                         [](quint32, quint32) { return QByteArray(17, 0); }));
            const auto fixture = candidate.filePath("resident.json");
            atomicWrite(fixture, QJsonDocument(probes).toJson());
            candidate.setAutoRemove(false);
            qInfo().noquote() << "Resident allocation probes (no custom caller installation):"
                              << fixture;
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void customEncounterSource() {
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty())
            QSKIP("Authenticated compiler required");
        try {
            QFile source(":/game/encounters/custom.c"), header(":/game/encounters/custom.h");
            QVERIFY(source.open(QIODevice::ReadOnly));
            QVERIFY(header.open(QIODevice::ReadOnly));
            QTemporaryDir build;
            Compiler compiler(tools);
            compiler.authenticate();
            const QMap<QString, quint32> symbols{{"lc_custom_state", 0x8018a9e0},
                                                 {"lc_custom_index", 0x2800000},
                                                 {"lc_staged_scenario", 0x8018f481},
                                                 {"lc_character_records", 0x80195560},
                                                 {"lc_source_records", 0x801971f0},
                                                 {"lc_dma_read", 0x8009da50},
                                                 {"lc_random", 0x80092a60},
                                                 {"lc_character_create", 0x801d0840},
                                                 {"lc_actor_at_index", 0x801c8fe8},
                                                 {"lc_persuasion_filter_a", 0x801c8e30},
                                                 {"lc_persuasion_filter_b", 0x801c8e9c},
                                                 {"lc_persuasion_filter_c", 0x801c8e64},
                                                 {"lc_persuasion_filter_d", 0x801c8e80},
                                                 {"lc_persuasion_eligible", 0x801c8b68},
                                                 {"lc_persuasion_random", 0x8009c7cc}};
            const auto module =
                compiler.compileModule(source.readAll(), build.path(), "custom_core", 0x801897d0,
                                       {{"custom.h", header.readAll()}}, symbols);
            QVERIFY(module.address + module.bytes.size() <= symbols["lc_custom_state"]);
            QJsonObject entries, bindings;
            for (auto it = module.entries.begin(); it != module.entries.end(); ++it)
                entries[it.key()] = double(it.value());
            for (auto it = symbols.begin(); it != symbols.end(); ++it)
                bindings[it.key()] = double(it.value());
            const QString fixture = QCoreApplication::applicationDirPath() + "/custom-source.json";
            atomicWrite(
                fixture,
                QJsonDocument(QJsonObject{{"address", double(module.address)},
                                          {"code", QString::fromLatin1(module.bytes.toHex())},
                                          {"entries", entries},
                                          {"symbols", bindings}})
                    .toJson());
            qInfo().noquote() << "Custom encounter core:" << module.bytes.size() << "bytes; fixture"
                              << fixture;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void scenarioRatesWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadEncounters();
            const auto original = project.encounters()->json();
            QVERIFY(!project.encounters()->scenarioRates().hasOverrides());
            auto model = *project.encounters();
            model.setGlobalRate(
                {{"divisor", 1000000}, {"normalPassCount", 20000}, {"alternatePassCount", 50}});
            model.setScenarioBranch(59, true, ScenarioRoll{0xffffffff, 0x80000000});
            model.setScenarioBranch(59, false, ScenarioRoll{1, 0});
            model.setScenarioBranch(64, false, ScenarioRoll{65536, 65536});
            model.setScenarioBranch(65, true, ScenarioRoll{3, 1});
            model.setScenarioBranch(66, false, ScenarioRoll{7, 2});
            model.setScenarioBranch(255, true, ScenarioRoll{0xffffffff, 0xffffffff});
            project.setEncounters(model.json());
            const auto before = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, model.setScenarioBranch(1, true, ScenarioRoll{0, 0}));
            QVERIFY_THROWS_EXCEPTION(Error, model.setScenarioBranch(1, true, ScenarioRoll{5, 6}));
            QVERIFY_THROWS_EXCEPTION(Error, model.setScenarioBranch(256, true, ScenarioRoll{5, 1}));
            QCOMPARE(model.json(), project.encounters()->json());
            auto bad = model.json();
            bad["scenarioRates"] = QJsonObject{
                {"version", 1},
                {"rows",
                 QJsonObject{
                     {"65", QJsonObject{{"normal", QJsonObject{{"mode", "override"},
                                                               {"divisor", 1},
                                                               {"passCount", true}}},
                                        {"alternate", QJsonObject{{"mode", "inherit"}}}}}}}};
            QVERIFY_THROWS_EXCEPTION(Error, project.setEncounters(bad));
            QCOMPARE(project.manifest(), before);
            QTemporaryDir folder, build;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("rates.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("rates.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.encounters()->json(), model.json());
            }
            const auto result = rebuild(project, tools, build.path());
            const auto placement = ScenarioRates::placement(Rom::fromBytes(result.image));
            QVERIFY(placement);
            QCOMPARE(placement->offset, quint32(rom.bytes().size()));
            QCOMPARE(result.image.size(), rom.bytes().size() + 4352);
            auto conserved = result.image.first(rom.bytes().size());
            conserved.replace(0x13c060, 1084, rom.bytes().mid(0x13c060, 1084));
            conserved.replace(0x27c4200, 64, rom.bytes().mid(0x27c4200, 64));
            conserved.replace(0x10, 8, rom.bytes().mid(0x10, 8));
            QCOMPARE(conserved, rom.bytes());
            auto checksummed = result.image;
            Rom::updateChecksum(checksummed);
            QCOMPARE(checksummed, result.image);
            QTemporaryDir output(QDir(QFileInfo(path).absolutePath())
                                     .filePath("Test ROMs/native-scenario-rates-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto target = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, target);
            auto reopened = Project::fromRom(Rom::open(target));
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->json(), model.json());
            atomicWrite(output.filePath("scenario-rates.json"),
                        QJsonDocument(QJsonObject{{"rom", target},
                                                  {"sourceRom", path},
                                                  {"dataOffset", double(placement->offset)},
                                                  {"global", model.globalRate().json()},
                                                  {"rates", model.scenarioRates().json()}})
                            .toJson());
            qInfo().noquote() << "Scenario-rate candidate:"
                              << output.filePath("scenario-rates.json");
            auto changed = *reopened.encounters();
            changed.setGlobalRate(
                {{"divisor", 72000}, {"normalPassCount", 1000}, {"alternatePassCount", 0}});
            reopened.setEncounters(changed.json());
            const auto updated = rebuild(reopened, tools, build.path());
            const auto nextPlacement = ScenarioRates::placement(Rom::fromBytes(updated.image));
            QVERIFY(nextPlacement);
            QCOMPARE(nextPlacement->offset, placement->offset);
            QCOMPARE(updated.image.size(), result.image.size());
            QCOMPARE(u32(updated.image, placement->offset + 16 + 12),
                     quint32(1000)); // Unlisted key1 inherits normal.
            QCOMPARE(u32(updated.image, placement->offset + 65 * 16 + 12),
                     quint32(1)); // Explicit key65 keeps its count.
            for (int key = 0; key < 256; ++key)
                for (bool normal : {false, true})
                    changed.setScenarioBranch(key, normal, std::nullopt);
            reopened.setEncounters(changed.json());
            const auto removed = rebuild(reopened, tools, build.path());
            QVERIFY(!ScenarioRates::placement(Rom::fromBytes(removed.image)));
            QCOMPARE(removed.image.size(), rom.bytes().size());
            QCOMPARE(EncounterRate::load(Rom::fromBytes(removed.image)).json(),
                     changed.globalRate().json());
            // Removing an allocation beneath another owner's data must not truncate that data.
            auto trailingImage = result.image + QByteArray(32, char(0x5a));
            auto trailing = Project::fromRom(Rom::fromBytes(trailingImage));
            trailing.loadEncounters();
            auto cleared = *trailing.encounters();
            for (int key = 0; key < 256; ++key)
                for (bool normal : {false, true})
                    cleared.setScenarioBranch(key, normal, std::nullopt);
            trailing.setEncounters(cleared.json());
            const auto trailingResult = rebuild(trailing, tools, build.path());
            QCOMPARE(trailingResult.image.size(), trailingImage.size());
            QCOMPARE(trailingResult.image.last(32), QByteArray(32, char(0x5a)));
            QCOMPARE(trailingResult.image.mid(placement->offset, 4352), QByteArray(4352, char(-1)));
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{
                     {"neutral_encounters",
                      QJsonObject{
                          {"scenario_rates",
                           QJsonObject{
                               {"59", QJsonObject{
                                          {"slice", 41},
                                          {"normal", QJsonObject{{"mode", "override"},
                                                                 {"divisor", 65536},
                                                                 {"pass_count", 65536}}},
                                          {"alternate", QJsonObject{{"mode", "disabled"}}}}}}}}}}}};
            auto imported = Project::fromRom(rom);
            imported.importLegacy(legacy);
            QVERIFY(imported.pendingLegacySections().isEmpty());
            QCOMPARE(imported.encounters()->scenarioRates().branch(59, true)->passCount,
                     quint32(65536));
            QCOMPARE(imported.encounters()->scenarioRates().branch(59, false)->passCount,
                     quint32(0));
            imported.setEncounters(original);
            auto restored = Project::fromRom(rom);
            restored.applyManifest(imported.manifest());
            QCOMPARE(restored.encounters()->json(), original);
            auto oldNative = imported.manifest();
            auto oldEncounters = oldNative["encounters"].toObject();
            oldEncounters.remove("scenarioRates");
            oldNative["encounters"] = oldEncounters;
            restored.applyManifest(oldNative);
            QCOMPARE(restored.encounters()->scenarioRates().branch(59, true)->passCount,
                     quint32(65536));
            for (int at : {0x13c270, 0x27c4200 + 36, int(placement->offset) + 4096, 0x388d8}) {
                auto corrupt = result.image;
                corrupt[at] ^= 4;
                QVERIFY_THROWS_EXCEPTION(Error, EncounterRate::load(Rom::fromBytes(corrupt)));
            }
            auto occupied = rom.bytes();
            occupied[0x27c4200] = 'X';
            auto collision = Project::fromRom(Rom::fromBytes(occupied));
            collision.loadEncounters();
            collision.setEncounters(model.json());
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(collision, tools, build.path()));
            QCOMPARE(project.appendClass(1, "Scenario tester"), 165);
            auto combined = *project.encounters();
            combined.setPair(45, 9, 165, 0, 166);
            combined.setReward(165, CreatureReward{{0x8115, 44, 0}, {2, 1, 1}});
            project.setEncounters(combined.json());
            const auto composed = rebuild(project, tools, build.path());
            auto composedProject = Project::fromRom(Rom::fromBytes(composed.image));
            composedProject.loadEncounters();
            QCOMPARE(composedProject.records(Table::Classes).size(), 166);
            QCOMPARE(composedProject.encounters()->json(), combined.json());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void creatureRewardsWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadEncounters();
            const auto original = project.encounters()->json();
            const auto rewards = project.encounters()->rewards();
            // Independent search follows halfword keys, first match, and the zero-key fallback.
            for (int id = 0; id < 256; ++id) {
                int row = 0;
                while (u16(rom.bytes(), 0x142258 + row * 8) &&
                       u16(rom.bytes(), 0x142258 + row * 8) != id)
                    ++row;
                for (int s = 0; s < 3; ++s)
                    QCOMPARE(rewards.row(id).outcomes[s],
                             int(u16(rom.bytes(), 0x14225a + row * 8 + s * 2)));
            }
            auto special = rom.bytes();
            put16(special, 0x142258,
                  0x100 | u8(special, 0x142259)); // Nonzero pad is part of the key.
            put16(special, 0x142258 + 35 * 8 + 2,
                  0x8001); // Zero key need not mean zero outcomes.
            const auto specialRewards = CreatureRewards::load(Rom::fromBytes(special));
            QCOMPARE(specialRewards.row(255).outcomes[0], 0x8001);
            QCOMPARE(specialRewards.row(u8(special, 0x142259)).outcomes[0], 0x8001);
            QTemporaryDir build, folder;
            Compiler compiler(tools);
            compiler.authenticate();
            QFile source(":/game/encounters/rewards.s");
            QVERIFY(source.open(QIODevice::ReadOnly));
            QCOMPARE(
                compiler.assembleOwner(CreatureRewards::definitions(0, false) + source.readAll(),
                                       build.path(), "reward_retail", CreatureRewards::ownerAddress,
                                       CreatureRewards::services(), true),
                rom.bytes().mid(0x101ddc, 4388));
            QCOMPARE(rebuild(project, tools, build.path()).image, rom.bytes());
            auto model = *project.encounters();
            for (int id = 0; id < 256; ++id) {
                CreatureReward row{{(id % 277 + 1) | 0x8000, id % 44 + 1, 0},
                                   {id % 7, (id * 3) % 11, (id * 5) % 13}};
                if (id % 5 == 0)
                    row.weights = {65535, 65535, 65535};
                if (id % 5 == 1)
                    row.weights = {0, 0, 1};
                if (id % 5 == 2)
                    row.weights = {0, 1, 0};
                if (id % 5 == 3)
                    row.weights = {1, 0, 0};
                if (id % 5 == 4)
                    row.weights = {1, 2, 3};
                model.setReward(id, row);
            }
            project.setEncounters(model.json());
            const auto before = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error,
                                     model.setReward(1, CreatureReward{{1, 2, 3}, {0, 0, 0}}));
            QVERIFY_THROWS_EXCEPTION(Error,
                                     model.setReward(1, CreatureReward{{45, 2, 3}, {1, 1, 1}}));
            QVERIFY_THROWS_EXCEPTION(Error, model.setReward(256, CreatureReward{}));
            QCOMPARE(model.json(), project.encounters()->json());
            auto bad = model.json(), rewardJson = bad["rewards"].toObject();
            auto rows = rewardJson["rows"].toArray();
            rows[255] = QJsonObject{};
            rewardJson["rows"] = rows;
            bad["rewards"] = rewardJson;
            QVERIFY_THROWS_EXCEPTION(Error, project.setEncounters(bad));
            QCOMPARE(project.manifest(), before);
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("rewards.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("rewards.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.encounters()->json(), project.encounters()->json());
            }
            const auto result = rebuild(project, tools, build.path());
            const auto offset = CreatureRewards::placement(Rom::fromBytes(result.image));
            QVERIFY(offset);
            QCOMPARE(*offset, quint32(rom.bytes().size()));
            QCOMPARE(result.image.size(), rom.bytes().size() + 4096);
            QCOMPARE(result.image.mid(0x142258, 296), rom.bytes().mid(0x142258, 296));
            auto restoredBytes = result.image.first(rom.bytes().size());
            restoredBytes.replace(0x101ddc, 4388, rom.bytes().mid(0x101ddc, 4388));
            restoredBytes.replace(0x27c4100, 64, rom.bytes().mid(0x27c4100, 64));
            restoredBytes.replace(0x10, 8, rom.bytes().mid(0x10, 8));
            QCOMPARE(restoredBytes,
                     rom.bytes()); // Entire original image outside the owned ranges.
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            QTemporaryDir output(
                QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-rewards-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto target = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, target);
            auto reopened = Project::fromRom(Rom::open(target));
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->json(), model.json());
            atomicWrite(output.filePath("rewards.json"),
                        QJsonDocument(QJsonObject{{"rom", target},
                                                  {"rewards", model.rewards().json()},
                                                  {"sourceRom", path},
                                                  {"dataOffset", double(*offset)}})
                            .toJson());
            qInfo().noquote() << "Creature rewards candidate:" << output.filePath("rewards.json");
            auto changed = *reopened.encounters();
            changed.setReward(255, CreatureReward{{0x8115, 44, 0}, {65535, 0, 1}});
            reopened.setEncounters(changed.json());
            const auto next = rebuild(reopened, tools, build.path());
            QCOMPARE(next.image.size(), result.image.size());
            QCOMPARE(CreatureRewards::placement(Rom::fromBytes(next.image)), offset);
            QCOMPARE(CreatureRewards::load(Rom::fromBytes(next.image)).row(255).json(),
                     changed.rewards().row(255).json());
            QCOMPARE(reopened.appendClass(1, "Reward tester"), 165);
            auto expanded = *reopened.encounters();
            expanded.setPair(45, 9, 165, 0, 166);
            reopened.setEncounters(expanded.json());
            const auto grown = rebuild(reopened, tools, build.path());
            auto grownProject = Project::fromRom(Rom::fromBytes(grown.image));
            grownProject.loadEncounters();
            QCOMPARE(grownProject.records(Table::Classes).size(), 166);
            QCOMPARE(grownProject.encounters()->rewards().row(165).json(),
                     model.rewards().row(165).json());
            const int firstClass = u8(rom.bytes(), 0x142259);
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{{"creatureDrops",
                              QJsonObject{{"0", QJsonObject{{"record_index", 0},
                                                            {"classId", firstClass},
                                                            {"padByte", 0},
                                                            {"slots", QJsonArray{0x8115, 44, 0}},
                                                            {"weights", QJsonArray{0, 65535, 1}},
                                                            {"weights_modified", true}}}}}}}};
            auto imported = Project::fromRom(rom);
            imported.importLegacy(legacy);
            QVERIFY(imported.pendingLegacySections().isEmpty());
            const CreatureReward importedReward{{0x8115, 44, 0}, {0, 65535, 1}};
            QCOMPARE(imported.encounters()->rewards().row(firstClass).json(),
                     importedReward.json());
            auto merged = Project::fromRom(Rom::fromBytes(result.image));
            merged.importLegacy(legacy);
            QCOMPARE(merged.encounters()->rewards().row(firstClass).json(), importedReward.json());
            QCOMPARE(merged.encounters()->rewards().row(255).json(),
                     model.rewards().row(255).json());
            imported.setEncounters(original);
            auto restored = Project::fromRom(rom);
            restored.applyManifest(imported.manifest());
            QCOMPARE(restored.encounters()->json(), original);
            auto oldNative = imported.manifest();
            auto oldEncounters = oldNative["encounters"].toObject();
            oldEncounters.remove("rewards");
            oldNative["encounters"] = oldEncounters;
            restored.applyManifest(oldNative);
            QCOMPARE(restored.encounters()->rewards().row(firstClass).weights[1], 65535);
            for (const auto at : {0x101e00, 0x102488, 0x27c4100 + 28, int(*offset) + 10}) {
                auto corrupt = result.image;
                corrupt[at] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, CreatureRewards::load(Rom::fromBytes(corrupt)));
            }
            auto occupied = rom.bytes();
            occupied[0x27c4100] = 'X';
            auto blocked = Project::fromRom(Rom::fromBytes(occupied));
            blocked.loadEncounters();
            blocked.setEncounters(model.json());
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(blocked, tools, build.path()));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void encounterRateSourceExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto rate = EncounterRate::load(rom);
            QCOMPARE(rate.divisor(), quint32(72000));
            QCOMPARE(rate.normalPassCount(), quint32(51));
            QCOMPARE(rate.alternatePassCount(), quint32(11));
            QCOMPARE(EncounterRate::passingFraction(0x40000000, 0x20000000), 0.5);
            QCOMPARE(EncounterRate::passingFraction(0x80000000, 0x20000000), 0.5);
            QCOMPARE(EncounterRate::passingFraction(0xffffffff, 0x80000000), 1.0);
            QCOMPARE(EncounterRate::passingFraction(3, 1), 357913942.0 / 1073741824.0);
            QCOMPARE(EncounterRate::passingFraction(72000, 0), 0.0);
            QVERIFY_THROWS_EXCEPTION(Error, EncounterRate::passingFraction(0, 0));
            const auto retail = rate.json();
            const auto original =
                rom.bytes().mid(EncounterRate::romOffset, EncounterRate::codeSize);
            QFile file(":/game/encounters/dispatcher.s");
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto source = file.readAll();
            QTemporaryDir build;
            Compiler compiler(tools);
            compiler.authenticate();
            QCOMPARE(compiler.assembleOwner(rate.assemblyDefinitions(false) + source, build.path(),
                                            "global_retail", EncounterRate::ramAddress,
                                            EncounterRate::serviceAddresses(), true),
                     original);
            QJsonArray variants{QJsonObject{{"name", "retail"},
                                            {"rate", retail},
                                            {"code", QString::fromLatin1(original.toHex())}}};
            const QVector<QJsonObject> cases = {
                retail,
                {{"divisor", 1}, {"normalPassCount", 0}, {"alternatePassCount", 1}},
                {{"divisor", 65536}, {"normalPassCount", 32768}, {"alternatePassCount", 65536}},
                {{"divisor", 4294967295.0},
                 {"normalPassCount", 2147483648.0},
                 {"alternatePassCount", 4294967295.0}},
                {{"divisor", 1000000}, {"normalPassCount", 0}, {"alternatePassCount", 1000000}}};
            QByteArray last;
            for (int i = 0; i < cases.size(); ++i) {
                rate.apply(cases[i]);
                const auto code = compiler.assembleOwner(
                    rate.assemblyDefinitions() + source, build.path(),
                    QString("global_native_%1").arg(i), EncounterRate::ramAddress,
                    EncounterRate::serviceAddresses(), true);
                QCOMPARE(code.size(), EncounterRate::codeSize);
                for (int at = 0; at < code.size(); ++at)
                    if (!(at >= 0x188 && at < 0x190) && !(at >= 0x198 && at < 0x1d0))
                        QCOMPARE(code[at], original[at]);
                auto image = rom.bytes();
                image.replace(EncounterRate::romOffset, code.size(), code);
                QCOMPARE(EncounterRate::load(Rom::fromBytes(image)).json(), cases[i]);
                auto bad = image;
                bad[EncounterRate::romOffset + 0x220] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, EncounterRate::load(Rom::fromBytes(bad)));
                variants << QJsonObject{{"name", QString("native%1").arg(i)},
                                        {"rate", cases[i]},
                                        {"code", QString::fromLatin1(code.toHex())}};
                last = code;
            }
            const auto before = rate.json();
            for (const auto &bad : QVector<QJsonObject>{
                     {{"divisor", 0}, {"normalPassCount", 0}, {"alternatePassCount", 0}},
                     {{"divisor", 100}, {"normalPassCount", 101}, {"alternatePassCount", 10}},
                     {{"divisor", 100}, {"normalPassCount", true}, {"alternatePassCount", 10}},
                     {{"divisor", 4294967296.0},
                      {"normalPassCount", 0},
                      {"alternatePassCount", 10}}}) {
                QVERIFY_THROWS_EXCEPTION(Error, rate.apply(bad));
                QCOMPARE(rate.json(), before);
            }
            auto project = Project::fromRom(rom);
            project.loadEncounters();
            auto encounters = *project.encounters();
            encounters.setGlobalRate(before);
            project.setEncounters(encounters.json());
            QTemporaryDir folder;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("global.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("global.lcproject")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.encounters()->globalRate().json(), before);
            }
            const auto result = rebuild(project, tools, build.path());
            auto expected = rom.bytes();
            expected.replace(EncounterRate::romOffset, last.size(), last);
            Rom::updateChecksum(expected);
            QCOMPARE(result.image, expected);
            QTemporaryDir output(QDir(QFileInfo(path).absolutePath())
                                     .filePath("Test ROMs/native-global-rate-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto target = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, target);
            auto reopened = Project::fromRom(Rom::open(target));
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->globalRate().json(), before);
            auto edited = *reopened.encounters();
            edited.setGlobalRate(retail);
            reopened.setEncounters(edited.json());
            const auto rebuilt = rebuild(reopened, tools, build.path());
            QCOMPARE(EncounterRate::load(Rom::fromBytes(rebuilt.image)).json(), retail);
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{{"neutral_global_rate",
                              QJsonObject{{"schema", 2},
                                          {"divisor", 1000000},
                                          {"linked", false},
                                          {"normal", QJsonObject{{"pass_count", 125}}},
                                          {"alternate", QJsonObject{{"percent", 0.5}}}}}}}};
            auto imported = Project::fromRom(rom);
            imported.importLegacy(legacy);
            QVERIFY(imported.pendingLegacySections().isEmpty());
            QCOMPARE(imported.encounters()->globalRate().normalPassCount(), quint32(125));
            QCOMPARE(imported.encounters()->globalRate().alternatePassCount(), quint32(5000));
            auto restored = *imported.encounters();
            restored.setGlobalRate(retail);
            imported.setEncounters(restored.json());
            auto loaded = Project::fromRom(rom);
            loaded.applyManifest(imported.manifest());
            QCOMPARE(loaded.encounters()->globalRate().json(), retail);
            auto historical = imported.manifest();
            auto oldEncounters = historical["encounters"].toObject();
            oldEncounters.remove("globalRate");
            historical["encounters"] = oldEncounters;
            loaded.applyManifest(historical);
            QCOMPARE(loaded.encounters()->globalRate().normalPassCount(), quint32(125));
            rate.importLegacy({{"divisor", 10000}, {"percent", 1.5}});
            QCOMPARE(rate.normalPassCount(), quint32(150));
            QCOMPARE(rate.alternatePassCount(), quint32(150));
            rate.apply(retail);
            rate.importLegacy({{"divisor", 144000}, {"normal", QJsonObject{{"pass_count", 125}}}});
            QCOMPARE(rate.normalPassCount(), quint32(125));
            QCOMPARE(rate.alternatePassCount(), quint32(22));
            atomicWrite(
                output.filePath("global-rate.json"),
                QJsonDocument(QJsonObject{{"rom", target}, {"variants", variants}}).toJson());
            qInfo().noquote() << "Global rate candidate:" << output.filePath("global-rate.json");
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void encounterTablesExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadEncounters();
            QCOMPARE(project.encounters()->classAt(41, 2, 0), 0x4c);
            QCOMPARE(project.encounters()->classAt(41, 2, 1), 0x50);
            QCOMPARE(project.encounters()->scenarioKeys(41), QVector<int>{59});
            QCOMPARE(project.encounters()->scenarioKeys(45), QVector<int>{66});
            const auto original = project.encounters()->json();
            auto model = *project.encounters();
            auto expected = source.bytes();
            for (int p = 1; p <= 45; ++p)
                for (int s = 0; s < 10; ++s) {
                    const int first = (p * 7 + s * 3) % 165, second = (p * 11 + s * 5) % 165;
                    model.setPair(p, s, first, second, 165);
                    put8(expected, 0x141ed4 + (p - 1) * 20 + s * 2, quint8(first));
                    put8(expected, 0x141ed4 + (p - 1) * 20 + s * 2 + 1, quint8(second));
                }
            for (int t = 0; t < 32; ++t) {
                const int slot = t % 11, rate = slot ? (t * 17) % 101 : 0;
                model.setTerrain(t, rate, slot);
                put8(expected, 0x141e80 + t, quint8(rate));
                put8(expected, 0x141ea0 + t, quint8(slot));
            }
            project.setEncounters(model.json());
            const auto before = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, model.setPair(46, 0, 1, 2, 165));
            QVERIFY_THROWS_EXCEPTION(Error, model.setPair(45, 10, 1, 2, 165));
            QVERIFY_THROWS_EXCEPTION(Error, model.setPair(45, 0, 165, 2, 165));
            QVERIFY_THROWS_EXCEPTION(Error, model.setTerrain(31, 101, 1));
            QVERIFY_THROWS_EXCEPTION(Error, model.setTerrain(31, 1, 0));
            QCOMPARE(model.json(), project.encounters()->json());
            auto bad = model.json();
            auto pools = bad["pools"].toArray();
            auto row = pools[44].toArray();
            row[19] = true;
            pools[44] = row;
            bad["pools"] = pools;
            QVERIFY_THROWS_EXCEPTION(Error, project.setEncounters(bad));
            QCOMPARE(project.manifest(), before);
            QTemporaryDir folder, build;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("encounters.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("encounters.lcproject")}) {
                auto loaded = Project::fromRom(source);
                loaded.applyManifest(Project::readProject(location));
                QCOMPARE(loaded.encounters()->json(), project.encounters()->json());
            }
            const auto result = rebuild(project, tools, build.path());
            Rom::updateChecksum(expected);
            QCOMPARE(result.image, expected);
            QTemporaryDir output(QDir(QFileInfo(path).absolutePath())
                                     .filePath("Test ROMs/native-encounters-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto target = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, target);
            auto reopened = Project::fromRom(Rom::open(target));
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->json(), project.encounters()->json());
            atomicWrite(
                output.filePath("encounters.json"),
                QJsonDocument(QJsonObject{{"rom", target}, {"encounters", model.json()}}).toJson());
            qInfo().noquote() << "Encounter tables candidate:"
                              << output.filePath("encounters.json");
            // Legacy subset imports preserve the five pools the browser never exposed.
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{
                     {"neutral_encounters",
                      QJsonObject{
                          {"scenario_slices",
                           QJsonObject{
                               {"1",
                                QJsonObject{{"row", 0},
                                            {"slots", QJsonObject{{"0", QJsonArray{164, 0}}}}}}}},
                          {"terrain_rates",
                           QJsonObject{{"31", QJsonObject{{"rate", 100}, {"rawLookup", 10}}}}}}}}}};
            auto imported = Project::fromRom(source);
            imported.importLegacy(legacy);
            QVERIFY(imported.pendingLegacySections().isEmpty());
            QCOMPARE(imported.encounters()->classAt(1, 0, 0), 164);
            QCOMPARE(imported.encounters()->poolBytes().mid(800),
                     source.bytes().mid(0x141ed4 + 800, 100));
            imported.setEncounters(original);
            auto restored = Project::fromRom(source);
            restored.applyManifest(imported.manifest());
            QCOMPARE(restored.encounters()->json(), original);
            auto unsupported = legacy;
            auto patches = legacy["patches"].toObject();
            auto neutral = patches["neutral_encounters"].toObject();
            neutral["custom_squads"] = QJsonObject{{"example", true}};
            patches["neutral_encounters"] = neutral;
            unsupported["patches"] = patches;
            const auto beforeInvalidImport = imported.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, imported.importLegacy(unsupported));
            QCOMPARE(imported.manifest(), beforeInvalidImport);
            auto tampered = source.bytes();
            put32(tampered, 0x13c414, 0);
            QVERIFY_THROWS_EXCEPTION(Error, Encounters::load(Rom::fromBytes(tampered), 165));
            QCOMPARE(reopened.appendClass(1, "Encounter tester"), 165);
            auto expanded = *reopened.encounters();
            expanded.setPair(45, 9, 165, 0, 166);
            reopened.setEncounters(expanded.json());
            const auto grown = rebuild(reopened, tools, build.path());
            auto grownProject = Project::fromRom(Rom::fromBytes(grown.image));
            grownProject.loadEncounters();
            QCOMPARE(grownProject.encounters()->classAt(45, 9, 0), 165);
            QCOMPARE(grownProject.records(Table::Classes).size(), 166);
            QTemporaryDir expandedOutput(QDir(QFileInfo(path).absolutePath())
                                             .filePath("Test ROMs/native-encounters-class-XXXXXX"));
            QVERIFY(expandedOutput.isValid());
            expandedOutput.setAutoRemove(false);
            const auto expandedTarget = expandedOutput.filePath(QFileInfo(path).fileName());
            exportRom(reopened, grown, expandedTarget);
            atomicWrite(
                expandedOutput.filePath("encounters.json"),
                QJsonDocument(QJsonObject{{"rom", expandedTarget}, {"encounters", expanded.json()}})
                    .toJson());
            qInfo().noquote() << "Expanded-class encounter candidate:"
                              << expandedOutput.filePath("encounters.json");
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void consumableEffectWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadTexts();
            project.loadConsumableEffects();
            const auto retail = project.consumableEffects()->json();
            const auto description = project.textBanks()["consumables"].entries[1].bytes;
            const QJsonObject imported{{"1", QJsonObject{{"magnitude", 111}}},
                                       {"2", QJsonObject{{"magnitude", 222}}},
                                       {"3", QJsonObject{{"magnitude", 333}}},
                                       {"4", QJsonObject{{"magnitude", 65535}}},
                                       {"5", QJsonObject{{"magnitude", 0}}},
                                       {"10", QJsonObject{{"deltaMin", 7}, {"deltaMax", 12}}},
                                       {"11-16", QJsonObject{{"deltaMin", 3}, {"deltaMax", 5}}},
                                       {"17", QJsonObject{{"deltaMin", 7}, {"deltaMax", 9}}},
                                       {"18", QJsonObject{{"deltaMin", -8}, {"deltaMax", -4}}},
                                       {"19", QJsonObject{{"deltaMin", -5}, {"deltaMax", 5}}}};
            const QJsonObject legacy{{"format", "ob64-patch"},
                                     {"version", 36},
                                     {"patches", QJsonObject{{"consumableEffects", imported}}}};
            project.importLegacy(legacy);
            QVERIFY(project.pendingLegacySections().isEmpty());
            auto expectedDescription = description;
            expectedDescription.replace("100HP", "111HP");
            QCOMPARE(project.textBanks()["consumables"].entries[1].bytes, expectedDescription);
            QVERIFY(project.textBanks()["consumables"].text(2).contains("222HP"));
            QVERIFY(project.textBanks()["consumables"].text(3).contains("333HP"));
            auto before = project.manifest();
            for (const auto &invalid :
                 {QJsonObject{{"11", QJsonObject{{"deltaMin", 2}, {"deltaMax", 4}}}},
                  QJsonObject{{"3", QJsonObject{{"magnitude", 1000}}}},
                  QJsonObject{{"19", QJsonObject{{"deltaMin", 5}, {"deltaMax", -5}}}},
                  QJsonObject{{"1", QJsonObject{{"magnitude", true}}}}}) {
                auto bad = legacy;
                bad["patches"] = QJsonObject{{"consumableEffects", invalid}};
                QVERIFY_THROWS_EXCEPTION(Error, project.importLegacy(bad));
                QCOMPARE(project.manifest(), before);
            }
            project.setText("consumables", 1, "My custom healing text.");
            before = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error,
                                     project.editConsumableEffect("healLeaf", {{"magnitude", 42}}));
            QCOMPARE(project.manifest(), before);
            project.editConsumableEffect("healLeaf", {{"magnitude", 42}}, false);
            QCOMPARE(project.textBanks()["consumables"].text(1), "My custom healing text.");
            project.setText("consumables", 1, "Custom herbs restore 42HP.");
            project.editConsumableEffect("healLeaf", {{"magnitude", 111}});
            QCOMPARE(project.textBanks()["consumables"].text(1), "Custom herbs restore 111HP.");
            // Keep the native baseline after restoration; retained browser data must not revive
            // it.
            auto restored = project;
            restored.setConsumableEffects(retail);
            auto loaded = Project::fromRom(source);
            loaded.applyManifest(restored.manifest());
            QCOMPARE(loaded.consumableEffects()->json(), retail);
            auto historical = Project::fromRom(source).manifest();
            historical["legacyImport"] = legacy;
            loaded.applyManifest(historical);
            QCOMPARE(loaded.consumableEffects()->json(), project.consumableEffects()->json());
            QTemporaryDir folder, build;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("effects.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("effects.lcproject")}) {
                auto reopened = Project::fromRom(source);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.consumableEffects()->json(), project.consumableEffects()->json());
                QCOMPARE(reopened.textBanks()["consumables"].serialized(),
                         project.textBanks()["consumables"].serialized());
            }
            project.setValue(Table::Consumables, 1, 0, 4321);
            project.loadShops();
            auto shops = project.shops()->json();
            auto rows = shops["records"].toArray();
            auto row = rows[2].toObject();
            row["consumables"] = QJsonArray{19, 1, 3};
            rows[2] = row;
            shops["records"] = rows;
            project.setShops(shops);
            QCOMPARE(project.appendClass(1, "Effect tester"), 165);
            const auto result = rebuild(project, tools, build.path());
            QTemporaryDir output(QDir(QFileInfo(path).absolutePath())
                                     .filePath("Test ROMs/native-consumable-workflow-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto target = output.filePath(QFileInfo(path).fileName());
            exportRom(project, result, target);
            auto reopened = Project::fromRom(Rom::open(target));
            reopened.loadConsumableEffects();
            reopened.loadTexts();
            reopened.loadShops();
            QCOMPARE(reopened.consumableEffects()->json(), project.consumableEffects()->json());
            QCOMPARE(reopened.textBanks()["consumables"].serialized(),
                     project.textBanks()["consumables"].serialized());
            QCOMPARE(reopened.shops()->json(), project.shops()->json());
            QCOMPARE(reopened.records(Table::Classes).size(), 166);
            QCOMPARE(reopened.value(Table::Consumables, 1, 0), 4321);
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            // A subsequent effect edit composes with already-relocated classes and shops.
            reopened.editConsumableEffect("healSeed", {{"magnitude", 42}});
            const auto subsequent = rebuild(reopened, tools, build.path());
            auto again = Project::fromRom(Rom::fromBytes(subsequent.image));
            again.loadConsumableEffects();
            again.loadTexts();
            QCOMPARE(
                again.consumableEffects()->models()["healSeed"].toObject()["magnitude"].toInt(),
                42);
            QVERIFY(again.textBanks()["consumables"].text(2).contains("042HP"));
            atomicWrite(
                output.filePath("effects.json"),
                QJsonDocument(
                    QJsonObject{{"rom", target}, {"models", project.consumableEffects()->models()}})
                    .toJson());
            qInfo().noquote() << "Consumable workflow candidate:"
                              << output.filePath("effects.json");
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void consumableEffectSource() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto effects = ConsumableEffects::load(rom);
            QCOMPARE(effects.models().size(), 10);
            QCOMPARE(effects.models()["healLeaf"].toObject()["magnitude"].toInt(), 100);
            QCOMPARE(effects.models()["urnOfChaos"].toObject()["minimum"].toInt(), -3);
            const auto original =
                rom.bytes().mid(ConsumableEffects::romOffset, ConsumableEffects::codeSize);
            QFile file(":/game/consumables/effects.s");
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto source = file.readAll();
            QTemporaryDir directory;
            Compiler compiler(tools);
            compiler.authenticate();
            QCOMPARE(compiler.assembleOwner(effects.assemblyDefinitions() + source,
                                            directory.path(), "consumable_retail",
                                            ConsumableEffects::ramAddress,
                                            ConsumableEffects::serviceAddresses(), true),
                     original);
            effects.setModel("healLeaf", {{"magnitude", 111}});
            effects.setModel("healSeed", {{"magnitude", 222}});
            effects.setModel("healPack", {{"magnitude", 333}});
            effects.setModel("powerFruit", {{"magnitude", 65535}});
            effects.setModel("angelFruit", {{"magnitude", 0}});
            effects.setModel("cupOfLife", {{"minimum", 7}, {"maximum", 12}});
            effects.setModel("sharedStatBoosters11To16", {{"minimum", 3}, {"maximum", 5}});
            effects.setModel("scrollOfDiscipline", {{"minimum", 7}, {"maximum", 9}});
            effects.setModel("urnOfChaos", {{"minimum", -8}, {"maximum", -4}});
            effects.setModel("gobletOfDestiny", {{"minimum", -5}, {"maximum", 5}});
            const auto compiled = compiler.assembleOwner(
                effects.assemblyDefinitions() + source, directory.path(), "consumable_edited",
                ConsumableEffects::ramAddress, ConsumableEffects::serviceAddresses(), true);
            auto expected = original;
            const QList<QPair<int, int>> values = {
                {0x410f8, 111},   {0x410fc, 222}, {0x41108, 333}, {0x4110c, 333},
                {0x41248, 65535}, {0x4124c, 0},   {0x4134c, 6},   {0x41388, 7},
                {0x41444, 3},     {0x41480, 3},   {0x4149c, 3},   {0x414d8, 7},
                {0x41518, 5},     {0x41554, -8},  {0x41570, 11},  {0x415ac, -5}};
            for (auto [at, value] : values)
                put16(expected, at - int(ConsumableEffects::romOffset) + 2, quint16(value));
            QCOMPARE(compiled, expected);
            QCOMPARE(effects.expectedCode(original), expected);
            auto changed = rom.bytes();
            changed.replace(ConsumableEffects::romOffset, compiled.size(), compiled);
            QCOMPARE(ConsumableEffects::load(Rom::fromBytes(changed)).json(), effects.json());
            const auto before = effects.json();
            QVERIFY_THROWS_EXCEPTION(
                Error, effects.setModel("urnOfChaos", {{"minimum", -1}, {"maximum", 1}}));
            QVERIFY_THROWS_EXCEPTION(Error, effects.setModel("healPack", {{"magnitude", 1000}}));
            QVERIFY_THROWS_EXCEPTION(
                Error, effects.setModel("cupOfLife", {{"minimum", 10}, {"maximum", 9}}));
            QCOMPARE(effects.json(), before);
            for (auto site : {0x41098, 0x41108, 0x65d60}) {
                auto bad = changed;
                bad[site + 3] = char(u8(bad, site + 3) ^ 1);
                QVERIFY_THROWS_EXCEPTION(Error, ConsumableEffects::load(Rom::fromBytes(bad)));
            }
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir output(root + "/native-consumable-effects-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            Rom::updateChecksum(changed);
            const auto target = output.filePath(QFileInfo(path).fileName());
            atomicWrite(target, rom.serialize(changed));
            atomicWrite(
                output.filePath("effects.json"),
                QJsonDocument(QJsonObject{{"rom", target}, {"models", effects.models()}}).toJson());
            qInfo().noquote() << "Consumable effect source candidate:"
                              << output.filePath("effects.json");
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void consumableRecordsExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        const auto source = Rom::open(path);
        auto project = Project::fromRom(source);
        QCOMPARE(project.records(Table::Consumables).size(), 45);
        QVERIFY(project.record(Table::Consumables, 1).name.contains("Heal"));
        const auto original = project.record(Table::Consumables, 44).bytes;
        auto expected = original;
        put16(expected, 4, 0x0201);
        put16(expected, 6, 65535);
        put8(expected, 8, 2);
        put8(expected, 9, 7);
        put8(expected, 10, 0x42);
        put8(expected, 11, 0xa5);
        const QJsonObject legacy{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches",
             QJsonObject{
                 {"consumables",
                  QJsonObject{{"44", QJsonObject{{"flagHi", 0x0201},
                                                 {"price", 65535},
                                                 {"flagLo", QJsonArray{2, 7, 0x42, 0xa5}}}}}}}}};
        project.importLegacy(legacy);
        QCOMPARE(project.record(Table::Consumables, 44).bytes, expected);
        QVERIFY(project.pendingLegacySections().isEmpty());
        const auto before = project.manifest();
        for (auto bad : {QJsonValue(-1), QJsonValue(65536), QJsonValue(2.5), QJsonValue(true)}) {
            auto invalid = legacy;
            invalid["patches"] =
                QJsonObject{{"consumables", QJsonObject{{"44", QJsonObject{{"price", bad}}}}}};
            QVERIFY_THROWS_EXCEPTION(Error, project.importLegacy(invalid));
            QCOMPARE(project.manifest(), before);
        }
        auto badPointer = expected;
        badPointer[0] = 0;
        QVERIFY_THROWS_EXCEPTION(Error, project.replaceBytes(Table::Consumables, 44, badPointer));
        QTemporaryDir folder, build;
        project.saveFolder(folder.path());
        project.saveArchive(folder.filePath("consumables.lcproject"));
        for (const auto &location : {folder.path(), folder.filePath("consumables.lcproject")}) {
            auto reopened = Project::fromRom(source);
            reopened.applyManifest(Project::readProject(location));
            QCOMPARE(reopened.record(Table::Consumables, 44).bytes, expected);
        }
        const auto result = rebuild(project, tools, build.path());
        auto expectedImage = source.bytes();
        expectedImage.replace(0x645cc + 44 * 12, 12, expected);
        Rom::updateChecksum(expectedImage);
        QCOMPARE(result.image,
                 expectedImage); // every neighboring byte, not just exposed fields
        QTemporaryDir output(
            QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-consumables-XXXXXX"));
        QVERIFY(output.isValid());
        output.setAutoRemove(false);
        const auto target = output.filePath(QFileInfo(path).fileName());
        exportRom(project, result, target);
        auto reopened = Project::fromRom(Rom::open(target));
        QCOMPARE(reopened.record(Table::Consumables, 44).bytes, expected);
        qInfo().noquote() << "Consumable record export:" << target;
        // Native projects retained before this table was supported migrate once.
        auto historical = Project::fromRom(source).manifest();
        auto records = historical["records"].toObject();
        records.remove("consumables");
        historical["records"] = records;
        historical["legacyImport"] = legacy;
        reopened = Project::fromRom(source);
        reopened.applyManifest(historical);
        QCOMPARE(reopened.record(Table::Consumables, 44).bytes, expected);
        reopened.replaceBytes(Table::Consumables, 44, original);
        auto restored = Project::fromRom(source);
        restored.applyManifest(reopened.manifest());
        QCOMPARE(restored.record(Table::Consumables, 44).bytes, original);
        QCOMPARE(Rom::open(path).bytes(), source.bytes());
    }
    void shopLegacyImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadShops();
        const auto original = project.shops()->json();
        QJsonObject legacy{
            {"format", "ob64-patch"},
            {"version", 9},
            {"patches",
             QJsonObject{
                 {"shops", QJsonObject{{"2", QJsonObject{{"items", QJsonArray{277, 1}}}}}}}}};
        project.importLegacy(legacy);
        QCOMPARE(project.shops()->stock(2).equipment, QVector<quint16>({277, 1}));
        QCOMPARE(project.shops()->stock(2).consumables, QVector<quint16>({1, 2, 3, 4, 5, 6, 8}));
        QVERIFY(project.pendingLegacySections().isEmpty());
        legacy["version"] = 36;
        legacy["patches"] = QJsonObject{
            {"shops", QJsonObject{{"2", QJsonObject{{"consumables", QJsonArray{44, 1}}}}}}};
        legacy["rebuild_owners"] = QJsonObject{{"schema", "lordly-rebuild-owner-selection-v1"},
                                               {"shop_memberships", true}};
        project.importLegacy(legacy);
        QCOMPARE(project.shops()->stock(2).equipment, QVector<quint16>({277, 1}));
        QCOMPARE(project.shops()->stock(2).consumables, QVector<quint16>({44, 1}));
        QVERIFY(project.pendingLegacySections().isEmpty());
        const auto before = project.manifest();
        for (auto values : {QJsonArray{1, 1}, QJsonArray{45}, QJsonArray{1.5}}) {
            auto malformed = legacy;
            malformed["patches"] =
                QJsonObject{{"shops", QJsonObject{{"2", QJsonObject{{"consumables", values}}}}}};
            QVERIFY_THROWS_EXCEPTION(Error, project.importLegacy(malformed));
            QCOMPARE(project.manifest(), before);
        }
        // A project retained before Shops support now imports its archived edit.
        auto historical = Project::fromRom(project.rom()).manifest();
        historical["legacyImport"] = legacy;
        auto migrated = Project::fromRom(project.rom());
        migrated.applyManifest(historical);
        QCOMPARE(migrated.shops()->stock(2).consumables, QVector<quint16>({44, 1}));
        // Restoring stock must not resurrect the archived import on reopening.
        migrated.setShops(original);
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(migrated.manifest());
        QCOMPARE(reopened.shops()->json(), original);
        QVERIFY(reopened.pendingLegacySections().isEmpty());
    }
    void addedItemExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadIconArtwork();
            project.loadAnimationRoutes();
            project.loadTexts();
            const auto icon = project.iconArtwork()->words(1),
                       description = project.textBanks()["items"].entries[1].bytes;
            QCOMPARE(project.appendItem(1, "New blade"), 278);
            QCOMPARE(project.iconArtwork()->words(278), icon);
            QCOMPARE(project.textBanks()["items"].entries[278].bytes, description);
            project.setText("items", 278, "Forged for the new item test.");
            auto item = project.record(Table::Items, 278).bytes;
            put16(item, 8, 1234);
            project.replaceBytes(Table::Items, 278, item);
            project.loadShops();
            auto shops = *project.shops();
            shops.setStock(1, {{278, 277, 1}, {44}});
            project.setShops(shops.json());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            project.saveFolder(work.filePath("project"));
            project.saveArchive(work.filePath("project.lcproj"));
            for (const auto location :
                 {work.filePath("project"), work.filePath("project.lcproj")}) {
                auto restored = Project::fromRom(source);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.manifest(), project.manifest());
                QCOMPARE(restored.iconArtwork()->words(278), icon);
            }
            for (int stage = 0; stage < 2; ++stage) {
                if (stage) {
                    QCOMPARE(project.appendItem(278, "Second blade"), 279);
                    QCOMPARE(project.createItem("Blank accessory"), 280);
                    QCOMPARE(project.appendClass(5, "Item wielder"), 165);
                    auto row = project.record(Table::Classes, 165).bytes;
                    put16(row, 46, 280);
                    project.replaceBytes(Table::Classes, 165, row);
                    auto encounters = *project.encounters();
                    CreatureReward reward; reward.outcomes = {0x8000 | 280, 0, 0};
                    encounters.setReward(165, reward);
                    CustomSquad squad; squad.members = {{165,0,8,{280,0}}, {5,0,6,{278,0}}};
                    squad.rewards = reward; encounters.setCustomSquad(squad, 166);
                    project.setEncounters(encounters.json());
                }
                const auto result = rebuild(project, tools, work.path());
                auto reopened = Project::fromRom(Rom::fromBytes(result.image));
                reopened.loadTexts();
                reopened.loadIconArtwork();
                reopened.loadAnimationRoutes();
                reopened.loadShops(); reopened.loadEncounters();
                if (stage) QCOMPARE(reopened.encounters()->json(), project.encounters()->json());
                QCOMPARE(reopened.records(Table::Items).size(), stage ? 281 : 279);
                for (int id = 0; id < project.records(Table::Items).size(); ++id) {
                    QCOMPARE(reopened.record(Table::Items, id).bytes.mid(4),
                             project.record(Table::Items, id).bytes.mid(4));
                    QCOMPARE(reopened.itemNameBytes(id), project.itemNameBytes(id));
                }
                QCOMPARE(reopened.textBanks()["items"].serialized(),
                         project.textBanks()["items"].serialized());
                QCOMPARE(reopened.iconArtwork()->words(278), icon);
                QCOMPARE(reopened.animationRoutes()->section(2),
                         project.animationRoutes()->section(2));
                QCOMPARE(reopened.shops()->json(), project.shops()->json());
                const auto placed = ClassPlacement::read(reopened.rom());
                QVERIFY(placed);
                QCOMPARE(placed->layout.itemCount, stage ? 281 : 279);
                QVERIFY(placed->layout.end <= 0x80400000U);
                QTemporaryDir output(
                    QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-items-XXXXXX"));
                QVERIFY(output.isValid());
                output.setAutoRemove(false);
                const auto target = output.filePath(QFileInfo(path).fileName());
                exportRom(project, result, target);
                QJsonArray records, names;
                for (const auto &r : project.records(Table::Items)) {
                    records << QString::fromLatin1(r.bytes.toHex());
                    names << QString::fromLatin1(project.itemNameBytes(r.id).toHex());
                }
                atomicWrite(output.filePath("items.json"),
                            QJsonDocument(QJsonObject{{"rom", target},
                                                      {"records", records},
                                                      {"names", names},
                                                      {"shops", project.shops()->json()}})
                                .toJson());
                qInfo().noquote() << "New item candidate:" << output.path();
                project = std::move(reopened);
            }
            auto missing = Project::fromRom(source);
            missing.loadShops();
            auto badStock = *missing.shops();
            badStock.setStock(1, {{278}, {}});
            missing.setShops(badStock.json());
            QVERIFY_THROWS_EXCEPTION(Error, missing.validateItemReferences());
            auto data = project.itemData();
            while (data.entries().size() < ItemData::maximumCount)
                data.append(1, "Boundary");
            QCOMPARE(data.entries().size(), 511);
            QVERIFY_THROWS_EXCEPTION(Error, data.append(1, "Overflow"));
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void shopRuntimeExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler are required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadShops();
            auto shops = *project.shops();
            for (int id = 1; id < Shops::count; ++id) {
                ShopStock stock;
                for (int n = 0; n < 50; ++n)
                    stock.equipment << quint16(228 + (n + id) % 50);
                for (int n = 0; n < 30; ++n)
                    stock.consumables << quint16(15 + (n + id) % 30);
                shops.setStock(id, stock);
            }
            project.setShops(shops.json());
            QTemporaryDir build;
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            for (int stage = 0; stage < 3; ++stage) {
                if (stage == 1) {
                    project.appendClass(5, "Shop companion");
                    auto next = *project.shops();
                    QCOMPARE(next.addShop(), 36);
                    next.setStock(36, {{277, 1}, {44}});
                    project.setShops(next.json());
                    project.loadWorld();
                    auto world = *project.world();
                    auto town = world.town(1);
                    put8(town, 27, 36);
                    put8(town, 25, u8(town, 25) | 1);
                    world.setTown(1, town);
                    project.setWorld(world.json());
                }
                if (stage == 2)
                    project.setClassNameBytes(165, encodeClassName("Shop companion renamed"));
                const auto expected = project.shops()->json();
                const auto result = rebuild(project, tools, build.path());
                const auto placed = ShopPlacement::read(Rom::fromBytes(result.image));
                QVERIFY(placed);
                QCOMPARE(placed->version, stage ? 3 : 2);
                QCOMPARE(placed->count, stage ? 37 : 36);
                QCOMPARE(placed->offset % 16, 0U);
                auto checksum = result.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, result.image);
                QTemporaryDir output(root + "/native-shops-XXXXXX");
                QVERIFY(output.isValid());
                output.setAutoRemove(false);
                const auto target = output.filePath(QFileInfo(path).fileName());
                exportRom(project, result, target);
                auto reopened = Project::fromRom(Rom::open(target));
                reopened.loadShops();
                QCOMPARE(reopened.shops()->json(), expected);
                QVERIFY(!reopened.shops()->modified());
                QCOMPARE(reopened.records(Table::Classes).size(), stage ? 166 : 165);
                atomicWrite(
                    output.filePath("shops.json"),
                    QJsonDocument(QJsonObject{{"rom", target}, {"shops", expected}}).toJson());
                qInfo().noquote() << "Shop export:" << output.filePath("shops.json");
                for (auto at :
                     {placed->offset, ShopPlacement::descriptorOffset + 12,
                      ShopPlacement::producerOffset + 60, quint32(0x198f60), quint32(0x199560)}) {
                    auto damaged = result.image;
                    damaged[at] = char(u8(damaged, at) ^ 1);
                    QVERIFY_THROWS_EXCEPTION(Error, Shops::load(Rom::fromBytes(damaged)));
                }
                if (stage == 0) {
                    // A second shop edit reuses its owned allocation alongside new classes.
                    shops = *reopened.shops();
                    auto stock = shops.stock(35);
                    std::reverse(stock.consumables.begin(), stock.consumables.end());
                    shops.setStock(35, stock);
                    shops.setStock(33, {});
                    stock = shops.stock(34);
                    stock.consumables.clear();
                    shops.setStock(34, stock);
                    reopened.setShops(shops.json());
                }
                project = std::move(reopened);
            }
            auto foreign = source.bytes();
            put32(foreign, ShopPlacement::producerOffset, 0x08000000);
            QVERIFY_THROWS_EXCEPTION(Error, Shops::load(Rom::fromBytes(foreign)));
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void shopMembershipProjects() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadShops();
            auto shops = *project.shops();
            QVERIFY(shops.stock(0).equipment.isEmpty());
            QVERIFY(shops.stock(0).consumables.isEmpty());
            QCOMPARE(shops.stock(2).equipment.size(), 7);
            QCOMPARE(shops.stock(2).consumables, QVector<quint16>({1, 2, 3, 4, 5, 6, 8}));
            QVERIFY(shops.locations(2).contains("Jadd"));
            QVERIFY(!shops.locations(2).contains("Tacikent"));
            QCOMPARE(shops.json()["records"].toArray().size(), 36);
            const auto original = shops.json();
            ShopStock full;
            for (int id = 50; id > 0; --id)
                full.equipment << quint16(id);
            for (int id = 30; id > 0; --id)
                full.consumables << quint16(id);
            shops.setStock(2, full);
            QCOMPARE(shops.changedShops(), 1);
            const auto edited = shops.json();
            auto invalid = full;
            invalid.equipment << 51;
            QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(2, invalid));
            invalid = full;
            invalid.consumables << 31;
            QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(2, invalid));
            for (const auto id : {0, 278, 0x8001}) {
                invalid = full;
                invalid.equipment[0] = quint16(id);
                QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(2, invalid));
            }
            invalid = full;
            invalid.equipment[0] = invalid.equipment[1];
            QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(2, invalid));
            invalid = full;
            invalid.consumables[0] = 45;
            QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(2, invalid));
            QVERIFY_THROWS_EXCEPTION(Error, shops.setStock(0, full));
            QCOMPARE(shops.json(), edited);
            project.setShops(edited);
            QCOMPARE(project.changedRecords(), 1);
            const auto manifest = project.manifest();
            QTemporaryDir folder;
            project.saveFolder(folder.path());
            project.saveArchive(folder.filePath("shops.lcproject"));
            for (const auto &location : {folder.path(), folder.filePath("shops.lcproject")}) {
                auto reopened = Project::fromRom(project.rom());
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.shops()->json(), edited);
                QCOMPARE(reopened.shops()->stock(2).equipment, full.equipment);
                QCOMPARE(reopened.shops()->stock(2).consumables, full.consumables);
                for (int id = 1; id < 36; ++id)
                    if (id != 2)
                        QCOMPARE(reopened.shops()->json()["records"].toArray()[id],
                                 original["records"].toArray()[id]);
            }
            auto malformed = edited;
            auto records = malformed["records"].toArray();
            auto row = records[2].toObject();
            row["equipment"] = QJsonArray{1.5};
            records[2] = row;
            malformed["records"] = records;
            auto badManifest = manifest;
            badManifest["shops"] = malformed;
            QVERIFY_THROWS_EXCEPTION(Error, project.applyManifest(badManifest));
            QCOMPARE(project.manifest(), manifest);
            project.setShops(original);
            QVERIFY(!project.manifest().contains("shops"));
            QCOMPARE(project.changedRecords(), 0);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void esetFraming() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            QJsonArray fixtures;
            int found = 0;
            qsizetype at = 0;
            while ((at = rom.bytes().indexOf("-lh", at)) >= 0) {
                const auto start = at - 2;
                at += 3;
                std::optional<LhaMember> member;
                try {
                    member = LhaMember::read(rom.bytes(), start);
                } catch (const Error &) {
                    continue;
                }
                if (!member->name.startsWith("eset") || !member->name.endsWith(".bin"))
                    continue;
                const auto raw = member->decode(rom);
                auto eset = Eset::decode(raw);
                QCOMPARE(eset.encode(), raw);
                QCOMPARE(eset.baseLevel(), u8(raw, 8));
                QVERIFY(eset.deployments.size() <= 20);
                QVERIFY(eset.nodes.size() <= 16);
                QVERIFY(eset.conditions.size() <= 16);
                auto expected = raw;
                eset.setBaseLevel(99);
                expected[8] = 99;
                QCOMPARE(eset.encode(), expected);
                QVERIFY_THROWS_EXCEPTION(Error, eset.setBaseLevel(0));
                QVERIFY_THROWS_EXCEPTION(Error, eset.setBaseLevel(100));
                QCOMPARE(eset.encode(), expected);
                const auto tail = QByteArray::fromHex("a500ff001122");
                auto extra = Eset::decode(raw + tail);
                QCOMPARE(extra.encode(), raw + tail);
                extra.setBaseLevel(99);
                QCOMPARE(extra.encode(), expected + tail);
                auto broken = raw;
                put16(broken, 4, u16(broken, 4) + 1);
                QVERIFY_THROWS_EXCEPTION(Error, Eset::decode(broken));
                QVERIFY_THROWS_EXCEPTION(Error, Eset::decode(raw.first(16)));
                QVERIFY_THROWS_EXCEPTION(
                    Error, Eset::decode(raw.first(raw.size() - eset.tail.size() - 1)));
                // Counts change; the final deployment tail must become the new
                // node count/first ID, not the former row's route bytes.
                if (!eset.deployments.isEmpty()) {
                    auto row = eset.deployments.last();
                    row[0] = 49;
                    eset.deployments.last()[16] = 0;
                    eset.deployments.last()[17] = 0;
                    eset.deployments << row;
                    auto node = eset.nodes.last();
                    node[0] = char(4 + eset.nodes.size());
                    eset.nodes << node;
                    const auto grown = Eset::decode(eset.encode());
                    QCOMPARE(grown.deployments.size(), eset.deployments.size());
                    QCOMPARE(grown.nodes.size(), eset.nodes.size());
                    QCOMPARE(u8(grown.deployments.last(), 16), quint8(eset.nodes.size()));
                    QCOMPARE(u8(grown.deployments.last(), 17), u8(eset.nodes.first(), 0));
                    QCOMPARE(grown.conditions, eset.conditions);
                    QCOMPARE(grown.deployments[grown.deployments.size() - 2].last(2),
                             QByteArray(2, 0));
                }
                fixtures << QJsonObject{{"name", member->name},
                                        {"raw", QString::fromLatin1(raw.toHex())},
                                        {"edited", QString::fromLatin1(eset.encode().toHex())}};
                ++found;
            }
            QCOMPARE(found, 63);
            atomicWrite(QCoreApplication::applicationDirPath() + "/eset-framing.json",
                        QJsonDocument(fixtures).toJson(QJsonDocument::Compact));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void lhaEncoding() {
        try {
            QVector<QByteArray> inputs{{}, "A", "AB", "AAA", QByteArray(70000, char(-1))};
            QByteArray every;
            for (int i = 0; i < 256; ++i)
                every += char(i);
            inputs << every << every.repeated(400);
            quint32 random = 12345;
            for (const int size : {8192, 8193, 65536, 100000}) {
                QByteArray bytes(size, 0);
                for (auto &value : bytes) {
                    random = random * 1664525U + 1013904223U;
                    value = char(random >> 24);
                }
                inputs << bytes;
                if (size == 8192)
                    inputs << bytes + bytes.first(256);
            }
            QJsonArray examples;
            for (const auto &data : inputs) {
                const auto packed = encodeLh5(data);
                QCOMPARE(decodeLh5(packed, data.size()), data);
                QCOMPARE(encodeLh5(data), packed);
                if (!data.isEmpty())
                    QVERIFY_THROWS_EXCEPTION(
                        Error, decodeLh5(packed.first(packed.size() / 2), data.size()));
                examples << QJsonObject{{"data", QString::fromLatin1(data.toHex())},
                                        {"packed", QString::fromLatin1(packed.toHex())}};
            }
            QVERIFY(encodeLh5(every.repeated(400)).size() < 10000);
            QVERIFY(encodeLh5(QByteArray(70000, char(-1))).size() < 2000);
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (!path.isEmpty()) {
                const auto rom = Rom::open(path);
                for (const auto &name : {"enemydat.bin", "eset0_01.bin", "eset4_06a.bin"}) {
                    const auto member = LhaMember::find(rom, name);
                    const auto original = member.decode(rom);
                    auto data = original;
                    data[8] ^= 1;
                    data += every;
                    const auto rebuilt = member.rebuild(rom, data);
                    auto image = rom.bytes() + rebuilt;
                    image.append(QByteArray((4 - image.size() % 4) % 4, char(-1)));
                    const auto resultRom = Rom::fromBytes(image);
                    const auto result = LhaMember::read(resultRom.bytes(), rom.bytes().size());
                    QCOMPARE(result.name, member.name);
                    QCOMPARE(result.decode(resultRom), data);
                    QCOMPARE(result.headerSize, member.headerSize);
                    auto preserved = rebuilt.first(member.headerSize);
                    const auto header = rom.bytes().mid(member.offset, member.headerSize);
                    preserved.replace(7, 8, header.mid(7, 8));
                    preserved.replace(21, 2, header.mid(21, 2));
                    for (qsizetype at = 24; at + 2 < header.size();) {
                        const int size = u8(header, at) | (u8(header, at + 1) << 8);
                        if (!size)
                            break;
                        if (u8(header, at + 2) == 0)
                            preserved.replace(at + 3, 2, header.mid(at + 3, 2));
                        at += size;
                    }
                    QCOMPARE(preserved, header);
                    examples << QJsonObject{
                        {"name", name},
                        {"data", QString::fromLatin1(data.toHex())},
                        {"packed", QString::fromLatin1(rebuilt.mid(member.headerSize).toHex())},
                        {"header", QString::fromLatin1(rebuilt.first(member.headerSize).toHex())}};
                    QCOMPARE(member.decode(rom), original);
                }
            }
            atomicWrite(QCoreApplication::applicationDirPath() + "/lha-encoding.json",
                        QJsonDocument(examples).toJson(QJsonDocument::Compact));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void lhaArchives() {
        QCOMPARE(lhaCrc16("123456789"), quint16(0xbb3d));
        // One literal A, represented with the format's single-symbol trees.
        QString bits;
        auto add = [&](quint32 value, int width) {
            for (int i = width - 1; i >= 0; --i)
                bits += (value & (1U << i)) ? '1' : '0';
        };
        add(1, 16);
        add(0, 5);
        add(0, 5);
        add(0, 9);
        add(65, 9);
        add(0, 4);
        add(0, 4);
        QByteArray stream((bits.size() + 7) / 8, 0);
        for (qsizetype i = 0; i < bits.size(); ++i)
            if (bits[i] == '1')
                stream[i / 8] = char(quint8(stream[i / 8]) | (1 << (7 - i % 8)));
        QCOMPARE(decodeLh5(stream, 1), QByteArray("A"));
        for (qsizetype count = 0; count < stream.size(); ++count)
            QVERIFY_THROWS_EXCEPTION(Error, decodeLh5(stream.first(count), 1));
        QVERIFY_THROWS_EXCEPTION(Error, decodeLh5(QByteArray(32, 0), 1));
        auto invalidTree = stream;
        put8(invalidTree, 2, 0xf8); // Pre-tree length31 exceeds alphabet19.
        QVERIFY_THROWS_EXCEPTION(Error, decodeLh5(invalidTree, 1));
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied for archive CRC checks");
        try {
            const auto rom = Rom::open(path);
            const auto shops = LhaMember::find(rom, "shopcsv.bin");
            const auto towns = LhaMember::find(rom, "ktenmain.bin");
            QCOMPARE(shops.decodedSize, qsizetype(1098));
            QCOMPARE(towns.decodedSize, qsizetype(8848));
            const auto shopData = shops.decode(rom), townData = towns.decode(rom);
            QCOMPARE(lhaCrc16(shopData), quint16(0xa531));
            QCOMPARE(lhaCrc16(townData), quint16(0x1889));
            QCOMPARE(u16(shopData, 0), quint16(512));
            QCOMPARE(u16(shopData, 2), quint16(512)); // Empty slot0, not a duplicate of shop1.
            QCOMPARE(u16(shopData, 4), quint16(528));
            auto damaged = rom.bytes();
            damaged[shops.offset + 21] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, shops.decode(Rom::fromBytes(damaged)));
            damaged = rom.bytes();
            damaged[shops.offset + shops.headerSize + 40] ^= 0x40;
            QVERIFY_THROWS_EXCEPTION(Error, shops.decode(Rom::fromBytes(damaged)));
            damaged = rom.bytes().first(shops.offset + shops.headerSize + shops.storedSize - 1);
            QVERIFY_THROWS_EXCEPTION(Error, LhaMember::read(damaged, shops.offset));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void expandedMenuBuild() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler are needed for expanded menu installation checks");
        try {
            const auto rom = Rom::open(path);
            auto priorRom = rom;
            auto classes = ClassData::loadRetail(rom);
            auto promotion = Promotion::load(rom);
            Compiler compiler(tools);
            compiler.authenticate();
            QTemporaryDir work;
            QVERIFY(work.isValid());
            for (int count : {166, 256}) {
                while (classes.entries().size() < count) {
                    classes.append(5, "Silver Knight");
                    promotion.appendClass(5);
                }
                const auto runtime = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
                if (count == 256)
                    QVERIFY_THROWS_EXCEPTION(Error, linkClassRuntime(priorRom, runtime));
                const auto menus = buildPromotionRuntime(priorRom, runtime.data.layout, promotion,
                                                         compiler, work.path());
                QCOMPARE(menus.owners.size(), expandedPromotionOwners().size());
                const auto reopened = ExpandedPromotionResource::decode(menus.container);
                QCOMPARE(reopened.data.bytes, promotion.buildExpandedData().bytes);
                QCOMPARE(reopened.inventory.encode(), menus.inventory.encode());
                auto image = linkClassRuntime(priorRom, runtime, &menus);
                const auto candidateRom = Rom::fromBytes(image);
                const auto placed = ClassPlacement::read(candidateRom);
                QVERIFY(placed->saves.installed());
                placed->saves.verify(image);
                QCOMPARE(image.mid(SavePlacement::romAddress, placed->saves.codeBytes),
                         runtime.saves->code.bytes);
                for (const auto site : {SavePlacement::romAddress, 0x4c5cU, 0x38670U, 0x74cb4U,
                                        0x75028U, 0x564d0U, 0x15e578U}) {
                    auto changedSave = image;
                    changedSave[site] ^= 1;
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             ClassPlacement::read(Rom::fromBytes(changedSave)));
                }
                menus.inventory.verify(candidateRom,
                                       ClassRuntimeInventory::dataBases(runtime.data.layout));
                QCOMPARE(ClassData::load(candidateRom).entries().size(), count);
                QCOMPARE(Promotion::load(candidateRom).json(), promotion.json());
                const auto resource =
                    Resource::load(candidateRom, ExpandedPromotionResource::romOffset,
                                   ExpandedPromotionResource::slotSize);
                QCOMPARE(resource.decoded, promotion.buildExpandedData().bytes);
                // The trailing entry of the next routine is outside class-screen C.
                QCOMPARE(image.mid(0xd0380, 8), rom.bytes().mid(0xd0380, 8));
                auto damaged = image;
                damaged[0xae3f4] ^= 1;
                QVERIFY_THROWS_EXCEPTION(
                    Error,
                    menus.inventory.verify(Rom::fromBytes(damaged),
                                           ClassRuntimeInventory::dataBases(runtime.data.layout)));
                QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(damaged)));
                damaged = image;
                auto incomplete = menus.inventory;
                incomplete.owners.removeLast();
                damaged.replace(ExpandedPromotionResource::romOffset, menus.container.size(),
                                ExpandedPromotionResource::encode(
                                    promotion, promotion.buildExpandedData().bytes, incomplete));
                QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(damaged)));
                damaged = image;
                auto emptyClassChanged = promotion;
                emptyClassChanged.masks[0] ^= 1;
                damaged.replace(ExpandedPromotionResource::romOffset, menus.container.size(),
                                ExpandedPromotionResource::encode(
                                    emptyClassChanged, emptyClassChanged.buildExpandedData().bytes,
                                    menus.inventory));
                QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(damaged)));
                const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
                QVERIFY(QDir().mkpath(candidates));
                QTemporaryDir candidate(candidates + "/native-expanded-menu-XXXXXX");
                QVERIFY(candidate.isValid());
                const auto output = candidate.filePath(QFileInfo(path).fileName());
                atomicWrite(output, rom.serialize(image));
                atomicWrite(
                    candidate.filePath("menu-layout.json"),
                    QJsonDocument(QJsonObject{{"rom", output},
                                              {"count", count},
                                              {"ownerBytes", int(menus.layout.ownerBytes)},
                                              {"list", int(menus.layout.list)},
                                              {"ids", int(menus.layout.categoryClasses)},
                                              {"current", int(menus.layout.currentCounts)},
                                              {"eligible", int(menus.layout.eligibleCounts)},
                                              {"records", double(runtime.data.layout.records)},
                                              {"aliases", double(runtime.data.layout.aliases)},
                                              {"discovery", double(runtime.data.layout.discovery)}})
                        .toJson());
                candidate.setAutoRemove(false);
                qInfo().noquote() << "Expanded-class candidate (remaining consumers and full-game "
                                     "checks pending):"
                                  << candidate.filePath("menu-layout.json");
                priorRom = candidateRom;
                if (count == 166) {
                    // Previous save installation metadata omitted the Pak owners.
                    // Reopening and upgrading it must authenticate both sets.
                    auto older = image;
                    auto previousOwners = saveRuntimeCatalog()["pakOwners"].toArray();
                    for (const auto value : saveRuntimeCatalog()["lifecycleOwners"].toArray())
                        previousOwners.append(value);
                    for (const auto value : previousOwners) {
                        const auto row = value.toObject();
                        older.replace(row["start"].toInt(), row["size"].toInt(),
                                      rom.bytes().mid(row["start"].toInt(), row["size"].toInt()));
                    }
                    auto legacyPlacement = *placed;
                    legacyPlacement.saves.version = 1;
                    legacyPlacement.saves.ownerChecksum = saveRuntimeOwnerChecksum(older, 1);
                    older.replace(
                        ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize,
                        legacyPlacement.descriptor(runtime.compiledData, runtime.code.bytes));
                    priorRom = Rom::fromBytes(older);
                    QCOMPARE(ClassPlacement::read(priorRom)->saves.version, 1U);
                }
            }
            // Longer names move the discovery bitmap without changing the count.
            auto named = classes.entries().last();
            named.name = QByteArray(200, 'A');
            classes.set(255, named);
            const auto next = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
            const auto before = ClassPlacement::read(priorRom);
            QVERIFY(next.data.layout.discovery != before->layout.discovery);
            const auto renamed = Rom::fromBytes(linkClassRuntime(priorRom, next));
            QCOMPARE(ClassData::load(renamed).entries()[255].name, named.name);
            QCOMPARE(Promotion::load(renamed).json(), promotion.json());
            // Fresh compilation and metadata relocation must agree byte-for-byte.
            const auto recompiled =
                buildPromotionRuntime(priorRom, next.data.layout, promotion, compiler, work.path());
            for (auto it = recompiled.owners.begin(); it != recompiled.owners.end(); ++it)
                QCOMPARE(renamed.bytes().mid(it.key(), it.value().size()), it.value());
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void classNameProjectExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            QVERIFY(p.hasClassData());
            QCOMPARE(encodeClassName("Death\nBahamut"), p.classNameBytes(164));
            const auto knight = encodeClassName("Silver\nKnight");
            p.setClassNameBytes(5, knight);
            p.setClassNameBytes(164, encodeClassName("Ancient\nBahamut"));
            int str = -1;
            for (int n = 0; n < fields(Table::Classes).size(); ++n)
                if (fields(Table::Classes)[n].key == "strBase")
                    str = n;
            QVERIFY(str >= 0);
            p.setValue(Table::Classes, 5, str, 432);
            p.loadPromotion();
            auto families = *p.promotion();
            const auto family = families.addFamily("Order of Silver", 1);
            families.assign(5, family);
            p.setPromotion(families.json());
            QTemporaryDir folder, build;
            QVERIFY(folder.isValid() && build.isValid());
            p.saveFolder(folder.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(restored.classNameBytes(5), knight);
            QCOMPARE(restored.record(Table::Classes, 5).name, QString("Silver\nKnight"));
            QCOMPARE(restored.catalog()["CLASS_NAMES"].toObject()["5"],
                     QJsonValue("Silver\nKnight"));
            auto invalid = restored.manifest();
            invalid["classNames"] = QJsonObject{{"5", "410042"}};
            QVERIFY_THROWS_EXCEPTION(Error, restored.applyManifest(invalid));
            QCOMPARE(restored.classNameBytes(5), knight);
            auto result =
                rebuild(restored, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(candidates));
            QTemporaryDir output(candidates + "/native-class-name-XXXXXX");
            QVERIFY(output.isValid());
            const auto file = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, file);
            auto reopened = Project::fromRom(Rom::open(file));
            QCOMPARE(reopened.classNameBytes(5), knight);
            QCOMPARE(reopened.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
            QCOMPARE(reopened.value(Table::Classes, 5, str), 432);
            reopened.loadPromotion();
            QCOMPARE(reopened.promotion()->json(), families.json());
            const auto placement = ClassPlacement::read(reopened.rom());
            QVERIFY(placement.has_value());
            // The screen owner's last eight bytes begin the following routine.
            QCOMPARE(result.image.mid(0xd0380, 8), p.rom().bytes().mid(0xd0380, 8));
            const auto screenCode = result.image.mid(0xd0150, 568);
            const auto canonicalScreen = placement->canonicalReferences(screenCode, 0xd0150);
            QCOMPARE(placement->placedReferences(canonicalScreen, 0xd0150), screenCode);
            const auto countsCode = result.image.mid(0xbbdac, 784);
            const auto canonicalCounts = placement->canonicalReferences(countsCode, 0xbbdac);
            QCOMPARE(placement->placedReferences(canonicalCounts, 0xbbdac), countsCode);
            QCOMPARE(u32(countsCode, 676) & 65535, (placement->layout.records + 69) & 65535);
            auto alteredCode = countsCode;
            alteredCode[0] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, placement->canonicalReferences(alteredCode, 0xbbdac));
            alteredCode = countsCode;
            alteredCode[679] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, placement->canonicalReferences(alteredCode, 0xbbdac));
            reopened.setClassNameBytes(5, "K");
            const auto smaller =
                rebuild(reopened, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            QVERIFY(smaller.image.size() < result.image.size());
            // A later allocation must preserve unrelated bytes after the old payload.
            const QByteArray marker(16, char(-90));
            auto growing = Project::fromRom(Rom::fromBytes(smaller.image + marker));
            growing.setClassNameBytes(5, encodeClassName("Knight of the Silver Dawn"));
            growing.loadPromotion();
            auto changedFamilies = *growing.promotion();
            changedFamilies.setGate(5, 0, 91);
            growing.setPromotion(changedFamilies.json());
            const auto larger =
                rebuild(growing, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            QCOMPARE(larger.image.mid(smaller.image.size(), marker.size()), marker);
            auto finalProject = Project::fromRom(Rom::fromBytes(larger.image));
            finalProject.loadPromotion();
            QCOMPARE(finalProject.record(Table::Classes, 5).name,
                     QString("Knight of the Silver Dawn"));
            QCOMPARE(finalProject.promotion()->json(), changedFamilies.json());
            QCOMPARE(
                rebuild(finalProject, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path())
                    .image,
                larger.image);
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QVERIFY_THROWS_EXCEPTION(Error, encodeClassName("First\nSecond\nThird"));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void expandableClassData() {
        auto bytes = syntheticRom();
        bytes.replace(0x65dbc, 7, QByteArray("Knight\0", 7));
        for (int i = 0; i < 72; ++i)
            bytes[0x5dacc + i] = char(i ^ 0xa6);
        for (int id = 0; id < 165; ++id) {
            const auto record = 0x5db14 + id * 72;
            for (int field = 0; field < 72; ++field)
                bytes[record + field] = char(id * 13 + field);
            put32(bytes, record, 0x8018febc);
            for (int field = 0; field < 6; ++field)
                bytes[0x64be0 + id * 6 + field] = char(id + field);
        }
        bytes[0xdbe18 + 90] = 24;
        auto classes = ClassData::loadRetail(Rom::fromBytes(bytes));
        QCOMPARE(classes.entries().size(), 165);
        const auto retail = classes.build(0x80243db0);
        QCOMPARE(retail.layout.records, quint32(0x80243df8));
        QCOMPARE(retail.layout.appearances, quint32(0x80246c60));
        QCOMPARE(retail.layout.aliases, quint32(0x80247040));
        QCOMPARE(retail.layout.names, quint32(0x802470f0));
        QCOMPARE(retail.layout.discovery, quint32(0x80247100));
        QCOMPARE(retail.layout.end, quint32(0x80247120));
        QCOMPARE(classes.append(90, "Sentinel"), 165);
        QCOMPARE(classes.entries()[165].namedAlias, quint8(0));
        QCOMPARE(classes.entries()[90].namedAlias, quint8(24));
        for (int id = 166; id < 256; ++id)
            QCOMPARE(classes.append(5, "Knight"), id);
        const auto built = classes.build(0x80243db0);
        const auto &layout = built.layout;
        QCOMPARE(layout.count, 256);
        QCOMPARE(built.bytes.first(72), bytes.mid(0x5dacc, 72));
        QCOMPARE(built.bytes.mid(layout.discovery - layout.indexBase, 32), QByteArray(32, 0));
        QVERIFY(layout.end <= 0x80400000);
        for (int id = 0; id < 256; ++id) {
            auto source = id < 165 ? id : id == 165 ? 90 : 5;
            const auto record = 72 + id * 72;
            QCOMPARE(built.bytes.mid(record + 4, 68), bytes.mid(0x5db14 + source * 72 + 4, 68));
            QCOMPARE(built.bytes.mid(layout.appearances - layout.indexBase + id * 6, 6),
                     bytes.mid(0x64be0 + source * 6, 6));
            const auto pointer = u32(built.bytes, record);
            QVERIFY(pointer >= layout.names && pointer < layout.discovery);
            QVERIFY((pointer & 3) == 0);
            const auto offset = pointer - layout.indexBase;
            const auto end = built.bytes.indexOf(char(0), offset);
            QCOMPARE(built.bytes.mid(offset, end - offset),
                     QByteArray(id == 165 ? "Sentinel" : "Knight"));
        }
        // Equal names share storage, including a freshly appended class.
        QCOMPARE(u32(built.bytes, 72), u32(built.bytes, 72 + 255 * 72));
        QVERIFY_THROWS_EXCEPTION(Error, classes.append(5, "Overflow"));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x80243db1));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x80243db0, 0x802441b0));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x803ff000, 0xffffffff));
        auto bad = classes.entries()[5];
        bad.name = QByteArray("Bad\0Name", 8);
        QVERIFY_THROWS_EXCEPTION(Error, classes.set(5, bad));
        QCOMPARE(classes.entries()[5].name, QByteArray("Knight"));
        bad = classes.entries()[5];
        bad.record.chop(1);
        QVERIFY_THROWS_EXCEPTION(Error, classes.set(5, bad));
        put32(bytes, 0x5db14, 0x80180000);
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::loadRetail(Rom::fromBytes(bytes)));
    }
    void retailClassDataCompilation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Set LORDLY_TEST_ROM and LORDLY_N64_TOOLCHAIN for the class-data build");
        const auto rom = Rom::open(path);
        auto classes = ClassData::loadRetail(rom);
        const auto original = classes.entries();
        QCOMPARE(original[5].name, QByteArray("Knight"));
        QCOMPARE(original[164].name, QByteArray::fromHex("0e44656174681063426168616d75740f"));
        QCOMPARE(classes.append(5, "Silver Knight"), 165);
        const auto result = classes.build(0x80243db0);
        QTemporaryDir work;
        QVERIFY(work.isValid());
        Compiler compiler(tools);
        compiler.authenticate();
        const auto runtime = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
        QCOMPARE(runtime.compiledData, result.bytes);
        QCOMPARE(runtime.data.layout.end, result.layout.end);
        QCOMPARE(runtime.dataRomOffset, quint32(0x2800000));
        QCOMPARE(runtime.code.entries.size(), 3);
        QVERIFY(runtime.code.entries.contains("lc_boot_classes"));
        QVERIFY(runtime.code.entries.contains("lc_heap_reset"));
        QVERIFY(runtime.code.entries.contains("lc_secondary_reset"));
        QVERIFY(runtime.code.address + runtime.code.bytes.size() <= runtime.directoryAddress);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 buildClassRuntime(classes, compiler, work.path(), 0x2800001));
        QVERIFY_THROWS_EXCEPTION(Error,
                                 buildClassRuntime(classes, compiler, work.path(), 0x3fffff0));
        for (int id = 0; id < 165; ++id) {
            const auto record = result.layout.records - result.layout.indexBase + id * 72;
            QCOMPARE(result.bytes.mid(record + 4, 68), original[id].record.sliced(4));
            const auto offset = u32(result.bytes, record) - result.layout.indexBase;
            QCOMPARE(result.bytes.mid(offset, original[id].name.size()), original[id].name);
            QCOMPARE(u8(result.bytes, offset + original[id].name.size()), quint8(0));
        }
        const auto linked = linkClassRuntime(rom, runtime);
        const auto relocated = Rom::fromBytes(linked);
        const auto placement = ClassPlacement::read(relocated);
        QVERIFY(placement.has_value());
        QCOMPARE(placement->layout.count, 166);
        QCOMPARE(placement->dataRomOffset, runtime.dataRomOffset);
        QCOMPARE((u32(linked, 0x25a8) & 0x3ffffffU) << 2, runtime.placement.bootEntry & 0xfffffffU);
        QCOMPARE(u32(linked, 0x1148), quint32(0x2c42000a));
        QVERIFY(runtime.saves.has_value());
        const auto cacheEnd =
            quint32(((u32(linked, 0x2438) & 65535) << 16) + qint16(u32(linked, 0x243c) & 65535));
        QCOMPARE(cacheEnd, quint32(0x80187f00U + runtime.saves->code.bytes.size()));
        auto reopened = ClassData::load(relocated);
        QCOMPARE(reopened.entries().size(), 166);
        for (int id = 0; id < 166; ++id) {
            QCOMPARE(reopened.entries()[id].record.sliced(4),
                     classes.entries()[id].record.sliced(4));
            QCOMPARE(reopened.entries()[id].appearance, classes.entries()[id].appearance);
            QCOMPARE(reopened.entries()[id].name, classes.entries()[id].name);
            QCOMPARE(reopened.entries()[id].namedAlias, classes.entries()[id].namedAlias);
        }
        auto corrupt = linked;
        corrupt[ClassPlacement::descriptorRomOffset + 124] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        corrupt = linked;
        corrupt[runtime.dataRomOffset + 100] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        corrupt = linked;
        corrupt[ClassPlacement::codeRomOffset] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        // A valid CRC is not sufficient: pointers must belong to the names allocation.
        auto badData = runtime.compiledData;
        put32(badData, 72, runtime.data.layout.discovery);
        corrupt = linked;
        corrupt.replace(runtime.dataRomOffset, badData.size(), badData);
        corrupt.replace(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize,
                        runtime.placement.descriptor(badData, runtime.code.bytes));
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        reopened.append(5, "Second new class");
        const auto next = buildClassRuntime(reopened, compiler, work.path(), runtime.dataRomOffset);
        QVERIFY(next.compiledData.size() > runtime.compiledData.size());
        const auto relinked = linkClassRuntime(relocated, next);
        QCOMPARE(ClassData::load(Rom::fromBytes(relinked)).entries().size(), 167);
        QCOMPARE(ClassData::load(Rom::fromBytes(relinked)).entries()[166].name,
                 QByteArray("Second new class"));
        QVERIFY_THROWS_EXCEPTION(
            Error, linkClassRuntime(Rom::fromBytes(linked + QByteArray(16, char(-90))), next));
        corrupt = linked;
        put32(corrupt, 0x43100, u32(corrupt, 0x43100) ^ 1);
        QVERIFY_THROWS_EXCEPTION(Error, linkClassRuntime(Rom::fromBytes(corrupt), next));
        // Keep this owned candidate for instruction-level boot/reader checks.
        const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(candidates));
        QTemporaryDir candidate(candidates + "/native-class-link-XXXXXX");
        QVERIFY(candidate.isValid());
        const auto output = candidate.filePath(QFileInfo(path).fileName());
        atomicWrite(output, rom.serialize(linked));
        candidate.setAutoRemove(false);
        qInfo().noquote() << "Relocated-data candidate (remaining class consumers not expanded):"
                          << output;
        QCOMPARE(Rom::open(path).bytes(), rom.bytes());
    }
    void sourcePreprocessing() {
        auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty())
            QSKIP("Set LORDLY_N64_TOOLCHAIN to exercise the bundled compiler");
        Compiler compiler(tools);
        compiler.authenticate();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        atomicWrite(dir.filePath("contract.h"), "#error Stale build header was included\n");
        const QByteArray source = R"C(
/* A maintained source may contain comments, includes and macros. */
#include "contract.h"
#if defined(_WIN32) || defined(i386) || defined(__i386__)
#error Host target macros leaked into the N64 compilation
#endif
#if __mips != 3 || !defined(_MIPSEB)
#error Missing N64 target definitions
#endif
unsigned char result[] = { LC_TWICE(21), sizeof(void *), sizeof(long), sizeof("/* kept */") };
)C";
        auto built = compiler.compile(source, dir.path(), "preprocessing", 0, ".data",
                                      {{"contract.h", "#define LC_TWICE(x) ((x) * 2)\n"}});
        QCOMPARE(built.first(4), QByteArray::fromHex("2a04040b"));
        for (auto byte : built.sliced(4))
            QCOMPARE(byte, char(0));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compile("#include <missing.h>\n", dir.path(), "missing", 0, ".data"));
    }
    void linkedModuleEntries() {
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty())
            QSKIP("Set LORDLY_N64_TOOLCHAIN to exercise linked C modules");
        QTemporaryDir work;
        QVERIFY(work.isValid());
        Compiler compiler(tools);
        compiler.authenticate();
        const auto module =
            compiler.compileModule("unsigned int first(unsigned int x) { return x + 7; }\n"
                                   "unsigned int second(unsigned int x) { return x ^ 90; }\n",
                                   work.path(), "entries", 0x80187c14);
        QCOMPARE(module.entries.size(), 2);
        QVERIFY(module.entries.contains("first") && module.entries.contains("second"));
        QVERIFY(module.entries["first"] != module.entries["second"]);
        // Independent MIPS expectations: jr ra; addiu v0,a0,7 / xori v0,a0,90.
        QCOMPARE(module.bytes.mid(module.entries["first"] - module.address, 8),
                 QByteArray::fromHex("03e0000824820007"));
        QCOMPARE(module.bytes.mid(module.entries["second"] - module.address, 8),
                 QByteArray::fromHex("03e000083882005a"));
        const auto leaf = compiler.compileModule("void nothing(void) {}\n", work.path(),
                                                 "alignment_leaf", 0x80100000);
        QCOMPARE(leaf.functionExtent, quint32(8));
        QCOMPARE(leaf.fitOwner(8), QByteArray::fromHex("03e0000800000000"));
        QCOMPARE(leaf.fitOwner(20), QByteArray::fromHex("03e0000800000000") + QByteArray(12, 0));
        // The zero delay instruction belongs to the function, unlike zero link padding.
        QVERIFY_THROWS_EXCEPTION(Error, leaf.fitOwner(4));
        // Deliberately unaligned output-section start forces linker padding;
        // +69 crosses a signed low-half boundary, and -57 checks negative addends.
        const QMap<QString, quint32> bindings = {{"records", 0x80307ff0}, {"other", 0x80301230}};
        const auto references = compiler.compileModule(
            "extern unsigned char records[], other[];\n"
            "unsigned int first(unsigned int id) { return records[id * 72 + 69]; }\n"
            "unsigned int second(unsigned int id) { return records[id * 72 - 57] + other[id]; "
            "}\n",
            work.path(), "references", 0x80100004, {}, bindings);
        QVERIFY(references.entries["first"] > references.address);
        QSet<QString> observed;
        for (const auto &ref : references.addressReferences) {
            QVERIFY(ref.highOffset < references.functionExtent);
            QVERIFY(ref.lowOffset < references.functionExtent);
            const auto high = u32(references.bytes, ref.highOffset);
            const auto low = u32(references.bytes, ref.lowOffset);
            const quint32 target = ((high & 65535) << 16) + quint32(qint16(low & 65535));
            QCOMPARE(target, bindings[ref.symbol] + quint32(ref.addend));
            observed.insert(ref.symbol + ":" + QString::number(ref.addend));
        }
        QCOMPARE(observed, QSet<QString>({"records:69", "records:-57", "other:0"}));
        QFile objectFile(work.filePath("references.o"));
        QVERIFY(objectFile.open(QIODevice::ReadOnly));
        const auto object = objectFile.readAll();
        QVERIFY_THROWS_EXCEPTION(Error,
                                 readMipsAddressReferences(object.first(24), references, bindings));
        auto altered = references;
        altered.bytes[altered.addressReferences.first().lowOffset + 3] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, readMipsAddressReferences(object, altered, bindings));
        auto dirtyPadding = leaf;
        dirtyPadding.bytes.append(char(1));
        QVERIFY_THROWS_EXCEPTION(Error, dirtyPadding.fitOwner(8));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compileModule("volatile unsigned int state = 4;\nunsigned int "
                                          "use_state(void) { return state; }\n",
                                          work.path(), "requires_data", 0x80187c14));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compileModule(
                       "unsigned int state;\nunsigned int use_state(void) { return state; }\n",
                       work.path(), "requires_bss", 0x80187c14));
    }
    void byteOrders() {
        const auto expected = QByteArray::fromHex("8037124011223344") + QByteArray(56, 0);
        const auto swapped = QByteArray::fromHex("3780401222114433") + QByteArray(56, 0);
        const auto little = QByteArray::fromHex("4012378044332211") + QByteArray(56, 0);
        for (const auto &input : {expected, swapped, little}) {
            auto rom = Rom::fromBytes(input);
            QCOMPARE(rom.bytes(), expected);
            QCOMPARE(rom.serialize(rom.bytes()), input);
        }
        QVERIFY_THROWS_EXCEPTION(Error, Rom::fromBytes(expected + char(0)));
        QVERIFY_THROWS_EXCEPTION(Error, Rom::fromBytes(QByteArray(64, 0)));
    }
    void compressionCommands() {
        QCOMPARE(decodeLzss(QByteArray::fromHex("42414243"), 3), QByteArray("ABC"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("20"), 2), QByteArray(2, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0102"), 5), QByteArray(5, char(-1)));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0201"), 4), QByteArray(4, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("424142438802"), 7), QByteArray("ABCABCA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("4041100000"), 5), QByteArray("AAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("404100000000"), 6), QByteArray("AAAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("8000"), 3), QByteArray(3, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("030f4042"), 1), QByteArray("B"));
        QVERIFY_THROWS_EXCEPTION(Error, decodeLzss(QByteArray::fromHex("424142"), 3));
        QVERIFY_THROWS_EXCEPTION(Error, decodeLzss(QByteArray::fromHex("80"), 3));
        QByteArray mixed;
        for (int i = 0; i < 1024; ++i)
            mixed.append(char(i));
        mixed += QByteArray(1000, 0) + QByteArray(700, char(-1));
        QCOMPARE(decodeLzss(encodeLzss(mixed), mixed.size()), mixed);
    }
    void compressionMatchBoundaries() {
        QByteArray noise(70000, 0); quint32 state = 0x9137ab35;
        for (auto &byte : noise) { state = state * 1664525U + 1013904223U; byte = char(state >> 24); }
        for (int distance : {2047,2048,2049,16383,16384,16385,65535,65536,65537}) {
            auto input = noise.left(distance) + noise.left(260);
            input += QByteArray(600, 0) + QByteArray(600, char(-1));
            const auto encoded = encodeLzss(input);
            QCOMPARE(decodeLzss(encoded, input.size()), input);
            QCOMPARE(encodeLzss(input), encoded);
        }
    }
    void compressionSpriteCompatibility() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        // Recorded from the previous exhaustive encoder on these exact ROM resources.
        const std::array<quint32,2> keys{0x3c00e0,0x3c412e};
        const std::array<QByteArray,2> expected{
            "6c7f7f7cc8eab92ce8bb2db851cba968dc52439d01a0509be848735d4cf82ede",
            "5d7101da661c6883022a854bab6d269c0e66e766331dbfea841c158b688513a3"};
        for (int i=0;i<2;++i) {
            const auto at=0x594280+keys[i], size=u32(rom.bytes(),at), decodedSize=u32(rom.bytes(),at+4);
            const auto decoded=decodeLzss(rom.bytes().mid(at+8,size-4),decodedSize);
            QElapsedTimer timer; timer.start(); const auto encoded=encodeLzss(decoded);
            qInfo()<<"Sprite compressor"<<keys[i]<<"bytes"<<decoded.size()<<"milliseconds"<<timer.elapsed();
            QCOMPARE(sha256(encoded),expected[i]); QCOMPARE(decodeLzss(encoded,decoded.size()),decoded);
        }
    }
    void recordFramingAndPreservation() {
        auto data = syntheticRom();
        const int knight = 5;
        auto header = 0x5dad8 + (knight + 1) * 72 - 12;
        data[header + 7] = char(-61);
        put16(data, header + 8, 123);
        put16(data, header + 12, 456);
        auto p = Project::fromRom(Rom::fromBytes(data));
        const auto &row = p.record(Table::Classes, knight);
        QCOMPARE(u16(row.bytes, 8), quint16(123));
        QCOMPARE(u16(row.bytes, 12), quint16(456));
        int str = -1;
        for (int n = 0; n < fields(Table::Classes).size(); ++n)
            if (fields(Table::Classes)[n].key == "strBase")
                str = n;
        QVERIFY(str >= 0);
        p.setValue(Table::Classes, knight, str, 900);
        auto rebuilt = p.rebuildRecords();
        auto expected = data;
        put16(expected, header + 12, 900);
        QCOMPARE(rebuilt, expected);
        QVERIFY_THROWS_EXCEPTION(Error, p.setValue(Table::Classes, knight, str, 65536));
        QCOMPARE(p.rom().bytes(), data);
    }
    void projectPersistence() {
        auto rom = Rom::fromBytes(syntheticRom());
        auto p = Project::fromRom(rom);
        p.setValue(Table::Classes, 5, 0, 2);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        p.saveFolder(dir.path());
        auto saved = Project::readProject(dir.path());
        saved["extensionExample"] = QJsonObject{{"keep", 42}};
        auto restored = Project::fromRom(rom);
        restored.applyManifest(saved);
        QCOMPARE(restored.value(Table::Classes, 5, 0), 2);
        QCOMPARE(restored.manifest()["extensionExample"], saved["extensionExample"]);
        auto archive = dir.filePath("share.lcproject");
        restored.saveArchive(archive);
        auto packed = Project::readProject(archive);
        QVERIFY(!packed.contains("sourcePath"));
        QCOMPARE(packed["records"], saved["records"]);
        auto wrong = saved;
        wrong["sourceSha256"] = "wrong";
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyManifest(wrong));
        QCOMPARE(restored.value(Table::Classes, 5, 0), 2);
    }
    void retailChecksumAndCompiledExport() {
        try {
            auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Set LORDLY_TEST_ROM to run the private-ROM integration check");
            auto source = Rom::open(path);
            source.requireSupported();
            QCOMPARE(QString::fromLatin1(sha256(source.bytes())),
                     QString("571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a"));
            auto checked = source.bytes();
            Rom::updateChecksum(checked);
            QCOMPARE(u32(checked, 0x10), quint32(0xe6419bc5));
            QCOMPARE(u32(checked, 0x14), quint32(0x69011de3));
            QCOMPARE(checked, source.bytes());
            auto project = Project::fromRom(source);
            auto prior = project.value(Table::Classes, 5, 4);
            project.setValue(Table::Classes, 5, 4, (prior + 1) & 255);
            QTemporaryDir build;
            QVERIFY(build.isValid());
            auto result =
                rebuild(project, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto expected = project.rebuildRecords();
            Rom::updateChecksum(expected);
            QCOMPARE(result.image, expected);
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(candidates));
            QTemporaryDir candidate(candidates + "/native-export-XXXXXX");
            QVERIFY(candidate.isValid());
            auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(project, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.value(Table::Classes, 5, 4), (prior + 1) & 255);
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
            QVERIFY_THROWS_EXCEPTION(Error, exportRom(reopened, result, output));
            QVERIFY(QFileInfo::exists(build.filePath("classes_records.c")));
            QVERIFY(QFileInfo::exists(build.filePath("func_000bbd50.compiler.s")));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void browserProjectImport() {
        try {
        QJsonObject legacy{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches",
             QJsonObject{
                 {"classDefs",
                  QJsonObject{
                      {"5", QJsonObject{{"record_index", 6},
                                        {"bytes", QJsonObject{{"0", 1}, {"1", 44}, {"64", 2}}}}}}},
                 {"items", QJsonObject{{"7", QJsonObject{{"str", -4},
                                                         {"growthHpStr", 2},
                                                         {"growthUnknown", 1},
                                                         {"growthInt", 3},
                                                         {"growthAgi", 1}}}}},
                 {"item_prices", QJsonObject{{"7", 1234}}},
                 {"art", QJsonObject{{"schemaVersion", 5}, {"avatars", QJsonObject{}}}}}}};
        auto p = Project::fromRom(Rom::fromBytes(syntheticRom()));
        p.importLegacy(legacy);
        QCOMPARE(u16(p.record(Table::Classes, 5).bytes, 12), quint16(300));
        QCOMPARE(u8(p.record(Table::Classes, 5).bytes, 4), quint8(2));
        QCOMPARE(u8(p.record(Table::Items, 7).bytes, 10), quint8(252));
        QCOMPARE(u8(p.record(Table::Items, 7).bytes, 24), quint8(0x9d));
        QCOMPARE(u16(p.record(Table::Items, 7).bytes, 8), quint16(1234));
        QVERIFY(p.pendingLegacySections().isEmpty());
        QCOMPARE(p.manifest()["legacyImport"].toObject(), legacy);
        auto before = p.manifest();
        auto malformed = legacy;
        auto patches = malformed["patches"].toObject();
        patches["items"] = QJsonObject{{"7", QJsonObject{{"str", 200}}}};
        malformed["patches"] = patches;
        QVERIFY_THROWS_EXCEPTION(Error, p.importLegacy(malformed));
        QCOMPARE(p.manifest(), before);
        patches = legacy["patches"].toObject();
        patches["tools"] = QJsonObject{{"futureTool", false}};
        legacy["patches"] = patches;
        p.importLegacy(legacy);
        QCOMPARE(p.pendingLegacySections(), QStringList{"tools"});
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void retailResourceCompression() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            const auto rom = Rom::open(path);
            const std::array<std::pair<int, int>, 5> resources = {{{0x34fa4e, 9710},
                                                                   {0x352b88, 2064},
                                                                   {0x353f92, 6408},
                                                                   {0x356180, 3154},
                                                                   {0x3a960c, 384}}};
            for (auto [relative, allocation] : resources) {
                auto resource = Resource::load(rom, 0x20248c2 + relative, allocation);
                auto rebuilt = resource.container(resource.decoded);
                QCOMPARE(rebuilt.size(), allocation);
                QCOMPARE(decodeLzss(rebuilt.mid(8, u32(rebuilt, 0) - 4), u32(rebuilt, 4)),
                         resource.original);
            }
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void textEncodingAndReferences() {
        QCOMPARE(encodeText("First\nSecond", false),
                 QByteArray::fromHex("466972737481975365636f6e64"));
        QCOMPARE(encodeText("A\nB\nC", true), QByteArray::fromHex("0e411030420f81970e431030200f"));
        QCOMPARE(decodeText(QByteArray::fromHex("41819632383030819742876a")),
                 QString("A{REF:2800}\nB{CTRL:876A}"));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("{BYTE:0D}{BYTE:0A}", false));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("{REF:9999}", false));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("1\n2\n3\n4\n5", false));
        TextBank bank;
        bank.spec.id = "classes";
        auto original = QByteArray("Knight") + QByteArray::fromHex("8197819632383030");
        bank.entries = {{QByteArray(), QByteArray()}, {original, original}};
        bank.entries.resize(225);
        QCOMPARE(bank.text(1), QString("Knight"));
        bank.setText(1, "Armored fighter");
        QCOMPARE(bank.entries[1].bytes,
                 QByteArray("Armored fighter") + QByteArray::fromHex("8197819632383030"));
        QCOMPARE(bank.entries[1].original, original);
    }
    void descriptionRuntimeBounds() {
        TextBank bank;
        bank.spec.id = "classes";
        bank.spec.count = 225;
        bank.entries.resize(225);
        bank.setText(1, QString(1278, 'A'));
        QCOMPARE(bank.entries[1].bytes.size() + 2, 0x500);
        const auto maximum = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(1, QString(1279, 'A')));
        QCOMPARE(bank.serialized(), maximum);
        bank.setText(1, "{HELP:2800}{HELP:2801}");
        bank.setText(200, QString(639, 'B'));
        bank.setText(201, QString(639, 'C'));
        const auto valid = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(201, QString(640, 'C')));
        QCOMPARE(bank.serialized(), valid);
        bank.setText(200, "{HELP:2801}");
        const auto chained = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(201, "{HELP:2800}"));
        QCOMPARE(bank.serialized(), chained);
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(2, "{REF:10}"));
    }
    void retailDescriptionExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            p.loadTexts();
            auto originalBanks = p.textBanks();
            int editedId = -1;
            for (int id = 1; id <= 164; ++id)
                if (originalBanks["classes"].entries[id].bytes.contains(
                        QByteArray::fromHex("8196"))) {
                    editedId = id;
                    break;
                }
            QVERIFY(editedId > 0);
            auto original = originalBanks["classes"].entries[editedId].bytes;
            auto ref = original.indexOf(QByteArray::fromHex("8196"));
            QVERIFY(ref >= 0);
            auto suffix = original.mid(
                ref >= 2 && original.mid(ref - 2, 2) == QByteArray::fromHex("8197") ? ref - 2
                                                                                    : ref);
            p.setText("classes", editedId, "Armored fighter");
            QCOMPARE(p.textBanks()["classes"].entries[editedId].bytes,
                     QByteArray("Armored fighter") + suffix);
            QTemporaryDir projectDir;
            p.saveFolder(projectDir.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(projectDir.path()));
            QCOMPARE(restored.textBanks()["classes"].serialized(),
                     p.textBanks()["classes"].serialized());
            QTemporaryDir build;
            auto result = rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir outputDir(candidates + "/native-text-XXXXXX");
            QVERIFY(outputDir.isValid());
            auto output = outputDir.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            reopened.loadTexts();
            for (auto bank = originalBanks.begin(); bank != originalBanks.end(); ++bank) {
                for (int i = 0; i < bank->entries.size(); ++i) {
                    auto expected = bank.key() == "classes" && i == editedId
                                        ? QByteArray("Armored fighter") + suffix
                                        : bank->entries[i].bytes;
                    QCOMPARE(reopened.textBanks()[bank.key()].entries[i].bytes, expected);
                }
            }
            QCOMPARE(u32(result.image, p.textBanks()["classes"].resource.romOffset), quint32(6403));
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QVERIFY(QFileInfo::exists(build.filePath("descriptions_classes.compiler.s")));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void addedClassPublicExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler are required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadTexts();
            const auto originalDescriptions = project.textBanks()["classes"].entries;
            QCOMPARE(project.createClass("Fresh class"), 165);
            QVERIFY(project.textBanks()["classes"].entries[225].bytes.isEmpty());
            QTemporaryDir folder;
            project.saveFolder(folder.path());
            auto loaded = Project::fromRom(source);
            loaded.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(loaded.manifest(), project.manifest());
            QTemporaryDir build;
            for (const int count : {166, 256, 256}) {
                while (project.records(Table::Classes).size() < count)
                    project.appendClass(
                        5, QString("New class %1").arg(project.records(Table::Classes).size()));
                const auto description = QString("Class %1 export description").arg(count);
                project.setText("classes", Project::classDescriptionIndex(count - 1), description);
                auto promotion = *project.promotion();
                promotion.setGate(count - 1, 0, 75);
                promotion.assign(
                    count - 1,
                    promotion.createFamily(QString("Family %1 %2")
                                               .arg(count)
                                               .arg(promotion.families["families"].toArray().size())));
                project.setPromotion(promotion.json());
                const auto expectedData = project.classData().build(0x80243db0).bytes;
                const auto result = rebuild(project, tools, build.path());
                const auto candidateRoot =
                    QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
                QTemporaryDir output(candidateRoot + "/native-added-class-XXXXXX");
                QVERIFY(output.isValid());
                output.setAutoRemove(false);
                const auto outputPath = output.filePath(QFileInfo(path).fileName());
                exportRom(project, result, outputPath);
                auto reopened = Project::fromRom(Rom::open(outputPath));
                reopened.loadTexts();
                reopened.loadPromotion();
                QCOMPARE(reopened.records(Table::Classes).size(), count);
                QCOMPARE(reopened.classData().build(0x80243db0).bytes, expectedData);
                QCOMPARE(reopened.promotion()->json(), project.promotion()->json());
                QCOMPARE(reopened.textBanks()["classes"].serialized(),
                         project.textBanks()["classes"].serialized());
                QCOMPARE(
                    reopened.textBanks()["classes"].text(Project::classDescriptionIndex(count - 1)),
                    description);
                for (int id = 165; id < 225; ++id)
                    QCOMPARE(reopened.textBanks()["classes"].entries[id].bytes,
                             originalDescriptions[id].bytes);
                const auto placed = ClassPlacement::read(reopened.rom());
                QVERIFY(placed->descriptionEntry);
                QCOMPARE(placed->saves.version, 3U);
                auto checksum = result.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, result.image);
                const auto layout = ClassMenuLayout::build(
                    placed->layout, reopened.promotion()->buildExpandedData());
                atomicWrite(
                    output.filePath("menu-layout.json"),
                    QJsonDocument(QJsonObject{{"rom", outputPath},
                                              {"count", count},
                                              {"ownerBytes", int(layout.ownerBytes)},
                                              {"list", int(layout.list)},
                                              {"ids", int(layout.categoryClasses)},
                                              {"current", int(layout.currentCounts)},
                                              {"eligible", int(layout.eligibleCounts)},
                                              {"records", double(placed->layout.records)},
                                              {"aliases", double(placed->layout.aliases)},
                                              {"discovery", double(placed->layout.discovery)}})
                        .toJson());
                qInfo().noquote() << "Public added-class export:"
                                  << output.filePath("menu-layout.json");
                project = std::move(reopened);
            }
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void addedClassProjects() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadTexts();
            project.loadPromotion();
            const auto baseline = project.manifest();
            auto templateBytes = project.record(Table::Classes, 5).bytes;
            templateBytes[71] = char(u8(templateBytes, 71) ^ 0x40);
            project.replaceBytes(Table::Classes, 5, templateBytes);
            project.setText("classes", 5, "New template description");
            auto promotion = *project.promotion();
            promotion.setGate(5, 0, 123);
            const auto family = promotion.addFamily("New family", 1);
            promotion.assign(5, family);
            promotion.setMask(5, 0x5a);
            project.setPromotion(promotion.json());
            const auto templateAsset = project.classData().entries()[5];
            QCOMPARE(project.appendClass(5, "Authored class"), 165);
            QCOMPARE(project.record(Table::Classes, 165).romOffset, qsizetype(-1));
            QCOMPARE(project.record(Table::Classes, 165).bytes, templateBytes);
            const auto addedAsset = project.classData().entries()[165];
            QCOMPARE(addedAsset.appearance, templateAsset.appearance);
            QCOMPARE(addedAsset.namedAlias, quint8(0));
            QCOMPARE(project.textBanks()["classes"].entries[225].bytes,
                     project.textBanks()["classes"].entries[5].bytes);
            QCOMPARE(project.promotion()->gates.mid(165 * 8, 8), promotion.gates.mid(40, 8));
            QCOMPARE(u8(project.promotion()->masks, 165), quint8(0x5a));
            QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), family);
            project.setText("classes", 225, "Edited added description");
            project.setClassNameBytes(165, encodeClassName("Edited added class"));
            for (int id = 166; id <= 255; ++id)
                QCOMPARE(project.appendClass(165, QString("Added class %1").arg(id)), id);
            const auto authored = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, project.appendClass(5, "Too many"));
            QVERIFY_THROWS_EXCEPTION(Error, project.createClass("Too many"));
            QCOMPARE(project.manifest(), authored);
            QTemporaryDir folder, archive;
            project.saveFolder(folder.path());
            project.saveArchive(archive.filePath("added.lcproject"));
            for (const auto &location : {folder.path(), archive.filePath("added.lcproject")}) {
                auto reopened = Project::fromRom(project.rom());
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.manifest(), authored);
                QCOMPARE(reopened.classData().build(0x80243db0).bytes,
                         project.classData().build(0x80243db0).bytes);
                QCOMPARE(reopened.textBanks()["classes"].serialized(),
                         project.textBanks()["classes"].serialized());
                QCOMPARE(reopened.promotion()->json(), project.promotion()->json());
                reopened.applyManifest(baseline);
                QCOMPARE(reopened.records(Table::Classes).size(), 165);
                QCOMPARE(reopened.textBanks()["classes"].entries.size(), 225);
                QCOMPARE(reopened.promotion()->classCount(), 81);
                QCOMPARE(reopened.changedRecords(), 0);
                QVERIFY(!reopened.catalog()["CLASS_NAMES"].toObject().contains("165"));
            }
            auto damaged = authored;
            damaged.remove("promotion");
            QVERIFY_THROWS_EXCEPTION(Error, project.applyManifest(damaged));
            QCOMPARE(project.manifest(), authored);
            auto rows = damaged["addedClasses"].toArray();
            auto row = rows[0].toObject();
            row["appearance"] = "00";
            rows[0] = row;
            damaged["addedClasses"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, project.applyManifest(damaged));
            QCOMPARE(project.manifest(), authored);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void growingClassDescriptions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadTexts();
            const auto original = project.textBanks();
            quint32 random = 0x18462476;
            const QByteArray alphabet(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
            auto edit = [&](Project &p, int width) {
                for (int id = 1; id < 165; ++id) {
                    QString text;
                    for (int line = 0; line < 3; ++line) {
                        if (line)
                            text += '\n';
                        for (int col = 0; col < width; ++col) {
                            random = random * 1664525U + 1013904223U;
                            text += QChar(alphabet[(random >> 16) % alphabet.size()]);
                        }
                    }
                    p.setText("classes", id, text);
                }
            };
            edit(project, 32);
            QVERIFY(encodeLzss(project.textBanks()["classes"].serialized()).size() > 6399);
            project.setClassNameBytes(1, encodeClassName("Description owner"));
            QTemporaryDir build;
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            auto result = rebuild(project, tools, build.path());
            const auto first = ClassTextPlacement::read(Rom::fromBytes(result.image));
            QVERIFY(first.has_value());
            QCOMPARE(u32(result.image, 0x66d60), first->offset - ClassTextPlacement::keyBase);
            QCOMPARE(((u32(result.image, 0x56404) & 65535) << 16) |
                         (u32(result.image, 0x56408) & 65535),
                     first->offset - ClassTextPlacement::keyBase);
            auto reopened = Project::fromRom(Rom::fromBytes(result.image));
            reopened.loadTexts();
            for (auto it = original.begin(); it != original.end(); ++it)
                for (int id = 0; id < it->entries.size(); ++id)
                    if (it.key() != "classes" || id == 0 || id >= 165)
                        QCOMPARE(reopened.textBanks()[it.key()].entries[id].bytes,
                                 it->entries[id].bytes);
            QCOMPARE(reopened.textBanks()["classes"].serialized(),
                     project.textBanks()["classes"].serialized());
            // Class data follows the first text allocation. Growing text must
            // move past it, retain it, and allow the next class relink.
            const auto classes = ClassPlacement::read(reopened.rom());
            QVERIFY(classes.has_value());
            edit(reopened, 64);
            reopened.setClassNameBytes(1, encodeClassName("Description owner with a longer name"));
            auto grown = rebuild(reopened, tools, build.path());
            const auto second = ClassTextPlacement::read(Rom::fromBytes(grown.image));
            QVERIFY(second.has_value());
            QVERIFY(second->offset >= result.image.size());
            auto again = Project::fromRom(Rom::fromBytes(grown.image));
            again.loadTexts();
            QCOMPARE(again.textBanks()["classes"].serialized(),
                     reopened.textBanks()["classes"].serialized());
            QCOMPARE(again.classNameBytes(1), reopened.classNameBytes(1));
            for (const auto at : {quint32(0x66d60), quint32(0x56404), second->offset + 12,
                                  ClassTextPlacement::descriptorOffset + 12}) {
                auto damaged = grown.image;
                put8(damaged, at, u8(damaged, at) ^ 1);
                QVERIFY_THROWS_EXCEPTION(Error, TextBank::load(Rom::fromBytes(damaged)));
            }
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir output(candidates + "/native-growing-text-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            exportRom(reopened, grown, output.filePath(QFileInfo(path).fileName()));
            qInfo().noquote() << "Growing description candidate:" << output.path();
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void classMenuSourceLayout() {
        const auto base = syntheticPromotion();
        // Every supported class count gets independent, terminated buffers;
        // a mismatch with either runtime table must fail before compilation.
        for (int count = 165; count <= 256; ++count) {
            auto promotion = base;
            promotion.growToClassCount(count);
            const auto payload = promotion.buildExpandedData();
            ClassDataLayout classes;
            classes.count = count;
            classes.indexBase = 0x80243db0;
            classes.records = 0x80243df8;
            classes.appearances = 0x8024a000;
            classes.aliases = 0x80250000;
            classes.discovery = 0x80250800;
            const auto layout = ClassMenuLayout::build(classes, payload);
            QCOMPARE(layout.capacity, count + 1);
            QCOMPARE(layout.list, quint32(0x2528));
            const quint32 arrayBytes = (count + 1) * 2;
            QVERIFY(layout.list + arrayBytes <= layout.categoryClasses);
            QVERIFY(layout.categoryClasses + arrayBytes <= layout.currentCounts);
            QVERIFY(layout.currentCounts + arrayBytes <= layout.eligibleCounts);
            QVERIFY(layout.eligibleCounts + arrayBytes <= layout.ownerBytes);
            QCOMPARE((layout.categoryClasses | layout.currentCounts | layout.eligibleCounts |
                      layout.ownerBytes) &
                         15,
                     quint32(0));
            if (count == 166) {
                QCOMPARE(layout.categoryClasses, quint32(0x2680));
                QCOMPARE(layout.currentCounts, quint32(0x27d0));
                QCOMPARE(layout.eligibleCounts, quint32(0x2920));
                QCOMPARE(layout.ownerBytes, quint32(0x2a70));
            }
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (!tools.isEmpty() && (count == 166 || count == 255 || count == 256)) {
                Compiler compiler(tools);
                compiler.authenticate();
                QTemporaryDir work;
                for (const auto &entry : promotionOwners()["owners"].toArray()) {
                    const auto owner = entry.toObject();
                    if (!owner.contains("cSource"))
                        continue;
                    QFile source(":/game/promotion/" + owner["cSource"].toString());
                    QVERIFY(source.open(QIODevice::ReadOnly));
                    QMap<QString, quint32> symbols = layout.dataSymbols;
                    const auto bindings = owner["symbols"].toObject();
                    for (auto it = bindings.begin(); it != bindings.end(); ++it)
                        if (!symbols.contains(it.key()))
                            symbols[it.key()] = quint32(it.value().toDouble());
                    const auto input = source.readAll();
                    const auto code =
                        compiler.compileModule(input, work.path(), owner["name"].toString(),
                                               quint32(owner["start"].toInt()) + 0x801135a0U,
                                               {{"class_menu_config.h", layout.cHeader}}, symbols);
                    const auto size = owner.value("cExtent").toInt(owner["size"].toInt());
                    QVERIFY2(code.functionExtent <= quint32(size),
                             qPrintable(QString("%1 with %2 classes needs %3 bytes, allocation %4")
                                            .arg(owner["name"].toString())
                                            .arg(count)
                                            .arg(code.functionExtent)
                                            .arg(size)));
                    QCOMPARE(code.fitOwner(size).size(), size);
                    if (owner["cSource"] == "category-class-counts.c") {
                        QVector<RuntimeDataReference> references;
                        for (const auto &ref : code.addressReferences) {
                            if (ref.symbol == "lc_class_records")
                                references.append(
                                    {"classes", ref.highOffset, ref.lowOffset, ref.addend + 72});
                            else if (ref.symbol == "lc_named_class_map")
                                references.append(
                                    {"aliases", ref.highOffset, ref.lowOffset, ref.addend});
                            else if (ref.symbol == "lc_discovery_flags")
                                references.append(
                                    {"discovery", ref.highOffset, ref.lowOffset, ref.addend});
                        }
                        QVERIFY(references.size() >= 3);
                        const quint32 start = owner["start"].toInt();
                        const auto bytes = code.fitOwner(size);
                        const auto bases = ClassRuntimeInventory::dataBases(classes);
                        ClassRuntimeInventory inventory;
                        inventory.classCount = count;
                        inventory.owners.append(
                            ClassRuntimeInventory::capture(start, bytes, references, bases));
                        const auto saved = inventory.encode();
                        const auto restored = ClassRuntimeInventory::decode(saved);
                        QCOMPARE(restored.encode(), saved);
                        auto moved = bases;
                        moved["classes"] += 0x8440;
                        moved["aliases"] += 0x8100;
                        moved["discovery"] += 0x8040;
                        auto nextSymbols = symbols;
                        nextSymbols["lc_class_records"] = moved["classes"] + 72;
                        nextSymbols["lc_named_class_map"] = moved["aliases"];
                        nextSymbols["lc_discovery_flags"] = moved["discovery"];
                        const auto recompiled =
                            compiler
                                .compileModule(input, work.path(), "moved_counts", code.address,
                                               {{"class_menu_config.h", layout.cHeader}},
                                               nextSymbols)
                                .fitOwner(size);
                        QCOMPARE(restored.relocate(start, bytes, bases, moved), recompiled);
                        QCOMPARE(restored.relocate(start, recompiled, moved, bases), bytes);
                        auto image = syntheticRom();
                        image.replace(start, size, bytes);
                        restored.verify(Rom::fromBytes(image), bases);
                        const auto tableBytes =
                            compiler.compileData(payload.bytes, work.path(), "expanded_tables");
                        const auto slot =
                            ExpandedPromotionResource::encode(promotion, tableBytes, restored);
                        const auto reopened = ExpandedPromotionResource::decode(slot);
                        QCOMPARE(reopened.data.bytes, tableBytes);
                        QCOMPARE(reopened.inventory.encode(), saved);
                        image.replace(ExpandedPromotionResource::romOffset, slot.size(), slot);
                        const auto ordinary = Resource::load(Rom::fromBytes(image),
                                                             ExpandedPromotionResource::romOffset,
                                                             ExpandedPromotionResource::slotSize);
                        QCOMPARE(ordinary.decoded,
                                 tableBytes); // Metadata isn't decoded into game data.
                        auto damagedSlot = slot;
                        damagedSlot[8] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        constexpr int footerAt = ExpandedPromotionResource::slotSize - 64;
                        const auto metadataAt = u32(slot, footerAt + 40);
                        damagedSlot = slot;
                        damagedSlot[metadataAt + 24] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        damagedSlot = slot;
                        damagedSlot[footerAt - 1] = 0;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        // Recompute footer CRC so this checks geometry, not only checksum
                        // rejection.
                        for (const auto pair :
                             {QPair<int, quint32>{40, 16}, {44, 0xfffffff0}, {24, 0}}) {
                            damagedSlot = slot;
                            put32(damagedSlot, footerAt + pair.first, pair.second);
                            put32(damagedSlot, footerAt + 60, crc32(damagedSlot.mid(footerAt, 60)));
                            QVERIFY_THROWS_EXCEPTION(
                                Error, ExpandedPromotionResource::decode(damagedSlot));
                        }
                        auto wrongInventory = restored;
                        wrongInventory.classCount = count == 256 ? 255 : 256;
                        QVERIFY_THROWS_EXCEPTION(Error, ExpandedPromotionResource::encode(
                                                            promotion, tableBytes, wrongInventory));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::encode(
                                                     promotion, tableBytes.first(100), restored));
                        ClassRuntimeInventory empty;
                        empty.classCount = count;
                        QVERIFY_THROWS_EXCEPTION(Error, empty.verify(Rom::fromBytes(image), bases));
                        auto corrupt = bytes;
                        corrupt[0] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 restored.relocate(start, corrupt, bases, moved));
                        corrupt = saved;
                        corrupt[corrupt.size() - 1] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error, ClassRuntimeInventory::decode(corrupt));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ClassRuntimeInventory::decode(saved.first(23)));
                        auto invalid = inventory;
                        invalid.owners.append(invalid.owners.first());
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        invalid = inventory;
                        invalid.owners[0].start += 4;
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        invalid = inventory;
                        invalid.owners[0].references[0].low = size;
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        moved["aliases"] = 0x80400000;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 restored.relocate(start, bytes, bases, moved));
                    }
                    const QSet<QString> discoveryReaders = {
                        "family-menu.c", "class-discovery.c", "equipment-class-menu.c",
                        "class-categories.c", "category-class-counts.c"};
                    if (discoveryReaders.contains(owner["cSource"].toString()))
                        QVERIFY(std::any_of(
                            code.addressReferences.begin(), code.addressReferences.end(),
                            [](const auto &ref) { return ref.symbol == "lc_discovery_flags"; }));
                }
            }
            if (count == 256) {
                QCOMPARE(layout.categoryClasses, quint32(0x2730));
                QCOMPARE(layout.currentCounts, quint32(0x2940));
                QCOMPARE(layout.eligibleCounts, quint32(0x2b50));
                QCOMPARE(layout.ownerBytes, quint32(0x2d60));
                auto mismatched = payload;
                --mismatched.classCount;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(classes, mismatched));
                mismatched = payload;
                ++mismatched.maskOffset;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(classes, mismatched));
                auto invalid = classes;
                invalid.discovery = 0x803ffff0;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                invalid = classes;
                invalid.aliases = classes.records + 16;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                invalid = classes;
                invalid.records |= 1;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                if (!tools.isEmpty()) {
                    Compiler compiler(tools);
                    compiler.authenticate();
                    QTemporaryDir work;
                    const auto configData = compiler.compile(
                        "#include \"class_menu_config.h\"\nunsigned int config[] = {"
                        "LC_CLASS_COUNT, LC_LIST_OFFSET, LC_CATEGORY_CLASSES_OFFSET, "
                        "LC_CATEGORY_CURRENT_OFFSET, LC_CATEGORY_ELIGIBLE_OFFSET, "
                        "LC_DISCOVERY_ADDRESS, LC_FAMILY_OFFSET, LC_MASK_RESOURCE_OFFSET, "
                        "LC_FLAGS_RESOURCE_OFFSET};\n",
                        work.path(), "menu_config", 0, ".data",
                        {{"class_menu_config.h", layout.cHeader}}, layout.dataSymbols);
                    QCOMPARE(configData.first(36),
                             QByteArray::fromHex("0000010000002528000027300000294000002b50802508000"
                                                 "000080000000c0000000d00"));
                }
            }
        }
    }
    void expandablePromotionData() {
        auto base = syntheticPromotion();
        const auto assignments = base.families["assignments"].toArray();
        auto legacy = base.json();
        legacy.remove("flags");
        auto legacyRestored = base;
        legacyRestored.apply(legacy);
        QCOMPARE(legacyRestored.flags, base.flags);
        auto model = base;
        QCOMPARE(model.appendClass(5), 165);
        QCOMPARE(model.classCount(), 166);
        QCOMPARE(model.gates.first(648), base.gates);
        QCOMPARE(model.masks.first(81), base.masks);
        QCOMPARE(model.flags.first(81), base.flags);
        QCOMPARE(model.gates.mid(81 * 8, 84 * 8), QByteArray(84 * 8, 0));
        QCOMPARE(model.masks.mid(81, 84), QByteArray(84, 0));
        QCOMPARE(model.flags.mid(81, 84), QByteArray(84, 0));
        for (int id = 81; id < 165; ++id)
            QCOMPARE(model.families["assignments"].toArray()[id].toInt(), 0);
        QCOMPARE(model.gates.mid(165 * 8, 8), base.gates.mid(5 * 8, 8));
        QCOMPARE(model.masks[165], base.masks[5]);
        QCOMPARE(model.flags[165], base.flags[5]);
        QCOMPARE(model.families["assignments"].toArray()[165], assignments[5]);
        const auto family = model.addFamily("New order", 1);
        model.assign(165, family);
        model.setGate(165, 0, 177);
        model.setMask(165, 0xa5);
        model.setEquipmentSetListed(165, true);
        QCOMPARE(u8(model.flags, 165), quint8(0xd0)); // Preserve unknown bits 4 and 6.
        const auto data = model.buildExpandedData();
        QCOMPARE(data.classCount, 166);
        QCOMPARE(data.familyOffset, 1328);
        QCOMPARE(data.maskOffset, 1992);
        QCOMPARE(data.flagsOffset, 2158);
        QCOMPARE(data.catalogOffset, 2324);
        QCOMPARE(u8(data.bytes, 1320), quint8(177));
        QCOMPARE(u32(data.bytes, 1328 + 165 * 4), quint32(0x10010));
        QCOMPARE(u8(data.bytes, 1992 + 165), quint8(0xa5));
        QCOMPARE(u8(data.bytes, 2158 + 165), quint8(0xd0));
        QCOMPARE(u32(data.bytes, 2324), quint32(0x4c43464d));
        QCOMPARE(u32(data.bytes, 2328), quint32(data.bytes.size() - 2332));
        QCOMPARE(QJsonDocument::fromJson(data.bytes.mid(2332)).object(), model.families);
        auto restored = base;
        restored.apply(model.json());
        QCOMPARE(restored.json(), model.json());
        restored = base;
        restored.applyExpandedData(data.bytes, 166);
        QCOMPARE(restored.json(), model.json());
        auto corrupt = data.bytes;
        corrupt[1328 + 165 * 4] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 166));
        corrupt = data.bytes;
        corrupt[2159] = 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 166));
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(data.bytes.first(2331), 166));
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(data.bytes, 167));
        QCOMPARE(restored.json(), model.json());
        auto padded = base;
        padded.growToClassCount(165);
        const auto paddedData = padded.buildExpandedData();
        QCOMPARE(paddedData.bytes.mid(2310, 2), QByteArray(2, 0));
        corrupt = paddedData.bytes;
        corrupt[2311] = 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 165));
        const auto before = model.json();
        QVERIFY_THROWS_EXCEPTION(Error, model.appendClass(166));
        QVERIFY_THROWS_EXCEPTION(Error, model.growToClassCount(164));
        QCOMPARE(model.json(), before);
        while (model.classCount() < 256)
            model.appendClass(165);
        const auto full = model.buildExpandedData();
        QCOMPARE(full.familyOffset, 2048);
        QCOMPARE(full.maskOffset, 3072);
        QCOMPARE(full.flagsOffset, 3328);
        QCOMPARE(full.catalogOffset, 3584);
        QCOMPARE(u8(full.bytes, 255 * 8), quint8(177));
        QCOMPARE(u32(full.bytes, 2048 + 255 * 4), quint32(0x10010));
        QCOMPARE(u8(full.bytes, 3072 + 255), quint8(0xa5));
        QCOMPARE(u8(full.bytes, 3328 + 255), quint8(0xd0));
        restored.applyExpandedData(full.bytes, 256);
        QCOMPARE(restored.json(), model.json());
        const auto beforeLinks = model.json();
        model.setFamilyLinks(family, {1, 2});
        const auto linked = model.buildExpandedData();
        QCOMPARE(u32(linked.bytes, linked.familyOffset + 165 * 4), quint32(0x10001));
        QCOMPARE(u32(linked.bytes, linked.familyOffset + 2 * 4), quint32(0x20001));
        QCOMPARE(linked.bytes.mid(linked.maskOffset, 256), model.masks);
        restored.applyExpandedData(linked.bytes, 256);
        QCOMPARE(restored.json(), model.json());
        QVERIFY_THROWS_EXCEPTION(Error, model.setFamilyLinks(family, {0}));
        model.setFamilyLinks(family, {});
        QCOMPARE(model.json(), beforeLinks);
        const auto atLimit = model.json();
        QVERIFY_THROWS_EXCEPTION(Error, model.appendClass(165));
        QCOMPARE(model.json(), atLimit);
        model.masks.chop(1);
        QVERIFY_THROWS_EXCEPTION(Error, model.buildExpandedData());
    }
    void promotionFamiliesExport() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            p.loadPromotion();
            auto model = *p.promotion();
            // Documented retail Knight requirements provide an independent framing check.
            QCOMPARE(u8(model.gates, 5 * 8), quint8(72));
            QCOMPARE(u8(model.gates, 5 * 8 + 1), quint8(67));
            QCOMPARE(u8(model.gates, 5 * 8 + 6), quint8(40));
            auto maskOnly = p;
            auto maskModel = model;
            maskModel.setMask(5, 0xa5);
            const auto knightFlags = u8(maskModel.flags, 5);
            maskModel.setEquipmentSetListed(5, !(knightFlags & 0x80));
            maskOnly.setPromotion(maskModel.json());
            QTemporaryDir maskWork;
            const auto maskResult =
                rebuild(maskOnly, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            QCOMPARE(Promotion::load(Rom::fromBytes(maskResult.image)).json(), maskModel.json());
            QCOMPARE(maskResult.image.mid(0xbbd80, 44), p.rom().bytes().mid(0xbbd80, 44));
            QCOMPARE(u8(maskResult.image, 0xdbd2c + 5 * 2),
                     quint8(u8(p.rom().bytes(), 0xdbd2c + 5 * 2) ^ 0x80));
            QCOMPARE(maskResult.image.mid(0x27d0000, 65536), p.rom().bytes().mid(0x27d0000, 65536));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bbd80.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bf5dc.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bc0bc.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bbdac.compiler.s")));
            QVERIFY(maskResult.image.mid(0xbf5dc, 332) != p.rom().bytes().mid(0xbf5dc, 332));
            // A compiled reader can precede relocation, or be rebuilt after it.
            auto namedMask = Project::fromRom(Rom::fromBytes(maskResult.image));
            namedMask.setClassNameBytes(5, "Steel Knight");
            const auto maskAndName =
                rebuild(namedMask, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            auto placedMask = Project::fromRom(Rom::fromBytes(maskAndName.image));
            placedMask.loadPromotion();
            auto nextMasks = *placedMask.promotion();
            nextMasks.setMask(5, 3);
            placedMask.setPromotion(nextMasks.json());
            const auto updatedMask =
                rebuild(placedMask, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            auto maskReopened = Project::fromRom(Rom::fromBytes(updatedMask.image));
            maskReopened.loadPromotion();
            QCOMPARE(maskReopened.promotion()->masks, nextMasks.masks);
            QCOMPARE(maskReopened.classNameBytes(5), QByteArray("Steel Knight"));
            auto family = model.addFamily("Order of Dawn", 1);
            QCOMPARE(family, 16);
            model.assign(2, family);
            model.assign(5, family);
            model.setFamilyLinks(family, {1});
            QCOMPARE(u32(model.decodedFamilies(), 648 + 5 * 4), quint32(0x10001));
            QCOMPARE(u32(model.decodedFamilies(), 648 + 6 * 4), quint32(0x10001));
            model.setGate(5, 0, 90);
            model.setMask(5, 0xa5);
            model.setEquipmentSetListed(5, !(u8(model.flags, 5) & 0x80));
            QVERIFY_THROWS_EXCEPTION(Error, model.addFamily("order of dawn", 1));
            QVERIFY_THROWS_EXCEPTION(Error, model.assign(0, 16));
            QVERIFY_THROWS_EXCEPTION(Error, model.assign(5, 65534));
            p.setPromotion(model.json());
            QTemporaryDir projectDir;
            p.saveFolder(projectDir.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(projectDir.path()));
            QCOMPARE(restored.promotion()->json(), model.json());
            QTemporaryDir build;
            auto result = rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-promotion-XXXXXX");
            QVERIFY(candidate.isValid());
            auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            reopened.loadPromotion();
            QCOMPARE(reopened.promotion()->json(), model.json());
            QVERIFY(reopened.promotion()->expanded);
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QTemporaryDir repeatBuild;
            auto repeat =
                rebuild(reopened, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), repeatBuild.path());
            QCOMPARE(repeat.image, result.image);
            auto occupied = p.rom().bytes();
            occupied[0x27d0000] = 0;
            auto conflict = Project::fromRom(Rom::fromBytes(occupied));
            conflict.setPromotion(model.json());
            QVERIFY_THROWS_EXCEPTION(Error,
                                     rebuild(conflict, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),
                                             repeatBuild.path()));
            QVERIFY(QFileInfo::exists(build.filePath("class_families.compiler.s")));
            QVERIFY(QFileInfo::exists(build.filePath("func_000bf248.compiler.s")));
            const auto familyRom = Rom::fromBytes(result.image);
            const auto classes = ClassData::load(familyRom);
            Compiler compiler(qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"));
            compiler.authenticate();
            const auto runtime = buildClassRuntime(classes, compiler, repeatBuild.path(),
                                                   quint32(result.image.size()));
            const auto combined = linkClassRuntime(familyRom, runtime);
            QCOMPARE(Promotion::load(Rom::fromBytes(combined)).json(), model.json());
            auto altered = combined;
            // This is a relocated class operand in the shared eligibility owner.
            put32(altered, 0xc3998, u32(altered, 0xc3998) ^ 1);
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(altered)));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void relocatedPromotionMigration() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM not supplied");
            const auto retail = Rom::open(path);
            auto image = retail.bytes();
            auto gates = Promotion::load(retail).gates;
            quint32 seed = 0x2468ace1;
            for (int i = 8; i < gates.size(); ++i) {
                seed = seed * 1664525u + 1013904223u;
                gates[i] = char(seed >> 24);
            }
            gates[40] = char(123); // Independently chosen Knight STR requirement.
            const auto stream = encodeLzss(gates);
            QVERIFY(stream.size() > 376);
            QVERIFY(stream.size() <= 659);
            const auto stored = (stream.size() + 1) & ~qsizetype(1);
            QByteArray slot(1024, char(-1)), descriptor(64, 0);
            put32(slot, 0, stored + 4);
            put32(slot, 4, 648);
            slot.replace(8, stored, QByteArray(stored, 3));
            slot.replace(8, stream.size(), stream);
            put32(descriptor, 0, 0x4f425347);
            put32(descriptor, 4, 0x02000100);
            put32(descriptor, 8, stream.size());
            put32(descriptor, 12, stored);
            put32(descriptor, 16, 648);
            put32(descriptor, 20, crc32(gates));
            put32(descriptor, 28, crc32(descriptor.first(28)));
            slot.replace(960, 64, descriptor);
            image.replace(0x27b0800, 1024, slot);
            int firstKeySite = -1;
            for (auto v : promotionOwners()["owners"].toArray()) {
                const auto owner = v.toObject();
                if (!owner.contains("keySite"))
                    continue;
                const auto site = owner["keySite"].toInt();
                if (firstKeySite < 0)
                    firstKeySite = site;
                // Existing OBSG-v2 loader contract: ROM entry minus 0x594280.
                const quint32 key = 0x27b0800 - 0x594280;
                put32(image, site, 0x3c040000 | (key >> 16));
                put32(image, site + 4, 0x34840000 | (key & 65535));
            }
            auto p = Project::fromRom(Rom::fromBytes(image));
            p.loadPromotion();
            QVERIFY(p.promotion()->directGates);
            QCOMPARE(p.promotion()->gates, gates);
            QCOMPARE(u8(p.promotion()->gates, 40), quint8(123));
            QTemporaryDir work;
            // Merely opening/rebuilding preserves the user's existing export.
            QCOMPARE(rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), work.path()).image,
                     image);
            auto model = *p.promotion();
            model.setGate(5, 0, 124);
            p.setPromotion(model.json());
            const auto result =
                rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), work.path());
            QCOMPARE(result.image.mid(0x27b0800, 1024), QByteArray(1024, char(-1)));
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-promotion-migration-XXXXXX");
            QVERIFY(candidate.isValid());
            const auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Promotion::load(Rom::open(output));
            QVERIFY(reopened.expanded);
            QVERIFY(!reopened.directGates);
            QCOMPARE(reopened.json(), model.json());
            QCOMPARE(u8(reopened.gates, 40), quint8(124));
            auto corrupt = image;
            corrupt[0x27b0800 + 960 + 20] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt.replace(firstKeySite, 8, retail.bytes().mid(firstKeySite, 8));
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt.replace(0x27b0800, 1024, QByteArray(1024, char(-1)));
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt[0x27b0800 + 960 + 4] = 1;
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            QCOMPARE(Rom::open(path).bytes(), retail.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "engine_tests.moc"
