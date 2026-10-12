#include "engine/project.h"
#include "engine/project_references.h"
#include "engine/actor_art.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
using namespace lordly;
class ChangeReportTests : public QObject {
    Q_OBJECT
    QJsonObject catalog() {
        QFile file(":/catalog.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing test catalog");
        return QJsonDocument::fromJson(file.readAll()).object();
    }
    static int field(Table table, const QString &key) {
        for (int i = 0; i < fields(table).size(); ++i)
            if (fields(table)[i].key == key)
                return i;
        throw Error("Missing test field");
    }
  private slots:
    void referenceRomComparison() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto retail=Rom::open(path); auto clean=Project::fromRom(retail,catalog());
            QVERIFY(clean.compareWithRom(retail).entries.isEmpty());
            auto inherited=retail.bytes();
            put16(inherited,clean.record(Table::Items,1).romOffset+8,4321);
            put8(inherited,0x239cd2,quint8(-3));
            put8(inherited,Training::tableOffset+6*24+2,0);
            Rom::updateChecksum(inherited);
            auto project=Project::fromRom(Rom::fromBytes(inherited,"edited.z64"),catalog());
            QVERIFY(project.changeReport().entries.isEmpty());
            project.setValue(Table::Items,1,field(Table::Items,"str"),-7);
            const auto before=project.manifest(); const auto report=project.compareWithRom(retail);
            bool item=false,tactic=false,training=false;
            for(const auto &row:report.entries) {
                if(row.location.editor=="items" && row.location.id==1) {
                    item=true; QVERIFY(row.details.join(" ").contains("4321")); QVERIFY(row.details.join(" ").contains("-7"));
                }
                tactic |= row.location.editor=="rule" && row.location.key=="tacticLeader" && row.details.join(" ").contains("-3");
                training |= row.location.editor=="training" && row.location.id==7 && row.location.child==0;
            }
            QVERIFY(item);QVERIFY(tactic);QVERIFY(training);QCOMPARE(project.manifest(),before);
            QCOMPARE(report.entries.size(),3); // Relocation, unedited art and data must not become phantom changes.
        } catch(const std::exception &e) {QFAIL(e.what());}
    }
    void trainingClassSizeValidation() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadTraining();
        auto classes=project.classData(); project.training()->validate(classes,int(project.records(Table::Items).size()));
        for(int id=1;id<classes.entries().size();++id) {
            auto entry=classes.entries()[id];put8(entry.record,ClassData::unitSizeOffset,2);classes.set(id,entry);
        }
        QVERIFY_THROWS_EXCEPTION(Error,project.training()->validate(classes,int(project.records(Table::Items).size())));
    }
    void referenceInspection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path), catalog());
            const auto before = project.manifest();
            const auto stockIssues = exportIssues(project);
            QVERIFY(stockIssues.isEmpty());
            for (const auto &issue : stockIssues) QVERIFY2(!issue.blocking, qPrintable(issue.message));
            QCOMPARE(project.manifest(), before);
            project.setValue(Table::Classes, 2, field(Table::Classes, "equipment0"), 277);
            project.loadShops();
            auto shops = *project.shops(); shops.setStock(35, {{277}, {1}});
            project.setShops(shops.json());
            const auto uses = findProjectUses(project, "items", 277);
            bool fighter = false, shop = false;
            for (const auto &use : uses) {
                fighter |= use.location.editor == "classes" && use.location.id == 2 && use.details.join(" ").contains("277");
                shop |= use.location.editor == "shops" && use.location.id == 35;
            }
            QVERIFY(fighter); QVERIFY(shop);
            project.setValue(Table::Classes, 2, field(Table::Classes, "equipment0"), 999);
            const auto issues = exportIssues(project);
            bool linked = false;
            for (const auto &issue : issues)
                linked |= issue.blocking && issue.location.editor == "classes" && issue.location.id == 2 && issue.message.contains("999");
            QVERIFY(linked);
            for (const auto &use : findProjectUses(project, "items", 277))
                QVERIFY(!(use.location.editor == "classes" && use.location.id == 2 && use.subject.endsWith("Weapon")));
            project.setValue(Table::Classes, 2, field(Table::Classes, "equipment0"), 277);
            for (const auto &issue : exportIssues(project)) QVERIFY2(!issue.blocking, qPrintable(issue.message));
            project.loadCombatArtwork();
            const auto art = ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            QString binding;
            for (const auto &candidate : project.combatArtwork()->bindings())
                if (candidate.descriptorKey == art.descriptorKey()) { binding = candidate.id; break; }
            QVERIFY(!binding.isEmpty());
            bool spriteUse = false;
            for (const auto &use : findProjectUses(project, "combatSprite", -1, binding))
                spriteUse |= use.location.editor == "combatAnimation" && use.location.id == 2;
            QVERIFY(spriteUse);
            const int added=project.appendClass(2,"Army reference copy");project.loadArmyArtwork();
            const auto classes=project.classData();const int atlas=ArmyArtwork::classLane(classes.entries()[2])*2;
            const int model=ArmyArtwork::classModel(classes.entries()[2]);
            auto usesArmy=[&](int model,int cls) {
                const auto uses=findProjectUses(project,"armySprite",atlas,QString::number(model));
                return std::any_of(uses.begin(),uses.end(),[&](const auto &use){return use.location.id==cls;});
            };
            QVERIFY(usesArmy(model,2));QVERIFY(usesArmy(model,added));
            auto army=*project.armyArtwork();const int copy=army.isolateClass(classes,added);QVERIFY(copy!=model);
            project.setArmyArtwork(army.json());QVERIFY(!usesArmy(model,added));QVERIFY(usesArmy(copy,added));
            QTemporaryDir dir; project.saveFolder(dir.filePath("project"));
            auto reopened = Project::fromRom(project.rom(), catalog());
            reopened.applyManifest(Project::readProject(dir.filePath("project")));
            QCOMPARE(findProjectUses(reopened, "items", 277).size(), findProjectUses(project, "items", 277).size());
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void recordsAndPersistence() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom, catalog());
            QVERIFY(project.changeReport().entries.isEmpty());
            const auto original = project.record(Table::Items, 1).bytes;
            project.setValue(Table::Items, 1, field(Table::Items, "str"), -7);
            project.setValue(Table::Items, 1, field(Table::Items, "growthInt"), 3);
            project.setValue(Table::Items, 1, field(Table::Items, "price"), 4321);
            const auto changes = project.changeReport();
            QCOMPARE(changes.entries.first().location.editor, QString("items"));
            QCOMPARE(changes.entries.first().location.id, 1);
            const auto text = changes.text();
            QVERIFY(text.contains(QString("Stats / STR: %1 → -7").arg(qint8(original[10]))));
            QVERIFY(text.contains(
                QString("Growth / INT: %1 → 3").arg((quint8(original[24]) >> 2) & 3)));
            QVERIFY(text.contains(QString("Identity / Price: %1 → 4321").arg(u16(original, 8))));
            QVERIFY(!text.contains("HP and STR:"));
            QVERIFY(!text.contains(QFileInfo(path).absolutePath()));
            project.replaceBytes(Table::Items, 1, original);
            QVERIFY(project.changeReport().entries.isEmpty());
            project.loadTexts();
            QVERIFY(project.changeReport().entries.isEmpty());
            project.setClassNameBytes(2, encodeClassName("New Fighter"));
            project.setText("classes", 2, "A changed fighter.");
            const auto id = project.appendClass(2, "New Class");
            project.loadPromotion();
            auto promotion = *project.promotion();
            promotion.setGate(2, 0, 123);
            const auto family = promotion.addFamily("Report Family", 1);
            promotion.assign(id, family);
            project.setPromotion(promotion.json());
            project.loadShops();
            auto shops = *project.shops();
            shops.setStock(35, {{1, 277}, {1, 44}});
            project.setShops(shops.json());
            project.loadIconArtwork();
            auto icons = *project.iconArtwork();
            auto words = icons.words(0x201);
            put16(words, 0, quint16(u16(words, 0) ^ 1));
            icons.setWords(0x201, words);
            project.setIconArtwork(icons.json());
            auto library = project.sprites();
            library.replace(SpriteAsset::blank("Report sprite", {16, 16}));
            project.setSprites(library);
            const auto before = project.manifest();
            const auto report = project.changeReport().text();
            QCOMPARE(project.manifest(), before);
            QVERIFY(report.contains("Name: \"Fighter\" → \"New Fighter\""));
            QVERIFY(report.contains("A changed fighter."));
            QVERIFY(report.contains("Added class; initial field values follow."));
            QVERIFY(report.contains("Report Family"));
            QVERIFY(report.contains("Heal Leaf (1)"));
            QVERIFY(report.contains("Item icons\n==========\nHeal Leaf (1)"));
            QVERIFY(!report.contains(QString::fromLatin1(words.toBase64())));
            QVERIFY(report.contains("Report sprite"));
            QTemporaryDir dir;
            project.saveFolder(dir.filePath("project"));
            project.saveArchive(dir.filePath("project.lcproject"));
            for (const auto &location :
                 {dir.filePath("project"), dir.filePath("project.lcproject")}) {
                auto reopened = Project::fromRom(rom, catalog());
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.changeReport().text(), report);
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void otherEditingAreas() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path), catalog());
            project.loadTools();
            project.loadEncounters();
            project.loadScenarios();
            project.loadWorld();
            project.loadScenePrograms();
            project.loadSceneEvents();
            project.loadDialogues();
            project.loadAnimationRoutes();
            project.loadAnimationOverrides();
            project.loadAnimationSequences();
            project.loadArmyArtwork();
            project.loadCombatArtwork();
            project.loadAvatarArtwork();
            project.loadConsumableEffects();
            QVERIFY(project.changeReport().entries.isEmpty());
            auto tools = *project.tools();
            tools.setExperiencePercent(137);
            project.setTools(tools.json());
            auto encounters = *project.encounters();
            encounters.setTerrain(1, 43, encounters.lookup(1));
            encounters.setPair(45, 9, 164, 2, int(project.records(Table::Classes).size()));
            project.setEncounters(encounters.json());
            auto scenarios = *project.scenarios();
            scenarios.setBaseLevel(10, 12, project.classData());
            project.setScenarios(scenarios.json());
            auto world = *project.world();
            const int town = u16(world.scenario(52).sites, 1);
            auto townBytes = world.town(town);
            put16(townBytes, 22, 4321);
            world.setTown(town, townBytes);
            project.setWorld(world.json());
            auto scenes = *project.scenePrograms();
            const auto authored =
                scenes.createScene("Report scene", scenes.scenes().first(), false);
            project.setScenePrograms(scenes.json());
            auto dialogue = *project.dialogues();
            dialogue.setEntry(Dialogues::Table::Primary, 0, 0, "Report dialogue@a");
            project.setDialogues(dialogue.json());
            auto routes = *project.animationRoutes();
            auto row = routes.entry(2);
            row.speeds[0] = row.speeds[0] == 7 ? 8 : 7;
            routes.setEntry(2, row);
            project.setAnimationRoutes(routes.json());
            const auto before = project.manifest();
            const auto report = project.changeReport().text();
            QCOMPARE(project.manifest(), before);
            for (const auto &area :
                 {"Optional tools", "Neutral encounters", "Scenarios", "Sites and treasures",
                  "Cutscenes", "Dialogue", "Animation assignments"})
                QVERIFY2(report.contains(area), area);
            QVERIFY(report.contains("Experience Scale Percent: 100 → 137"));
            QVERIFY(report.contains("Report scene"));
            bool encounterLinked = false;
            for (const auto &change : project.changeReport().entries)
                if (change.location.editor == "encounters" && change.location.id == 45) {
                    encounterLinked = true;
                    QVERIFY(change.details.join(" ").contains("Slot 10 / member 1"));
                    QVERIFY(change.details.join(" ").contains("164"));
                }
            QVERIFY(encounterLinked);
            QVERIFY(report.contains("Primary archive 0 / entry 0"));
            scenes.removeScene(authored);
            project.setScenePrograms(scenes.json());
            QVERIFY(!project.changeReport().text().contains("Report scene"));
            QTemporaryDir dir;
            project.saveFolder(dir.filePath("project"));
            auto reopened = Project::fromRom(project.rom(), catalog());
            reopened.applyManifest(Project::readProject(dir.filePath("project")));
            QCOMPARE(reopened.changeReport().text(), project.changeReport().text());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(ChangeReportTests)
#include "change_report_tests.moc"
