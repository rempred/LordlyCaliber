#include "engine/project.h"
#include "engine/promotion.h"
#include "engine/event_script.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class EventTests : public QObject {
    Q_OBJECT
  private slots:
    void flagInstructionControls() {
        // Six handler groups share this small script. The check branches to the
        // second return. Its destination byte must never change during flag edits.
        const auto bytes = QByteArray::fromHex("000c000c000c000c000c000c0010ffff68666901716069666061ac012f002f00");
        const auto controls = EventScript::flagSteps(bytes);
        QCOMPARE(controls.size(), 2);
        QCOMPARE(controls[16].flag, 102); QVERIFY(!controls[16].check); QVERIFY(controls[16].value);
        QVERIFY(controls[22].check); QCOMPARE(controls[22].target, 30);
        auto edited = EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 250}, {"value", false}}},
                                                 {"22", QJsonObject{{"flag", 250}, {"value", true}}}});
        auto expected = bytes; expected[17] = char(250); expected[19] = 0; expected[23] = char(250); expected[26] = char(0xbc);
        QCOMPARE(edited, expected);
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"18", QJsonObject{{"flag", 1}, {"value", true}}}}));
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 256}, {"value", true}}}}));
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 1}, {"value", 1}}}}));
        // An alternate handler can enter the middle of a would-be compound.
        // Such words must not be offered as one editable operation.
        auto shared = QByteArray::fromHex("000c000c000c000c000c000c00120014ffff6866690171602f00");
        QVERIFY(EventScript::flagSteps(shared).isEmpty());
    }
    void scenarioFlagsProjectAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and N64 tools required");
        try {
            const auto rom = Rom::open(path);
            int decoded = 0;
            for (int row = 4; row <= 109; ++row) {
                if (row >= 65 && row < 71) continue;
                QVERIFY(!EventScript::decode(SceneEvents::retailRow(rom, row)).isEmpty()); ++decoded;
            }
            QCOMPARE(decoded, 100);
            auto project = Project::fromRom(rom); project.loadSceneEvents();
            auto events = *project.sceneEvents();
            QCOMPARE(events.scenarioRows(1), QVector<int>({4, 71}));
            const auto initial = events.json();
            QVERIFY_THROWS_EXCEPTION(Error, events.editFlag(4, 0x2f0, 256, true));
            QCOMPARE(events.json(), initial);
            const auto original = SceneEvents::retailRow(rom, 4);
            const auto controls = EventScript::flagSteps(original);
            QCOMPARE(controls[0x2f0].flag, 102); QVERIFY(controls[0x2f0].value);
            events.editFlag(4, 0x2f0, 250, false);
            auto expected = original; expected[0x2f1] = char(250); expected[0x2f3] = 0;
            QCOMPARE(events.program(4), expected);
            auto restored = events; restored.restoreFlags(4); restored.validate(*project.scenePrograms());
            QCOMPARE(restored.json(), project.sceneEvents()->json());
            QVERIFY(!restored.modified()); QCOMPARE(restored.changedSites(), 0);
            // Compose a short-form launch requiring an appended stub with a flag edit.
            QString launch;
            for (const auto &site : SceneEvents::sites()) if (site.row == 4 && site.shortForm) { launch = site.id; break; }
            QVERIFY(!launch.isEmpty());
            auto scenes = *project.scenePrograms();
            const auto added = scenes.createScene("Flag integration scene", scenes.scenes().first(), false);
            const int selector = scenes.resource(added).selectors.first(); QVERIFY(selector > 255);
            project.setScenePrograms(scenes.json()); events.bind(launch, selector, scenes);
            project.setSceneEvents(events.json());
            QVERIFY(project.changeReport().text().contains("Tenne Plains completed"));
            QVERIFY(project.changeReport().text().contains("Flag 250"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("flags-project"));
            auto saved = Project::fromRom(rom); saved.applyManifest(Project::readProject(work.filePath("flags-project")));
            QCOMPARE(saved.sceneEvents()->json(), events.json());
            auto forged = events.json(); auto flags = forged["flags"].toObject();
            flags["4"] = QJsonObject{{"0", QJsonObject{{"flag", 1}, {"value", true}}}}; forged["flags"] = flags;
            QVERIFY_THROWS_EXCEPTION(Error, saved.setSceneEvents(forged));
            QCOMPARE(saved.sceneEvents()->json(), events.json());
            const auto built = rebuild(saved, tools, work.path());
            auto crc = built.image; Rom::updateChecksum(crc); QCOMPARE(crc, built.image);
            const auto exported = Rom::fromBytes(built.image);
            const auto placement = *SceneProgramResources::placement(exported);
            QCOMPARE(u32(built.image, SceneProgramResources::descriptorOffset + 8), quint32(5));
            QCOMPARE(placement.eventRows, QSet<int>{4});
            QCOMPARE(SceneEvents::readRow(exported, 4), events.program(4));
            QCOMPARE(SceneEvents::load(exported).json(), events.json());
            QCOMPARE(SceneEvents::retailRow(exported, 4), original);
            auto reopened = Project::fromRom(exported); reopened.loadSceneEvents();
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            auto restoredExport = *reopened.sceneEvents(); restoredExport.editFlag(4, 0x2f0, 102, true);
            restoredExport.restoreFlags(4); restoredExport.validate(*reopened.scenePrograms());
            QCOMPARE(restoredExport.json(), reopened.sceneEvents()->json());
            // Remove only the flag edit: preserve the other scene work and downgrade metadata.
            auto cleared = *reopened.sceneEvents(); cleared.editFlag(4, 0x2f0, 102, true);
            reopened.setSceneEvents(cleared.json());
            const auto cleanFlags = rebuild(reopened, tools, work.path());
            QCOMPARE(u32(cleanFlags.image, SceneProgramResources::descriptorOffset + 8), quint32(4));
            QCOMPARE(SceneEvents::load(Rom::fromBytes(cleanFlags.image)).target(launch), selector);
            QVERIFY(SceneEvents::load(Rom::fromBytes(cleanFlags.image)).flagEdits().isEmpty());
            QDir parent(QFileInfo(path).absolutePath()); QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scenario-event-flags-XXXXXX"));
            QVERIFY(output.isValid()); output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(saved, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).json(), events.json());
            qInfo().noquote() << "Scenario event export:" << candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void launchRewriteCompatibility() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-event-rows.cjs"), path,
                                     work.filePath("rows.json")});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile input(work.filePath("rows.json"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto rows = QJsonDocument::fromJson(input.readAll()).object()["rows"].toArray();
            QCOMPARE(SceneEvents::sites().size(), 1998);
            int shortCount = 0, checked = 0;
            for (const auto value : rows) {
                const auto row = value.toObject();
                const auto index = row["row"].toInt();
                const auto retail = SceneEvents::retailRow(rom, index);
                QMap<QString, int> high, low;
                for (const auto &site : SceneEvents::sites())
                    if (site.row == index) {
                        high[site.id] = 319;
                        low[site.id] = 2;
                        shortCount += site.shortForm;
                        ++checked;
                    }
                QCOMPARE(SceneEvents::bindRow(retail, index, high),
                         QByteArray::fromHex(row["high"].toString().toLatin1()));
                const auto direct = SceneEvents::bindRow(retail, index, low);
                QCOMPARE(direct.size(), retail.size());
                auto changedLow = low;
                for (auto it = changedLow.begin(); it != changedLow.end();) {
                    if (SceneEvents::site(it.key()).selector == 2)
                        it = changedLow.erase(it);
                    else
                        ++it;
                }
                QCOMPARE(SceneEvents::discoverRow(retail, direct, index), changedLow);
                QCOMPARE(SceneEvents::discoverRow(
                             retail, QByteArray::fromHex(row["low"].toString().toLatin1()), index),
                         changedLow);
                auto damaged = direct;
                damaged[0] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, SceneEvents::discoverRow(retail, damaged, index));
            }
            QCOMPARE(checked, 1998);
            QCOMPARE(shortCount, 402);
            QVERIFY(!SceneEvents::load(rom).modified());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void exportedSceneRemoval() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            const auto rom = Rom::open(path);
            auto scenes = ScenePrograms::load(rom);
            const auto source = scenes.scenes().first();
            const auto caller = scenes.createScene("Caller", source, false);
            const auto target = scenes.createScene("Target", source, false);
            const auto targetSelector = scenes.resource(target).selectors.first();
            const auto callerSelector = scenes.resource(caller).selectors.first();
            QByteArray transfer(16, 0);
            put32(transfer, 0, 0x80000003);
            put32(transfer, 4, targetSelector);
            put32(transfer, 8, 0x80000001);
            put32(transfer, 12,
                  u32(scenes.program(caller).source, scenes.program(caller).source.size() - 4));
            scenes.replace(caller, transfer);
            const QString launch = "event-director:1:b0032";
            auto project = Project::fromRom(rom);
            project.setScenePrograms(scenes.json());
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            events.bind(launch, targetSelector, scenes);
            project.setSceneEvents(events.json());
            QTemporaryDir work;
            const auto built = rebuild(project, tools, work.path());
            const QByteArray foreign(64, 'Y');
            const auto loadedRom = Rom::fromBytes(built.image + foreign);
            auto loaded = Project::fromRom(loadedRom);
            loaded.loadSceneEvents();
            auto next = *loaded.scenePrograms();
            const auto before = next.json();
            QVERIFY_THROWS_EXCEPTION(Error, next.removeScene(target));
            QCOMPARE(next.json(), before);
            auto unsafe = next.authoredPlan();
            unsafe.erase(
                std::remove_if(unsafe.begin(), unsafe.end(),
                               [&](const auto &row) { return row.id == target.authoredId; }),
                unsafe.end());
            QVERIFY_THROWS_EXCEPTION(Error, SceneProgramResources::rebuild(loadedRom, {}, unsafe));
            auto forged = before;
            auto rows = forged["authored"].toArray();
            for (int i = rows.size() - 1; i >= 0; --i)
                if (rows[i].toObject()["id"] == target.authoredId)
                    rows.removeAt(i);
            forged["authored"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, next.apply(forged));
            next.removeCommand(caller, 0);
            loaded.setScenePrograms(next.json());
            next.removeScene(target);
            QVERIFY_THROWS_EXCEPTION(Error, loaded.setScenePrograms(next.json()));
            auto restoredEvents = *loaded.sceneEvents();
            restoredEvents.restore(launch);
            loaded.setSceneEvents(restoredEvents.json());
            loaded.setScenePrograms(next.json());
            QVERIFY(!loaded.scenePrograms()->scenes().contains(target));
            QVERIFY_THROWS_EXCEPTION(Error, loaded.scenePrograms()->program(target));
            loaded.saveFolder(work.filePath("deleted-project"));
            auto saved = Project::fromRom(loadedRom);
            saved.loadSceneEvents();
            saved.applyManifest(Project::readProject(work.filePath("deleted-project")));
            QCOMPARE(saved.scenePrograms()->json(), loaded.scenePrograms()->json());
            QCOMPARE(saved.sceneEvents()->json(), loaded.sceneEvents()->json());
            const auto removed = rebuild(saved, tools, work.path());
            QCOMPARE(removed.image.mid(built.image.size(), foreign.size()), foreign);
            QCOMPARE(u32(removed.image, 0x1f3ca88 + targetSelector * 4), quint32(0));
            const auto reopened = ScenePrograms::load(Rom::fromBytes(removed.image));
            QVERIFY(reopened.availableSelectors().contains(targetSelector));
            QVERIFY(!reopened.authoredScenes().contains(target.authoredId));
            QCOMPARE(reopened.program(caller).source, next.program(caller).source);
            next.removeScene(caller);
            saved.setScenePrograms(next.json());
            const auto empty = rebuild(saved, tools, work.path());
            QVERIFY(!SceneProgramResources::placement(Rom::fromBytes(empty.image)));
            QCOMPARE(u32(empty.image, 0x1f3ca88 + callerSelector * 4), quint32(0));
            QCOMPARE(empty.image.last(foreign.size()), foreign);
            auto tail = Project::fromRom(Rom::fromBytes(built.image));
            tail.loadSceneEvents();
            auto tailEvents = *tail.sceneEvents();
            tailEvents.restore(launch);
            tail.setSceneEvents(tailEvents.json());
            auto tailScenes = *tail.scenePrograms();
            tailScenes.removeScene(caller);
            tailScenes.removeScene(target);
            tail.setScenePrograms(tailScenes.json());
            const auto pristine = rebuild(tail, tools, work.path());
            QCOMPARE(pristine.image, rom.bytes());
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scene-delete-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(saved, empty, candidate);
            QVERIFY(ScenePrograms::load(Rom::open(candidate)).authoredScenes().isEmpty());
            qInfo().noquote() << "Scene deletion export candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void bindingProjectAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadSceneEvents();
            auto scenes = *project.scenePrograms();
            const auto source = scenes.original().entries().first();
            const auto created = scenes.createScene("Event-bound scene", source.key, false);
            project.setScenePrograms(scenes.json());
            auto events = *project.sceneEvents();
            const auto selector = scenes.resource(created).selectors.first();
            const QString shortId = "event-director:1:b0032", longId = "event-director:1:b0862";
            events.bind(shortId, selector, scenes);
            events.bind(longId, selector, scenes);
            QVERIFY_THROWS_EXCEPTION(Error, events.bind(shortId, 65535, scenes));
            QCOMPARE(events.changedSites(), 2);
            project.setSceneEvents(events.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.sceneEvents()->json(), events.json());
            const auto built = rebuild(project, tools, work.path());
            auto checkedImage = built.image;
            Rom::updateChecksum(checkedImage);
            QCOMPARE(checkedImage, built.image);
            auto exportedRom = Rom::fromBytes(built.image);
            const auto allocation = *SceneProgramResources::placement(exportedRom);
            QCOMPARE(allocation.eventRows, QSet<int>{1});
            QCOMPARE(allocation.authored.size(), 1);
            QCOMPARE(SceneEvents::load(exportedRom).json(), events.json());
            QCOMPARE(SceneEvents::readRow(exportedRom, 1), events.plan().rows[1]);
            QCOMPARE(ScenePrograms::load(exportedRom).program(created).source,
                     scenes.program(created).source);
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/event-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).json(), events.json());
            qInfo().noquote() << "Event export candidate:" << candidate;
            const QByteArray foreign(64, 'X');
            auto withForeign = built.image + foreign;
            auto again = Project::fromRom(Rom::fromBytes(withForeign));
            again.loadSceneEvents();
            auto unbound = *again.sceneEvents();
            unbound.restore(shortId);
            unbound.restore(longId);
            again.setSceneEvents(unbound.json());
            const auto rebuilt = rebuild(again, tools, work.path());
            QCOMPARE(rebuilt.image.mid(built.image.size(), foreign.size()), foreign);
            QCOMPARE(u32(rebuilt.image, SceneEvents::directory + 8), SceneEvents::retailKey(1));
            const auto clean = Rom::fromBytes(rebuilt.image);
            QVERIFY(SceneEvents::load(clean).bindings().isEmpty());
            QCOMPARE(ScenePrograms::load(clean).program(created).source,
                     scenes.program(created).source);
            // A binding-only allocation can be removed entirely and safely truncate its own tail.
            auto only = SceneEvents::load(rom);
            only.bind(shortId, 2, *project.scenePrograms());
            auto plan = only.plan();
            const auto bound = Rom::fromBytes(SceneProgramResources::rebuild(rom, {}, {}, &plan));
            SceneEventBuild empty;
            const auto original = SceneProgramResources::rebuild(bound, {}, {}, &empty);
            QCOMPARE(original, rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(EventTests)
#include "event_tests.moc"
