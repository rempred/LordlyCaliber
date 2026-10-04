#include "engine/project.h"
#include "engine/promotion.h"
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
