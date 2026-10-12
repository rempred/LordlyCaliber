#include "engine/project.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
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
