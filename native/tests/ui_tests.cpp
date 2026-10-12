#include "app/actor_pose_editor.h"
#include "app/change_report_dialog.h"
#include "app/animation_editor.h"
#include "app/animation_override_editor.h"
#include "app/army_editor.h"
#include "app/art_editor.h"
#include "app/combat_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/dialogue_inspector.h"
#include "app/dialogue_preview.h"
#include "app/encounter_editor.h"
#include "app/terrain_assignments.h"
#include "app/main_window.h"
#include "app/theme.h"
#include "app/formation_board.h"
#include "app/portrait_editor.h"
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/save_editor.h"
#include "app/save_army_drag.h"
#include "app/scenario_editor.h"
#include "app/scenario_map_editor.h"
#include "engine/squad_members.h"
#include "app/scene_command_editor.h"
#include "app/scene_action_editor.h"
#include "app/scene_event_editor.h"
#include "app/scene_preview.h"
#include "app/shop_editor.h"
#include "app/sprite_editor.h"
#include "app/rom_art_import.h"
#include "app/testing_editor.h"
#include "engine/project64_session.h"
#include "app/text_workspace.h"
#include "app/tools_editor.h"
#include "app/transform_preview.h"
#include "app/value_editor.h"
#include "engine/scene_import.h"
#include "engine/scene_timed_authoring.h"
#include "engine/sprite_sources.h"
#include "game/classes/save_format.h"
#include "rebuild/compiler.h"
#include <QAction>
#include <cmath>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QScopeGuard>
#include <QScrollBar>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QTableView>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QUndoGroup>
#include <QWheelEvent>
#include <QtTest>
#include <bit>
#include "app/category_icon_editor.h"
using namespace lordly;
static bool selectMapObject(QWidget &editor, MapObject kind, int row) {
    auto objects = editor.findChild<QListWidget *>("mapObjects");
    for (int i = 0; objects && i < objects->count(); ++i) {
        auto item = objects->item(i);
        if (item->data(Qt::UserRole).toInt() == int(kind) &&
            item->data(Qt::UserRole + 1).toInt() == row) {
            objects->setCurrentItem(item);
            return true;
        }
    }
    return false;
}
class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void artLibraryRomImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.show();
            bool selected = false;
            QTimer::singleShot(100, &editor, [&] {
                auto dialog = editor.findChild<RomArtImport *>();
                if (!dialog)
                    return;
                auto tabs = dialog->findChild<QTabWidget *>();
                tabs->setCurrentIndex(1);
                auto list = dialog->findChild<QListWidget *>("romArtList");
                list->setCurrentRow(0);
                auto button = dialog->findChild<QPushButton *>("romArtImportSelected");
                selected = button && button->isEnabled();
                if (selected)
                    button->click();
                else
                    dialog->reject();
            });
            editor.findChild<QPushButton *>("spriteImportRom")->click();
            QVERIFY(selected);
            QCOMPARE(project.sprites().assets().size(), 1);
            const auto asset = project.sprites().assets().first();
            QCOMPARE(asset.frames.first().layers.first().image,
                     IconArtwork::load(project.rom()).image(1));
            undo.undo();
            QVERIFY(project.sprites().assets().isEmpty());
            undo.redo();
            QCOMPARE(project.sprites().asset(asset.id), asset);
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(project.manifest());
            QCOMPARE(reopened.sprites().asset(asset.id), asset);
            RomArtImport picker(project.rom(), [](SpriteAsset) {});
            picker.show();
            auto categories = picker.findChild<QComboBox *>("romArtCategory");
            auto list = picker.findChild<QListWidget *>("romArtList");
            for (int kind : {1, 2, 3, 4}) {
                categories->setCurrentIndex(kind);
                QVERIFY(list->count() > 0);
                list->setCurrentRow(0);
                QVERIFY(picker.findChild<QPushButton *>("romArtImportSelected")->isEnabled());
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void testingColdBoot() {
#ifndef Q_OS_WIN
        QSKIP("Project64 is Windows-only");
#endif
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        if (ScenarioSeeder::project64Running()) QSKIP("Do not disturb a real Project64 session");
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto exe = temp.filePath("Project64.exe"), log = temp.filePath("boots.jsonl");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/project64_fixture.exe", exe));
        const auto oldName = QCoreApplication::applicationName();
        const auto oldOrganization = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("LordlyCaliber-Tests");
        QCoreApplication::setApplicationName("Cold-boot-ui-" + QUuid::createUuid().toString(QUuid::Id128));
        const QList<QByteArray> envs{"LORDLY_P64_FIXTURE_SAVE", "LORDLY_P64_FIXTURE_FLUSH", "LORDLY_P64_FIXTURE_LOG"};
        QList<QByteArray> previous;
        for (const auto &name : envs) previous << qgetenv(name.constData());
        const auto cleanup = qScopeGuard([&] {
            QFile::remove(temp.filePath("flush.sra"));
            try { closeProject64(exe); } catch (...) {}
            for (int i = 0; i < envs.size(); ++i) {
                if (previous[i].isNull()) qunsetenv(envs[i].constData());
                else qputenv(envs[i].constData(), previous[i]);
            }
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QCoreApplication::setOrganizationName(oldOrganization);
        });
        const auto read = [](const QString &p) { QFile f(p); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
        const auto write = [](const QString &p, const QByteArray &bytes) {
            QDir().mkpath(QFileInfo(p).absolutePath()); QFile f(p);
            return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
        };
        try {
            const auto rom = Rom::open(path);
            ScenarioSeeder model(rom);
            const auto dest = ScenarioSeeder::batteryPath(rom, exe);
            const auto flushed = model.seed(4);
            QVERIFY(write(dest, model.seed(0)));
            QVERIFY(write(temp.filePath("flush.sra"), flushed));
            qputenv(envs[0].constData(), dest.toUtf8());
            qputenv(envs[1].constData(), temp.filePath("flush.sra").toUtf8());
            qputenv(envs[2].constData(), log.toUtf8());
            QSettings().setValue("testing/project64", exe);
            QSettings().setValue("testing/rom", path);
            QCOMPARE(QSettings().value("testing/project64").toString(), exe);
            TestingEditor editor(path);
            editor.resize(1150, 740); editor.show();
            QTRY_VERIFY_WITH_TIMEOUT(editor.ready(), 30000);
            auto list = editor.findChild<QTreeWidget *>("seederMissions");
            auto status = editor.findChild<QLabel *>("seederStatus");
            QVERIFY(!editor.findChild<QPushButton *>("seederLaunch"));
            QCOMPARE(list->topLevelItemCount(), model.choices().size());
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                QVERIFY(qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(i), 2)));
            // Selecting a row no longer writes the save or restarts the emulator.
            list->setCurrentItem(list->topLevelItem(1));
            QCOMPARE(read(dest), model.seed(0));
            QProcess old;
            old.start(exe, {path});
            QVERIFY(old.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 1, 10000);
            const int choice = 8;
            auto target = list->topLevelItem(choice);
            qobject_cast<QPushButton *>(list->itemWidget(target, 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith("Started:") || list->isEnabled(), 30000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QTRY_COMPARE_WITH_TIMEOUT(old.state(), QProcess::NotRunning, 10000);
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 2, 10000);
            const auto installed = read(log + ".sra");
            QCOMPARE(SaveDocument::open(rom, installed).memory(), SaveDocument::open(rom, model.seed(choice)).memory());
            QCOMPARE(read(dest), installed);
            const auto backups = QDir(QFileInfo(dest).absolutePath()).entryList({"*.bak"}, QDir::Files);
            QCOMPARE(backups.size(), 1);
            QCOMPARE(read(QFileInfo(dest).dir().filePath(backups[0])), flushed);
            const auto boot = QJsonDocument::fromJson(read(log).trimmed().split('\n').last()).object();
            QCOMPARE(boot["args"].toArray()[1].toString(), path);
            QVERIFY(status->text().contains(model.choices()[choice].location));
            QVERIFY(editor.grab().save("testing-cold-boot.png"));
            // Imported saves get their own row and keep the entire battery unchanged.
            const auto imported = temp.filePath("my-save.sra");
            const auto full = model.seed(12);
            QVERIFY(write(imported, full));
            editor.loadSave(imported);
            QCOMPARE(list->topLevelItemCount(), model.choices().size() + 1);
            auto mine = list->topLevelItem(0);
            list->setCurrentItem(list->topLevelItem(5));
            qobject_cast<QPushButton *>(list->itemWidget(mine, 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 3, 10000);
            QCOMPARE(read(log + ".sra"), full);
            QCOMPARE(read(dest), full);
            // A different installation must be left running, with no save replacement.
            QFile::remove(temp.filePath("flush.sra"));
            closeProject64(exe);
            const auto otherDir = temp.filePath("other");
            QVERIFY(QDir().mkpath(otherDir));
            const auto otherExe = QDir(otherDir).filePath("Project64.exe");
            QVERIFY(QFile::copy(exe, otherExe));
            QProcess other;
            const auto stopOther = qScopeGuard([&] {
                try { closeProject64(otherExe); } catch (...) {}
                other.waitForFinished(3000);
            });
            other.start(otherExe, {path});
            QVERIFY(other.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 4, 10000);
            qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(3), 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
            QVERIFY2(status->text().contains("Another Project64 installation"), qPrintable(status->text()));
            QCOMPARE(other.state(), QProcess::Running);
            QCOMPARE(read(dest), full);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void testingDefaultArmy() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QStandardPaths::setTestModeEnabled(true);
        const auto oldName = QCoreApplication::applicationName();
        QCoreApplication::setApplicationName("Testing-ui-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        QString defaultPath;
        const auto cleanup = qScopeGuard([&] {
            if (!defaultPath.isEmpty())
                QFile::remove(defaultPath);
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QStandardPaths::setTestModeEnabled(false);
        });
        try {
            MainWindow window;
            window.resize(1500, 950);
            window.show();
            QTimer messages;
            messages.setInterval(100);
            connect(&messages, &QTimer::timeout, &window, [&] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto box = qobject_cast<QMessageBox *>(widget)) {
                        qWarning() << "Unexpected test dialog:" << box->text();
                        box->reject();
                    }
            });
            messages.start();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<TestingEditor *>() != nullptr, 30000);
            auto seeder = window.findChild<TestingEditor *>();
            auto save = window.findChild<SaveEditor *>();
            auto tabs = window.findChild<QTabWidget *>();
            QVERIFY(save);
            QVERIFY(tabs);
            QCOMPARE(tabs->tabText(tabs->count() - 1), QString("Art Library"));
            QVERIFY(window.findChild<QLabel *>("cutsceneWipBadge"));
            tabs->setCurrentWidget(seeder);
            QTRY_VERIFY_WITH_TIMEOUT(seeder->ready(), 30000);
            QCOMPARE(seeder->findChild<QTreeWidget *>("seederMissions")->topLevelItemCount(), 58);
            defaultPath = seeder->defaultArmyPath();
            seeder->findChild<QPushButton *>("seederEditArmy")->click();
            QCOMPARE(tabs->currentWidget(), save);
            QVERIFY(save->document());
            QVERIFY(save->edit(SaveTable::Characters, 1, "luck", 33));
            save->findChild<QPushButton *>("saveDefaultArmy")->click();
            QCOMPARE(tabs->currentWidget(), seeder);
            QFile f(defaultPath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            auto army = SaveDocument::open(save->project()->rom(), f.readAll());
            f.close();
            QCOMPARE(army.value(SaveTable::Characters, 1, "luck").toInt(), 33);
            seeder->findChild<QPushButton *>("seederEditArmy")->click();
            QCOMPARE(save->document()->value(SaveTable::Characters, 1, "luck").toInt(), 33);
            save->findChild<QPushButton *>("saveDefaultArmy")->click();
            QTest::qWait(100);
            QVERIFY(window.grab().save("testing.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void inheritedSceneReplay_data() {
        QTest::addColumn<quint32>("key");
        QTest::addColumn<int>("mode");
        QTest::newRow("world") << 0x19abed8U << 2;
        QTest::newRow("room") << 0x19b46a6U << 0;
        QTest::newRow("ending-high") << 0x19f4e1aU << 2;
        QTest::newRow("ending-middle") << 0x19f4ebaU << 2;
    }
    void inheritedSceneReplay() {
        QFETCH(quint32, key);
        QFETCH(int, mode);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, key, mode);
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 30000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        QVERIFY(preview.findChild<QComboBox *>("sceneLaunchContext")->isVisible());
        QVERIFY(preview.findChild<QComboBox *>("sceneLiveActors")->count() > 0);
        const auto initial = preview.renderedFrame();
        QVERIFY(!initial.isNull());
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        QMap<int, QImage> frames;
        for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
            if (i % 45 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
            if (i == 49 || i % 500 == 499)
                frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
            if (i == 49) {
                const auto frame = preview.renderedFrame();
                int visible = 0;
                for (int y = 0; y < frame.height(); ++y)
                    for (int x = 0; x < frame.width(); ++x) {
                        const auto color = frame.pixelColor(x, y);
                        visible += color.alpha() && (color.red() || color.green() || color.blue());
                    }
                QVERIFY2(visible > 100, "Inherited scene must show its scenery or dialogue before fading out");
                QVERIFY(frame.save(QString("native-inherited-%1-early.png").arg(QString::number(key,16))));
            }
        }
        QVERIFY2(status->text().contains("Scene released.") ||
                     status->text().contains("Scene returned to the game"),
                 qPrintable(status->text()));
        qInfo() << QString::number(key, 16) << "finished" << preview.property("sceneTick");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        position->setValue(0);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 0, 20000);
        QCOMPARE(preview.renderedFrame(), initial);
        for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
            position->setValue(it.key());
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
            QCOMPARE(preview.renderedFrame(), it.value());
        }
        preview.grab().save(QString("native-inherited-%1.png").arg(QString::number(key, 16)));
    }
    void inheritedSceneEventEdits() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadSceneEvents();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Replacement launch", 0x1a14970, true);
        project.setScenePrograms(programs.json());
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1200, 800);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                scenes->setCurrentRow(i);
        auto contexts = editor.findChild<QComboBox *>("sceneLaunchContext");
        QCOMPARE(contexts->currentData().toString(), QString("direct"));
        auto sites = editor.findChild<QTableWidget *>("sceneEventSites");
        for (int i = 0; i < sites->rowCount(); ++i)
            if (sites->item(i, 0)->text() == "event-director:4:b0058")
                sites->setCurrentCell(i, 0);
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        target->setCurrentIndex(target->findData(programs.resource(id).selectors.first()));
        editor.findChild<QPushButton *>("sceneEventAssign")->click();
        QVERIFY(contexts->currentData().toString().startsWith("event-director:4:b0058:"));
        undo.undo();
        QCOMPARE(contexts->currentData().toString(), QString("direct"));
        undo.redo();
        QVERIFY(contexts->currentData().toString().startsWith("event-director:4:b0058:"));
    }
    void partySceneReplay_data() {
        QTest::addColumn<quint32>("offset");
        QTest::addColumn<quint32>("key");
        QTest::newRow("preserved") << 0x1f450d0U << 0x19b0e4cU;
        QTest::newRow("preserved-return") << 0x1f4c45aU << 0x19b81d6U;
        QTest::newRow("encounter") << 0x1f40650U << 0x19ac3ccU;
        QTest::newRow("formation") << 0x1f3f242U << 0x19aafc2U;
        QTest::newRow("world-party") << 0x1fa4d0aU << 0x1a10a8aU;
    }
    void partySceneReplay() {
        QFETCH(quint32, offset);
        QFETCH(quint32, key);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
        {
            preview.selectScene(scenes, key, 2);
            QMap<int, QImage> frames;
            int mostActors = 0;
            for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
                if (i % 45 == 0 && confirm->isVisible())
                    confirm->click();
                else
                    step->click();
                mostActors = qMax(mostActors, objects->count());
                if (i == 49 || i % 250 == 249)
                    frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
            }
            QVERIFY2(status->text().contains(offset == 0x1f40650U ? "Scene returned to the game"
                                                                  : "Scene released."),
                     qPrintable(QString("%1: %2").arg(offset, 0, 16).arg(status->text())));
            QVERIFY(mostActors >= 2);
            QVERIFY(!frames.isEmpty());
            qInfo() << QString::number(offset, 16) << "released at" << preview.property("sceneTick")
                    << "with up to" << mostActors << "Actors";
            auto position = preview.findChild<QSpinBox *>("scenePosition");
            for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
                position->setValue(it.key());
                QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
                QCOMPARE(preview.renderedFrame(), it.value());
            }
            if (offset == 0x1fa4d0aU) {
                QApplication::processEvents();
                preview.grab().save("native-party-preview.png");
            }
        }
        QCOMPARE(scenes.json(), before);
        QCOMPARE(Rom::open(path).bytes(), rom.bytes());
    }
    void sceneCompletionControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            const auto id = scenes.createScene("Completion controls", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850);
            editor.show();
            auto list = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1000, 10000);
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) {
                    list->setCurrentRow(i);
                    break;
                }
            auto actions = editor.findChild<SceneActionEditor *>();
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            for (auto type : {7U, 0x1bU, 0x2cU}) {
                position->setCurrentIndex(position->count() - 1);
                kind->setCurrentIndex(kind->findData(type));
                QVERIFY(add->isEnabled());
                add->click();
                const int at = actions->findChild<QListWidget *>("sceneActionList")
                                   ->currentItem()
                                   ->data(Qt::UserRole)
                                   .toInt();
                const auto before = project.scenePrograms()->program(id).source;
                auto wait = actions->findChild<QCheckBox *>("sceneActionWait");
                QVERIFY(wait && wait->isEnabled());
                QVERIFY(!wait->isChecked());
                wait->click();
                const auto after = project.scenePrograms()->program(id).source;
                QVERIFY(after != before);
                undo.undo();
                QCOMPARE(project.scenePrograms()->program(id).source, before);
                undo.redo();
                QCOMPARE(project.scenePrograms()->program(id).source, after);
                actions->selectScene(id, at);
                QVERIFY(actions->findChild<QCheckBox *>("sceneActionWait")->isChecked());
                if (type == 0x2c) {
                    field(0)->setText("15");
                    field(2)->setText("1.25");
                    field(4)->setText("12");
                    const auto p = project.scenePrograms()->program(id);
                    QCOMPARE(p.commands[at].operands[0], quint32(15000));
                    QCOMPARE(p.commands[at + 5].operands[1], quint32(12));
                }
            }
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-action-completion.png"));
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.scenePrograms()->program(id).source,
                     project.scenePrograms()->program(id).source);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneChoiceAuthoring() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            project.loadDialogues();
            auto scenes = *project.scenePrograms();
            const auto id =
                scenes.createScene("Desktop two-option choice", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(scenes.json());
            const auto blank = scenes.program(id);
            const auto originalText = project.dialogues()->json();
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1150, 650);
            editor.show();
            editor.selectScene(id, SceneActions::positions(blank).last().command);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            auto add = editor.findChild<QPushButton *>("sceneActionInsert");
            auto editOptions = [&](const QString &first, const QString &second, bool accept) {
                QTimer::singleShot(0, &editor, [&, first, second, accept] {
                    auto dialog = editor.findChild<QDialog *>("sceneChoiceDialog");
                    QVERIFY(dialog);
                    auto a = dialog->findChild<QLineEdit *>("sceneChoiceOption1");
                    auto b = dialog->findChild<QLineEdit *>("sceneChoiceOption2");
                    auto buttons = dialog->findChild<QDialogButtonBox *>();
                    a->setText("@");
                    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
                    a->setText(first);
                    b->setText(second);
                    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
                    buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)
                        ->click();
                });
            };
            kind->setCurrentIndex(kind->findData(SceneActions::Choice));
            QVERIFY(add->isEnabled());
            editOptions("Continue", "Return", false);
            add->click();
            QCOMPARE(undo.count(), 0);
            QCOMPARE(project.dialogues()->json(), originalText);
            editOptions("Continue", "Return", true);
            add->click();
            QCOMPARE(undo.count(), 1);
            const auto added = project.scenePrograms()->program(id);
            QCOMPARE(SceneActions::choices(added).size(), 1);
            auto choice = SceneActions::choices(added).first();
            const int entry = added.commands[choice.command].operands[2];
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->first,
                     QString("Continue"));
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            QCOMPARE(project.dialogues()->json(), originalText);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, added.source);
            editor.selectScene(id, choice.command);
            editOptions("Proceed", "Go back", true);
            editor.findChild<QPushButton *>("sceneActionEditText")->click();
            QCOMPARE(undo.count(), 2);
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->second,
                     QString("Go back"));
            undo.undo();
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->second,
                     QString("Return"));
            undo.redo();
            for (int option : {1, 2}) {
                choice = SceneActions::choices(project.scenePrograms()->program(id)).first();
                int end = option == 1 ? choice.firstEnd : choice.secondEnd;
                editor.selectScene(id, choice.command);
                position->setCurrentIndex(position->findData(end));
                QVERIFY(position->currentText().contains(QString("After option %1").arg(option)));
                kind->setCurrentIndex(kind->findData(0x1b));
                QVERIFY(add->isEnabled());
                add->click();
                editor.findChild<QLineEdit *>("sceneActionField0")->setText("1");
                editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(option))
                    ->setText("255");
                editor.findChild<QLineEdit *>("sceneActionField4")->setText("0");
                editor.findChild<QLineEdit *>("sceneActionField5")->setText("255");
                choice = SceneActions::choices(project.scenePrograms()->program(id)).first();
                end = option == 1 ? choice.firstEnd : choice.secondEnd;
                position->setCurrentIndex(position->findData(end));
                kind->setCurrentIndex(kind->findData(0xe));
                QVERIFY(add->isEnabled());
                add->click();
                editor.findChild<QLineEdit *>("sceneActionField0")->setText("3");
            }
            const auto complete = project.scenePrograms()->program(id).source;
            editor.selectScene(
                id, SceneActions::choices(project.scenePrograms()->program(id)).first().firstBegin);
            QVERIFY(editor.findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .startsWith("Option 1"));
            QApplication::processEvents();
            QVERIFY(editor.findChild<QLineEdit *>("sceneActionField0")->isVisible());
            QVERIFY(editor.grab().save("native-choice-authoring.png"));
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.scenePrograms()->program(id).source, complete);
            QImage outcomes[2];
            for (int option : {1, 2}) {
                ScenePreview preview(restored.rom());
                preview.resize(1250, 900);
                preview.show();
                preview.selectScene(*restored.scenePrograms(), id, 2, restored.dialogues());
                auto step = preview.findChild<QPushButton *>("sceneStep");
                QVERIFY(step);
                auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
                for (int i = 0; i < 300 && preview.property("sceneDialogueState").toInt() != 7; ++i)
                    step->click();
                QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
                QCOMPARE(preview.property("sceneDialogueState").toInt(), 7);
                if (option == 2)
                    preview.findChild<QPushButton *>("panelDown")->click();
                preview.findChild<QPushButton *>("panelConfirm")->click();
                for (int i = 0; i < 200 && !preview.property("sceneStopped").toBool(); ++i)
                    step->click();
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                outcomes[option - 1] = preview.renderedFrame();
                // Imported automatic selection must reach the same visible branch as manual input.
                QFile inputFile(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-room-input.json"));
                QVERIFY(inputFile.open(QIODevice::ReadOnly));
                auto input = QJsonDocument::fromJson(inputFile.readAll()).object();
                input["assetId"] = preview.playbackAssetId(); input.remove("actorInputRows");
                auto group = input["externalProducers"].toObject(); auto services = group["value"].toObject();
                for (const auto key : {"directorLaunch", "sharedActor", "mapMenu", "imageEcho", "framebuffer"}) services.remove(key);
                auto schedule = services["resourceSchedule"].toObject();
                schedule["pageAdvancePolicy"] = "automatic";
                schedule["dialogueChoicePolicy"] = option == 1 ? "first" : "second";
                schedule["controller"] = QJsonObject{{"throughPass", 3999}, {"changes", QJsonArray{
                    QJsonObject{{"pass",0},{"actionMask",0},{"directionMask",0},{"historyMask",0},{"dummyMask",0}}}}};
                services["resourceSchedule"] = schedule; group["value"] = services; input["externalProducers"] = group;
                preview.importPlaybackInputs(QJsonDocument(input).toJson(QJsonDocument::Compact));
                preview.seekToUpdate(500);
                QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 10000);
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                QCOMPARE(preview.renderedFrame(), outcomes[option - 1]);
            }
            QVERIFY(outcomes[0] != outcomes[1]);
            QCOMPARE(project.scenePrograms()->program(id).source, complete);
            editor.selectScene(
                id, SceneActions::choices(project.scenePrograms()->program(id)).first().command);
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, complete);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneActionExecutionTimeline() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Timed editing", 0x1fa8bf4 - 0x594284, true);
        auto program = SceneActions::insert(programs.program(id), 1, 0xe, 5);
        int exit = -1;
        for (int i = 0; i < program.commands.size(); ++i) if (program.commands[i].opcode == 0x7d) { exit = i; break; }
        QVERIFY(exit >= 0);
        program = SceneActions::insert(program, exit, 0x1b);
        programs.replace(id, program.source); project.setScenePrograms(programs.json());
        const auto before = project.scenePrograms()->json();
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1300, 900); editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
        for (int i = 0; i < scenes->count(); ++i) if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
        auto actions = editor.findChild<SceneActionEditor *>();
        auto list = actions->findChild<QListWidget *>("sceneActionList");
        auto preview = editor.findChild<ScenePreview *>();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
        int fade = -1, wait = -1;
        for (int i = 0; i < list->count(); ++i) {
            const auto opcode = program.commands[list->item(i)->data(Qt::UserRole).toInt()].opcode;
            if (opcode == 0x1b) fade = i;
            if (opcode == 1) wait = i;
        }
        QVERIFY(fade >= 0 && wait >= 0);
        const int fadeWord = program.commands[list->item(fade)->data(Qt::UserRole).toInt()].word;
        list->setCurrentRow(-1); list->setCurrentRow(fade);
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(), 6, 10000);
        QCOMPARE(preview->commandTimes().value(fadeWord, -1), 6);
        QVERIFY(list->item(fade)->text().contains("update 6 (0.10 s)"));
        list->setCurrentRow(wait);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
        QVERIFY(list->item(wait)->text().startsWith("At playhead"));
        preview->seekToCommand(fadeWord);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 6);
        const auto text = list->item(fade)->text();
        actions->updateTimeline(SceneId(123), {{fadeWord, 999}}, fadeWord, 999);
        QCOMPARE(list->item(fade)->text(), text);
        QCOMPARE(project.scenePrograms()->json(), before); QCOMPARE(undo.count(), 0);
    }
    void importedPlaybackInputs() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Imported actor playback", 0x1fa8bf4 - 0x594284, true);
            auto importedProgram = SceneActions::insert(programs.program(id), 1, 0xe, 10);
            const int queryIndex = SceneActions::positions(importedProgram).last().command;
            importedProgram.commands.insert(queryIndex, {0, 0x11, {0, 0, 0}});
            importedProgram.commands.insert(queryIndex + 1, {0, 0x80000002, {}});
            importedProgram.commands.insert(queryIndex + 2, {0, 0x80000000, {}});
            importedProgram = DirectorProgram::decode(importedProgram.encode());
            const int queryWord = importedProgram.commands[queryIndex].word;
            programs.replace(id, importedProgram.source);
            project.setScenePrograms(programs.json());
            const auto unchanged = project.manifest();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 900); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
            auto preview = editor.findChild<ScenePreview *>();
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(preview);
            auto instance = ActorInstance::create(project.rom(), {0, 1, 0, 0, 0, {10, 7, 20}, 0, 0}, 2);
            auto record = instance.record();
            QByteArray movement(16, 0); put32(movement, 0, std::bit_cast<quint32>(2.f)); put16(movement, 12, 5);
            QJsonArray entries;
            entries << QJsonObject{{"identity", "imported-actor"}, {"recordHex", QString::fromLatin1(record.toHex())},
                                   {"movementHex", QString::fromLatin1(movement.toHex())}};
            for (int i = 1; i < 28; ++i) entries << QJsonValue(QJsonValue::Null);
            auto known = [](QJsonValue value) { return QJsonObject{{"status", "known"}, {"value", value}}; };
            QJsonObject input{{"schema", "ob64-cutscene-launch-inputs.v1"}, {"assetId", preview->playbackAssetId()},
                {"invocationId", "native-desktop-import"}, {"sourceIdentity", "Explicit position/movement fixture"},
                {"evidenceGrade", "Candidate"}, {"schedulerBranch", known("normal")},
                {"existingActors", known(QJsonObject{{"slots", entries}, {"otherJobsEmpty", true}})}};
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            auto state = preview->findChild<QLabel *>("sceneActorState");
            QVERIFY2(state->text().contains("X 10.000 · Y 7.000 · Z 20.000"), qPrintable(state->text()));
            preview->seekToUpdate(1); QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
            QVERIFY2(state->text().contains("X 12.000 · Y 7.000 · Z 20.000"), qPrintable(state->text()));
            const auto frame = preview->renderedFrame();
            auto bad = input; bad["assetId"] = "wrong-resource";
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(bad).toJson()));
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs("{broken"));
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QByteArray(131073, ' ')));
            auto unresolved = input;
            auto invalidRecord = record; put32(invalidRecord, 0xe8, 9999);
            auto invalidEntries = entries; auto invalidActor = entries[0].toObject();
            invalidActor["recordHex"] = QString::fromLatin1(invalidRecord.toHex()); invalidEntries[0] = invalidActor;
            unresolved["existingActors"] = known(QJsonObject{{"slots", invalidEntries}, {"otherJobsEmpty", true}});
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(unresolved).toJson()));
            QCOMPARE(preview->property("sceneTick").toInt(), 1); QCOMPARE(preview->renderedFrame(), frame);
            preview->seekToUpdate(0); QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            preview->seekToUpdate(1); QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
            QCOMPARE(preview->renderedFrame(), frame);
            input.remove("existingActors");
            input["capturedSnapshot"] = known(QJsonObject{{"slots", entries}, {"sceneMode", 2},
                {"observedParserCursor", 123}, {"resumeState", "unknown"}, {"otherJobOwners", "unknown"}});
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            QCOMPARE(preview->property("sceneTick").toInt(), 0);
            QVERIFY(preview->property("sceneStopped").toBool());
            QVERIFY(state->text().contains("X 10.000 · Y 7.000 · Z 20.000"));
            QVERIFY(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("no elapsed updates"));
            // The qualified one-update profile supplies a real held query and disjoint owners.
            auto captured = input["capturedSnapshot"].toObject()["value"].toObject();
            captured["sceneMode"] = 0; captured["observedParserCursor"] = queryWord;
            input["capturedSnapshot"] = known(captured);
            QByteArray primary(0x1cb2, 0); put32(primary, 0x18, 0x80320000); put32(primary, 0xf8, 0x80320400);
            QJsonArray menuOwners; for (int i = 0; i < 14; ++i) menuOwners << QJsonValue(QJsonValue::Null);
            QJsonObject resume{{"entry", "normal-director-held-movement-query"}, {"origin", "prospective-captured-state"},
                {"updates", 1}, {"registeredCounter", 0}, {"tailTimer", -1},
                {"primaryOwnerAddress", qint64(0x80300000)}, {"primaryOwnerHex", QString::fromLatin1(primary.toHex())},
                {"secondaryOwnerAddress", qint64(0x80310000)}, {"secondaryOwnerHex", QString::fromLatin1(QByteArray(0x844,0).toHex())},
                {"menuRootAddress", qint64(0x80330000)}, {"menuRootHex", QString::fromLatin1(QByteArray(0xd8,0).toHex())}, {"menuOwners", menuOwners}};
            input["capturedResume"] = known(resume);
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            QVERIFY(!preview->property("sceneStopped").toBool());
            preview->seekToUpdate(10); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 1);
            QVERIFY2(state->text().contains("X 12.000 · Y 7.000 · Z 20.000"), qPrintable(state->text()));
            QVERIFY(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("declared 1 updates"));
            const auto resumedFrame = preview->renderedFrame();
            auto endingMovement = movement; put16(endingMovement, 12, 1);
            auto endingEntries = entries; auto endingActor = endingEntries[0].toObject();
            endingActor["movementHex"] = QString::fromLatin1(endingMovement.toHex()); endingEntries[0] = endingActor;
            captured["slots"] = endingEntries; auto badResume = input; badResume["capturedSnapshot"] = known(captured);
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(badResume).toJson()));
            QCOMPARE(preview->renderedFrame(), resumedFrame);
            preview->findChild<QPushButton *>("sceneClearPlaybackInputs")->click();
            QVERIFY(!preview->property("sceneStopped").toBool());
            QCOMPARE(preview->findChild<QComboBox *>("sceneLiveActors")->count(), 0);
            const auto context = preview->findChild<QLabel *>("scenePreviewContext")->text();
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(unresolved).toJson()));
            QCOMPARE(preview->findChild<QLabel *>("scenePreviewContext")->text(), context);
            QCOMPARE(project.manifest(), unchanged); QCOMPARE(undo.count(), 0);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void importedRoomLaunch() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto programs = ScenePrograms::load(rom);
            QFile file(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-room-input.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto data = file.readAll();
            ScenePreview preview(rom); preview.resize(1000, 900); preview.show();
            preview.selectScene(programs, programs.original().selectors()[39], 0);
            QCOMPARE(preview.playbackAssetId(), QString("rom-director:01F4558C"));
            const auto inputs = ScenePlaybackInputs::parse(data, preview.playbackAssetId());
            ScenePlaybackStart start; start.mode = 0;
            auto cameras = SceneCameras::freshModeTwo(rom);
            inputs.configure(start, cameras);
            QFile expected(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-director-launch.json"));
            QVERIFY(expected.open(QIODevice::ReadOnly));
            const auto prefix = QByteArray::fromHex(QJsonDocument::fromJson(expected.readAll()).object()["prefix"].toObject()["cameraHex"].toString().toLatin1());
            QCOMPARE(cameras.actor().bank(), prefix.first(56));
            QCOMPARE(cameras.registered().bank(), prefix.mid(88, 56));
            preview.importPlaybackInputs(data);
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            preview.seekToUpdate(600);
            QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneTick").toInt() == 600 || preview.property("sceneStopped").toBool(), 30000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            QVERIFY(preview.findChild<QComboBox *>("sceneLiveActors")->count() > 0);
            const auto frame = preview.renderedFrame();
            preview.seekToUpdate(500); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 500, 30000);
            preview.seekToUpdate(600); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 600, 30000);
            QCOMPARE(preview.renderedFrame(), frame);
            QApplication::processEvents();
            QVERIFY2(preview.rect().contains(preview.findChild<QComboBox *>("sceneLiveActors")->geometry()), "Live Actor controls must remain within the preview");
            QVERIFY(preview.grab().save("native-imported-room.png"));
            preview.seekToUpdate(4000);
            QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 60000);
            QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
            qInfo() << "Imported room released at update" << preview.property("sceneTick").toInt();
            auto omitted = QJsonDocument::fromJson(data).object();
            auto group = omitted["externalProducers"].toObject(); auto services = group["value"].toObject();
            auto drawing = services["framebuffer"].toObject(); drawing["backgroundPolicy"] = "omit";
            services["framebuffer"] = drawing; group["value"] = services; omitted["externalProducers"] = group;
            preview.importPlaybackInputs(QJsonDocument(omitted).toJson(QJsonDocument::Compact));
            preview.seekToUpdate(600); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 600, 30000);
            QVERIFY(preview.renderedFrame() != frame);
            auto confirm = preview.findChild<QPushButton *>("panelConfirm");
            QVERIFY(confirm->isVisible() && confirm->isEnabled());
            confirm->click();
            QCOMPARE(preview.property("sceneTick").toInt(), 601);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void importedGraduationCapture_data() {
        QTest::addColumn<bool>("continuous");
        QTest::newRow("held") << false;
        QTest::newRow("continuous") << true;
    }
    void importedGraduationCapture() {
        QFETCH(bool, continuous);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto programs = ScenePrograms::load(rom);
            QFile file(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath(continuous ? "../../tests/fixtures/cutscene-graduation-continuous.json" : "../../tests/fixtures/cutscene-graduation-resume.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
            ScenePreview preview(rom); preview.resize(1000, 900); preview.show();
            preview.selectScene(programs, programs.original().selectors()[2], 2);
            preview.importPlaybackInputs(QJsonDocument(fixture["input"].toObject()).toJson(QJsonDocument::Compact));
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            preview.seekToUpdate(69);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 69, 30000);
            if (!continuous) QVERIFY2(status->text().contains("declared 69 updates"), qPrintable(status->text()));
            auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(objects->count(), 8); objects->setCurrentIndex(objects->findData(1));
            const auto expected = fixture["expected"].toArray()[68].toObject();
            QCOMPARE(expected["update"].toInt(), 69);
            QByteArray record;
            for (const auto &entry : expected["actors"].toArray()) if (entry.toObject()["slot"].toInt() == 1)
                record = QByteArray::fromHex(entry.toObject()["recordHex"].toString().toLatin1());
            QCOMPARE(record.size(), 336);
            const auto position = QString("X %1 · Y %2 · Z %3")
                .arg(std::bit_cast<float>(u32(record, 0x11c)),0,'f',3)
                .arg(std::bit_cast<float>(u32(record, 0x120)),0,'f',3)
                .arg(std::bit_cast<float>(u32(record, 0x124)),0,'f',3);
            QVERIFY2(preview.findChild<QLabel *>("sceneActorState")->text().contains(position),
                     qPrintable(preview.findChild<QLabel *>("sceneActorState")->text()));
            const auto frame = preview.renderedFrame();
            preview.seekToUpdate(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
            preview.seekToUpdate(69); QTRY_COMPARE(preview.property("sceneTick").toInt(), 69);
            QCOMPARE(preview.renderedFrame(), frame);
            QVERIFY(preview.grab().save("native-imported-graduation.png"));
            if (continuous) {
                preview.seekToUpdate(1000);
                QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 30000);
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                QVERIFY(preview.property("sceneTick").toInt() < 1000);
                qInfo() << "Imported continuous capture released at update" << preview.property("sceneTick").toInt();
                auto neutral = fixture["input"].toObject();
                auto group = neutral["capturedResume"].toObject(); auto resume = group["value"].toObject();
                auto services = resume["resourceServices"].toObject(); auto schedule = services["resourceSchedule"].toObject();
                schedule["pageAdvancePolicy"] = "neutral"; services["resourceSchedule"] = schedule;
                resume["resourceServices"] = services; group["value"] = resume; neutral["capturedResume"] = group;
                preview.importPlaybackInputs(QJsonDocument(neutral).toJson(QJsonDocument::Compact));
                preview.seekToUpdate(400); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 400, 30000);
                QVERIFY(!preview.property("sceneStopped").toBool());
                QVERIFY(preview.property("sceneDialogueState").toInt() == 3 || preview.property("sceneDialogueState").toInt() == 6);
            }
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void sceneTimedPlacementAndHold() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and export tools required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Timed placement and hold", 0x1fa8bf4 - 0x594284, true);
            auto p = SceneActions::insert(programs.program(id), 1, 0x2a);
            p = SceneActions::edit(p, 1, 1, "84");
            p = SceneActions::edit(p, 1, 2, "85");
            p = SceneActions::insert(p, SceneActions::positions(p).last().command, 0xe, 5);
            programs.replace(id, p.source); project.setScenePrograms(programs.json());
            const auto original = p.source;
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 900); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
            auto actions = editor.findChild<SceneActionEditor *>();
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(3);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            auto state = preview->findChild<QLabel *>("sceneActorState");
            const auto before = preview->renderedFrame();
            QPoint actorPixel(-1, -1);
            for (int y = 0; y < 240 && actorPixel.x() < 0; ++y)
                for (int x = 0; x < 320; ++x) if (preview->actorAt({x, y}) == 0) { actorPixel = {x, y}; break; }
            QVERIFY(actorPixel.x() >= 0);
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            const auto pixel = view->mapFromScene(QPointF(actorPixel) + QPointF(.5, .5));
            const auto destination = pixel + QPoint(35, 0);
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pixel);
            QTest::mouseMove(view->viewport(), destination);
            QTest::keyClick(view, Qt::Key_Escape);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
            QCOMPARE(undo.count(), 0); QCOMPARE(project.scenePrograms()->program(id).source, original);
            QCOMPARE(preview->renderedFrame(), before);
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pixel);
            QTest::mouseMove(view->viewport(), destination);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
            QCOMPARE(undo.count(), 1);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            QVERIFY(preview->renderedFrame() != before);
            QVERIFY(state->text().startsWith("Class pair 84 / 85"));
            QVERIFY(state->text().contains("Y 0.000"));
            const auto placed = project.scenePrograms()->program(id);
            int place = -1;
            for (const auto &action : SceneActions::inspect(placed)) if (action.name == "Place Actor") place = action.command;
            QVERIFY(place >= 0);
            QCOMPARE(preview->commandTimes().value(placed.commands[place].word, -1), 3);
            const auto placedState = state->text();
            preview->seekToUpdate(2); QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(state->text() != placedState);
            preview->seekToUpdate(3); QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            QCOMPARE(state->text(), placedState);
            preview->seekToUpdate(10); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 6); // Splitting the wait preserves the exit.
            undo.undo(); QCOMPARE(project.scenePrograms()->program(id).source, original);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(id).source, placed.source);

            // Add at playhead uses the same preserved timing; no selection-based insertion.
            preview->seekToUpdate(3); QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            pages->setCurrentWidget(actions);
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            kind->setCurrentIndex(kind->findData(0x1b));
            actions->findChild<QPushButton *>("sceneActionInsertAtPlayhead")->click();
            QCOMPARE(undo.index(), 2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            auto field = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(field); field->setText("10");
            // Concurrent movement and pan deliberately continue beyond the six-update exit.
            p = project.scenePrograms()->program(id);
            auto exit = SceneActions::positions(p).last().command;
            p = SceneActions::insert(p, exit, 7);
            p.commands[exit].operands = {0, quint32(-1000), quint32(-1000), 80000, 10000, 10, 0};
            p = DirectorProgram::decode(p.encode());
            exit = SceneActions::positions(p).last().command;
            p = SceneActions::insert(p, exit, 0x2c);
            p = SceneActions::edit(p, exit, 4, "8");
            auto changed = *project.scenePrograms(); changed.replace(id, p.source);
            pushSceneProgramChange(&project, &undo, "Concurrent actions", changed);
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(30); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 6);
            auto warning = actions->findChild<QLabel *>("sceneExitWarning");
            QVERIFY2(warning->text().contains("Actor 0 movement"), qPrintable(warning->text()));
            QVERIFY(warning->text().contains("screen fade"));
            QVERIFY(warning->text().contains("camera pan"));
            pages->setCurrentWidget(actions);
            auto hold = actions->findChild<QPushButton *>("sceneHoldUntilFinished");
            QVERIFY(hold->isVisible() && hold->isEnabled());
            const int beforeHold = undo.index();
            hold->click(); QCOMPARE(undo.index(), beforeHold + 1);
            const auto held = project.scenePrograms()->program(id).source;
            undo.undo(); QCOMPARE(project.scenePrograms()->program(id).source, p.source);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(id).source, held);
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(60); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QVERIFY(preview->property("sceneTick").toInt() >= 16);
            QVERIFY2(warning->text().isEmpty(), qPrintable(warning->text()));
            QVERIFY2(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("Scene released"),
                     qPrintable(preview->findChild<QLabel *>("scenePlaybackStatus")->text()));
            const auto heldFrame = preview->renderedFrame();
            const int heldTick = preview->property("sceneTick").toInt();
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, held);
            const auto built = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir output(QDir(root).filePath("native-timed-scene-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, candidate);
            const auto reopenedRom = Rom::open(candidate);
            const auto reopened = ScenePrograms::load(reopenedRom);
            const int selector = restored.scenePrograms()->resource(id).selectors.first();
            const SceneId reopenedId(reopened.original().selectors()[selector]);
            QCOMPARE(reopened.program(reopenedId).source, held);
            ScenePreview check(reopenedRom); check.selectScene(reopened, reopenedId, 2);
            check.seekToUpdate(60); QTRY_VERIFY(check.property("sceneStopped").toBool());
            QCOMPARE(check.property("sceneTick").toInt(), heldTick);
            QCOMPARE(check.renderedFrame(), heldFrame);
            // Editing both fields to the native retain-position sentinel must still place.
            auto negative = SceneActions::edit(placed, place, 1, "-1");
            negative = SceneActions::edit(negative, place, 2, "-1");
            auto negativePrograms = *project.scenePrograms(); negativePrograms.replace(id, negative.source);
            ScenePreview negativePreview(project.rom()); negativePreview.selectScene(negativePrograms, id, 2);
            negativePreview.seekToUpdate(3); QTRY_COMPARE(negativePreview.property("sceneTick").toInt(), 3);
            QVERIFY2(negativePreview.findChild<QLabel *>("sceneActorState")->text().contains("X -1.000 · Y 0.000 · Z -1.000"),
                     qPrintable(negativePreview.findChild<QLabel *>("sceneActorState")->text()));
            auto checksum = built.image; Rom::updateChecksum(checksum); QCOMPARE(checksum, built.image);
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            output.setAutoRemove(false); qInfo().noquote() << "Timed scene export:" << candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void sceneActorClassChoices() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Named Actor choices", 0x1fa8bf4 - 0x594284, true);
        programs.replace(id, SceneActions::insert(programs.program(id), 1, 0x2a).source);
        project.setScenePrograms(programs.json());
        QUndoStack undo;
        RecordModel classes(&project, Table::Classes, &undo);
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1300, 900);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1499, 10000);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                list->setCurrentRow(i);
        auto actions = editor.findChild<SceneActionEditor *>();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
        actions->findChild<QListWidget *>("sceneActionList")->setCurrentRow(0);
        auto field = [&](int i) { return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i)); };
        auto choice = [&](int i) { return actions->findChild<QComboBox *>(QString("sceneActionClassChoice%1").arg(i)); };
        QVERIFY(choice(1) && choice(2));
        QCOMPARE(choice(1)->currentData().toInt(), 2);
        QVERIFY(choice(1)->currentText().contains(project.record(Table::Classes, 2).name));
        QVERIFY(classes.setData(classes.index(2, 1), "Scene Fighter", Qt::EditRole));
        QVERIFY(choice(1)->currentText().contains("Scene Fighter"));
        undo.undo();
        QVERIFY(choice(1)->currentText().contains(project.record(Table::Classes, 2).name));
        const int added = classes.appendClass(2, "New Actor");
        QVERIFY(added >= 0);
        QVERIFY(choice(1)->findData(added) >= 0);
        choice(1)->setFocus();
        QTest::keyClick(choice(1), Qt::Key_End);
        QCOMPARE(choice(1)->currentData().toInt(), added);
        QCOMPARE(field(1)->text(), QString::number(added));
        QCOMPARE(project.scenePrograms()->program(id).commands[2].operands[1], quint32(added));
        undo.undo();
        QVERIFY(choice(1));
        QCOMPARE(choice(1)->currentData().toInt(), 2);
        undo.undo();
        QCOMPARE(choice(1)->findData(added), -1);
        undo.redo();
        QVERIFY(choice(1)->findData(added) >= 0);
        undo.redo();
        QCOMPARE(choice(1)->currentData().toInt(), added);
        const auto beforeDraft = project.scenePrograms()->program(id).source;
        field(4)->setText("-");
        QVERIFY(actions->property("invalidEdits").toBool());
        QVERIFY(classes.setData(classes.index(added, 1), "Renamed Actor", Qt::EditRole));
        QCOMPARE(field(4)->text(), QString("-"));
        QVERIFY(actions->property("invalidEdits").toBool());
        QVERIFY(choice(1)->currentText().contains("Renamed Actor"));
        QCOMPARE(project.scenePrograms()->program(id).source, beforeDraft);
        undo.undo();
        QCOMPARE(field(4)->text(), QString("-"));
        QVERIFY(choice(1)->currentText().contains("New Actor"));
        actions->findChild<QPushButton *>("sceneActionDiscard")->click();
        field(2)->setText("0x54");
        QCOMPARE(choice(2)->currentData().toInt(), 84);
        QCoreApplication::processEvents();
        QVERIFY(choice(1)->isVisible());
        QVERIFY(editor.grab().save("native-action-class-choices.png"));
        auto symbolic = *project.scenePrograms();
        symbolic.setOperand(id, 2, 1, 0x08880001);
        pushSceneProgramChange(&project, &undo, "Use launch class", symbolic);
        actions->selectCommand(1);
        QVERIFY(field(1)->isReadOnly());
        QCOMPARE(field(1)->text(), QString("Launch value 1"));
        QVERIFY(!choice(1));
        auto separated = *project.scenePrograms();
        separated.insertCommand(id, 2, 0);
        pushSceneProgramChange(&project, &undo, "Separate Actor body action", separated);
        actions->selectCommand(3);
        QVERIFY(!choice(1));
        QVERIFY(choice(2)->findData(-1) >= 0);
        choice(2)->setFocus();
        QTest::keyClick(choice(2), Qt::Key_Home);
        QCOMPARE(project.scenePrograms()->program(id).commands[3].operands[5], quint32(-1));
        QCOMPARE(project.scenePrograms()->program(id).commands[3].operands[1], quint32(0x08880001));
    }
    void classActorCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Class Actor preview", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(programs.json());
            const auto blank = programs.program(id);
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 1000);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                    scenes->setCurrentRow(i);
            auto actions = editor.findChild<SceneActionEditor *>();
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x2a));
            QVERIFY(add->isEnabled());
            add->click();
            QCOMPARE(
                SceneActions::inspect(project.scenePrograms()->program(id)).first().commandCount,
                2);
            auto field = [&](int i) {
                return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            QVERIFY(field(8));
            field(0)->setText("5");
            field(1)->setText("84");
            field(2)->setText("85");
            field(4)->setText("40.000");
            field(6)->setText("10.000");
            const auto created = project.scenePrograms()->program(id);
            QCOMPARE(created.commands[1].operands[0], quint32(5));
            QCOMPARE(created.commands[2].operands[0], quint32(5));
            field(1)->setText(QString::number(project.records(Table::Classes).size()));
            QVERIFY(actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, created.source);
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            auto remove = actions->findChild<QPushButton *>("sceneActionRemove");
            remove->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, created.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            kind->setCurrentIndex(kind->findData(0xe));
            position->setCurrentIndex(position->count() - 1);
            add->click();
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            QVERIFY(preview->findChild<QLabel *>("scenePreviewContext")
                        ->text()
                        .contains("class-default equipment"));
            auto step = preview->findChild<QPushButton *>("sceneStep");
            auto status = preview->findChild<QLabel *>("scenePlaybackStatus");
            step->click();
            QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
            auto actors = preview->findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(actors->count(), 1);
            QCOMPARE(actors->currentData().toInt(), 5);
            const auto expectedState =
                QString("Class pair 84 / 85 · X 40.000 · Y 0.000 · Z 10.000");
            auto actorState = preview->findChild<QLabel *>("sceneActorState");
            QCOMPARE(actorState->text(), expectedState);
            step->click();
            const auto frame = preview->renderedFrame();
            QVERIFY(!frame.isNull());
            auto seek = preview->findChild<QSpinBox *>("scenePosition");
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(actors->count(), 0);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(actorState->text(), expectedState);
            QCOMPARE(preview->renderedFrame(), frame);
            QCoreApplication::processEvents();
            QVERIFY(preview->isVisible());
            QVERIFY(editor.grab().save("native-created-class-actor.png"));
            QPoint actorPixel(-1, -1);
            for (int y = 0; y < 240 && actorPixel.x() < 0; ++y)
                for (int x = 0; x < 320; ++x)
                    if (preview->actorAt({x, y}) == 5) {
                        actorPixel = {x, y};
                        break;
                    }
            QVERIFY(actorPixel.x() >= 0);
            actors->setCurrentIndex(-1);
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            const auto viewportPoint = view->mapFromScene(QPointF(actorPixel) + QPointF(.5, .5));
            const auto beforeClick = project.scenePrograms()->json();
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::ControlModifier, viewportPoint);
            QTest::mouseMove(view->viewport(), viewportPoint + QPoint(40, 0));
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::ControlModifier,
                                viewportPoint + QPoint(40, 0));
            QCOMPARE(actors->currentIndex(), -1); // Control-drag pans without editing.
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(actors->currentData().toInt(), 5);
            QCOMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(project.scenePrograms()->json(), beforeClick);
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(preview->actorAt(actorPixel), -1);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->actorAt(actorPixel), 5);
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(pages->currentWidget(), actions);
            QCOMPARE(field(0)->text(), QString("5"));
            QCOMPARE(field(1)->text(), QString("84"));
            const auto beforePosition = project.scenePrograms()->program(id).source;
            const int beforeUndo = undo.index();
            auto choose = actions->findChild<QPushButton *>("sceneActionChoosePosition");
            QVERIFY(choose->isVisible() && choose->isEnabled());
            choose->click();
            QCOMPARE(pages->currentWidget(), preview);
            QVERIFY(preview->findChild<QPushButton *>("sceneCancelPosition")->isVisible());
            preview->findChild<QPushButton *>("sceneCancelPosition")->click();
            QCOMPARE(undo.index(), beforeUndo);
            QCOMPARE(project.scenePrograms()->program(id).source, beforePosition);
            pages->setCurrentWidget(actions);
            choose->click();
            QCoreApplication::processEvents();
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                              view->mapFromScene(QPointF(170, 150)));
            QCOMPARE(pages->currentWidget(), actions);
            QCOMPARE(undo.index(), beforeUndo + 1);
            const auto placed = project.scenePrograms()->program(id);
            QVERIFY(placed.source != beforePosition);
            QCOMPARE(placed.commands[1].operands[5], created.commands[1].operands[5]);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, beforePosition);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, placed.source);
            pages->setCurrentWidget(preview);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(preview->renderedFrame() != frame);
            pages->setCurrentWidget(actions);
            position->setCurrentIndex(position->count() - 1);
            kind->setCurrentIndex(kind->findData(7));
            add->click();
            const int moveCommand = actions->findChild<QListWidget *>("sceneActionList")
                                        ->currentItem()
                                        ->data(Qt::UserRole)
                                        .toInt();
            choose->click();
            QCoreApplication::processEvents();
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                              view->mapFromScene(QPointF(150, 140)));
            QCOMPARE(pages->currentWidget(), actions);
            const auto destination = project.scenePrograms()->program(id);
            QCOMPARE(destination.commands[1].operands, placed.commands[1].operands);
            QCOMPARE(destination.commands[moveCommand].opcode, quint32(7));
            QVERIFY(destination.commands[moveCommand].operands[3] != 0 ||
                    destination.commands[moveCommand].operands[4] != 0);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopened.scenePrograms()->program(id).source,
                     project.scenePrograms()->program(id).source);
            // A pre-existing native retain sentinel stays opaque when placement changes.
            auto retained = *project.scenePrograms();
            retained.setOperand(id, 2, 5, quint32(-1));
            pushSceneProgramChange(&project, &undo, "Retain Actor body source", retained);
            actions->selectCommand(1);
            QVERIFY(field(2)->isReadOnly());
            field(4)->setText("41.000");
            QVERIFY(!actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[1].operands[4], quint32(41000));
            QCOMPARE(project.scenePrograms()->program(id).commands[2].operands[5], quint32(-1));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneActionInsertion() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            SceneId source;
            for (auto id : programs.scenes())
                if (programs.canCreateBlank(id)) {
                    source = id;
                    break;
                }
            QVERIFY(source);
            const auto id = programs.createScene("Desktop action authoring", source, true);
            project.setScenePrograms(programs.json());
            const auto blank = programs.program(id);
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 1000, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) {
                    scenes->setCurrentRow(i);
                    break;
                }
            auto actions = editor.findChild<SceneActionEditor *>("sceneActionEditor");
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            auto remove = actions->findChild<QPushButton *>("sceneActionRemove");
            QCOMPARE(position->count(), 1);
            QCOMPARE(position->currentData().toInt(), 3);
            kind->setCurrentIndex(kind->findData(0xe));
            QVERIFY(add->isEnabled());
            add->click();
            QCOMPARE(project.scenePrograms()->program(id).source,
                     SceneActions::insert(blank, 3, 0xe).source);
            auto duration = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(duration);
            duration->setText("7");
            const auto expected = SceneActions::insert(blank, 3, 0xe, 7).source;
            QCOMPARE(project.scenePrograms()->program(id).source, expected);
            duration->setText("-");
            QVERIFY(!add->isEnabled());
            QVERIFY(!remove->isEnabled());
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(remove->isEnabled());
            remove->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, expected);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(id).source, expected);
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            QVERIFY(editor.grab().save("native-action-insertion.png"));
            position->setCurrentIndex(position->count() - 1);
            project.loadDialogues();
            const auto originalText = project.dialogues()->json();
            const auto sharedLine = project.dialogues()->entry(Dialogues::Table::Primary, 5, 0);
            const auto priorScene = project.scenePrograms()->program(id).source;
            const auto undoCount = undo.index();
            kind->setCurrentIndex(kind->findData(0xbf));
            QVERIFY(add->isEnabled());
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                auto speaker = dialog->findChild<QLineEdit *>("sceneDialogueSpeaker");
                auto text = dialog->findChild<QPlainTextEdit *>("sceneDialogueText");
                auto buttons = dialog->findChild<QDialogButtonBox *>();
                speaker->setText("Joe");
                text->setPlainText("@");
                QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
                text->setPlainText("A new line with its own text entry.");
                QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
                buttons->button(QDialogButtonBox::Ok)->click();
            });
            add->click();
            QCOMPARE(undo.index(), undoCount + 1);
            QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 5, 0), sharedLine);
            undo.undo();
            QCOMPARE(project.dialogues()->json(), originalText);
            QCOMPARE(project.scenePrograms()->program(id).source, priorScene);
            undo.redo();
            const auto beforeId = project.scenePrograms()->program(id);
            int dialogueCommand = -1;
            for (const auto &a : SceneActions::inspect(beforeId))
                if (a.name == "Dialogue")
                    dialogueCommand = a.command;
            QVERIFY(dialogueCommand >= 0);
            auto windowId = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(windowId);
            QCOMPARE(windowId->accessibleName(), QString("Window ID"));
            windowId->setText("7");
            const auto connected = project.scenePrograms()->program(id);
            QCOMPARE(connected.commands[dialogueCommand].operands[0], quint32(7));
            QCOMPARE(connected.commands[dialogueCommand + 4].operands[2], quint32(7));
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, beforeId.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, connected.source);
            const int dialogueEntry = connected.commands[dialogueCommand].operands[2];
            QVERIFY(dialogueEntry > 0);
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                QCOMPARE(dialog->findChild<QLineEdit *>("sceneDialogueSpeaker")->text(),
                         QString("Joe"));
                dialog->findChild<QPlainTextEdit *>("sceneDialogueText")
                    ->setPlainText("Revised spoken dialogue.");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
            actions->findChild<QPushButton *>("sceneActionEditText")->click();
            QCOMPARE(Dialogues::spokenFields(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry))
                         ->second,
                     QString("Revised spoken dialogue."));
            undo.undo();
            undo.redo();
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                dialog->findChild<QPushButton *>("sceneDialogueRaw")->click();
            });
            actions->findChild<QPushButton *>("sceneActionEditText")->click();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            auto dialogue = qobject_cast<DialogueInspector *>(pages->currentWidget());
            QVERIFY(dialogue);
            QCOMPARE(dialogue->findChild<QComboBox *>("dialogueArchives")->currentData().toInt(),
                     5);
            QCOMPARE(dialogue->findChild<QListWidget *>("dialogueEntries")->currentRow(),
                     dialogueEntry);
            dialogue->findChild<QPlainTextEdit *>("dialogueSource")
                ->setPlainText("Desktop dialogue action.");
            QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry),
                     QByteArray("Desktop dialogue action."));
            project.saveFolder(work.path());
            auto withDialogue = Project::fromRom(project.rom());
            withDialogue.applyManifest(Project::readProject(work.path()));
            QCOMPARE(withDialogue.scenePrograms()->program(id).source, connected.source);
            QCOMPARE(withDialogue.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry),
                     QByteArray("Desktop dialogue action."));
            pages->setCurrentWidget(actions);
            QTRY_VERIFY(actions->findChild<QLineEdit *>("sceneActionField0"));
            QTRY_VERIFY(actions->findChild<QLineEdit *>("sceneActionField0")->isVisible());
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("7"));
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-dialogue-action.png"));
            position->setCurrentIndex(position->findData(3));
            auto move = actions->findChild<QPushButton *>("sceneActionMove");
            QVERIFY(move->isEnabled());
            const auto reordered = SceneActions::move(connected, dialogueCommand, 3);
            move->click();
            QCOMPARE(project.scenePrograms()->program(id).source, reordered.source);
            QCOMPARE(actions->findChild<QListWidget *>("sceneActionList")
                         ->currentItem()
                         ->data(Qt::UserRole)
                         .toInt(),
                     3);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, connected.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, reordered.source);
            project.saveFolder(work.path());
            auto orderedProject = Project::fromRom(project.rom());
            orderedProject.applyManifest(Project::readProject(work.path()));
            QCOMPARE(orderedProject.scenePrograms()->program(id).source, reordered.source);
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-action-order.png"));
            kind->setCurrentIndex(kind->findData(0x14));
            add->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[3].opcode, quint32(0x14));
            position->setCurrentIndex(position->findData(4));
            kind->setCurrentIndex(kind->findData(0x15));
            add->click();
            auto direction = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(direction);
            QCOMPARE(direction->accessibleName(), QString("Turn phase direction"));
            const auto beforeTurn = project.scenePrograms()->program(id).source;
            direction->setText("0");
            QVERIFY(actions->property("invalidEdits").toBool());
            QVERIFY(!add->isEnabled());
            QVERIFY(!move->isEnabled());
            QCOMPARE(project.scenePrograms()->program(id).source, beforeTurn);
            direction->setText("-1");
            QVERIFY(!actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[4].operands[3], quint32(-1));
            position->setCurrentIndex(position->findData(5));
            kind->setCurrentIndex(kind->findData(0x1c));
            add->click();
            auto channel = actions->findChild<QLineEdit *>("sceneActionField1");
            QVERIFY(channel);
            QCOMPARE(channel->accessibleName(), QString("Drawing channel"));
            channel->setText("3");
            QCOMPARE(project.scenePrograms()->program(id).commands[5].operands[1], quint32(3));
            const auto actorControls = project.scenePrograms()->program(id);
            QCOMPARE(actorControls.commands[4].operands[0], actorControls.commands[3].operands[0]);
            QCOMPARE(actorControls.commands[5].operands[0], actorControls.commands[3].operands[0]);
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-actor-actions.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void bodyActionEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        const SceneId key(0x1fa8bf4 - 0x594284);
        const auto original = project.scenePrograms()->program(key);
        int command = -1;
        for (int i = 0; i < original.commands.size(); ++i)
            if (original.commands[i].opcode == 0x2a) {
                command = i;
                break;
            }
        QVERIFY(command >= 0);
        QUndoStack undo;
        SceneActionEditor editor(&project, &undo, [] {});
        editor.resize(1100, 700);
        editor.show();
        editor.selectScene(key, command);
        auto primary = editor.findChild<QLineEdit *>("sceneActionField1");
        auto secondary = editor.findChild<QLineEdit *>("sceneActionField2");
        auto pose = editor.findChild<QLineEdit *>("sceneActionField3");
        QVERIFY(primary && secondary && pose);
        QVERIFY(primary->isReadOnly() && secondary->isReadOnly());
        QVERIFY(primary->toolTip().contains("launch event"));
        QVERIFY(!pose->isReadOnly());
        const int changed = original.commands[command].operands[2] == 0 ? 1 : 0;
        pose->setText(QString::number(changed));
        QMetaObject::invokeMethod(pose, "editingFinished");
        QCOMPARE(undo.count(), 1);
        auto expected = original.source;
        put32(expected, (original.commands[command].word + 3) * 4, quint32(changed));
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, original.source);
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        editor.selectScene(key, command);
        QCoreApplication::processEvents();
        QTRY_VERIFY(editor.findChild<QLineEdit *>("sceneActionField3")->isVisible());
        QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField3")->text(), QString::number(changed));
        QVERIFY(editor.findChild<QLineEdit *>("sceneActionField1")->text().startsWith("Launch value "));
        QVERIFY(editor.grab().save("native-body-action.png"));
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.scenePrograms()->program(key).source, expected);
    }
    void structuredSceneActions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 1000, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole).toUInt() == 0x1a14970) { scenes->setCurrentRow(i); break; }
            const auto source = project.scenePrograms()->program(0x1a14970);
            int command = -1;
            for (const auto &a : SceneActions::inspect(source)) if (a.name == "Move Actor") { command = a.command; break; }
            QVERIFY(command >= 0);
            auto actions = editor.findChild<SceneActionEditor *>("sceneActionEditor");
            QVERIFY(actions);
            auto list = actions->findChild<QListWidget *>("sceneActionList");
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toInt() == command) { list->setCurrentRow(i); break; }
            auto field = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(field); QCOMPARE(field->accessibleName(), QString("Target X"));
            QCOMPARE(editor.findChild<QTableWidget *>("cutsceneCommands")->currentRow(), command);
            field->setText("-12.375");
            auto expected = source.source; put32(expected, source.commands[command].word * 4 + 16, quint32(-12375));
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QCOMPARE(editor.findChild<QLineEdit *>("sceneOperand3")->text(), QString("-12375"));
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, source.source);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            field = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(field); QCOMPARE(field->text(), QString("-12.375"));
            field->setText("-"); QVERIFY(actions->property("invalidEdits").toBool());
            QVERIFY(!scenes->isEnabled());
            QVERIFY(!editor.findChild<SceneCommandEditor *>("sceneCommandEditor")->isEnabled());
            QVERIFY(!editor.findChild<QComboBox *>("cutsceneBackgroundChoice")->isEnabled());
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!actions->property("invalidEdits").toBool()); QVERIFY(scenes->isEnabled());
            auto raw = editor.findChild<QLineEdit *>("sceneOperand3");
            raw->setText("-"); QVERIFY(!actions->isEnabled());
            editor.findChild<QPushButton *>("sceneCommandDiscard")->click(); QVERIFY(actions->isEnabled());
            QTemporaryDir work; project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            QVERIFY(editor.grab().save("native-scene-actions.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void cutsceneBackgroundSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850); editor.show();
            auto list = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1000, 10000);
            int row = -1;
            for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole).toUInt() == 0x1a14970) row = i;
            QVERIFY(row >= 0); list->setCurrentRow(row);
            auto choice = editor.findChild<QComboBox *>("cutsceneBackgroundChoice");
            auto apply = editor.findChild<QPushButton *>("cutsceneBackgroundApply");
            auto restore = editor.findChild<QPushButton *>("cutsceneBackgroundRestore");
            QCOMPARE(choice->count(), 63); QCOMPARE(choice->currentData().toInt(), 34);
            const auto original = project.scenePrograms()->program(0x1a14970).source;
            choice->setCurrentIndex(choice->findData(57));
            QVERIFY(apply->isEnabled()); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            auto images = editor.findChild<QComboBox *>("cutsceneImages");
            QVERIFY(images->findData("archive:49") >= 0);
            apply->click();
            auto expected = original; put32(expected, 4, 57);
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QCOMPARE(choice->currentData().toInt(), 57); QVERIFY(!apply->isEnabled()); QVERIFY(restore->isEnabled());
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            QCOMPARE(choice->currentData().toInt(), 34);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            restore->click(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QTemporaryDir work; project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QTableWidget *>("cutsceneCommands")->setCurrentCell(0, 0);
            auto operand = editor.findChild<QLineEdit *>("sceneOperand0");
            QVERIFY(operand); operand->setText("-");
            QVERIFY(!choice->isEnabled() && !apply->isEnabled() && !restore->isEnabled());
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QPushButton *>("sceneCommandDiscard")->click();
            QVERIFY(choice->isEnabled() && restore->isEnabled());
            auto catalog = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
            catalog->setCurrentIndex(1);
            QVERIFY(!choice->isEnabled() && !apply->isEnabled() && !restore->isEnabled());
            catalog->setCurrentIndex(0);
            QCOMPARE(choice->currentData().toInt(), 57);
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QTRY_VERIFY_WITH_TIMEOUT(!editor.property("displayedImage").toString().isEmpty(), 30000);
            QVERIFY(editor.grab().save("native-background-selection.png"));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void projectChangeReview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            QTemporaryDir dir;
            const auto source = dir.filePath("source.v64");
            QVERIFY(QFile::copy(path, source));
            auto project = Project::fromRom(Rom::open(source));
            project.setClassNameBytes(2, encodeClassName("Review Fighter"));
            const auto before = project.manifest();
            const auto expected = project.changeReport().text();
            ChangeReportDialog dialog(project);
            dialog.show();
            auto copy = dialog.findChild<QPushButton *>("copyChangeReport");
            QTRY_VERIFY_WITH_TIMEOUT(copy->isEnabled(), 30000);
            auto text = dialog.findChild<QPlainTextEdit *>("changeReportText");
            QCOMPARE(text->toPlainText(), expected);
            copy->click();
            QCOMPARE(QApplication::clipboard()->text(), expected);
            auto search = dialog.findChild<QLineEdit *>("changeReportSearch");
            search->setText("Review Fighter");
            QCOMPARE(text->textCursor().selectedText(), QString("Review Fighter"));
            const auto output = dir.filePath("changes.txt");
            QVERIFY_THROWS_EXCEPTION(Error, dialog.saveReport(source));
            QCOMPARE(Rom::open(source).bytes(), project.rom().bytes());
            dialog.saveReport(output);
            QFile saved(output); QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), expected.toUtf8());
            QCOMPARE(project.manifest(), before);
            auto entries = dialog.findChild<QTreeWidget *>("changeReportEntries");
            QVERIFY(entries && entries->topLevelItemCount() > 0);
            QTreeWidgetItem *classChange = nullptr;
            for (int i = 0; i < entries->topLevelItemCount(); ++i) {
                const auto row = entries->topLevelItem(i);
                const auto target = row->data(0, Qt::UserRole).value<EditLocation>();
                if (target.editor == "classes" && target.id == 2) classChange = row;
            }
            QVERIFY(classChange); entries->setCurrentItem(classChange);
            QVERIFY(dialog.grab().save("project-changes.png"));
            QSignalSpy jump(&dialog, &ChangeReportDialog::editRequested);
            dialog.findChild<QPushButton *>("openChangeEdit")->click();
            QCOMPARE(jump.count(), 1);
            QCOMPARE(jump.at(0).at(0).value<EditLocation>().id, 2);
            dialog.close();
            MainWindow window;
            auto action = window.findChild<QAction *>("reviewProjectChanges");
            QVERIFY(action); QVERIFY(!action->isEnabled());
            window.loadRom(source);
            QTRY_VERIFY_WITH_TIMEOUT(action->isEnabled(), 10000);
            window.show();
            auto classes = window.findChild<QTableView *>("classesTable");
            classes->sortByColumn(1, Qt::DescendingOrder);
            window.navigateTo({"classes", 2});
            QVERIFY(classes->isVisible());
            QCOMPARE(classes->currentIndex().siblingAtColumn(0).data(Qt::EditRole).toInt(), 2);
            window.navigateTo({"shops", 35});
            auto shops = window.findChild<QListWidget *>("shopList");
            QVERIFY(shops->isVisible());
            QCOMPARE(shops->currentItem()->data(Qt::UserRole).toInt(), 35);
            bool opened = false;
            QTimer::singleShot(0, &window, [&] {
                const auto review = window.findChild<ChangeReportDialog *>("projectChanges");
                opened = review != nullptr;
                if (review) review->reject();
            });
            action->trigger();
            QVERIFY(opened);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void portraitDitherAssignment() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            QImage source(40, 48, QImage::Format_RGBA8888);
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 40; ++x)
                    source.setPixelColor(x, y, {x * 255 / 39, y * 255 / 47, (x * 7 + y * 3) % 256});
            const auto asset = SpriteAsset::fromImage("Dither example", source);
            auto library = project.sprites();
            library.replace(asset);
            project.setSprites(library);
            PortraitEditor editor(&project, &undo, [] {});
            editor.resize(1250, 790);
            editor.show();
            editor.selectPortrait(1, 0);
            auto choices = editor.findChild<QComboBox *>("portraitLibrary");
            choices->setCurrentIndex(choices->findData(asset.id));
            const auto before = project.avatarArtwork()->json();
            const auto expected = AvatarArtwork::mapImage(source, true, Qt::black, true);
            bool visited = false;
            const auto visit = [&](bool accept) {
                QTimer::singleShot(0, &editor, [&, accept] {
                    auto dialog = editor.findChild<QDialog *>("portraitImagePreparation");
                    QVERIFY(dialog);
                    auto cleanup = qScopeGuard([&] { dialog->reject(); });
                    auto check = dialog->findChild<QCheckBox *>("portraitImportDither");
                    QVERIFY(check);
                    QVERIFY(!check->isChecked());
                    check->setChecked(true);
                    const auto expectedPixmap =
                        QPixmap::fromImage(AvatarArtwork::render(expected).scaled(200, 240));
                    bool matches = false;
                    for (auto label : dialog->findChildren<QLabel *>())
                        if (!label->pixmap().isNull() &&
                            label->pixmap().toImage() == expectedPixmap.toImage())
                            matches = true;
                    QVERIFY(matches);
                    QCOMPARE(project.avatarArtwork()->json(), before);
                    QCOMPARE(undo.count(), 0);
                    QVERIFY(dialog->grab().save("portrait-dither-preparation.png"));
                    visited = true;
                    if (accept) {
                        cleanup.dismiss();
                        dialog->accept();
                    }
                });
                editor.findChild<QPushButton *>("portraitAssignLibrary")->click();
            };
            visit(false);
            QVERIFY(visited);
            QCOMPARE(project.avatarArtwork()->json(), before);
            QCOMPARE(undo.count(), 0);
            visited = false;
            visit(true);
            QVERIFY(visited);
            QCOMPARE(project.avatarArtwork()->words(project.classData(), 1, 0), expected);
            QCOMPARE(undo.count(), 1);
            undo.undo();
            QCOMPARE(project.avatarArtwork()->json(), before);
            undo.redo();
            QCOMPARE(project.avatarArtwork()->words(project.classData(), 1, 0), expected);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.avatarArtwork()->words(restored.classData(), 1, 0), expected);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void spriteVideoExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.resize(1400, 900); editor.show();
            auto asset = SpriteAsset::blank("Video export", {32, 32});
            asset.frames[0].layers[0].image.fill(Qt::red); asset.frames[0].ticks = 3;
            editor.addAsset(asset);
            const auto original = project.sprites().json(); const int commands = undo.count();
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteVideoOptions"); QVERIFY(dialog);
                dialog->findChild<QSpinBox *>("spriteVideoScale")->setValue(2);
                dialog->findChild<QSpinBox *>("spriteVideoRepeats")->setValue(2);
                QVERIFY(dialog->grab().save("sprite-video-options.png"));
                dialog->reject();
            });
            auto menu = editor.findChild<QPushButton *>("spriteExport")->menu();
            const auto actions = menu->actions();
            const auto action = std::find_if(actions.cbegin(), actions.cend(),
                                              [](QAction *a) { return a->text().startsWith("MP4 video"); });
            QVERIFY(action != actions.cend()); (*action)->trigger();
            QTemporaryDir work;
            QVERIFY(editor.exportVideo(work.filePath("sprite.mp4"), {2, 2, Qt::black}));
            QVERIFY(QFileInfo(work.filePath("sprite.mp4")).size() > 100);
            QCOMPARE(project.sprites().json(), original); QCOMPARE(undo.count(), commands);
            QVERIFY(!editor.findChild<QDialog *>("spriteVideoProgress"));
            QVERIFY(editor.exportVideo(work.filePath("sprite.apng"), {1, 0, Qt::black, SpriteVideoFormat::AnimatedPng}));
            const auto preview = QImage(work.filePath("sprite.apng")).convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(preview.size(), asset.size);
            const auto expected = asset.composite(0);
            QCOMPARE(QByteArray(reinterpret_cast<const char *>(preview.constBits()), preview.sizeInBytes()),
                     QByteArray(reinterpret_cast<const char *>(expected.constBits()), expected.sizeInBytes()));
            QCOMPARE(project.sprites().json(), original); QCOMPARE(undo.count(), commands);
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void spriteLayerImagePreparation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {}); editor.resize(1400, 900); editor.show();
            auto original = SpriteAsset::blank("Prepared layer", {6, 4});
            original.kind = "sequence";
            auto &first = original.frames.first();
            first.layers.first().position = {-2, 3};
            auto other = first.layers.first(); other.id = SpriteLibrary::newId(); other.image.fill(Qt::blue);
            first.layers << other;
            auto second = first; second.id = SpriteLibrary::newId(); original.frames << second;
            editor.addAsset(original);
            editor.findChild<QListWidget *>("spriteLayers")->setCurrentRow(1);
            QImage source(2, 2, QImage::Format_RGBA8888);
            source.setPixelColor(0, 0, QColor(120, 40, 20, 128)); source.setPixelColor(1, 0, Qt::green);
            source.setPixelColor(0, 1, QColor(9, 8, 7, 0)); source.setPixelColor(1, 1, Qt::red);
            QTemporaryDir work; const auto imagePath = work.filePath("source.png"); QVERIFY(source.save(imagePath));
            const int initialCommands = undo.count();
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                dialog->reject();
            });
            editor.importLayerImage(imagePath);
            QCOMPARE(undo.count(), initialCommands); QCOMPARE(project.sprites().asset(original.id), original);
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                dialog->findChild<QComboBox *>("spriteLayerImportFit")->setCurrentIndex(3);
                dialog->findChild<QSpinBox *>("spriteLayerImportX")->setValue(1);
                QVERIFY(dialog->grab().save("sprite-layer-preparation.png"));
                dialog->accept();
            });
            editor.importLayerImage(imagePath);
            QCOMPARE(undo.count(), initialCommands + 1);
            const auto changed = project.sprites().asset(original.id);
            QCOMPARE(changed.frames[1], original.frames[1]);
            QCOMPARE(changed.frames[0].layers[1], original.frames[0].layers[1]);
            QCOMPARE(changed.frames[0].layers[0].position, QPoint(-2, 3));
            QCOMPARE(changed.frames[0].layers[0].id, original.frames[0].layers[0].id);
            const auto result = changed.frames[0].layers[0].image;
            QCOMPARE(result.size(), QSize(6, 4));
            for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
                QCOMPARE(result.pixelColor(3 + x, 1 + y), source.pixelColor(x, y));
            QCOMPARE(result.pixelColor(0, 0), QColor(Qt::transparent));
            undo.undo(); QCOMPARE(project.sprites().asset(original.id), original);
            undo.redo(); QCOMPARE(project.sprites().asset(original.id), changed);
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.sprites().asset(original.id), changed);
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                undo.undo(); dialog->accept();
            });
            QVERIFY_THROWS_EXCEPTION(Error, editor.importLayerImage(imagePath));
            QCOMPARE(project.sprites().asset(original.id), original);
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void actorPoseLibraryTransfer() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QUndoStack undo;
            CutsceneEditor editor(rom, nullptr, &project, &undo, [] {});
            editor.resize(1500, 1050); editor.show();
            auto inspector = editor.findChild<ActorPoseEditor *>(); QVERIFY(inspector);
            for (auto tabs : editor.findChildren<QTabWidget *>())
                if (tabs->indexOf(inspector) >= 0) tabs->setCurrentWidget(inspector);
            auto kind = inspector->findChild<QComboBox *>("actorSourceKind");
            auto bank = inspector->findChild<QComboBox *>("actorBank");
            auto pose = inspector->findChild<QComboBox *>("actorPose");
            auto copy = inspector->findChild<QPushButton *>("actorCopyToLibrary");
            QVERIFY(copy && copy->isVisible());
            bank->setCurrentIndex(bank->findData(30));
            pose->setCurrentIndex(pose->findData(12));
            inspector->findChild<QSpinBox *>("actorVariant")->setValue(3);
            copy->click();
            QCOMPARE(project.sprites().assets().size(), 1);
            const auto first = project.sprites().assets().first();
            const auto expected = spriteAssetFromPose(rom, ActorArt::load(rom, 30), 12, "Expected", 3);
            QCOMPARE(first.provenance["variant"].toInt(), 3);
            QCOMPARE(first.frames.size(), expected.frames.size());
            for (int f = 0; f < first.frames.size(); ++f) QCOMPARE(first.composite(f), expected.composite(f));
            kind->setCurrentIndex(1); bank->setCurrentIndex(bank->findData(2));
            pose->setCurrentIndex(pose->findData(40));
            auto equipment = inspector->findChild<QComboBox *>("actorEquipment");
            equipment->setCurrentIndex(equipment->findData(11));
            copy->click();
            QCOMPARE(project.sprites().assets().size(), 2);
            const auto second = project.sprites().assets().last();
            QCOMPARE(second.provenance["equipment"].toInt(), 11);
            const auto art = ActorArt::loadBody(rom, 2, 2, 0, 0, project.bodyAnimationData());
            const auto body = spriteAssetFromPose(rom, art, 40, "Expected", 0, BodyAppearance{11, {}});
            QCOMPARE(second.frames.size(), body.frames.size());
            for (int f = 0; f < second.frames.size(); ++f) QCOMPARE(second.composite(f), body.composite(f));
            undo.undo(); QCOMPARE(project.sprites().assets().size(), 1);
            QCOMPARE(project.sprites().assets().first(), first);
            undo.redo(); QCOMPARE(project.sprites().assets().last(), second);
            QTemporaryDir work; project.saveFolder(work.path());
            auto restored = Project::fromRom(rom); restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.sprites().json(), project.sprites().json());
            QVERIFY(inspector->grab().save("actor-library-transfer.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void bodyConstructionPolicies() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {}); editor.selectClass(135);
            auto construction = editor.findChild<QComboBox *>("animationConstruction");
            auto fixed = editor.findChild<QCheckBox *>("animationFixedPose");
            QVERIFY(construction); QVERIFY(fixed);
            QCOMPARE(construction->currentIndex(), 1); QVERIFY(fixed->isChecked());
            construction->setCurrentIndex(2); fixed->setChecked(false);
            QCOMPARE(project.animationRoutes()->entry(135).construction, 2);
            QVERIFY(!project.animationRoutes()->entry(135).fixedPose);
            undo.undo(); QVERIFY(fixed->isChecked()); undo.undo(); QCOMPARE(construction->currentIndex(), 1);
            undo.redo(); undo.redo();
            const int id = project.appendClass(135, "Body policy clone");
            editor.selectClass(id); QCOMPARE(construction->currentIndex(), 2); QVERIFY(!fixed->isChecked());
            auto source = editor.findChild<QComboBox *>("animationSource");
            source->setCurrentIndex(source->findData(135));
            // Copying an ordinary set clears both inherited special policies.
            source->setCurrentIndex(source->findData(30));
            editor.findChild<QPushButton *>("animationCopySource")->click();
            QCOMPARE(construction->currentIndex(), 0); QVERIFY(!fixed->isChecked());
            undo.undo(); QCOMPARE(construction->currentIndex(), 2);
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(Rom::open(path)); restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationRoutes()->entry(id).construction, 2);
            QVERIFY(!restored.animationRoutes()->entry(id).fixedPose);
            editor.resize(1250, 850); editor.show();
            QVERIFY(editor.grab().save("body-construction-policies.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void equipmentArtworkPolicies() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {}); editor.selectClass(57);
            auto items = editor.findChild<QComboBox *>("animationEquipmentArt");
            auto children = editor.findChild<QCheckBox *>("animationEquipmentChildren");
            QVERIFY(items); QVERIFY(children);
            QCOMPARE(items->currentData().toInt(), 2); QVERIFY(children->isChecked());
            items->setCurrentIndex(items->findData(11)); children->setChecked(false);
            QCOMPARE(project.animationRoutes()->entry(57).equipmentArt, qint16(11));
            QVERIFY(!project.animationRoutes()->entry(57).equipmentChildArt);
            undo.undo(); QVERIFY(children->isChecked()); undo.undo(); QCOMPARE(items->currentData().toInt(), 2);
            undo.redo(); undo.redo();
            const int id = project.appendClass(57, "Equipment art clone");
            editor.selectClass(id); QCOMPARE(items->currentData().toInt(), 11); QVERIFY(!children->isChecked());
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(Rom::open(path)); restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationRoutes()->entry(id).equipmentArt, qint16(11));
            QVERIFY(!restored.animationRoutes()->entry(id).equipmentChildArt);
            editor.resize(1250, 800); editor.show();
            QVERIFY(editor.grab().save("equipment-artwork-policies.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void stagedAnimationPreviewAndUndo() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);QUndoStack undo;
            CutsceneEditor cutscenes(rom,nullptr,&project,&undo,[]{});
            QTRY_VERIFY(cutscenes.findChild<QListWidget *>("cutsceneScenes")->count()>0);
            AnimationEditor edits(&project,&undo,[]{});
            AnimationOverrideEditor overrides(&project,&undo,[]{});
            RecordModel classEditor(&project,Table::Classes,&undo);
            auto inspector=cutscenes.findChild<ActorPoseEditor *>();QVERIFY(inspector);
            auto kind=inspector->findChild<QComboBox *>("actorSourceKind"),bank=inspector->findChild<QComboBox *>("actorBank");
            kind->setCurrentIndex(1);bank->setCurrentIndex(bank->findData(2));
            auto position=inspector->findChild<QSpinBox *>("actorPosition");position->setValue(1);
            auto view=inspector->findChild<QGraphicsView *>("actorCanvas");
            auto flat=[&]{QImage image(320,240,QImage::Format_RGBA8888);image.fill(Qt::transparent);QPainter painter(&image);view->scene()->render(&painter);return image;};
            const auto original=flat();
            edits.selectClass(2);auto source=edits.findChild<QComboBox *>("animationSource");
            source->setCurrentIndex(source->findData(3));edits.findChild<QPushButton *>("animationCopySource")->click();
            QCOMPARE(position->value(),1);const auto changed=flat();QVERIFY(changed!=original);
            undo.undo();QCOMPARE(flat(),original);QCOMPARE(position->value(),1);undo.redo();QCOMPARE(flat(),changed);
            undo.undo();
            auto cls=overrides.findChild<QComboBox *>("overrideClass"),action=overrides.findChild<QComboBox *>("overrideAction");
            cls->setCurrentIndex(cls->findData(2));action->setCurrentIndex(action->findData(5));
            auto normal=overrides.findChild<QComboBox *>("overrideNormal"),blocked=overrides.findChild<QComboBox *>("overrideBlocked");
            normal->setCurrentIndex(normal->findData(40));blocked->setCurrentIndex(blocked->findData(41));
            overrides.findChild<QPushButton *>("overrideAdd")->click();
            QCOMPARE(project.animationOverrides()->entries().size(),1);QVERIFY2(overrides.findChild<QLabel *>("overrideNotice")->text().isEmpty(),qPrintable(overrides.findChild<QLabel *>("overrideNotice")->text()));
            auto previewAction=inspector->findChild<QComboBox *>("actorAttackAction"),mode=inspector->findChild<QComboBox *>("actorAttackMode"),pose=inspector->findChild<QComboBox *>("actorPose");
            previewAction->setCurrentIndex(previewAction->findData(5));QCOMPARE(pose->currentData().toInt(),40);
            mode->setCurrentIndex(2);QCOMPARE(pose->currentData().toInt(),41);position->setValue(1);const auto blockedFrame=flat();
            undo.undo();QCOMPARE(pose->currentData().toInt(),project.bodyAnimationData()->selector(2,2,5,2,0));QCOMPARE(position->value(),1);
            undo.redo();QCOMPARE(pose->currentData().toInt(),41);QCOMPARE(flat(),blockedFrame);
            const int id=classEditor.appendClass(2,"Preview class");QCOMPARE(id,165);
            bank->setCurrentIndex(bank->findData(id));QCOMPARE(bank->currentData().toInt(),id);QCOMPARE(pose->currentData().toInt(),41);
            position->setValue(1);auto projected=inspector->findChild<QCheckBox *>("actorProjected");projected->setChecked(true);
            QVERIFY(!inspector->property("placementFrame").value<QImage>().isNull());
            undo.undo();QVERIFY(bank->findData(id)<0);undo.redo();QVERIFY(bank->findData(id)>=0);
            // A body-route edit must invalidate captured scene frames and replay to the same tick.
            auto scenes=ScenePrograms::load(rom);const quint32 key=0x1fa8bf4-0x594284;auto program=scenes.program(key);
            for(auto &command:program.commands)if(command.opcode==0x1b){command.operands[4]=0;command.operands[5]=0;break;}
            scenes.replace(key,program.encode());auto preview=cutscenes.findChild<ScenePreview *>();QVERIFY(preview);
            preview->selectScene(scenes,key,2);auto scenePosition=preview->findChild<QSpinBox *>("scenePosition");
            scenePosition->setValue(3);QTRY_COMPARE(preview->property("sceneTick").toInt(),3);const auto sceneOriginal=preview->renderedFrame();
            edits.selectClass(0x58);source->setCurrentIndex(source->findData(2));edits.findChild<QPushButton *>("animationCopySource")->click();
            QTRY_COMPARE(preview->property("sceneTick").toInt(),3);const auto sceneChanged=preview->renderedFrame();QVERIFY(sceneChanged!=sceneOriginal);
            scenePosition->setValue(1);QTRY_COMPARE(preview->property("sceneTick").toInt(),1);scenePosition->setValue(3);QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneChanged);
            undo.undo();QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneOriginal);
            undo.redo();QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneChanged);
            bank->setCurrentIndex(bank->findData(id));position->setValue(1);inspector->resize(1280,850);inspector->show();QVERIFY(inspector->grab().save("staged-animation-preview.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }

    void attackAnimationOverrides() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty()) QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationEditor animations(&project, &undo, []{});
            AnimationOverrideEditor editor(&project, &undo, []{});
            RecordModel classes(&project, Table::Classes, &undo);
            editor.resize(1100, 820); editor.show();
            auto cls = editor.findChild<QComboBox *>("overrideClass");
            auto action = editor.findChild<QComboBox *>("overrideAction");
            auto route = editor.findChild<QComboBox *>("overrideRoute");
            auto normal = editor.findChild<QComboBox *>("overrideNormal");
            auto blocked = editor.findChild<QComboBox *>("overrideBlocked");
            auto rows = editor.findChild<QTableWidget *>("overrideRows");
            auto add = editor.findChild<QPushButton *>("overrideAdd");
            auto update = editor.findChild<QPushButton *>("overrideUpdate");
            auto remove = editor.findChild<QPushButton *>("overrideRemove");
            auto clear = editor.findChild<QPushButton *>("overrideClear");
            auto notice = editor.findChild<QLabel *>("overrideNotice");
            QVERIFY(cls && action && route && normal && blocked && rows && add && update && remove && clear && notice);
            cls->setCurrentIndex(cls->findData(2));
            action->setCurrentIndex(action->findData(5));
            QVERIFY(normal->findData(40) >= 0); QVERIFY(blocked->findData(41) >= 0);
            normal->setCurrentIndex(normal->findData(40));
            blocked->setCurrentIndex(blocked->findData(41));
            add->click(); QCOMPARE(rows->rowCount(), 1); QCOMPARE(undo.count(), 1);
            QJsonObject expected{{"classId",2},{"actionId",5},{"normalSelector",40},{"blockedSelector",41}};
            QCOMPARE(project.animationOverrides()->entries()[0].toObject(), expected);
            normal->setCurrentIndex(normal->findData(41)); update->click();
            expected["normalSelector"] = 41;
            QCOMPARE(project.animationOverrides()->entries()[0].toObject(), expected);
            undo.undo(); QCOMPARE(normal->currentData().toInt(), 40);
            undo.redo(); QCOMPARE(normal->currentData().toInt(), 41);
            const auto beforeBad = project.animationOverrides()->json(); const int count = undo.count();
            add->click(); QCOMPARE(project.animationOverrides()->json(), beforeBad); QCOMPARE(undo.count(), count);
            QVERIFY(!notice->text().isEmpty());
            action->setEditText("No such action"); update->click();
            QCOMPARE(project.animationOverrides()->json(), beforeBad); QVERIFY(notice->text().contains("search results"));
            action->setCurrentIndex(action->findData(5));
            // Clearing a selection must not reselect its still-current table row.
            cls->setCurrentIndex(cls->findData(1)); QCOMPARE(cls->currentData().toInt(), 1);
            QVERIFY(rows->selectedItems().isEmpty()); update->click();
            QCOMPARE(project.animationOverrides()->json(), beforeBad);
            cls->setCurrentIndex(cls->findData(2)); route->setCurrentIndex(route->findData(0));
            QCOMPARE(route->currentData().toInt(), 0); normal->setCurrentIndex(normal->findData(40));
            blocked->setCurrentIndex(blocked->findData(41)); add->click();
            QCOMPARE(rows->rowCount(), 2); QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            const auto twoRows = project.animationOverrides()->json();
            remove->click(); QCOMPARE(rows->rowCount(), 1); undo.undo();
            QCOMPARE(project.animationOverrides()->json(), twoRows);
            const int id = classes.appendClass(2, "Override test"); QCOMPARE(id, 165);
            QCOMPARE(rows->rowCount(), 4); QVERIFY(cls->findData(id) >= 0);
            animations.selectClass(id); cls->setCurrentIndex(cls->findData(id));
            undo.undo(); QCOMPARE(rows->rowCount(), 2); QCOMPARE(cls->findData(id), -1);
            QVERIFY(animations.findChild<QComboBox *>("animationClass")->findData(id) < 0);
            undo.redo(); QCOMPARE(rows->rowCount(), 4); QVERIFY(cls->findData(id) >= 0);
            const auto fourRows = project.animationOverrides()->json();
            clear->click(); QCOMPARE(rows->rowCount(), 0); undo.undo();
            QCOMPARE(project.animationOverrides()->json(), fourRows); QCOMPARE(rows->rowCount(), 4);
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopened.animationOverrides()->json(), fourRows);
            rows->selectRow(0); QVERIFY(editor.grab().save("attack-overrides-workspace.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }

    void classAnimationAssignments() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            const auto original = AnimationRoutes::load(rom);
            auto project = Project::fromRom(rom);
            const int id = project.appendClass(2, "Animation test");
            auto missing = *project.animationRoutes();
            missing.requireAssignment(id);
            project.setAnimationRoutes(missing.json());
            QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {});
            editor.resize(1100, 820);
            editor.show();
            editor.selectClass(id);
            auto sources = editor.findChild<QComboBox *>("animationSource");
            auto copy = editor.findChild<QPushButton *>("animationCopySource");
            auto handle = editor.findChild<QSpinBox *>("animationHandle0");
            auto speed = editor.findChild<QSpinBox *>("animationSpeed0");
            auto action = editor.findChild<QComboBox *>("animationAction");
            auto actions = editor.findChild<QTableWidget *>("animationActions");
            auto add = editor.findChild<QPushButton *>("animationAddAction");
            auto update = editor.findChild<QPushButton *>("animationUpdateAction");
            auto remove = editor.findChild<QPushButton *>("animationRemoveAction");
            auto notice = editor.findChild<QLabel *>("animationNotice");
            QVERIFY(sources && copy && handle && speed && action && actions && add && update &&
                    remove && notice);
            QVERIFY(!handle->isEnabled());
            QVERIFY(notice->text().contains("needs an animation source"));
            sources->setEditText("No such animation source");
            copy->click();
            QVERIFY(project.animationRoutes()->pending().contains(id));
            QVERIFY(notice->text().contains("search results"));
            sources->setCurrentIndex(sources->findData(3));
            copy->click();
            QVERIFY(project.animationRoutes()->entry(id) == original.entry(3));
            QVERIFY(handle->isEnabled());
            undo.undo();
            QVERIFY(project.animationRoutes()->pending().contains(id));
            QVERIFY(!handle->isEnabled());
            undo.redo();
            QVERIFY(project.animationRoutes()->entry(id) == original.entry(3));
            handle->setValue(original.row(2)[0]);
            speed->setValue(37);
            QCOMPARE(project.animationRoutes()->row(id)[0], original.row(2)[0]);
            QCOMPARE(project.animationRoutes()->entry(id).speeds[0], 37);
            const auto beforeBad = project.animationRoutes()->json();
            handle->setValue(65535);
            QCOMPARE(project.animationRoutes()->json(), beforeBad);
            QVERIFY(notice->text().contains("missing descriptor"));
            QCOMPARE(handle->value(), int(original.row(2)[0]));
            action->setCurrentIndex(action->findData(200));
            for (int mode = 0; mode < 3; ++mode) {
                auto input = editor.findChild<QSpinBox *>(QString("animationSelector%1").arg(mode));
                QVERIFY(input);
                input->setValue(40 + mode);
            }
            const int priorRows = actions->rowCount();
            add->click();
            QCOMPARE(actions->rowCount(), priorRows + 1);
            QCOMPARE(project.animationRoutes()->entry(id).selectors.right(5),
                     QByteArray::fromHex("c828292aff"));
            const auto added = project.animationRoutes()->json();
            add->click();
            QCOMPARE(project.animationRoutes()->json(), added);
            QVERIFY(notice->text().contains("already has"));
            editor.findChild<QSpinBox *>("animationSelector1")->setValue(38);
            update->click();
            QCOMPARE(project.animationRoutes()->entry(id).selectors.right(5),
                     QByteArray::fromHex("c828262aff"));
            remove->click();
            QCOMPARE(actions->rowCount(), priorRows);
            undo.undo();
            QCOMPARE(actions->rowCount(), priorRows + 1);
            const auto edited = project.animationRoutes()->entry(id);
            QCOMPARE(edited.kind, original.entry(3).kind);
            QCOMPARE(edited.attributes, original.entry(3).attributes);
            QVERIFY(project.animationRoutes()->entry(2) == original.entry(2));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(work.filePath("project")));
            QVERIFY(reopened.animationRoutes()->entry(id) == edited);
            const auto built = rebuild(reopened, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-animation-ui-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(reopened, built, output);
            QVERIFY(AnimationRoutes::load(Rom::open(output)).entry(id) == edited);
            qInfo().noquote() << "Animation assignment candidate:" << output;
            QVERIFY(editor.grab().save("animation-assignments-workspace.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }

    void editedCombatPreviewAndUndo() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadCombatArtwork();QUndoStack undo;
            CutsceneEditor editor(rom,nullptr,&project,&undo,[]{});editor.resize(1300,900);editor.show();
            auto inspector=editor.findChild<ActorPoseEditor *>();QVERIFY(inspector);
            auto kind=inspector->findChild<QComboBox *>("actorSourceKind"),bank=inspector->findChild<QComboBox *>("actorBank");
            kind->setCurrentIndex(1);bank->setCurrentIndex(bank->findData(1));
            auto position=inspector->findChild<QSpinBox *>("actorPosition");position->setValue(1);
            auto projected=inspector->findChild<QCheckBox *>("actorProjected");auto view=inspector->findChild<QGraphicsView *>("actorCanvas");
            auto flat=[&]{QImage image(320,240,QImage::Format_RGBA8888);image.fill(Qt::transparent);QPainter painter(&image);view->scene()->render(&painter);return image;};
            const auto original=flat();const auto cursor=inspector->property("poseCursor");
            projected->setChecked(true);const auto projectedOriginal=inspector->property("placementFrame").value<QImage>();QVERIFY(!projectedOriginal.isNull());projected->setChecked(false);
            const auto body=ActorArt::loadBody(rom,1,1,0,0);const auto layer=body.bodyLayers(rom,inspector->property("frameToken").toInt(),body.defaultAppearance(rom)).first();
            const auto id=CombatArtwork::identity(body.descriptorKey(),layer.metadata.art);const auto sprite=project.combatArtwork()->original(id);const int child=layer.child<sprite.children().size()?layer.child:0;
            CombatEditor artEditor(&project,&undo,[]{});artEditor.selectBinding(id,child);auto blank=project.combatArtwork()->image(id,child);blank.fill(Qt::transparent);artEditor.assignImage(blank);
            const auto edited=flat();QVERIFY(edited!=original);QCOMPARE(position->value(),1);QCOMPARE(inspector->property("poseCursor"),cursor);
            projected->setChecked(true);const auto projectedEdited=inspector->property("placementFrame").value<QImage>();QVERIFY(projectedEdited!=projectedOriginal);
            position->setValue(18);position->setValue(1);QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedEdited);
            undo.undo();QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedOriginal);undo.redo();QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedEdited);
            projected->setChecked(false);QCOMPARE(flat(),edited);undo.undo();QCOMPARE(flat(),original);
            // The dynamic scene rebuilds captured/checkpoint frames at the same tick.
            auto scenes=ScenePrograms::load(rom);const quint32 key=0x1fa8bf4-0x594284;auto program=scenes.program(key);
            for(auto &command:program.commands)if(command.opcode==0x1b){command.operands[4]=0;command.operands[5]=0;break;}
            scenes.replace(key,program.encode());ScenePreview preview(rom);preview.resize(1200,850);preview.show();preview.selectScene(scenes,key,2);
            auto scenePosition=preview.findChild<QSpinBox *>("scenePosition");scenePosition->setValue(3);QTRY_COMPARE(preview.property("sceneTick").toInt(),3);const auto sceneOriginal=preview.renderedFrame();
            auto staged=CombatArtwork::load(rom);QSet<quint32> descriptors;
            for(int flags=0;flags<4;++flags)descriptors.insert(ActorArt::loadBody(rom,0x58,0x58,flags/2,flags%2).descriptorKey());
            for(const auto &binding:staged.bindings())if(descriptors.contains(binding.descriptorKey)){
                const auto object=staged.original(binding.id);
                for(int variant=0;variant<object.children().size();++variant)if(object.children()[variant].colorOffset>=0&&object.children()[variant].alphaOffset>=0){auto pixels=object.pixels(variant);pixels.alpha.fill(0);staged.setPixels(binding.id,variant,pixels);}
            }
            QVERIFY(staged.modified());preview.setArtwork(staged.previewEdits());QTRY_COMPARE(preview.property("sceneTick").toInt(),3);const auto sceneEdited=preview.renderedFrame();QVERIFY(sceneEdited!=sceneOriginal);
            scenePosition->setValue(1);QTRY_COMPARE(preview.property("sceneTick").toInt(),1);scenePosition->setValue(3);QTRY_COMPARE(preview.property("sceneTick").toInt(),3);QCOMPARE(preview.renderedFrame(),sceneEdited);
            preview.setArtwork({});QTRY_COMPARE(preview.property("sceneTick").toInt(),3);QCOMPARE(preview.renderedFrame(),sceneOriginal);
        }catch(const Error &e){QFAIL(e.what());}
    }
    void combatArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CombatEditor editor(&project,&undo,[]{});editor.resize(1340,850);editor.show();
            const auto id=CombatArtwork::identity(0x3bac1c,0);editor.selectBinding(id,0);
            auto descriptor=editor.findChild<QComboBox *>("combatDescriptor");QVERIFY(descriptor);QCOMPARE(descriptor->count(),166);QVERIFY(descriptor->currentText().contains("Soldier"));
            auto canvas=editor.findChild<SpriteCanvas *>("combatCanvas");QVERIFY(canvas);QVERIFY2(canvas->isEnabled(),qPrintable(editor.findChild<QLabel *>("combatNotice")->text()));
            const auto original=project.combatArtwork()->pixels(id,0);const int width=project.combatArtwork()->original(id).width(),pixel=width+1;
            const int index=(u8(original.color,pixel)+7)&255;editor.findChild<QSpinBox *>("combatInk")->setValue(index);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(1,1)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.combatArtwork()->pixels(id,0).color,pixel),quint8(index));QCOMPARE(project.combatArtwork()->pixels(id,0).alpha,original.alpha);QCOMPARE(undo.count(),1);
            auto channel=editor.findChild<QComboBox *>("combatChannel");channel->setCurrentIndex(1);const int alpha=(u8(original.alpha,pixel)+3)&15;editor.findChild<QSpinBox *>("combatInk")->setValue(alpha);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(1,1)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.combatArtwork()->pixels(id,0).alpha,pixel),quint8(alpha));const auto painted=project.combatArtwork()->pixels(id,0);
            undo.undo();QCOMPARE(project.combatArtwork()->pixels(id,0).alpha,original.alpha);undo.redo();QCOMPARE(project.combatArtwork()->pixels(id,0),painted);
            const auto image=project.combatArtwork()->image(id,0);
            const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).frames[0].layers[0].image,image);
            editor.findChild<QPushButton *>("combatRestore")->click();QCOMPARE(project.combatArtwork()->pixels(id,0),original);
            auto library=editor.findChild<QComboBox *>("combatLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("combatImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});editor.findChild<QPushButton *>("combatAssignLibrary")->click();
            QCOMPARE(project.combatArtwork()->image(id,0),image);
            QTemporaryDir work;editor.exportImage(work.filePath("combat.png"));QCOMPARE(QImage(work.filePath("combat.png")).convertToFormat(QImage::Format_RGBA8888),image);
            undo.undo();editor.importImage(work.filePath("combat.png"));QCOMPARE(project.combatArtwork()->image(id,0),image);QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));
            project.saveFolder(work.filePath("folder"));auto loaded=Project::fromRom(project.rom());loaded.applyManifest(Project::readProject(work.filePath("folder")));QCOMPARE(loaded.combatArtwork()->json(),project.combatArtwork()->json());
            editor.selectBinding(id,1);QVERIFY(canvas->isEnabled());
            QCOMPARE(project.combatArtwork()->pixels(id,1),project.combatArtwork()->original(id).pixels(1));
            descriptor->setCurrentIndex(descriptor->findData(0x100ad4cU));QVERIFY(!canvas->isEnabled());
            editor.selectBinding(id,0);channel->setCurrentIndex(2);QVERIFY(canvas->isEnabled());
            QVERIFY(editor.grab().save("combat-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void armyArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;ArmyEditor editor(&project,&undo,[]{});editor.resize(1300,820);editor.show();
            auto models=editor.findChild<QListWidget *>("armyModels");QVERIFY(models);QVERIFY(models->count()>0);
            editor.selectModel(3,24);QCOMPARE(models->count(),64);auto canvas=editor.findChild<SpriteCanvas *>("armyCanvas");QVERIFY(canvas);QVERIFY(!canvas->isEnabled());
            auto create=editor.findChild<QPushButton *>("armyCreate");QVERIFY(create->isEnabled());create->click();QVERIFY(canvas->isEnabled());QVERIFY(project.armyArtwork()->present(3,24));QCOMPARE(project.armyArtwork()->indices(3,24),QByteArray(384,char(-1)));
            editor.findChild<QSpinBox *>("armyInk")->setValue(1);QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,3)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.armyArtwork()->indices(3,24),3*16+2),quint8(1));QCOMPARE(undo.count(),2);
            const auto painted=project.armyArtwork()->indices(3,24);undo.undo();QCOMPARE(project.armyArtwork()->indices(3,24),QByteArray(384,char(-1)));undo.redo();
            editor.findChild<QComboBox *>("armyPalette")->setCurrentIndex(1);QCOMPARE(project.armyArtwork()->indices(3,24),painted);
            const auto image=project.armyArtwork()->image(3,24,1);const auto pixel=image.pixelColor(2,3);const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).frames[0].layers[0].image,image);
            editor.selectModel(3,25,1);auto library=editor.findChild<QComboBox *>("armyLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("armyImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});editor.findChild<QPushButton *>("armyAssignLibrary")->click();
            QVERIFY(project.armyArtwork()->present(3,25));QCOMPARE(project.armyArtwork()->image(3,25,1).pixelColor(2,3),pixel);QCOMPARE(project.armyArtwork()->image(3,25,1).pixelColor(0,0).alpha(),0);
            QTemporaryDir work;editor.exportImage(work.filePath("army.png"));QCOMPARE(QImage(work.filePath("army.png")).convertToFormat(QImage::Format_RGBA8888),project.armyArtwork()->image(3,25,1));
            editor.selectModel(3,26,1);editor.importImage(work.filePath("army.png"));QCOMPARE(project.armyArtwork()->image(3,26,1).pixelColor(2,3),pixel);
            const auto saved=project.armyArtwork()->json();project.saveFolder(work.filePath("project"));auto reopened=Project::fromRom(project.rom());reopened.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(reopened.armyArtwork()->json(),saved);
            editor.findChild<QPushButton *>("armyRestore")->click();QVERIFY(!project.armyArtwork()->present(3,26));undo.undo();QCOMPARE(project.armyArtwork()->json(),saved);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("army-artwork-workspace.png"));
            QCOMPARE(project.appendClass(1,"New Army class"),165);const auto added=project.classData().entries()[165];editor.selectModel(ArmyArtwork::classLane(added)*2,ArmyArtwork::classModel(added));QVERIFY(editor.findChild<QLabel *>("armyConsumers")->text().contains("New Army class (165)"));
            editor.selectModel(0,56);editor.findChild<QPushButton *>("armyCreate")->click();editor.selectModel(1,56);editor.findChild<QPushButton *>("armyCreate")->click();
            auto targetClass=editor.findChild<QComboBox *>("armyRouteClass");targetClass->setCurrentIndex(targetClass->findData(165));editor.findChild<QPushButton *>("armyAssignRoute")->click();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);QVERIFY(project.armyArtwork()->routes().contains(165));
            undo.undo();QVERIFY(!project.armyArtwork()->routes().contains(165));undo.redo();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);
            editor.findChild<QPushButton *>("armyRestoreRoute")->click();QVERIFY(!project.armyArtwork()->routes().contains(165));undo.undo();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);
            editor.selectModel(4,16,0);QVERIFY(!editor.findChild<QPushButton *>("armyAssignRoute")->isEnabled());QVERIFY(editor.grab().save("army-artwork-loaded-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void classLabelsPreserveNames() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        QFile labels(":/catalog.json"); QVERIFY(labels.open(QIODevice::ReadOnly));
        const auto catalog=QJsonDocument::fromJson(labels.readAll()).object();
        auto project=Project::fromRom(Rom::open(path),catalog);
        const auto original=project.classData();
        const auto names=catalog["CLASS_NAMES"].toObject();
        for (int id=0; id<165; ++id)
            QCOMPARE(project.record(Table::Classes,id).name,names[QString::number(id)].toString());
        QUndoStack undo;
        RecordModel model(&project,Table::Classes,&undo);
        const auto index=model.index(154,1);
        QCOMPARE(index.data().toString(),QString("Sword Master (Kageiye)"));
        QCOMPARE(index.data(Qt::EditRole).toString(),QString("Sword Master"));
        QVERIFY(!model.setData(index,index.data(Qt::EditRole),Qt::EditRole));
        QCOMPARE(project.classNameBytes(154),original.entries()[154].name);
        QVERIFY(model.setData(index,"Custom duelist",Qt::EditRole));
        QCOMPARE(index.data().toString(),QString("Custom duelist"));
        undo.undo();
        QCOMPARE(index.data().toString(),QString("Sword Master (Kageiye)"));
        QCOMPARE(project.classNameBytes(154),original.entries()[154].name);
        undo.redo();
        QTemporaryDir work; project.saveFolder(work.filePath("project"));
        auto reopened=Project::fromRom(project.rom(),catalog);
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.record(Table::Classes,154).name,QString("Custom duelist"));
        QCOMPARE(reopened.classNameBytes(154),QByteArray("Custom duelist"));
        QCOMPARE(reopened.record(Table::Classes,103).name,QString("Temple Cmdr (Baldwin)"));
    }
    void armyClassIsolation() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));
        const int added=project.appendClass(5,"Separate Knight");
        QUndoStack undo; ArmyEditor editor(&project,&undo,[]{}); editor.show();
        editor.selectClass(added);
        auto models=editor.findChild<QListWidget *>("armyModels");
        QSet<int> listed;
        for(int row=0;row<models->count();++row) listed << models->item(row)->data(Qt::UserRole+1).toInt();
        QVERIFY(listed.contains(5)); QVERIFY(listed.contains(added));
        const int source=ArmyArtwork::classModel(project.classData().entries()[5]);
        const auto front=project.armyArtwork()->indices(0,source), back=project.armyArtwork()->indices(1,source);
        auto canvas=editor.findChild<SpriteCanvas *>("armyCanvas");
        editor.findChild<QSpinBox *>("armyInk")->setValue((u8(front,0)+1)%256);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));
        const int copy=ArmyArtwork::classModel(project.classData().entries()[added]);
        QVERIFY(copy!=source);
        QCOMPARE(project.armyArtwork()->indices(0,source),front);
        QCOMPARE(project.armyArtwork()->indices(1,source),back);
        QCOMPARE(project.armyArtwork()->indices(1,copy),back);
        const auto copied=project.armyArtwork()->indices(0,copy);
        QVERIFY(copied!=front);
        undo.undo(); QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[added]),source);
        undo.redo(); QCOMPARE(project.armyArtwork()->indices(0,copy),copied);
        editor.selectClass(5);
        editor.findChild<QSpinBox *>("armyInk")->setValue((u8(front,0)+2)%256);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));
        QCOMPARE(project.armyArtwork()->indices(0,copy),copied);
        QTemporaryDir work; project.saveFolder(work.filePath("project"));
        auto reopened=Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.armyArtwork()->json(),project.armyArtwork()->json());
        QCOMPARE(reopened.armyArtwork()->indices(0,copy),copied);
        editor.selectClass(added);
        editor.findChild<QPushButton *>("armyRestore")->click();
        QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[added]),source);
        QCOMPARE(project.armyArtwork()->indices(0,source),front);
    }
    void indexedPixelTools() {
        SpriteCanvas canvas;canvas.setZoom(8);canvas.setGrid(false);canvas.show();
        QVector<QColor> palette(256,QColor(Qt::black));palette[0]=Qt::transparent;palette[1]=palette[2]=Qt::red;
        QByteArray pixels(12,char(1));pixels[0]=2;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);
        connect(&canvas,&SpriteCanvas::indicesCommitted,&canvas,[&](const QByteArray &next,QPoint at){QCOMPARE(at,QPoint{});pixels=next;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);});
        const auto original=canvas.grab().toImage();canvas.setIndexColor(2);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));
        QCOMPARE(quint8(pixels[5]),quint8(2));QCOMPARE(canvas.grab().toImage(),original);
        palette[2]=Qt::blue;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);
        QCOMPARE(canvas.grab().toImage().pixelColor(12,12),QColor(Qt::blue));
        canvas.setTool(SpriteCanvas::Tool::Select);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));
        canvas.setTool(SpriteCanvas::Tool::Move);QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));QTest::mouseMove(&canvas,QPoint(20,12));QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(20,12));
        QCOMPARE(quint8(pixels[5]),quint8(0));QCOMPARE(quint8(pixels[6]),quint8(2));
        canvas.clearSelection();canvas.setTool(SpriteCanvas::Tool::Eraser);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(20,12));QCOMPARE(quint8(pixels[6]),quint8(0));
        QSignalSpy picked(&canvas,&SpriteCanvas::indexPicked);canvas.setTool(SpriteCanvas::Tool::Pick);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));QCOMPARE(picked.count(),1);QCOMPARE(picked.first().first().toInt(),2);
    }
    void portraitArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
            PortraitEditor editor(&project,&undo,[]{});editor.resize(1250,790);editor.show();
            auto list=editor.findChild<QListWidget *>("portraitClasses");QVERIFY(list);QCOMPARE(list->count(),165);
            const auto classes=project.classData();const auto before=project.avatarArtwork()->words(classes,1,0);
            const auto variant=project.avatarArtwork()->words(classes,1,1);
            auto palette=editor.findChild<QTableWidget *>("portraitPalette");QVERIFY(palette);const auto cell=palette->item(0,0);QVERIFY(cell);
            const auto color=cell->data(Qt::UserRole).value<QColor>();
            QTest::mouseClick(palette->viewport(),Qt::LeftButton,Qt::NoModifier,palette->visualItemRect(cell).center());
            const auto image=project.avatarArtwork()->image(classes,1,0);QPoint target;bool found=false;
            for(int y=0;y<48 && !found;++y)for(int x=0;x<40;++x)if(image.pixelColor(x,y)!=color){target={x,y};found=true;break;}QVERIFY(found);
            auto canvas=editor.findChild<SpriteCanvas *>("portraitCanvas");QVERIFY(canvas);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,target*canvas->zoom()+QPoint(2,2));
            QVERIFY(project.avatarArtwork()->modified(1,0));QCOMPARE(project.avatarArtwork()->image(classes,1,0).pixelColor(target),color);
            QCOMPARE(project.avatarArtwork()->words(classes,1,1),variant);QCOMPARE(undo.count(),1);
            undo.undo();QCOMPARE(project.avatarArtwork()->words(classes,1,0),before);undo.redo();
            const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).composite(0),project.avatarArtwork()->image(classes,1,0));
            const auto edited=project.avatarArtwork()->words(classes,1,0);editor.selectPortrait(2,1);
            editor.findChild<QCheckBox *>("portraitBoth")->setChecked(true);
            auto library=editor.findChild<QComboBox *>("portraitLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("portraitImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
            editor.findChild<QPushButton *>("portraitAssignLibrary")->click();
            for(int selector=0;selector<2;++selector)QCOMPARE(project.avatarArtwork()->words(classes,2,selector),edited);
            QTemporaryDir work;editor.exportImage(work.filePath("portrait.png"));
            QCOMPARE(QImage(work.filePath("portrait.png")).convertToFormat(QImage::Format_RGBA8888),AvatarArtwork::render(edited));
            editor.selectPortrait(3,0);editor.findChild<QCheckBox *>("portraitBoth")->setChecked(false);editor.importImage(work.filePath("portrait.png"));
            QCOMPARE(project.avatarArtwork()->words(classes,3,0),edited);
            const auto saved=project.avatarArtwork()->json();project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(project.rom());loaded.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(loaded.avatarArtwork()->json(),saved);
            editor.findChild<QPushButton *>("portraitRestore")->click();QVERIFY(!project.avatarArtwork()->modified(3,0));undo.undo();QCOMPARE(project.avatarArtwork()->json(),saved);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("portrait-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void iconArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;ArtEditor editor(&project,&undo,[]{});editor.resize(1250,790);editor.show();
            auto list=editor.findChild<QListWidget *>("artIcons");QVERIFY(list);QCOMPARE(list->count(),277);
            const auto original=project.iconArtwork()->words(1);auto canvas=editor.findChild<SpriteCanvas *>("artIconCanvas");QVERIFY(canvas);auto tools=editor.findChild<QComboBox *>("artPixelTool");
            tools->setCurrentIndex(int(SpriteCanvas::Tool::Eraser));
            // Pick an opaque source pixel so erasing necessarily changes the icon.
            const auto source=project.iconArtwork()->image(1);QPoint target;bool found=false;
            for(int y=0;y<16 && !found;++y)for(int x=0;x<16;++x)if(source.pixelColor(x,y).alpha()){target={x,y};found=true;break;}QVERIFY(found);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,target*canvas->zoom()+QPoint(2,2));QVERIFY(project.iconArtwork()->modified(1));QCOMPARE(project.iconArtwork()->image(1).pixelColor(target).alpha(),0);QCOMPARE(undo.count(),1);
            undo.undo();QCOMPARE(project.iconArtwork()->words(1),original);undo.redo();
            const auto libraryId=editor.copyToLibrary();QCOMPARE(project.sprites().asset(libraryId).frames[0].layers[0].image,project.iconArtwork()->image(1));QCOMPARE(project.sprites().assets().size(),1);
            editor.selectIcon(2);auto library=editor.findChild<QComboBox *>("artLibraryAsset");library->setCurrentIndex(library->findData(libraryId));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("artImagePreparation");QVERIFY(dialog);dialog->findChild<QComboBox *>("artImportFit")->setCurrentIndex(3);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
            editor.findChild<QPushButton *>("artAssignLibrary")->click();QCOMPARE(project.iconArtwork()->words(2),project.iconArtwork()->words(1));
            const auto edited=project.iconArtwork()->json();QTemporaryDir work;editor.exportImage(work.filePath("icon.png"));QCOMPARE(QImage(work.filePath("icon.png")).convertToFormat(QImage::Format_RGBA8888),project.iconArtwork()->image(2));
            editor.selectIcon(0x201);QCOMPARE(list->count(),44);const auto consumable=project.iconArtwork()->words(0x201);editor.importImage(work.filePath("icon.png"));QVERIFY(project.iconArtwork()->words(0x201)!=consumable);undo.undo();QCOMPARE(project.iconArtwork()->json(),edited);
            project.saveFolder(work.filePath("project"));auto restored=Project::fromRom(project.rom());restored.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(restored.iconArtwork()->json(),edited);QCOMPARE(restored.sprites().json(),project.sprites().json());
            editor.selectIcon(2);editor.findChild<QPushButton *>("artRestore")->click();QVERIFY(!project.iconArtwork()->modified(2));undo.undo();QCOMPARE(project.iconArtwork()->json(),edited);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("icon-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void spriteUsedColorPicker() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        SpriteEditor editor(&project, &undo, [] {});
        editor.resize(1500, 900);
        editor.show();
        auto asset = SpriteAsset::blank("Color scopes", {300, 1});
        asset.kind = "sequence";
        for (int x = 0; x < 300; ++x)
            asset.frames[0].layers[0].image.setPixel(x, 0,
                                                     qRgba(x & 255, x >> 8, 17, 64 + x % 128));
        auto hidden = asset.frames[0].layers[0];
        hidden.id = SpriteLibrary::newId();
        hidden.visible = false;
        hidden.image = QImage(1, 1, QImage::Format_RGBA8888);
        const QColor hiddenColor(8, 9, 10, 0);
        hidden.image.fill(hiddenColor);
        asset.frames[0].layers << hidden;
        auto second = asset.frames[0];
        second.id = SpriteLibrary::newId();
        second.layers.resize(1);
        second.layers[0].image.fill(QColor(2, 3, 4, 7));
        asset.frames << second;
        editor.addAsset(asset);
        editor.findChild<QListWidget *>("spriteLayers")->setCurrentRow(1);
        const auto original = project.sprites().json();
        const auto initialUndo = undo.count();
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("spriteUsedColorsDialog");
            QVERIFY(dialog);
            struct Cleanup {
                QDialog *d;
                ~Cleanup() {
                    if (d->isVisible())
                        d->reject();
                }
            } cleanup{dialog};
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            auto scope = dialog->findChild<QComboBox *>("spriteColorScope");
            auto grid = dialog->findChild<QListWidget *>("spriteUsedColorGrid");
            auto count = dialog->findChild<QLabel *>("spriteColorCount");
            QVERIFY(count->text().startsWith("300 used colors"));
            QCOMPARE(grid->count(), 128);
            const auto first = grid->item(0)->data(Qt::UserRole);
            dialog->findChild<QPushButton *>("spriteColorsNext")->click();
            QVERIFY(grid->item(0)->data(Qt::UserRole) != first);
            scope->setCurrentIndex(2);
            scope->setCurrentIndex(0);
            scope->setCurrentIndex(1);
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            QVERIFY(count->text().startsWith("301 used colors"));
            scope->setCurrentIndex(2);
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            QVERIFY(count->text().startsWith("302 used colors"));
            auto item = grid->item(0);
            QCOMPARE(item->data(Qt::UserRole).toUInt(), hiddenColor.rgba());
            grid->setFocus();
            QTest::mouseClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier,
                              grid->visualItemRect(item).center());
            QTest::keyClick(grid, Qt::Key_Right);
            QTest::keyClick(grid, Qt::Key_Left);
            QCOMPARE(grid->currentItem()->data(Qt::UserRole).toUInt(), hiddenColor.rgba());
            QVERIFY(dialog->grab().save("sprite-used-colors.png"));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        editor.findChild<QPushButton *>("spriteUsedColors")->click();
        QCOMPARE(editor.findChild<QPushButton *>("spriteColor")->text(),
                 hiddenColor.name(QColor::HexArgb));
        QCOMPARE(project.sprites().json(), original);
        QCOMPARE(undo.count(), initialUndo);
        QTimer::singleShot(0, &editor, [&] {
            auto d = editor.findChild<QDialog *>("spriteUsedColorsDialog");
            QVERIFY(d);
            d->findChild<QComboBox *>("spriteColorScope")->setCurrentIndex(2);
            d->reject();
        });
        editor.findChild<QPushButton *>("spriteUsedColors")->click();
        QCOMPARE(editor.findChild<QPushButton *>("spriteColor")->text(),
                 hiddenColor.name(QColor::HexArgb));
        QCOMPARE(project.sprites().json(), original);
        QCOMPARE(undo.count(), initialUndo);
        auto canvas = editor.findChild<SpriteCanvas *>();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          QPoint(2 * canvas->zoom() + 2, 2));
        QCOMPARE(project.sprites().asset(asset.id).frames[0].layers[0].image.pixelColor(2, 0),
                 hiddenColor);
        undo.undo();
        QCOMPARE(project.sprites().json(), original);
    }
    void spriteWorkspaceEditing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.resize(1400, 820);
            editor.show();
            auto asset = SpriteAsset::blank("Layered test", {16, 12});
            editor.addAsset(asset);
            const auto id = asset.id;
            auto canvas = editor.findChild<SpriteCanvas *>();
            auto tool = editor.findChild<QComboBox *>("spriteTool");
            auto frameList = editor.findChild<QListWidget *>("spriteFrames");
            auto layerList = editor.findChild<QListWidget *>("spriteLayers");
            auto click = [&](const char *name) {
                auto button = editor.findChild<QPushButton *>(name);
                QVERIFY(button);
                button->click();
            };
            auto paint = [&](QPoint from, QPoint to) {
                QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                                  from * canvas->zoom() + QPoint(2, 2));
                QTest::mouseMove(canvas, to * canvas->zoom() + QPoint(2, 2));
                QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                    to * canvas->zoom() + QPoint(2, 2));
            };
            QApplication::processEvents();
            canvas->setColor(QColor(200, 40, 20, 128));
            paint({1, 1}, {5, 1});
            QCOMPARE(undo.count(), 2);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 1),
                     QColor(200, 40, 20, 128));
            undo.undo();
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 1),
                     QColor(Qt::transparent));
            undo.redo();
            click("spriteAddLayer");
            QCOMPARE(layerList->currentRow(), 0);
            QCOMPARE(project.sprites().asset(id).frames[0].layers.size(), 2);
            canvas->setColor(Qt::blue);
            paint({2, 2}, {4, 2});
            QCOMPARE(project.sprites().asset(id).frames[0].layers[1].image.pixelColor(3, 2),
                     QColor(Qt::blue));
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 2),
                     QColor(Qt::transparent));
            editor.findChild<QSpinBox *>("spriteLayerX")->setValue(-1);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[1].position, QPoint(-1, 0));
            auto visible = editor.findChild<QCheckBox *>("spriteLayerVisible");
            visible->setChecked(false);
            QCOMPARE(project.sprites().asset(id).composite(0).pixelColor(2, 2),
                     QColor(Qt::transparent));
            undo.undo();
            click("spriteLayerDown");
            QCOMPARE(layerList->currentRow(), 1);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].position, QPoint(-1, 0));
            undo.undo();
            layerList->setCurrentRow(0);
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Select));
            paint({1, 2}, {2, 2});
            QCOMPARE(canvas->selection(), QRect(1, 2, 2, 1));
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Eraser));
            paint({0, 2}, {4, 2});
            auto erased = project.sprites().asset(id).frames[0].layers[1].image;
            QCOMPARE(erased.pixelColor(2, 2), QColor(Qt::transparent));
            QCOMPARE(erased.pixelColor(3, 2), QColor(Qt::transparent));
            QCOMPARE(erased.pixelColor(4, 2), QColor(Qt::blue));
            undo.undo();
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Pencil));
            const int count = undo.count();
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                              QPoint(9, 9) * canvas->zoom());
            QTest::keyClick(canvas, Qt::Key_Escape);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                QPoint(10, 9) * canvas->zoom());
            QCOMPARE(undo.count(), count);
            click("spriteDuplicateFrame");
            QCOMPARE(frameList->currentRow(), 1);
            QCOMPARE(project.sprites().asset(id).frames.size(), 2);
            editor.findChild<QSpinBox *>("spriteFrameTicks")->setValue(3);
            QCOMPARE(project.sprites().asset(id).frames[1].ticks, 3);
            click("spriteFrameLeft");
            QCOMPARE(frameList->currentRow(), 0);
            QCOMPARE(project.sprites().asset(id).frames[0].ticks, 3);
            QTemporaryDir work;
            editor.exportAsset(work.filePath("sprite.json"), true);
            editor.exportAsset(work.filePath("sprite.lcsprite"), false);
            editor.exportImage(work.filePath("frame.png"), "PNG");
            QCOMPARE(QImage(work.filePath("frame.png")).convertToFormat(QImage::Format_RGBA8888),
                     project.sprites().asset(id).composite(0));
            editor.exportImage(work.filePath("frame.jpg"), "JPEG");
            QVERIFY(!QImage(work.filePath("frame.jpg")).isNull());
            const auto before = project.sprites().asset(id);
            editor.importFile(work.filePath("sprite.json"));
            QCOMPARE(project.sprites().assets().size(), 2);
            QVERIFY(editor.selectedAsset() != id);
            auto imported = project.sprites().asset(editor.selectedAsset());
            imported.id = id;
            QCOMPARE(imported.json(true), before.json(true));
            undo.undo();
            QCOMPARE(project.sprites().assets().size(), 1);
            editor.importFile(work.filePath("frame.png"));
            QCOMPARE(project.sprites().assets().size(), 2);
            QCOMPARE(project.sprites().asset(editor.selectedAsset()).composite(0),
                     before.composite(0));
            undo.undo();
            editor.selectAsset(id);
            click("spriteDelete");
            QVERIFY(project.sprites().assets().isEmpty());
            undo.undo();
            QCOMPARE(project.sprites().asset(id).json(), before.json());
            editor.selectAsset(id);
            QVERIFY(editor.grab().save("sprite-workspace.png"));
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportImage(path, "PNG"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void initTestCase() {
        // The offscreen Qt platform does not enumerate Windows system fonts.
        QFontDatabase::addApplicationFont(
            QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));
        applyEditorTheme(*qApp);
    }
    void experienceToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 420);
        editor.show();
        auto percent = editor.findChild<QSpinBox *>("experienceScale");
        auto weights = editor.findChild<QLabel *>("experienceWeights");
        auto restore = editor.findChild<QPushButton *>("experienceRestore");
        QVERIFY(percent->isEnabled());
        QCOMPARE(percent->value(), 100);
        percent->setValue(137);
        QCOMPARE(project.tools()->experiencePercent(), 137);
        QVERIFY(weights->text().contains("109.6%"));
        QVERIFY(restore->isEnabled());
        undo.undo();
        QCOMPARE(percent->value(), 100);
        undo.redo();
        QCOMPARE(percent->value(), 137);
        QVERIFY(editor.grab().save("experience-tools.png"));
        restore->click();
        QCOMPARE(percent->value(), 100);
        undo.undo();
        QCOMPARE(percent->value(), 137);
        editor.findChild<QPushButton *>("experienceRetail")->click();
        QCOMPARE(project.tools()->experiencePercent(), 100);
        auto bytes = rom.bytes();
        bytes[0x21928c] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor disabled(&foreign, &foreignUndo, [] {});
        QVERIFY(!disabled.findChild<QSpinBox *>("experienceScale")->isEnabled());
        QVERIFY(disabled.findChild<QLabel *>("toolsNotice")->text().contains("unrecognized"));
    }
    void chaosToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 440);
        editor.show();
        auto box = editor.findChild<QCheckBox *>("chaosCounter");
        auto restore = editor.findChild<QPushButton *>("chaosRestore");
        QVERIFY(box->isEnabled());
        QVERIFY(!box->isChecked());
        QVERIFY(editor.findChild<QLabel *>("chaosNotice")->text().contains("8 MiB"));
        box->click();
        QVERIFY(project.tools()->chaosCounter().enabled());
        QVERIFY(restore->isEnabled());
        QVERIFY(!editor.findChild<QPushButton *>("experienceRestore")->isEnabled());
        editor.findChild<QSpinBox *>("experienceScale")->setValue(137);
        undo.undo();
        QVERIFY(box->isChecked());
        QCOMPARE(project.tools()->experiencePercent(), 100);
        undo.undo();
        QVERIFY(!box->isChecked());
        undo.redo();
        QVERIFY(box->isChecked());
        restore->click();
        QVERIFY(!box->isChecked());
        undo.undo();
        QVERIFY(box->isChecked());
        QVERIFY(editor.grab().save("chaos-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x3078c] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor disabled(&foreign, &foreignUndo, [] {});
        QVERIFY(!disabled.findChild<QCheckBox *>("chaosCounter")->isEnabled());
        QVERIFY(disabled.findChild<QSpinBox *>("experienceScale")->isEnabled());
    }
    void displayToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 700);
        editor.show();
        auto luck = editor.findChild<QCheckBox *>("luckDisplay");
        auto alignment = editor.findChild<QCheckBox *>("alignmentDisplay");
        QVERIFY(luck->isEnabled());
        QVERIFY(alignment->isEnabled());
        luck->click();
        alignment->click();
        QVERIFY(project.tools()->displayTool(ToolLabel::Luck).enabled());
        QVERIFY(project.tools()->displayTool(ToolLabel::Alignment).enabled());
        undo.undo();
        QVERIFY(luck->isChecked());
        QVERIFY(!alignment->isChecked());
        undo.redo();
        QVERIFY(alignment->isChecked());
        editor.findChild<QPushButton *>("luckRestore")->click();
        QVERIFY(!luck->isChecked());
        QVERIFY(alignment->isChecked());
        undo.undo();
        QVERIFY(luck->isChecked());
        QVERIFY(editor.grab().save("display-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x351c0] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor held(&foreign, &foreignUndo, [] {});
        QVERIFY(!held.findChild<QCheckBox *>("alignmentDisplay")->isEnabled());
        QVERIFY(held.findChild<QCheckBox *>("luckDisplay")->isEnabled());
    }
    void highAttackToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(900, 900);
        editor.show();
        auto box = editor.findChild<QCheckBox *>("highAttack");
        auto restore = editor.findChild<QPushButton *>("highAttackRestore");
        QVERIFY(box->isEnabled());
        QVERIFY(!box->isChecked());
        const auto notice = editor.findChild<QLabel *>("highAttackNotice")->text();
        QVERIFY(notice.contains("8 MiB"));
        QVERIFY(notice.contains("Interpreter"));
        QVERIFY(notice.contains("pending"));
        box->click();
        QVERIFY(project.tools()->highAttack().enabled());
        editor.findChild<QCheckBox *>("luckDisplay")->click();
        undo.undo();
        QVERIFY(box->isChecked());
        undo.undo();
        QVERIFY(!box->isChecked());
        undo.redo();
        QVERIFY(box->isChecked());
        restore->click();
        QVERIFY(!box->isChecked());
        undo.undo();
        QVERIFY(box->isChecked());
        QVERIFY(editor.grab().save("high-attack-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x34f00] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor held(&foreign, &foreignUndo, [] {});
        QVERIFY(!held.findChild<QCheckBox *>("highAttack")->isEnabled());
        QVERIFY(held.findChild<QCheckBox *>("luckDisplay")->isEnabled());
    }
    void saveRosterControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-export.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1200, 850);
            editor.show();
            editor.loadBytes(input);
            const int added = project.appendClass(5, "New recruit class");
            QCOMPARE(added, 165);
            auto add = editor.findChild<QPushButton *>("saveAddCharacter");
            auto remove = editor.findChild<QPushButton *>("saveRemoveCharacter");
            auto grid = editor.findChild<QTableView *>("saveCharacters");
            QVERIFY(add && remove && grid);
            QVERIFY(add->isEnabled());
            QVERIFY(!remove->isEnabled());
            bool offered = false;
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("saveNewCharacterDialog");
                if (!dialog)
                    return;
                auto choices = dialog->findChild<QComboBox *>("saveNewCharacterClass");
                auto name = dialog->findChild<QLineEdit *>("saveNewCharacterName");
                auto level = dialog->findChild<QSpinBox *>("saveNewCharacterLevel");
                offered = choices && name && level && choices->findData(added) >= 0;
                if (!offered) {
                    dialog->reject();
                    return;
                }
                choices->setCurrentIndex(choices->findData(added));
                name->setText("Recruit");
                level->setValue(20);
                dialog->accept();
            });
            add->click();
            QVERIFY(offered);
            QCOMPARE(editor.undoStack()->count(), 1);
            QCOMPARE(editor.document()->value(SaveTable::Characters, 21, "class").toInt(), added);
            QCOMPARE(editor.document()->value(SaveTable::Characters, 21, "level").toInt(), 20);
            QCOMPARE(editor.document()->value(SaveTable::Characters, 21, "name").toString(),
                     QString("Recruit"));
            QCOMPARE(grid->model()->index(grid->currentIndex().row(), 0).data().toInt(), 21);
            QVERIFY(remove->isEnabled());
            const auto created = editor.document()->serialize();
            QVERIFY(editor.grab().save("save-roster-controls.png"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), created);
            QVERIFY(!editor.removeCharacter(1));
            QCOMPARE(editor.document()->serialize(), created);
            auto tabs=editor.findChild<QTabWidget *>();
            tabs->setCurrentWidget(editor.findChild<QWidget *>("saveSquads"));
            editor.findChild<QSpinBox *>("saveUnitUnity")->setValue(99);
            QVERIFY(editor.hasPendingUnitEdits());
            QVERIFY(!editor.createCharacter(5, "Draft"));
            QVERIFY(!editor.removeCharacter(21));
            editor.findChild<QPushButton *>("saveUnitDiscard")->click();
            tabs->setCurrentIndex(0);
            for (int row = 0; row < grid->model()->rowCount(); ++row)
                if (grid->model()->index(row, 0).data().toInt() == 21)
                    grid->setCurrentIndex(grid->model()->index(row, 1));
            remove->click();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), created);
            const auto output = work.filePath("recruited.sra");
            editor.exportFile(output);
            QFile result(output);
            QVERIFY(result.open(QIODevice::ReadOnly));
            auto reopened = SaveDocument::open(rom, result.readAll(), 166);
            QCOMPARE(reopened.serialize(), created);
            QVERIFY(reopened.reserveMembers().contains(21));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveExportConsistency() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-export.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            const auto packed = payload.readAll();
            QByteArray input(0x10000, 0);
            for (int slot : {0, 1}) {
                const int at = int(lc_save_record_offset(slot, 0));
                input.replace(at + 42, 6180, packed);
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + at),
                                            lc_save_record_size(slot, 0), slot, 0));
            }
            const auto source = work.filePath("source.sra"), output = work.filePath("export.sra");
            atomicWrite(source, input);
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.loadBytes(input, source);
            auto classes = project.record(Table::Classes, 5).bytes;
            classes[44] = 7;
            project.replaceBytes(Table::Classes, 5, classes);
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(source));
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(work.path()));
            QCOMPARE(editor.document()->serialize(), input);
            QCOMPARE(editor.undoStack()->count(), 0);
            editor.exportFile(output);
            QCOMPARE(editor.undoStack()->count(), 1);
            QVERIFY(editor.undoStack()->isClean());
            const auto saved = editor.document()->serialize();
            QVERIFY(saved != input);
            QFile file(output);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), saved);
            file.close();
            auto reopened = SaveDocument::open(rom, saved);
            for (int slot : {0, 1}) {
                reopened.selectSlot(slot);
                QCOMPARE(u8(reopened.memory(), 0x1971f0 + 2 * 25 + 24), quint8(7));
                QCOMPARE(u8(reopened.memory(), 0x1969c1), quint8(7));
            }
            editor.exportFile(output);
            QCOMPARE(editor.undoStack()->count(), 1);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), saved);
            for (const auto &change :
                 {QVector<int>{5, 4, 2}, QVector<int>{5, 71, 0}, QVector<int>{33, 6, 1}}) {
                project = Project::fromRom(rom);
                editor.loadBytes(input, source);
                auto bytes = project.record(Table::Classes, change[0]).bytes;
                bytes[change[1]] = char(change[2]);
                project.replaceBytes(Table::Classes, change[0], bytes);
                const auto rejected = work.filePath(QString("rejected-%1.sra").arg(change[1]));
                QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(rejected));
                QVERIFY(!QFileInfo::exists(rejected));
                QCOMPARE(editor.document()->serialize(), input);
                QCOMPARE(editor.undoStack()->count(), 0);
            }
            QFile original(source);
            QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(original.readAll(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveCharacterClassCoordination() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-class.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto grid = editor.findChild<QTableView *>("saveCharacters");
            auto fields = editor.document()->fields(SaveTable::Characters);
            const auto field = std::find_if(fields.begin(), fields.end(), [](const auto &value) {
                return value.key == "secondaryClass";
            });
            QVERIFY(grid && field != fields.end());
            const int column = int(field - fields.begin()) + 1;
            const int row = editor.document()->rows(SaveTable::Characters).indexOf(18);
            QVERIFY(row >= 0);
            QVERIFY(grid->model()->setData(grid->model()->index(row, column), 44, Qt::EditRole));
            QCOMPARE(u8(editor.document()->memory(), 0x1971f0 + 2 * 25 + 24), quint8(7));
            QCOMPARE(u8(editor.document()->memory(), 0x1969c1), quint8(7));
            QCOMPARE(editor.undoStack()->count(), 1);
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            QVERIFY(!editor.edit(SaveTable::Characters, 1, "secondaryClass", 5));
            QCOMPARE(editor.document()->serialize(), result);
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("command"));
            const auto filename = work.filePath("class-coordinated.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLegionCreationControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-create-holes.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto create = editor.findChild<QPushButton *>("saveCreateLegion");
            auto join = editor.findChild<QPushButton *>("saveJoinLegion");
            auto target = editor.findChild<QComboBox *>("saveLegionTarget");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            auto label = editor.findChild<QLabel *>("saveUnitLegion");
            QVERIFY(create && join && target && unity && label);
            units->setCurrentRow(0);
            QVERIFY(!create->isEnabled());
            units->setCurrentRow(15);
            QVERIFY(create->isEnabled());
            unity->setValue(22);
            QVERIFY(!create->isEnabled());
            QVERIFY(!editor.createLegion(15));
            unity->setValue(23);
            create->click();
            QCOMPARE(editor.document()->legions().size(), 4);
            QVERIFY(label->text().contains("Legion 2"));
            QVERIFY(!create->isEnabled());
            const auto created = editor.document()->serialize();
            units->setCurrentRow(16);
            QVERIFY(join->isEnabled());
            QCOMPARE(target->currentData().toInt(), 1);
            join->click();
            QCOMPARE(editor.document()->legions()[1].units, QVector<int>({15, 16, 255, 255, 255}));
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-create-legion.png"));
            const auto filename = work.filePath("created-legion.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), created);
            units->setCurrentRow(15);
            editor.findChild<QPushButton *>("saveDisbandLegion")->click();
            QVERIFY(create->isEnabled());
            create->click();
            QCOMPARE(editor.document()->serialize(), created);
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!create->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLegionSquadCoordination() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-coordinate-full.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto remove = editor.findChild<QPushButton *>("saveMoveToReserve");
            auto detach = editor.findChild<QPushButton *>("saveDetachLegionUnit");
            auto add = editor.findChild<QPushButton *>("saveAddSoldierGroup");
            auto error = editor.findChild<QLabel *>("saveError");
            auto cell = editor.findChild<QComboBox *>("saveReserveCell");
            QVERIFY(units && members && remove && detach && add && error && cell);
            units->setCurrentRow(0);
            members->setCurrentRow(1);
            QVERIFY(remove->isEnabled());
            remove->click();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(error->text().contains("detach a follower"));
            QCOMPARE(editor.undoStack()->count(), 0);
            units->setCurrentRow(4);
            detach->click();
            units->setCurrentRow(0);
            members->setCurrentRow(1);
            remove->click();
            QCOMPARE(editor.document()->legionCapacity(0), 4);
            QCOMPARE(editor.document()->reserveSoldiers(), 10);
            QCOMPARE(u8(editor.document()->memory(), 0x1969c1), quint8(7));
            const auto removed = editor.document()->serialize();
            cell->setCurrentIndex(cell->findData(0));
            QVERIFY(add->isEnabled());
            add->click();
            QCOMPARE(editor.document()->legionCapacity(0), 5);
            QCOMPARE(editor.document()->reserveSoldiers(), 7);
            const auto created = editor.document()->serialize();
            const auto filename = work.filePath("coordinated-legion.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), removed);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), created);
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLegionJoinControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-join-1.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto label = editor.findChild<QLabel *>("saveUnitLegion");
            auto target = editor.findChild<QComboBox *>("saveLegionTarget");
            auto join = editor.findChild<QPushButton *>("saveJoinLegion");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && members && label && target && join && unity);
            units->setCurrentRow(0);
            QVERIFY(!join->isEnabled());
            units->setCurrentRow(15);
            QVERIFY(join->isEnabled());
            QCOMPARE(target->count(), 1);
            QVERIFY(target->currentText().contains("4/5"));
            unity->setValue(22);
            QVERIFY(!join->isEnabled());
            QVERIFY(!editor.joinLegion(0, 15));
            unity->setValue(23);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-join-legion.png"));
            join->click();
            QCOMPARE(units->count(), 17);
            QCOMPARE(editor.document()->legions()[0].units, QVector<int>({0, 15, 2, 3, 4}));
            QVERIFY(label->text().contains("5 squads"));
            QVERIFY(!join->isEnabled());
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(join->isEnabled());
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            auto detach = editor.findChild<QPushButton *>("saveDetachLegionUnit");
            QVERIFY(detach->isEnabled());
            detach->click();
            QCOMPARE(label->text(), QString("Independent squad"));
            QVERIFY(join->isEnabled());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), result);
            const auto filename = work.filePath("join-legion.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            units->setCurrentRow(16);
            QVERIFY(!join->isEnabled());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!join->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLegionFollowerControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legions.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto label = editor.findChild<QLabel *>("saveUnitLegion");
            auto detach = editor.findChild<QPushButton *>("saveDetachLegionUnit");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && members && label && detach && unity);
            units->setCurrentRow(0);
            QVERIFY(!detach->isEnabled());
            units->setCurrentRow(1);
            QVERIFY(detach->isEnabled());
            unity->setValue(22);
            QVERIFY(!detach->isEnabled());
            QVERIFY(!editor.detachLegionUnit(0, 1));
            unity->setValue(23);
            detach->click();
            QCOMPARE(units->count(), 17);
            QCOMPARE(label->text(), QString("Independent squad"));
            QVERIFY(!detach->isEnabled());
            QCOMPARE(editor.document()->legions()[0].units, QVector<int>({0, 3, 2, 255, 4}));
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(detach->isEnabled());
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            editor.findChild<QPushButton *>("saveDisbandUnit")->click();
            QCOMPARE(units->count(), 16);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), result);
            const auto filename = work.filePath("detach-legion.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            units->setCurrentRow(3);
            QVERIFY(label->text().contains("4 squads"));
            QVERIFY(detach->isEnabled());
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-detach-legion.png"));
            detach->click();
            QCOMPARE(editor.document()->legions()[0].units, QVector<int>({0, 255, 2, 255, 4}));
            QCOMPARE(label->text(), QString("Independent squad"));
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!detach->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLegionDisbandControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legions.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto label = editor.findChild<QLabel *>("saveUnitLegion");
            auto disband = editor.findChild<QPushButton *>("saveDisbandLegion");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && members && label && disband && unity);
            units->setCurrentRow(0);
            QVERIFY(label->text().contains("Legion 1"));
            QVERIFY(disband->isEnabled());
            unity->setValue(22);
            QVERIFY(!disband->isEnabled());
            unity->setValue(23);
            disband->click();
            QCOMPARE(editor.document()->legions().size(), 2);
            QCOMPARE(units->count(), 17);
            QCOMPARE(label->text(), QString("Independent squad"));
            QVERIFY(!disband->isEnabled());
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(disband->isEnabled());
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            members->setCurrentRow(1);
            editor.findChild<QPushButton *>("saveChangeLeader")->click();
            QCOMPARE(editor.document()->units()[0].members[0], 18);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), result);
            const auto filename = work.filePath("disband-legion.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            units->setCurrentRow(6);
            QVERIFY(label->text().contains("Legion 3"));
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-disband-legion.png"));
            disband->click();
            QCOMPARE(editor.document()->legions().size(), 1);
            QCOMPARE(units->count(), 17);
            QCOMPARE(label->text(), QString("Independent squad"));
            editor.findChild<QPushButton *>("saveDisbandUnit")->click();
            QCOMPARE(units->count(), 16);
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!disband->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSquadDisbandControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-disband-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            auto inputFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing disband fixture");
                QByteArray out(0x10000, 0);
                out.replace(0x3a, 6180, file.readAll());
                if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + 0x10),
                                         0x1850, 0, 0))
                    throw Error("Cannot seal disband fixture");
                return out;
            };
            const auto input = inputFor("disband-multi.bin");
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto disband = editor.findChild<QPushButton *>("saveDisbandUnit");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && disband && unity);
            units->setCurrentRow(1);
            QVERIFY(disband->isEnabled());
            unity->setValue(22);
            QVERIFY(!disband->isEnabled());
            unity->setValue(23);
            disband->click();
            QCOMPARE(units->count(), 4);
            QCOMPARE(editor.document()->reserveSoldiers(), 12);
            QVERIFY(editor.document()->reserveMembers().contains(4));
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            const auto filename = work.filePath("disband.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-disband-squad.png"));
            editor.loadBytes(inputFor("disband-last.bin"));
            QCOMPARE(units->count(), 1);
            disband->click();
            QCOMPARE(units->count(), 0);
            QVERIFY(!disband->isEnabled());
            QVERIFY(editor.findChild<QPushButton *>("saveCreateUnit")->isEnabled());
            editor.undoStack()->undo();
            QCOMPARE(units->count(), 1);
            editor.loadBytes(inputFor("disband-fixed.bin"));
            units->setCurrentRow(1);
            disband->click();
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("fixed"));
            QCOMPARE(units->count(), 5);
            editor.loadBytes(inputFor("disband-legion.bin"));
            units->setCurrentRow(4);
            disband->click();
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("legion"));
            QCOMPARE(units->count(), 5);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!disband->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSquadStockControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("squad-stock-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("stock-two.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto supply = editor.findChild<QComboBox *>("saveUnitSupply1");
            QVERIFY(units && supply);
            units->setCurrentRow(0);
            QVERIFY(supply->findData(24) >= 0);
            supply->setCurrentIndex(supply->findData(24));
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            units->setCurrentRow(1);
            supply->setCurrentIndex(supply->findData(24));
            QCOMPARE(editor.document()->consumableAvailable(24), 0);
            units->setCurrentRow(4);
            QCOMPARE(supply->findData(24), -1);
            QVERIFY(!editor.edit(SaveTable::Consumables, 23, "count", 1));
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("assigned"));
            units->setCurrentRow(0);
            QVERIFY(supply->findData(24) >= 0);
            supply->setCurrentIndex(supply->findData(0));
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            units->setCurrentRow(4);
            QVERIFY(supply->findData(24) >= 0);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->consumableAvailable(24), 0);
            QCOMPARE(supply->findData(24), -1);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            QVERIFY(supply->findData(24) >= 0);
            const auto filename = work.filePath("stock.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSquadCreationControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-create-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            auto inputFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing creation fixture");
                QByteArray out(0x10000, 0);
                out.replace(0x3a, 6180, file.readAll());
                if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + 0x10),
                                         0x1850, 0, 0))
                    throw Error("Cannot seal creation fixture");
                return out;
            };
            const auto input = inputFor("create.bin");
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto recruit=editor.findChild<QPushButton *>("saveArmyRecruitSquad");
            QVERIFY(recruit && recruit->isEnabled());
            const int newCharacter=editor.document()->freeCharacterSlot();
            bool sawRecruit=false;
            QTimer::singleShot(0,&editor,[&] {
                auto dialog=editor.findChild<QDialog *>("saveNewCharacterDialog");
                if (!dialog) return;
                auto choices=dialog->findChild<QComboBox *>("saveNewCharacterClass");
                auto name=dialog->findChild<QLineEdit *>("saveNewCharacterName");
                auto level=dialog->findChild<QSpinBox *>("saveNewCharacterLevel");
                sawRecruit=choices && name && level && choices->findData(5)>=0;
                if (!sawRecruit) {dialog->reject();return;}
                choices->setCurrentIndex(choices->findData(5)); name->setText("New leader"); level->setValue(30);
                dialog->accept();
            });
            recruit->click();
            QVERIFY(sawRecruit);
            QCOMPARE(editor.undoStack()->count(),1);
            QCOMPARE(editor.document()->units().size(),6);
            QCOMPARE(editor.document()->value(SaveTable::Characters,newCharacter,"name").toString(),QString("New leader"));
            QCOMPARE(editor.document()->units().last().members[0],newCharacter);
            QCOMPARE(editor.document()->value(SaveTable::Characters,newCharacter,"level").toInt(),30);
            const auto recruited=editor.document()->serialize();
            editor.undoStack()->undo(); QCOMPARE(editor.document()->serialize(),input);
            editor.undoStack()->redo(); QCOMPARE(editor.document()->serialize(),recruited);
            const auto recruitedFile=work.filePath("recruited-squad.SaveRAM");
            editor.exportFile(recruitedFile);
            QFile reopenedRecruit(recruitedFile); QVERIFY(reopenedRecruit.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom,reopenedRecruit.readAll()).memory(),editor.document()->memory());
            editor.resize(1440,1000);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/save-army-management.png"));
            editor.resize(1100,800);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/save-army-small.png"));
            auto details=editor.findChild<QTabWidget *>("saveArmyDetails");
            for (int page=1; page<details->count(); ++page) {
                details->setCurrentIndex(page);
                QCoreApplication::processEvents();
                QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+QString("/save-army-detail-%1.png").arg(page)));
            }
            details->setCurrentIndex(0);
            editor.loadBytes(input);
            QVERIFY(!editor.recruitUnit(5,"Invalid",9));
            QCOMPARE(editor.document()->serialize(),input); // No orphan recruit on invalid formation.
            QCOMPARE(editor.undoStack()->count(),0);
            auto leaders = editor.findChild<QComboBox *>("saveNewUnitLeader");
            auto cells = editor.findChild<QComboBox *>("saveNewUnitCell");
            auto create = editor.findChild<QPushButton *>("saveCreateUnit");
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(leaders && cells && create && units && unity);
            leaders->setCurrentIndex(leaders->findData(10));
            cells->setCurrentIndex(cells->findData(8));
            QVERIFY(create->isEnabled());
            unity->setValue(22);
            QVERIFY(!create->isEnabled());
            unity->setValue(23);
            QVERIFY(create->isEnabled());
            create->click();
            QCOMPARE(editor.document()->units().size(), 6);
            QCOMPARE(units->currentItem()->data(Qt::UserRole).toInt(), 6);
            QVERIFY(units->currentItem()->text().startsWith("Squad 2"));
            QCOMPARE(unity->value(), 0);
            QCOMPARE(leaders->findData(10), -1);
            QCOMPARE(editor.document()->units().last().members[0], 10);
            QCOMPARE(editor.document()->units().last().cells[0], 8);
            const auto created = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(leaders->findData(10) >= 0);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), created);
            const auto filename = work.filePath("created-squad.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            units->setCurrentRow(units->count() - 1);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-create-squad.png"));
            editor.loadBytes(inputFor("create-empty.bin"));
            QCOMPARE(units->count(), 0);
            QVERIFY(create->isEnabled());
            leaders->setCurrentIndex(leaders->findData(11));
            cells->setCurrentIndex(4);
            create->click();
            QCOMPARE(units->count(), 1);
            QCOMPARE(editor.document()->units()[0].members[0], 11);
            QCOMPARE(editor.document()->units()[0].label, 1);
            editor.loadBytes(inputFor("create-full.bin"));
            QVERIFY(!create->isEnabled());
            const auto full=editor.document()->serialize();
            QVERIFY(!editor.recruitUnit(5,"No room",4));
            QCOMPARE(editor.document()->serialize(),full);
            editor.loadBytes(inputFor("create-order-conflict.bin"));
            leaders->setCurrentIndex(leaders->findData(10));
            create->click();
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("inconsistent"));
            QCOMPARE(editor.document()->units().size(), 5);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!create->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveDirectExchangeControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-exchange-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("exchange.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto target = editor.findChild<QComboBox *>("saveSwapUnit");
            auto member = editor.findChild<QComboBox *>("saveSwapMember");
            auto apply = editor.findChild<QPushButton *>("saveSwapApply");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            auto leader = editor.findChild<QPushButton *>("saveChangeLeader");
            QVERIFY(units && members && target && member && apply && unity && leader);
            const auto before = editor.document()->memory();
            units->setCurrentRow(1);
            members->setCurrentRow(1);
            QVERIFY(!leader->isEnabled());
            target->setCurrentIndex(target->findData(3));
            member->setCurrentIndex(member->findData(1));
            QVERIFY(member->currentText().contains("Soldier group 120"));
            QVERIFY(apply->isEnabled());
            unity->setValue(22);
            QVERIFY(!apply->isEnabled());
            unity->setValue(23);
            QVERIFY(apply->isEnabled());
            apply->click();
            QCOMPARE(editor.document()->units()[1].members[1], 219);
            QCOMPARE(editor.document()->units()[2].members[1], 100);
            QCOMPARE(editor.document()->units()[1].unity, 23);
            QCOMPARE(editor.document()->memory().mid(0x190f80, 364), before.mid(0x190f80, 364));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            members->setCurrentRow(1);
            target->setCurrentIndex(target->findData(0));
            member->setCurrentIndex(member->findData(-3));
            QVERIFY(apply->isEnabled());
            apply->click();
            QCOMPARE(editor.document()->units()[1].members[1], 0);
            QCOMPARE(editor.document()->units()[0].members[3], 100);
            QCOMPARE(editor.document()->units()[0].cells[3], 2);
            QCOMPARE(int(u8(editor.document()->memory(), 0x1971f0 + 2 * 25 + 24)), 2);
            QCOMPARE(int(u8(editor.document()->memory(), 0x1971f0 + 24)), 7);
            QCOMPARE(editor.document()->units()[0].unity, 23);
            const auto moved = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), moved);
            const auto filename = work.filePath("direct-exchange.SaveRAM");
            editor.exportFile(filename);
            QFile saved(filename);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, saved.readAll()).memory(),
                     editor.document()->memory());
            units->setCurrentRow(0);
            members->setCurrentRow(3);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-direct-exchange.png"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            units->setCurrentRow(0);
            members->setCurrentRow(1);
            target->setCurrentIndex(target->findData(2));
            member->setCurrentIndex(member->findData(1));
            apply->click();
            QCOMPARE(editor.document()->units()[0].members[1], 100);
            QCOMPARE(editor.document()->units()[1].members[1], 2);
            QCOMPARE(editor.document()->memory().mid(0x190f80, 364), before.mid(0x190f80, 364));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!apply->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveLeaderControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-leader-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing leader fixture");
                QByteArray input(0x10000, 0);
                input.replace(0x3a, 6180, file.readAll());
                if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                         0x1850, 0, 0))
                    throw Error("Cannot seal leader save");
                return input;
            };
            const auto input = batteryFor("unit-payload.bin");
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto leader = editor.findChild<QPushButton *>("saveChangeLeader");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && members && leader && unity);
            units->setCurrentRow(0);
            QVERIFY(!leader->isEnabled());
            members->setCurrentRow(2);
            QVERIFY(leader->isEnabled());
            unity->setValue(22);
            QVERIFY(!leader->isEnabled());
            unity->setValue(23);
            QVERIFY(leader->isEnabled());
            leader->click();
            QCOMPARE(editor.document()->units()[0].members, QVector<int>({3, 1, 2, 0, 0}));
            QCOMPARE(editor.document()->units()[0].cells, QVector<int>({8, 0, 4, 0, 0}));
            QCOMPARE(editor.document()->units()[0].unity, 23);
            QVERIFY(units->item(0)->text().contains("Member3"));
            QVERIFY(members->item(0)->text().contains("Leader"));
            const auto changed = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), changed);
            const auto filename = work.filePath("leader.SaveRAM");
            editor.exportFile(filename);
            QFile output(filename);
            QVERIFY(output.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, output.readAll()).memory(),
                     editor.document()->memory());
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-leader-change.png"));
            units->setCurrentRow(1);
            members->setCurrentRow(1);
            QVERIFY(!leader->isEnabled());
            editor.loadBytes(batteryFor("fixed-leader-81.bin"));
            units->setCurrentRow(0);
            members->setCurrentRow(1);
            const auto fixed = editor.document()->serialize();
            leader->click();
            QCOMPARE(editor.document()->serialize(), fixed);
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("fixed squad leader"));
            editor.loadBytes(batteryFor("commander-bad.bin"));
            units->setCurrentRow(0);
            members->setCurrentRow(1);
            const auto legion = editor.document()->serialize();
            leader->click();
            QCOMPARE(editor.document()->serialize(), legion);
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("commands a legion"));
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!leader->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSoldierReserveControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("soldier-reserve-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("unit-payload.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto cells = editor.findChild<QComboBox *>("saveReserveCell");
            auto create = editor.findChild<QPushButton *>("saveAddSoldierGroup");
            auto disband = editor.findChild<QPushButton *>("saveMoveToReserve");
            auto pool = editor.findChild<QLabel *>("saveSoldierPool");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            QVERIFY(units && members && cells && create && disband && pool && unity);
            units->setCurrentRow(0);
            QVERIFY(create->isEnabled());
            QVERIFY(cells->isEnabled());
            QVERIFY(pool->text().endsWith("9"));
            cells->setCurrentIndex(cells->findData(1));
            unity->setValue(22);
            QVERIFY(!create->isEnabled());
            unity->setValue(23);
            QVERIFY(create->isEnabled());
            create->click();
            QCOMPARE(editor.document()->units()[0].members[3], 101);
            QCOMPARE(editor.document()->reserveSoldiers(), 6);
            QCOMPARE(editor.document()->units()[0].unity, 23);
            QVERIFY(pool->text().endsWith("6"));
            const auto created = editor.document()->serialize();
            members->setCurrentRow(3);
            QVERIFY(disband->isEnabled());
            QVERIFY(disband->text().contains("Disband"));
            QVERIFY(editor.findChild<QPushButton *>("saveSwapApply")->isEnabled());
            disband->click();
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(pool->text().endsWith("9"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), created);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            units->setCurrentRow(1);
            members->setCurrentRow(1);
            QVERIFY(disband->toolTip().contains("Returns 1"));
            disband->click();
            QCOMPARE(editor.document()->reserveSoldiers(), 10);
            QCOMPARE(editor.document()->units()[1].members[1], 0);
            QCOMPARE(editor.document()->units()[1].unity, 23);
            cells->setCurrentIndex(cells->findData(7));
            create->click();
            QCOMPARE(editor.document()->reserveSoldiers(), 7);
            QCOMPARE(editor.document()->units()[1].members[1], 100);
            QCOMPARE(u16(editor.document()->memory(), 0x190f84), quint16(300));
            QCOMPARE(u8(editor.document()->memory(), 0x191074), quint8(0));
            const auto result = editor.document()->memory();
            const auto filename = work.filePath("soldier-reserve.SaveRAM");
            editor.exportFile(filename);
            QFile saved(filename);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, saved.readAll()).memory(), result);
            members->setCurrentRow(1);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-soldier-reserve.png"));
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.loadBytes(editor.document()->memory());
            QVERIFY(!create->isEnabled());
            QVERIFY(!disband->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveReserveDragging() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir()
                                     .filePath("save-unit-mips.cjs"), work.path()});
        QVERIFY(generator.waitForFinished(30000));
        QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
        auto fixture = [&](const QString &name) {
            QFile payload(work.filePath(name));
            if (!payload.open(QIODevice::ReadOnly)) return QByteArray{};
            QByteArray bytes(0x10000, 0);
            bytes.replace(0x3a, 6180, payload.readAll());
            if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(bytes.data() + 0x10),
                                     0x1850, 0, 0)) return QByteArray{};
            return bytes;
        };
        const auto input = fixture("unit-payload.bin");
        QVERIFY(!input.isEmpty());
        auto project = Project::fromRom(Rom::open(path));
        SaveEditor editor(&project);
        editor.resize(1400, 1000);
        editor.show();
        editor.loadBytes(input);
        auto panel = editor.findChild<QWidget *>("saveSquads");
        editor.findChild<QTabWidget *>()->setCurrentWidget(panel);
        auto reserves = editor.findChild<QListWidget *>("saveArmyReserve");
        auto units = editor.findChild<QListWidget *>("saveUnitList");
        auto board = editor.findChild<FormationBoard *>("saveUnitCellBoard");
        QVERIFY(reserves && units && board);
        QCoreApplication::processEvents();
        auto drop = [&](QWidget *target, QPoint point, const QMimeData &mime,
                        Qt::DropActions actions = Qt::MoveAction) {
            QDragEnterEvent enter(QPointF(point), actions, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &enter);
            if (!enter.isAccepted()) return false;
            QDragMoveEvent move(QPointF(point), actions, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &move);
            QDropEvent finish(point, actions, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &finish);
            return finish.isAccepted();
        };
        const QPoint reserveTarget = reserves->viewport()->rect().center();
        auto startAndCancel = [&](QWidget *source, QPoint from, QPoint to, int unit, int id) {
            bool started = false;
            const auto previousDrags = panel->findChildren<QDrag *>();
            QTimer::singleShot(0, &editor, [&] {
                for (auto drag : panel->findChildren<QDrag *>()) {
                    if (previousDrags.contains(drag)) continue;
                    const auto data = dynamic_cast<const SaveArmyDrag *>(drag->mimeData());
                    if (data && data->unit == unit && data->id == id && data->owner == panel)
                        started = true;
                }
                QDrag::cancel();
            });
            QTest::mousePress(source, Qt::LeftButton, Qt::NoModifier, from);
            QMouseEvent movement(QEvent::MouseMove, to, source->mapToGlobal(to),
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(source, &movement);
            QTest::mouseRelease(source, Qt::LeftButton, Qt::NoModifier, to);
            QCoreApplication::processEvents();
            return started;
        };
        auto sourceCell = board->cell(4);
        QVERIFY(startAndCancel(sourceCell, sourceCell->mapFrom(board, board->cellCenter(4)),
                               sourceCell->mapFrom(board, QPoint(-20, 60)), 0, 2));
        auto memberIcons = editor.findChild<QListWidget *>("saveUnitMembers");
        const auto memberPoint = memberIcons->visualItemRect(memberIcons->item(1)).center();
        QVERIFY(startAndCancel(memberIcons->viewport(), memberPoint, memberPoint + QPoint(25, 0), 0, 2));
        QCOMPARE(editor.document()->serialize(), input); // Cancelled gestures do not change a save.
        SaveArmyDrag member(panel, 0, 1, 2), leader(panel, 0, 0, 1);
        SaveArmyDrag foreign(&editor, 0, 1, 2), stale(panel, 0, 1, 3);
        QVERIFY(!drop(reserves->viewport(), reserveTarget, foreign));
        QVERIFY(!drop(reserves->viewport(), reserveTarget, stale));
        QVERIFY(!drop(reserves->viewport(), reserveTarget, leader));
        QVERIFY(!drop(reserves->viewport(), reserveTarget, member, Qt::CopyAction));
        QCOMPARE(editor.document()->serialize(), input);
        QCOMPARE(editor.undoStack()->count(), 0);
        auto expected = *editor.document();
        expected.apply(expected.planToReserve(0, 1, project.classData()), true);
        QVERIFY(drop(reserves->viewport(), reserveTarget, member));
        QCOMPARE(editor.document()->memory(), expected.memory());
        QCOMPARE(editor.undoStack()->count(), 1);
        QCOMPARE(reserves->count(), 1);
        QCOMPARE(reserves->item(0)->data(Qt::UserRole).toInt(), 2);
        editor.undoStack()->undo();
        QCOMPARE(editor.document()->serialize(), input);
        QCOMPARE(reserves->count(), 0);
        editor.undoStack()->redo();
        QCOMPARE(editor.document()->memory(), expected.memory());
        const auto reserved = editor.document()->serialize();
        const auto reservePoint = reserves->visualItemRect(reserves->item(0)).center();
        QVERIFY(startAndCancel(reserves->viewport(), reservePoint, reservePoint + QPoint(25, 0), -1, 2));
        QCOMPARE(editor.document()->serialize(), reserved);
        SaveArmyDrag reserve(panel, -1, -1, 2);
        QVERIFY(!drop(board, board->cellCenter(0), reserve)); // Leader occupies this cell.
        QCOMPARE(editor.document()->serialize(), reserved);
        auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
        unity->setValue(unity->value() + 1);
        QVERIFY(editor.hasPendingUnitEdits());
        QVERIFY(!drop(board, board->cellCenter(4), reserve));
        QCOMPARE(editor.document()->serialize(), reserved);
        editor.findChild<QPushButton *>("saveUnitDiscard")->click();
        reserves->setCurrentRow(0);
        QCOMPARE(editor.findChild<QTabWidget *>("saveArmyDetails")->currentIndex(), 0);
        expected.apply(expected.planFromReserve(2, 0, 4, project.classData()), true);
        QVERIFY(drop(board, board->cellCenter(4), reserve));
        QCOMPARE(editor.document()->memory(), expected.memory());
        QCOMPARE(editor.undoStack()->count(), 2);
        QCOMPARE(reserves->count(), 0);
        editor.undoStack()->undo();
        QCOMPARE(editor.document()->serialize(), reserved);
        // The overview chooses the first cell accepted by the same save rules.
        const auto destination = units->item(4);
        units->scrollToItem(destination);
        QCoreApplication::processEvents();
        auto nameplate = [&](QListWidgetItem *item) {
            const auto rect = units->visualItemRect(item);
            return QPoint(rect.center().x(), rect.bottom() - 16);
        };
        QVERIFY(drop(units->viewport(), nameplate(destination), reserve));
        bool found = false;
        for (const auto &unit : editor.document()->units())
            if (unit.index == 5) found = unit.members.contains(2);
        QVERIFY(found);
        const auto output = work.filePath("dragged.sra");
        editor.exportFile(output);
        QFile reopened(output);
        QVERIFY(reopened.open(QIODevice::ReadOnly));
        QCOMPARE(SaveDocument::open(project.rom(), reopened.readAll()).memory(),
                 editor.document()->memory());
        editor.undoStack()->undo();
        QCOMPARE(editor.document()->serialize(), reserved);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/save-reserve-drag.png"));
        // A real full-squad fixture must refuse reserve drops without losing the member.
        editor.loadBytes(fixture("unit-full-payload.bin"));
        QVERIFY(editor.moveToReserve(2, 2)); // Member 10 becomes available; squad 5 stays full.
        const auto full = editor.document()->serialize();
        SaveArmyDrag extra(panel, -1, -1, 10);
        const auto fullSquad = units->item(4);
        units->scrollToItem(fullSquad);
        QCoreApplication::processEvents();
        QVERIFY(!drop(units->viewport(), nameplate(fullSquad), extra));
        QCOMPARE(editor.document()->serialize(), full);
        // Existing carrying limits also apply when dragging out of a squad.
        editor.loadBytes(fixture("unit-capacity-payload.bin"));
        const auto overloaded = editor.document()->serialize();
        QVERIFY(!drop(reserves->viewport(), reserveTarget, member));
        QCOMPARE(editor.document()->serialize(), overloaded);
        // Overview gestures pick the actual member under the mouse, including sprites
        // above the tile, and place or exchange them in the hovered destination cell.
        auto overviewPoint = [&](int unit, int cell) {
            for (int row = 0; row < units->count(); ++row) {
                const auto item = units->item(row);
                if (item->data(Qt::UserRole).toInt() != unit) continue;
                units->scrollToItem(item);
                QCoreApplication::processEvents();
                const auto hits = item->data(ArmyOverviewHits).value<QImage>();
                for (int y = 0; y < hits.height(); ++y)
                    for (int x = 0; x < hits.width(); ++x)
                        if (qAlpha(hits.pixel(x, y)) && qRed(hits.pixel(x, y)) == cell + 1)
                            return units->visualItemRect(item).topLeft() + QPoint(6 + x, y);
            }
            return QPoint(-1, -1);
        };
        editor.loadBytes(input);
        const auto overviewSource = overviewPoint(0, 4);
        QVERIFY(overviewSource.x() >= 0);
        QVERIFY(startAndCancel(units->viewport(), overviewSource, overviewSource + QPoint(25, 0), 0, 2));
        QCOMPARE(editor.document()->serialize(), input);
        expected = *editor.document();
        expected.apply(expected.planUnitTransfer(0, 1, 5, 2, project.classData()), true);
        QVERIFY(drop(units->viewport(), overviewPoint(5, 2), member));
        QCOMPARE(editor.document()->memory(), expected.memory());
        QCOMPARE(editor.undoStack()->count(), 1);
        editor.undoStack()->undo();
        QCOMPARE(editor.document()->serialize(), input);
        editor.undoStack()->redo();
        QCOMPARE(editor.document()->memory(), expected.memory());
        editor.exportFile(work.filePath("overview-move.sra"));
        QFile overviewSave(work.filePath("overview-move.sra"));
        QVERIFY(overviewSave.open(QIODevice::ReadOnly));
        QCOMPARE(SaveDocument::open(project.rom(), overviewSave.readAll()).memory(), expected.memory());
        editor.undoStack()->undo();
        expected = *editor.document();
        expected.apply(expected.planUnitSwap(0, 1, 5, 1, project.classData()), true);
        QVERIFY(drop(units->viewport(), overviewPoint(5, 4), member));
        QCOMPARE(editor.document()->memory(), expected.memory());
        QCOMPARE(editor.document()->units()[0].unity, 23);
        QCOMPARE(editor.document()->units().last().unity, 23);
        editor.undoStack()->undo();
        QCOMPARE(editor.document()->serialize(), input);
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 0), member)); // Protected target leader.
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 2), leader));
        QCOMPARE(editor.document()->serialize(), input);
        unity->setValue(unity->value() + 1);
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 2), member));
        editor.findChild<QPushButton *>("saveUnitDiscard")->click();
        QCOMPARE(editor.document()->serialize(), input);
        editor.loadBytes(fixture("unit-full-payload.bin"));
        SaveArmyDrag tenth(panel, 2, 2, 10);
        const auto fullBefore = editor.document()->serialize();
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 6), tenth));
        QCOMPARE(editor.document()->serialize(), fullBefore);
        editor.loadBytes(fixture("unit-capacity-payload.bin"));
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 2), member));
        QCOMPARE(editor.document()->serialize(), overloaded);
        // A large member uses two places and needs its neighbouring cells clear.
        const auto classData = project.classData();
        int largeClass = -1;
        for (int id = 1; id < classData.entries().size(); ++id)
            if (classData.unitSize(id, id) == 2) { largeClass = id; break; }
        QVERIFY(largeClass >= 0);
        auto large = SaveDocument::open(project.rom(), input);
        large.apply(large.planToReserve(0, 2, classData), true);
        large.apply(large.planUnitEdit(0, {0, 8, 0, 0, 0}, 23, 2, classData), true);
        large.apply(large.planEdit(SaveTable::Characters, 2, "class", largeClass), true);
        large.apply(large.planEdit(SaveTable::Characters, 2, "secondaryClass", largeClass), true);
        editor.loadBytes(large.serialize());
        const auto largeBefore = editor.document()->serialize();
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 2), member)); // Adjacent to the middle cell.
        QCOMPARE(editor.document()->serialize(), largeBefore);
        large.apply(large.planFromReserve(3, 5, 2, classData), true);
        editor.loadBytes(large.serialize());
        const auto sixPlaces = editor.document()->serialize();
        QVERIFY(!drop(units->viewport(), overviewPoint(5, 6), member)); // Four small + one large = six.
        QCOMPARE(editor.document()->serialize(), sixPlaces);
        editor.loadBytes(input);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/save-overview-drag.png"));
        // Double-click the member under the pointer, including a different squad,
        // without changing the save or opening empty cells / Soldier records.
        auto tabs = editor.findChild<QTabWidget *>();
        auto doubleClick = [&](QPoint point) {
            QTest::mouseClick(units->viewport(), Qt::LeftButton, Qt::NoModifier, point);
            QTest::mouseDClick(units->viewport(), Qt::LeftButton, Qt::NoModifier, point);
            QCoreApplication::processEvents();
        };
        for (const auto [unit, cell, id] : {std::tuple{0, 4, 2}, std::tuple{5, 8, 9}}) {
            tabs->setCurrentWidget(panel);
            const auto point = overviewPoint(unit, cell);
            QTest::mouseClick(units->viewport(), Qt::LeftButton, Qt::NoModifier, point);
            bool opened = false;
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("saveMemberDialog");
                QVERIFY(dialog);
                const auto close = qScopeGuard([dialog] { if (dialog->isVisible()) dialog->reject(); });
                opened = true;
                QCOMPARE(tabs->currentWidget(), panel);
                QCOMPARE(dialog->findChild<QLineEdit *>("saveMember_name")->text(),
                         editor.document()->value(SaveTable::Characters, id, "name").toString());
                dialog->findChild<QSpinBox *>("saveMember_strength")->setValue(123);
                auto weapon = dialog->findChild<QComboBox *>("saveMember_weapon");
                weapon->setCurrentIndex(weapon->findData(1));
                dialog->findChild<QSpinBox *>("saveMember_hpMax")->setValue(200);
                dialog->findChild<QSpinBox *>("saveMember_hpCur")->setValue(250);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
                QVERIFY(dialog->isVisible());
                QVERIFY(!dialog->findChild<QLabel *>("saveMemberError")->text().isEmpty());
                dialog->findChild<QSpinBox *>("saveMember_hpCur")->setValue(180);
                QVERIFY(dialog->findChild<QLabel *>("saveMemberError")->text().isEmpty());
                QCOMPARE(editor.document()->serialize(), input); // Draft has not touched the army.
                QVERIFY(dialog->grab().save(QCoreApplication::applicationDirPath() + "/save-member-popup.png"));
                dialog->findChild<QDialogButtonBox *>()->button(
                    id == 2 ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
            });
            QMouseEvent doubleEvent(QEvent::MouseButtonDblClick, QPointF(point),
                                   QPointF(units->viewport()->mapToGlobal(point)),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(units->viewport(), &doubleEvent);
            QCoreApplication::processEvents();
            QVERIFY(opened);
            QCOMPARE(tabs->currentWidget(), panel);
            if (id == 2) {
                expected = SaveDocument::open(project.rom(), input);
                expected.apply(expected.planEdit(SaveTable::Characters, id, "hpMax", 200), true);
                expected.apply(expected.planEdit(SaveTable::Characters, id, "hpCur", 180), true);
                expected.apply(expected.planEdit(SaveTable::Characters, id, "strength", 123), true);
                expected.apply(expected.planEdit(SaveTable::Characters, id, "weapon", 1), true);
                QCOMPARE(editor.document()->memory(), expected.memory());
                QCOMPARE(editor.undoStack()->count(), 1);
                editor.undoStack()->undo();
            }
            QCOMPARE(editor.document()->serialize(), input);
        }
        tabs->setCurrentWidget(panel);
        for (const auto [unit, cell] : {std::pair{0, 2}, std::pair{2, 7}, std::pair{4, 7}}) {
            doubleClick(overviewPoint(unit, cell));
            QCOMPARE(tabs->currentWidget(), panel);
            QCOMPARE(editor.document()->serialize(), input);
        }
        doubleClick(nameplate(units->currentItem()));
        QCOMPARE(tabs->currentWidget(), panel);
        unity->setValue(unity->value() + 1);
        doubleClick(overviewPoint(5, 8));
        QCOMPARE(tabs->currentWidget(), panel);
        QVERIFY(editor.hasPendingUnitEdits());
        QCOMPARE(editor.document()->serialize(), input);
        editor.findChild<QPushButton *>("saveUnitDiscard")->click();
    }
    void saveSquadEditing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("save-unit-mips.cjs"),
                                     work.path()});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile payload(work.filePath("unit-payload.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 800);
            editor.show();
            editor.loadBytes(input);
            auto panel = editor.findChild<QWidget *>("saveSquads");
            editor.findChild<QTabWidget *>()->setCurrentWidget(panel);
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto members = editor.findChild<QListWidget *>("saveUnitMembers");
            auto unity = editor.findChild<QSpinBox *>("saveUnitUnity");
            auto tactic = editor.findChild<QComboBox *>("saveUnitTactic");
            auto apply = editor.findChild<QPushButton *>("saveUnitApply");
            auto supply = editor.findChild<QComboBox *>("saveUnitSupply1");
            auto supply0 = editor.findChild<QComboBox *>("saveUnitSupply0");
            QVERIFY(supply && supply0);
            QVERIFY(supply->findData(24) >= 0);
            QCOMPARE(supply->findData(21), -1);
            QCOMPARE(supply0->currentData().toInt(), 9);
            QVERIFY(!supply0->currentText().contains('\n'));
            QVERIFY(editor.findChild<QLabel *>("saveUnitCapacity")->text().contains("capacity 6"));
            unity->setValue(24);
            QVERIFY(
                !supply->isEnabled()); // A pending formation draft cannot be silently discarded.
            unity->setValue(23);
            QVERIFY(supply->isEnabled());
            const auto beforeSupplies = editor.document()->memory();
            supply->setCurrentIndex(supply->findData(24));
            QCOMPARE(editor.document()->units()[0].supplies[1], 24);
            auto afterSupplies = beforeSupplies;
            afterSupplies[0x1971f0 + 14] = 24;
            QCOMPARE(editor.document()->memory(), afterSupplies);
            QCOMPARE(editor.undoStack()->count(), 1);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            QCOMPARE(supply->currentData().toInt(), 0);
            editor.undoStack()->redo();
            QCOMPARE(supply->currentData().toInt(), 24);
            const auto supplyOutput = work.filePath("squad-supplies.SaveRAM");
            auto supplyMetadata = project.record(Table::Consumables, 24).bytes;
            const auto originalSupplyMetadata = supplyMetadata;
            supplyMetadata[5] = 1;
            project.replaceBytes(Table::Consumables, 24, supplyMetadata);
            panel->hide();
            panel->show();
            QVERIFY(supply->currentText().contains("Keep existing"));
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(supplyOutput));
            QVERIFY(!QFileInfo::exists(supplyOutput));
            project.replaceBytes(Table::Consumables, 24, originalSupplyMetadata);
            auto tooWide = project.record(Table::Consumables, 32).bytes;
            tooWide[5] = 0;
            project.replaceBytes(Table::Consumables, 32, tooWide);
            panel->hide();
            panel->show();
            QCOMPARE(supply->findData(32), -1);
            QVERIFY(supply->currentText().contains("Medal"));
            editor.exportFile(supplyOutput);
            QFile supplyFile(supplyOutput);
            QVERIFY(supplyFile.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, supplyFile.readAll()).memory(), afterSupplies);
            auto supplyScroll = editor.findChild<QScrollArea *>("saveUnitDetailScroll");
            QVERIFY(supplyScroll);
            editor.resize(1250, 1050);
            editor.findChild<QTabWidget *>("saveArmyDetails")->setCurrentIndex(3);
            supplyScroll->ensureWidgetVisible(editor.findChild<QComboBox *>("saveUnitSupply9"));
            QCoreApplication::processEvents();
            QVERIFY(supply->isVisible());
            QVERIFY(editor.grab().save("save-squad-supplies.png"));
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>("saveArmyDetails")->setCurrentIndex(0);
            QCOMPARE(units->count(), 5);
            QCOMPARE(members->item(0)->text(), QString("Member1 · Leader"));
            QVERIFY(editor.findChild<QPushButton *>("saveUnitCell0")->text().contains("Member1"));
            auto armyBoard = editor.findChild<FormationBoard *>("saveUnitCellBoard");
            QVERIFY(armyBoard);
            QVERIFY(armyBoard->cellCenter(6).y() > armyBoard->cellCenter(0).y()); // Saved front row nearest viewer.
            auto dragSaveMember = [&](int from, int to) {
                auto source = armyBoard->cell(from);
                const auto start = source->mapFrom(armyBoard, armyBoard->cellCenter(from));
                const auto end = source->mapFrom(armyBoard, armyBoard->cellCenter(to));
                QTest::mousePress(source, Qt::LeftButton, Qt::NoModifier, start);
                QMouseEvent movement(QEvent::MouseMove, end, source->mapToGlobal(end),
                                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(source, &movement);
                QTest::mouseRelease(source, Qt::LeftButton, Qt::NoModifier, end);
            };
            dragSaveMember(0, 4);
            QVERIFY(armyBoard->cell(4)->text().contains("Member1"));
            dragSaveMember(4, 0);
            QVERIFY(armyBoard->cell(0)->text().contains("Member1"));
            QCOMPARE(editor.document()->serialize(), input);
            QVERIFY(armyBoard->grab().save(QCoreApplication::applicationDirPath()+"/army-board-save.png"));
            members->setCurrentRow(0);
            editor.findChild<QPushButton *>("saveUnitCell4")
                ->click(); // Swap the leader and member2.
            unity->setValue(99);
            tactic->setCurrentIndex(tactic->findData(3));
            QCOMPARE(editor.document()->serialize(), input);
            QCOMPARE(editor.undoStack()->count(), 0);
            apply->click();
            QCOMPARE(editor.undoStack()->count(), 1);
            QCOMPARE(editor.document()->units()[0].cells, QVector<int>({4, 0, 8, 0, 0}));
            QCOMPARE(editor.document()->units()[0].unity, 99);
            QCOMPARE(editor.document()->units()[0].tactic, 3);
            const auto expected = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), expected);
            QVERIFY(editor.grab().save("save-squads.png"));
            const auto output = work.filePath("squads.SaveRAM");
            editor.exportFile(output);
            QFile saved(output);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto reopened = SaveDocument::open(rom, saved.readAll());
            QCOMPARE(reopened.memory(), editor.document()->memory());
            auto swap = editor.findChild<QPushButton *>("saveSwapApply");
            QVERIFY(!swap->isEnabled()); // Leader stays in place.
            members->setCurrentRow(1);
            auto targetUnit = editor.findChild<QComboBox *>("saveSwapUnit");
            auto targetMember = editor.findChild<QComboBox *>("saveSwapMember");
            targetUnit->setCurrentIndex(targetUnit->findData(5));
            targetMember->setCurrentIndex(targetMember->findData(1));
            QVERIFY(swap->isEnabled());
            unity->setValue(98);
            QVERIFY(!swap->isEnabled()); // Do not discard a pending squad edit.
            unity->setValue(99);
            QVERIFY(swap->isEnabled());
            swap->click();
            QCOMPARE(editor.document()->units()[0].members[1], 8);
            QCOMPARE(editor.document()->units().last().members[1], 2);
            QCOMPARE(editor.document()->units()[0].unity, 99);
            QCOMPARE(editor.document()->units().last().unity, 23);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            editor.undoStack()->redo();
            const auto swappedOutput = work.filePath("swapped.SaveRAM");
            editor.exportFile(swappedOutput);
            QFile swappedFile(swappedOutput);
            QVERIFY(swappedFile.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, swappedFile.readAll()).memory(),
                     editor.document()->memory());
            QVERIFY(editor.grab().save("save-member-swap.png"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            members->setCurrentRow(1);
            targetUnit->setCurrentIndex(targetUnit->findData(5));
            targetMember->setCurrentIndex(
                targetMember->findData(-3)); // Empty cell2, shown as cell3.
            QCOMPARE(swap->text(), QString("Move selected member"));
            QVERIFY(swap->isEnabled());
            unity->setValue(98);
            QVERIFY(!swap->isEnabled());
            unity->setValue(99);
            QVERIFY(swap->isEnabled());
            swap->click();
            QCOMPARE(editor.document()->units()[0].members[1], 0);
            QCOMPARE(editor.document()->units()[0].cells[1], 0);
            QCOMPARE(editor.document()->units().last().members[3], 2);
            QCOMPARE(editor.document()->units().last().cells[3], 2);
            QCOMPARE(editor.document()->units()[0].unity, 99);
            QCOMPARE(editor.document()->units().last().unity, 23);
            const auto moved = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), moved);
            const auto movedOutput = work.filePath("moved.SaveRAM");
            editor.exportFile(movedOutput);
            QFile movedFile(movedOutput);
            QVERIFY(movedFile.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, movedFile.readAll()).memory(),
                     editor.document()->memory());
            units->setCurrentRow(4);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-member-transfer.png"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            auto toReserve = editor.findChild<QPushButton *>("saveMoveToReserve");
            auto fromReserve = editor.findChild<QPushButton *>("saveAddFromReserve");
            auto reserveMember = editor.findChild<QComboBox *>("saveReserveMember");
            auto reserveCell = editor.findChild<QComboBox *>("saveReserveCell");
            QVERIFY(toReserve && fromReserve && reserveMember && reserveCell);
            units->setCurrentRow(0);
            QVERIFY(!toReserve->isEnabled()); // Leaders cannot move to reserve.
            members->setCurrentRow(1);
            QVERIFY(toReserve->isEnabled());
            unity->setValue(98);
            QVERIFY(!toReserve->isEnabled());
            unity->setValue(99);
            toReserve->click();
            QCOMPARE(editor.document()->reserveMembers(), QVector<int>{2});
            QCOMPARE(editor.document()->units()[0].members[1], 0);
            QCOMPARE(editor.document()->units()[0].unity, 0);
            const auto reserved = editor.document()->serialize();
            units->setCurrentRow(4);
            QCOMPARE(reserveMember->currentData().toInt(), 2);
            QVERIFY(reserveMember->currentText().contains("Member2"));
            reserveCell->setCurrentIndex(reserveCell->findData(2));
            QVERIFY(fromReserve->isEnabled());
            unity->setValue(22);
            QVERIFY(!fromReserve->isEnabled());
            unity->setValue(23);
            fromReserve->click();
            auto reserveExpected = SaveDocument::open(rom, moved).memory();
            reserveExpected[0x1971f0 + 12] = 0;
            reserveExpected[0x1971f0 + 5 * 25 + 12] = 0;
            QCOMPARE(editor.document()->memory(), reserveExpected);
            const auto reserveMoved = editor.document()->serialize();
            QVERIFY(editor.document()->reserveMembers().isEmpty());
            QVERIFY(!fromReserve->isEnabled());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), reserved);
            QVERIFY(fromReserve->isEnabled());
            editor.resize(1250, 1050);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("save-reserve-membership.png"));
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            editor.undoStack()->redo();
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), reserveMoved);
            const auto reserveOutput = work.filePath("reserve-transfer.SaveRAM");
            editor.exportFile(reserveOutput);
            QFile reserveFile(reserveOutput);
            QVERIFY(reserveFile.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, reserveFile.readAll()).memory(),
                     editor.document()->memory());
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            units->setCurrentRow(1);
            QCOMPARE(members->item(1)->text(), QString("Soldier group 1"));
            members->setCurrentRow(1);
            QVERIFY(toReserve->isEnabled());
            editor.findChild<QPushButton *>("saveUnitCell8")->click();
            apply->click();
            QCOMPARE(editor.document()->units()[1].cells[1], 8);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), expected);
            units->setCurrentRow(3);
            QCOMPARE(members->item(1)->text(), QString("Unknown member 220"));
            members->setCurrentRow(1);
            editor.findChild<QPushButton *>("saveUnitCell8")->click();
            apply->click();
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("unknown member"));
            QCOMPARE(editor.document()->serialize(), expected);
            QFile raw(work.filePath("unit-ram.bin"));
            QVERIFY(raw.open(QIODevice::ReadOnly));
            editor.loadBytes(raw.readAll());
            QVERIFY(!apply->isEnabled());
            QVERIFY(!toReserve->isEnabled());
            QVERIFY(!fromReserve->isEnabled());
            QVERIFY(!supply->isEnabled());
            QVERIFY(!editor.findChild<QPushButton *>("saveUnitCell0")->isEnabled());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSquadDraftProtection() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("save-unit-mips.cjs"),
                                     work.path()});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile payload(work.filePath("unit-payload.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            const auto packed = payload.readAll();
            QByteArray input(0x10000, 0);
            for (int slot : {0, 1}) {
                const int at = int(lc_save_record_offset(slot, 0));
                input.replace(at + 0x2a, 6180, packed);
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + at),
                                            lc_save_record_size(slot, 0), slot, 0));
            }
            MainWindow window;
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<SaveEditor *>() != nullptr, 20000);
            auto editor = window.findChild<SaveEditor *>();
            editor->loadBytes(input);
            auto workspace = window.findChild<QTabWidget *>();
            workspace->setCurrentWidget(editor);
            auto panel = editor->findChild<QWidget *>("saveSquads");
            editor->findChild<QTabWidget *>()->setCurrentWidget(panel);
            auto units = editor->findChild<QListWidget *>("saveUnitList");
            auto members = editor->findChild<QListWidget *>("saveUnitMembers");
            auto unity = editor->findChild<QSpinBox *>("saveUnitUnity");
            auto apply = editor->findChild<QPushButton *>("saveUnitApply");
            auto discard = editor->findChild<QPushButton *>("saveUnitDiscard");
            auto undo = editor->findChild<QPushButton *>("saveUndo");
            auto slotPicker = editor->findChild<QComboBox *>("saveSlot");
            auto group = window.findChild<QUndoGroup *>();
            QVERIFY(discard && undo && group);
            QVERIFY(!editor->modified());
            unity->setValue(99);
            members->setCurrentRow(1);
            editor->findChild<QPushButton *>("saveUnitCell5")->click();
            QVERIFY(editor->hasPendingUnitEdits());
            QVERIFY(editor->modified());
            QVERIFY(window.isWindowModified());
            QVERIFY(!units->isEnabled());
            QVERIFY(!slotPicker->isEnabled());
            QVERIFY(!undo->isEnabled());
            QVERIFY(!editor->findChild<QPushButton *>("exportSave")->isEnabled());
            QVERIFY(group->activeStack() == nullptr);
            editor->selectCharacter(1);
            QCOMPARE(editor->findChild<QTabWidget *>()->currentWidget(), panel);
            QVERIFY(editor->hasPendingUnitEdits());
            units->setCurrentRow(1);
            QCOMPARE(units->currentRow(), 0);
            QCOMPARE(unity->value(), 99);
            slotPicker->setCurrentIndex(slotPicker->findData(1));
            QCOMPARE(editor->document()->selectedSlot(), 0);
            QCOMPARE(slotPicker->currentData().toInt(), 0);
            QVERIFY(!editor->edit(SaveTable::Game, 0, "goth", 12345));
            const auto output = work.filePath("draft.SaveRAM");
            QVERIFY_THROWS_EXCEPTION(Error, editor->exportFile(output));
            QVERIFY(!QFileInfo::exists(output));
            group->undo();
            QCOMPARE(editor->document()->serialize(), input);
            workspace->setCurrentIndex(0);
            QVERIFY(group->activeStack() != nullptr);
            workspace->setCurrentWidget(editor);
            QVERIFY(group->activeStack() == nullptr);
            QCOMPARE(unity->value(), 99);
            QVERIFY(editor->findChild<QLabel *>("saveUnitDraftNote")->isVisible());
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save("save-squad-pending-draft.png"));
            apply->click();
            QVERIFY(!editor->hasPendingUnitEdits());
            QVERIFY(!discard->isEnabled());
            QCOMPARE(editor->document()->units()[0].unity, 99);
            QCOMPARE(editor->document()->units()[0].cells[1], 5);
            QCOMPARE(editor->undoStack()->count(), 1);
            QCOMPARE(group->activeStack(), editor->undoStack());
            group->undo();
            QCOMPARE(editor->document()->serialize(), input);
            QVERIFY(!editor->modified());
            unity->setValue(98);
            QVERIFY(!editor->findChild<QPushButton *>("saveRedo")->isEnabled());
            group->redo();
            QCOMPARE(editor->document()->serialize(), input);
            discard->click();
            QCOMPARE(unity->value(), 23);
            QVERIFY(!editor->hasPendingUnitEdits());
            QVERIFY(!editor->modified());
            QVERIFY(!window.isWindowModified());
            QVERIFY(editor->findChild<QPushButton *>("saveRedo")->isEnabled());
            editor->exportFile(output);
            QFile exported(output);
            QVERIFY(exported.open(QIODevice::ReadOnly));
            QCOMPARE(exported.readAll(), input);
            units->setCurrentRow(3); // Existing unqualified member220 cannot be repositioned.
            members->setCurrentRow(1);
            editor->findChild<QPushButton *>("saveUnitCell8")->click();
            apply->click();
            QVERIFY(editor->hasPendingUnitEdits());
            QVERIFY(discard->isEnabled());
            QVERIFY(editor->findChild<QLabel *>("saveError")->text().contains("unknown member"));
            QCOMPARE(editor->document()->serialize(), input);
            units->setCurrentRow(0);
            QCOMPARE(units->currentRow(), 3);
            discard->click();
            QVERIFY(!editor->hasPendingUnitEdits());
            units->setCurrentRow(0);
            QCOMPARE(units->currentRow(), 0);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveWorkspaceEditing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QFile file(QFileInfo(path).dir().filePath("tools/pj64_golden.sra"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto original = file.readAll();
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 700);
            editor.show();
            editor.loadBytes(original, file.fileName());
            auto grid = editor.findChild<QTableView *>("saveCharacters");
            auto model = grid->model();
            QVERIFY(model->rowCount() > 0);
            int hpColumn = -1;
            for (int i = 0; i < model->columnCount(); ++i)
                if (model->headerData(i, Qt::Horizontal).toString() == "Current HP")
                    hpColumn = i;
            QVERIFY(hpColumn > 0);
            const auto hpIndex = model->index(0, hpColumn);
            grid->setCurrentIndex(hpIndex);
            grid->scrollTo(hpIndex);
            grid->edit(hpIndex);
            QTRY_VERIFY(grid->findChild<QSpinBox *>() != nullptr);
            auto hpEditor = grid->findChild<QSpinBox *>();
            hpEditor->setValue(0);
            QTest::keyClick(hpEditor, Qt::Key_Return);
            QTRY_VERIFY(editor.modified());
            QCOMPARE(editor.document()->value(SaveTable::Characters, 1, "hpCur").toUInt(), 0U);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), original);
            QVERIFY(!editor.modified());
            editor.undoStack()->redo();
            QVERIFY(!editor.edit(SaveTable::Characters, 1, "element", 5));
            QVERIFY(!editor.findChild<QLabel *>("saveError")->text().isEmpty());
            QTemporaryDir work;
            const auto output = work.filePath("edited.sra");
            editor.exportFile(output);
            QVERIFY(!editor.modified());
            QFile saved(output);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, saved.readAll())
                         .value(SaveTable::Characters, 1, "hpCur")
                         .toUInt(),
                     0U);
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(file.fileName()));
            editor.findChild<QPushButton *>("restoreSave")->click();
            QCOMPARE(editor.document()->serialize(), original);
            QVERIFY(editor.modified()); // The last export contains the zero-HP edit.
            QVERIFY(editor.grab().save("save-workspace.png"));
            auto twoSlots = original;
            auto swap = [](QByteArray &bytes) {
                for (int i = 0; i < bytes.size(); i += 4) {
                    std::swap(bytes[i], bytes[i + 3]);
                    std::swap(bytes[i + 1], bytes[i + 2]);
                }
            };
            swap(twoSlots);
            twoSlots.replace(0x1860, 0x1850, twoSlots.mid(0x10, 0x1850));
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(twoSlots.data() + 0x1860),
                                        0x1850, 1, 0));
            swap(twoSlots);
            editor.loadBytes(twoSlots);
            auto slotPicker = editor.findChild<QComboBox *>("saveSlot");
            slotPicker->setCurrentIndex(slotPicker->findData(1));
            QCOMPARE(editor.document()->selectedSlot(), 1);
            QVERIFY(editor.edit(SaveTable::Game, 0, "goth", 12345));
            slotPicker->setCurrentIndex(slotPicker->findData(0));
            QVERIFY(editor.edit(SaveTable::Game, 0, "goth", 23456));
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->selectedSlot(), 1);
            QCOMPARE(editor.document()->serialize(), twoSlots);
            const auto beforeClass = project;
            QCOMPARE(project.appendClass(1,"Save Knight"),165);
            QVERIFY(editor.edit(SaveTable::Characters,1,"class",165));
            editor.exportFile(work.filePath("new-class.sra"));
            project = beforeClass;
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportFile(work.filePath("missing-class.sra")));
            editor.undoStack()->undo();
            editor.exportFile(work.filePath("restored-class.sra"));
            // Opening and confirming an unknown encoded name must not truncate it.
            auto raw = SaveDocument::open(rom,original).memory();
            raw.replace(0x193bf8,17,QByteArray(17,char(1)));
            editor.loadBytes(raw);
            grid->setCurrentIndex(model->index(0,1)); grid->scrollTo(model->index(0,1)); grid->edit(model->index(0,1));
            QTRY_VERIFY(grid->findChild<QLineEdit *>() != nullptr);
            auto unknownName = grid->findChild<QLineEdit *>();
            QCOMPARE(unknownName->text(),QString("{01}").repeated(17));
            QTest::keyClick(unknownName,Qt::Key_Return);
            QCoreApplication::processEvents();
            QCOMPARE(editor.document()->serialize(),raw);
            MainWindow window;
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<SaveEditor *>() != nullptr, 20000);
            auto integrated = window.findChild<SaveEditor *>();
            integrated->loadBytes(original);
            auto group = window.findChild<QUndoGroup *>();
            QVERIFY(group);
            auto tabs = window.findChild<QTabWidget *>();
            tabs->setCurrentWidget(integrated);
            QCOMPARE(group->activeStack(), integrated->undoStack());
            QVERIFY(integrated->edit(SaveTable::Game, 0, "goth", 12345));
            QVERIFY(window.isWindowModified());
            group->undo();
            QVERIFY(!integrated->modified());
            tabs->setCurrentIndex(0);
            QVERIFY(group->activeStack() != integrated->undoStack());
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void editUndoRedoPreservesRecord() {
        QByteArray data(0x2800000, char(0));
        put32(data, 0, 0x80371240);
        data.replace(0x20, 12, "OgreBattle64");
        data.replace(0x3b, 2, "NO");
        data[0x3e] = 'E';
        auto project = Project::fromRom(Rom::fromBytes(data));
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        auto original = project.record(Table::Classes, 5).bytes;
        QVERIFY(model.setData(model.index(5, 2), 2, Qt::EditRole));
        auto expected = original;
        expected[4] = 2;
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
        undo.undo();
        QCOMPARE(project.record(Table::Classes, 5).bytes, original);
        undo.redo();
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
        QVERIFY(!model.setData(model.index(5, 2), 256, Qt::EditRole));
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
    }
    void transformSequencePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        QByteArray transform(8 + 960 + 16, 0);
        put32(transform, 0, 2);
        put32(transform, 4, 1);
        put32(transform, 8 + 0xa0, 2000);
        put32(transform, 488 + 0xa0, 8000);
        for (int channel = 0; channel < 20; ++channel) {
            put32(transform, 8 + 0x190 + channel * 4, 1000);
            put32(transform, 488 + 0x190 + channel * 4, 1000);
        }
        put32(transform, 976, 1);
        put32(transform, 980, 0xffffffffU);
        const auto packed = encodeLzss(transform);
        put32(bytes, 0x2700000, quint32(packed.size() + 4));
        put32(bytes, 0x2700004, quint32(transform.size()));
        bytes.replace(0x2700008, packed.size(), packed);
        put32(bytes, 0x594280 + 0x19a63ec + 4, 0x2700000 - 0x594280);
        const auto rom = Rom::fromBytes(bytes);
        TransformPreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        DecodedImage image;
        image.origin = {-10, -10};
        image.composite = {20, 20, QByteArray(20 * 20 * 4, 0)};
        for (int i = 0; i < 400; ++i) {
            image.composite.rgba[i * 4] = char(-1);
            image.composite.rgba[i * 4 + 3] = char(-1);
        }
        preview.setImage(image);
        const QVector<quint32> words{0x36,  0, 0, 0, 0, 0,           120000,
                                     90000, 8, 0, 0, 3, 0x80000001U, 0xff000005U};
        QByteArray script(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(script, i * 4, words[i]);
        const auto program = DirectorProgram::decode(script);
        preview.selectCommand(program, 1);
        auto tick = preview.findChild<QSpinBox *>("transformPosition");
        auto channel = preview.findChild<QSpinBox *>("transformChannel");
        auto play = preview.findChild<QPushButton *>("transformPlay");
        auto step = preview.findChild<QPushButton *>("transformStep");
        auto status = preview.findChild<QLabel *>("transformStatus");
        QVERIFY(tick && channel && play && step && status);
        QVERIFY2(!preview.renderedFrame().isNull(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QCOMPARE(first.pixelColor(153, 120), QColor(Qt::red));
        QVERIFY(first.pixelColor(176, 120) != QColor(Qt::red));
        step->click();
        QCOMPARE(tick->value(), 1);
        QCOMPARE(std::bit_cast<float>(u32(preview.property("channelRecord").toByteArray(), 0x40)),
                 4.f);
        tick->setValue(3);
        QVERIFY(preview.renderedFrame().pixelColor(153, 120) != QColor(Qt::red));
        QCOMPARE(preview.renderedFrame().pixelColor(176, 120), QColor(Qt::red));
        QVERIFY(!play->isEnabled());
        QVERIFY(status->text().contains("Sequence complete"));
        channel->setValue(1);
        QCOMPARE(std::bit_cast<float>(u32(preview.property("channelRecord").toByteArray(), 0x40)),
                 0.f);
        channel->setValue(0);
        tick->setValue(0);
        QCOMPARE(preview.renderedFrame(), first);
        play->click();
        QTRY_COMPARE_WITH_TIMEOUT(tick->value(), 3, 2000);
        QCOMPARE(play->text(), QString("Play"));
        QVERIFY(!play->isEnabled());
        preview.selectSequence(0, 99, 3, SceneCamera(rom.bytes().mid(0x2866f0, 56), 1));
        QVERIFY(preview.renderedFrame().isNull());
        QVERIFY(!step->isEnabled());
        QVERIFY(status->text().contains("group"));
        preview.findChild<QPushButton *>("transformRestart")->click();
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save("transform-preview-ui.png"));
    }
    void imagePresentationPlayback_data() {
        QTest::addColumn<int>("transition");
        QTest::newRow("scripted-pan")<<-1;
        QTest::newRow("echo-forward")<<0;
        QTest::newRow("echo-reverse")<<1;
    }
    void imageSkipInputReplay() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Start-skip playback",source,false);
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0xb2};
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0xb3<<0x80000005U<<12345<<0x50<<0<<1<<0;
        for(int i=0;i<150;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        auto skip=preview.findChild<QPushButton *>("sceneSkip");auto view=preview.findChild<QGraphicsView *>("sceneCanvas");
        QVERIFY(!skip->isVisible());position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(skip->isVisible());
        const auto before=preview.renderedFrame();QTest::keyClick(view,Qt::Key_Return);
        QCOMPARE(preview.property("sceneTick").toInt(),11);QVERIFY(!skip->isVisible());
        position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto skipped=preview.renderedFrame();QVERIFY(skipped!=before);
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(14);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
        preview.selectScene(scenes,id,0);position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        skip->click();position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
    }
    void pathSpritePreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto id = scenes.createScene("Path-oriented sprites", source, false);
        QVector<quint32> words{0x80000007U,
                               47,
                               0x3a,
                               1,
                               0,
                               0,
                               0,
                               70,
                               70,
                               130,
                               8,
                               0,
                               0x80000000U,
                               0x49,
                               0,
                               3,
                               0,
                               90,
                               quint32(-1),
                               3,
                               0,
                               0,
                               1,
                               0x63,
                               0,
                               30,
                               1,
                               0,
                               quint32(-36),
                               0,
                               180,
                               3,
                               0,
                               quint32(-1),
                               0x63,
                               1,
                               30,
                               1,
                               0,
                               48,
                               0,
                               180,
                               quint32(-1),
                               0,
                               quint32(-1)};
        for (int i = 0; i < 140; ++i) {
            if (i == 65)
                words << 0x63 << 0 << 0 << 0 << 0 << 0 << 0 << 180 << 4 << 0 << 0;
            words << 0 << 0x80000000U;
        }
        words << 0x80000001U << 0xff000004U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1200, 900);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(objects->count(), 2);
        const auto first = preview.renderedFrame();
        bool visible = false;
        for (int y = 0; y < 240 && !visible; ++y)
            for (int x = 0; x < 320 && !visible; ++x)
                visible = preview.spriteAt({x, y}) >= 0;
        QVERIFY(visible);
        position->setValue(124);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 124, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto later = preview.renderedFrame();
        QVERIFY(later != first);
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QCOMPARE(preview.renderedFrame(), first);
        position->setValue(124);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 124);
        QCOMPARE(preview.renderedFrame(), later);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QCOMPARE(preview.renderedFrame(), first);
        QCoreApplication::processEvents();
        QVERIFY(preview.grab().save("scene-path-sprites.png"));
    }
    void pathRibbonPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto id = scenes.createScene("Path ribbon playback", source, false);
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0,0x80000000U,
                               0x49,0,3,0,90,0xffffffffU,3,0,0,1};
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0x4b<<0<<1;
        for(int i=0;i<35;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto background=preview.renderedFrame();
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto revealed=preview.renderedFrame();
        int changed=0;for(int y=0;y<240;++y)for(int x=0;x<320;++x)changed+=revealed.pixel(x,y)!=background.pixel(x,y);
        QVERIFY(changed>50);QVERIFY(status->text().contains("approximate strip"));
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        const auto complete=preview.renderedFrame();QVERIFY(complete!=revealed);
        QVERIFY(preview.grab().save("scene-path-ribbon.png"));
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),revealed);
        position->setValue(150);QTRY_COMPARE(preview.property("sceneTick").toInt(),150);QVERIFY(preview.renderedFrame()!=complete);
        position->setValue(174);QTRY_COMPARE(preview.property("sceneTick").toInt(),174);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QCOMPARE(preview.renderedFrame(),background);
        position->setValue(124);QTRY_COMPARE(preview.property("sceneTick").toInt(),124);QCOMPARE(preview.renderedFrame(),complete);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),revealed);
    }
    void storyOpeningDialogue() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, 0x1fa8bf4 - 0x594284, 2);
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        for (int i = 0; i < 200 && preview.property("sceneDialogueState").toInt() != 4; ++i) {
            step->click();
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        }
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 4);
        const int tick = preview.property("sceneTick").toInt();
        const auto frame = preview.renderedFrame();
        const auto pixels = frame.convertToFormat(QImage::Format_RGB32);
        int lit = 0;
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                lit += (pixels.pixel(x, y) & 0x00ffffffU) != 0;
        QVERIFY(lit > 100);
        QVERIFY(lit < 320 * 240 / 2); // Authored black fade remains behind the dialogue.
        QVERIFY(preview.grab().save("scene-story-opening.png"));
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        position->setValue(tick);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), tick);
        QCOMPARE(preview.renderedFrame(), frame);
        position->setValue(250);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 250, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), qulonglong(2));
        const auto room = preview.renderedFrame();
        QVERIFY(room != frame);
        QVERIFY(preview.grab().save("scene-story-revealed.png"));
        position->setValue(tick);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), tick);
        position->setValue(250);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 250, 20000);
        QCOMPARE(preview.renderedFrame(), room);
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), qulonglong(2));
    }
    void storyCompleteReplay() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        const auto savedScenes = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, 0x1fa8bf4 - 0x594284, 2);
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QMap<int, QImage> frames;
        for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
            if (i % 45 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
            if (i % 500 == 499)
                frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
        }
        QVERIFY2(status->text().contains("Scene released."), qPrintable(status->text()));
        const int end = preview.property("sceneTick").toInt();
        QVERIFY(end > 250);
        const auto finalFrame = preview.renderedFrame();
        const auto audioCount = preview.property("sceneAudioRequests").toULongLong();
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
            position->setValue(it.key());
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
            QCOMPARE(preview.renderedFrame(), it.value());
        }
        position->setValue(end);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), end, 20000);
        QCOMPARE(preview.renderedFrame(), finalFrame);
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), audioCount);
        QCOMPARE(scenes.json(), savedScenes);
        qInfo() << "Full story scene released at step" << end << "with" << audioCount
                << "audio requests;" << frames.size() << "replay samples";
    }
    void sceneDialogueReplay() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        auto dialogue = Dialogues::load(rom);
        dialogue.setEntry(Dialogues::Table::Primary, 341, 23,
                          "@v020@y003Edited @%0@aAfter confirm@s");
        const auto savedText = dialogue.json();
        const auto id = scenes.createScene("Dialogue replay", 0x1fa8bf4 - 0x594284, false);
        QVector<quint32> words{0x80000006U, 34,  0xbf, 0, 341, 23, 0, 160,
                               180,         192, 0,    0, 0,   0,  0, 1};
        for (int i = 0; i < 250; ++i)
            words << 0 << 0x80000000U;
        words << 6 << 0;
        for (int i = 0; i < 80; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << 0xff000001U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        const auto savedScenes = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 900);
        preview.show();
        preview.selectScene(scenes, id, 2, dialogue);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        const auto waiting = preview.renderedFrame();
        QVERIFY(preview.findChild<QPushButton *>("panelConfirm")->isVisible());
        QVERIFY(preview.grab().save("scene-dialogue.png"));
        preview.findChild<QPushButton *>("panelConfirm")->click();
        seek(125);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 4);
        const auto resumed = preview.renderedFrame();
        QVERIFY(resumed != waiting);
        seek(275);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), -1);
        QVERIFY(!preview.findChild<QPushButton *>("panelConfirm")->isVisible());
        seek(60);
        QCOMPARE(preview.renderedFrame(), waiting);
        seek(125);
        QCOMPARE(preview.renderedFrame(), resumed);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(125);
        QCOMPARE(preview.renderedFrame(), resumed);
        auto name = preview.findChild<QLineEdit *>("sceneDialogueName0");
        name->setText("Joe");
        QMetaObject::invokeMethod(name, "editingFinished");
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        QVERIFY(preview.renderedFrame() != waiting);
        QCOMPARE(dialogue.json(), savedText);
        QCOMPARE(scenes.json(), savedScenes);
        preview.setDialogues(dialogue);
        QCOMPARE(preview.property("sceneTick").toInt(), 60); // No reset without a text change.
        dialogue.setEntry(Dialogues::Table::Primary, 341, 23, "@v020@y003Current edit@aEnd@s");
        preview.setDialogues(dialogue);
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        QVERIFY(preview.renderedFrame() != waiting);
        name->setFocus(); // Focus loss during widget destruction must not restart dead state.
    }
    void storyCastPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const quint32 key = 0x1fa8bf4 - 0x594284;
        auto program = scenes.program(key);
        for (auto &command : program.commands)
            if (command.opcode == 0x1b) {
                command.operands[4] = 0;
                command.operands[5] = 0;
                break;
            }
        scenes.replace(key, program.encode());
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1200, 900);
        preview.show();
        preview.selectScene(scenes, key, 2);
        auto first = preview.findChild<QComboBox *>("sceneCast0");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(first->isVisible());
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto original = preview.renderedFrame();
        first->setCurrentIndex(first->findData(0x54));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto changed = preview.renderedFrame();
        QVERIFY(changed != original);
        QVERIFY(preview.grab().save("scene-story-cast.png"));
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QCOMPARE(preview.renderedFrame(), changed);
        QCOMPARE(scenes.json(), before);
        preview.selectScene(scenes, scenes.original().entries().first().key, 2);
        QVERIFY(!first->isVisible());
    }
    void savedStoryCastPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("story-cast-save-mips.cjs"),
                                 work.path()});
        QVERIFY(generator.waitForFinished(30000));
        QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
        QByteArray battery(0x10000, 0);
        for (int slot : {0, 1}) {
            QFile f(work.filePath(QString("cast-%1.bin").arg(slot)));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const int at = int(lc_save_record_offset(slot, 0));
            battery.replace(at + 0x2a, 6180, f.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(battery.data() + at),
                                        lc_save_record_size(slot, 0), slot, 0));
        }
        MainWindow window;
        window.resize(1600, 1050);
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<SaveEditor *>(), 20000);
        auto save = window.findChild<SaveEditor *>();
        auto cutscenes = window.findChild<CutsceneEditor *>();
        auto preview = window.findChild<ScenePreview *>();
        QVERIFY(save && cutscenes && preview);
        window.findChild<QTabWidget *>()->setCurrentWidget(cutscenes);
        cutscenes->findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(preview);
        auto sceneList = cutscenes->findChild<QListWidget *>("cutsceneScenes");
        QVERIFY(sceneList);
        QTRY_VERIFY_WITH_TIMEOUT(sceneList->count() >= 1498, 20000);
        for (int row = 0; row < sceneList->count(); ++row)
            if (sceneList->item(row)->data(Qt::UserRole).toUInt() == 0x1fa8bf4 - 0x594284)
                sceneList->setCurrentRow(row);
        auto scenes = ScenePrograms::load(rom);
        const quint32 key = 0x1fa8bf4 - 0x594284;
        auto program = scenes.program(key);
        for (auto &command : program.commands)
            if (command.opcode == 0x1b) {
                command.operands[4] = 0;
                command.operands[5] = 0;
                break;
            }
        scenes.replace(key, program.encode());
        const auto before = scenes.json();
        preview->selectScene(scenes, key, 2);
        auto use = preview->findChild<QPushButton *>("sceneUseSavedCast");
        auto reset = preview->findChild<QPushButton *>("sceneResetCast");
        auto source = preview->findChild<QLabel *>("sceneCastSource");
        auto primary = preview->findChild<QComboBox *>("sceneCast0");
        auto secondary = preview->findChild<QComboBox *>("sceneCastSecondary0");
        auto position = preview->findChild<QSpinBox *>("scenePosition");
        auto status = preview->findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(use->isEnabled());
        use->click();
        QVERIFY(source->text().contains("Open a save"));
        QCOMPARE(primary->currentData().toInt(), 0x58);
        save->loadBytes(battery);
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(secondary->currentData().toInt(), 0x55);
        QVERIFY(source->text().contains("slot 1"));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        QCoreApplication::processEvents();
        QVERIFY(preview->isVisible());
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        auto actors = preview->findChild<QComboBox *>("sceneLiveActors");
        auto actorState = preview->findChild<QLabel *>("sceneActorState");
        auto findActor = preview->findChild<QPushButton *>("sceneFindActorAction");
        QVERIFY(actors && actorState && findActor);
        QVERIFY(actors->count() >= 5);
        actors->setCurrentIndex(actors->findData(1));
        QVERIFY(actorState->text().contains("Class pair 86 / 87"));
        const auto placement =
            std::find_if(program.commands.cbegin(), program.commands.cend(),
                         [](const auto &c) { return c.opcode == 0x14 && c.operands[0] == 1; });
        QVERIFY(placement != program.commands.cend());
        for (int axis = 0; axis < 3; ++axis)
            QVERIFY(actorState->text().contains(
                QString::number(qint32(placement->operands[4 + axis]) / 1000., 'f', 3)));
        const auto state = actorState->text();
        const auto fromSave = preview->renderedFrame();
        position->setValue(1);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QCOMPARE(preview->renderedFrame(), fromSave);
        QCOMPARE(actors->currentData().toInt(), 1);
        QCOMPARE(actorState->text(), state);
        position->setValue(0);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
        QVERIFY(!findActor->isEnabled());
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        QCOMPARE(actors->currentData().toInt(), 1);
        QCOMPARE(actorState->text(), state);
        auto pages = cutscenes->findChild<QTabWidget *>("cutsceneDetailPages");
        auto actions = cutscenes->findChild<SceneActionEditor *>();
        QVERIFY(actions && findActor->isEnabled());
        findActor->click();
        QCOMPARE(pages->currentWidget(), actions);
        auto actionList = actions->findChild<QListWidget *>("sceneActionList");
        QVERIFY(actionList->currentItem()->text().contains("Create Actor"));
        QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("1"));
        const auto selectedAction = actionList->currentRow();
        auto field = actions->findChild<QLineEdit *>("sceneActionField4");
        field->setText("-");
        QMetaObject::invokeMethod(field, "editingFinished");
        QVERIFY(actions->property("invalidEdits").toBool());
        pages->setCurrentWidget(preview);
        actors->setCurrentIndex(actors->findData(0));
        findActor->click();
        QCOMPARE(pages->currentWidget(), preview);
        QCOMPARE(actionList->currentRow(), selectedAction);
        QCOMPARE(field->text(), QString("-"));
        actions->findChild<QPushButton *>("sceneActionDiscard")->click();
        preview->selectScene(scenes, key, 2);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        actors->setCurrentIndex(actors->findData(1));

        QCoreApplication::processEvents();
        QVERIFY(window.grab().save("scene-saved-cast.png"));
        actors->showPopup();
        QTRY_VERIFY(actors->view()->isVisible());
        actors->view()->setCurrentIndex(actors->model()->index(actors->findData(2), 0));
        QCOMPARE(actors->view()->currentIndex().data(Qt::UserRole).toInt(), 2);
        position->setValue(4);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 4);
        QVERIFY(actors->view()->isVisible());
        QCOMPARE(actors->view()->currentIndex().data(Qt::UserRole).toInt(), 2);
        actors->hidePopup();
        QCOMPARE(actors->currentData().toInt(), 1); // Browsing did not commit a selection.
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);

        secondary->setCurrentIndex(secondary->findData(0x54));
        QCOMPARE(preview->property("sceneTick").toInt(), 0);
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(source->text(), QString("Manual preview cast"));
        use->click();
        QCOMPARE(secondary->currentData().toInt(), 0x55);
        auto slot = save->findChild<QComboBox *>("saveSlot");
        slot->setCurrentIndex(slot->findData(1));
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QVERIFY(source->text().contains("slot 2"));
        auto invalid = save->document()->memory();
        put8(invalid, 0x193681, 255);
        save->loadBytes(invalid);
        use->click();
        QVERIFY(source->text().contains("Cast unchanged"));
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QCOMPARE(save->document()->serialize(), invalid);
        save->loadBytes(battery);
        use->click();
        reset->click();
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QCOMPARE(source->text(), QString("Sample cast"));
        auto army = preview->findChild<QPushButton *>("sceneUseArmyCast");
        QVERIFY(army && army->isEnabled());
        QVERIFY(save->edit(SaveTable::Characters, 1, "class", 0x56));
        QVERIFY(save->edit(SaveTable::Characters, 1, "secondaryClass", 0x57));
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpMax", 100));
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpCur", 100));
        const auto armySave = save->document()->serialize();
        army->click();
        QCOMPARE(primary->currentData().toInt(), 0x56);
        QCOMPARE(secondary->currentData().toInt(), 0x57);
        QVERIFY(source->text().contains("Current army from slot 1"));
        QCOMPARE(preview->findChild<QComboBox *>("sceneCast1")->currentData().toInt(), 4);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        actors->setCurrentIndex(actors->findData(0));
        QVERIFY(actorState->text().contains("Class pair 86 / 87"));
        const auto armyPlacement =
            std::find_if(program.commands.cbegin(), program.commands.cend(),
                         [](const auto &c) { return c.opcode == 0x14 && c.operands[0] == 0; });
        QVERIFY(armyPlacement != program.commands.cend());
        for (int axis = 0; axis < 3; ++axis)
            QVERIFY(actorState->text().contains(
                QString::number(qint32(armyPlacement->operands[4 + axis]) / 1000., 'f', 3)));
        const auto armyFrame = preview->renderedFrame();
        QVERIFY(!armyFrame.isNull());
        position->setValue(0);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        QCOMPARE(preview->renderedFrame(), armyFrame);
        QCOMPARE(save->document()->serialize(), armySave);
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpCur", 0));
        army->click();
        QCOMPARE(primary->currentData().toInt(), 4);
        save->undoStack()->undo();
        army->click();
        QCOMPARE(primary->currentData().toInt(), 0x56);
        QCOMPARE(scenes.json(), before);
        save->loadBytes(battery);
        reset->click();
        auto proxy = qobject_cast<QAbstractProxyModel *>(
            window.findChild<QTableView *>("classesTable")->model());
        QVERIFY(proxy);
        auto classes = qobject_cast<RecordModel *>(proxy->sourceModel());
        QVERIFY(classes);
        const int added = classes->appendClass(0x55, "Cast companion");
        QCOMPARE(added, 165);
        QCOMPARE(secondary->itemText(secondary->findData(added)), QString("Cast companion"));
        auto withAdded = save->document()->memory();
        put8(withAdded, 0x193681, quint8(added));
        save->loadBytes(withAdded);
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(secondary->currentData().toInt(), added);
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        // The active project undo stack removes the new class, not any save bytes.
        auto undoGroup = window.findChild<QUndoGroup *>();
        QVERIFY(undoGroup);
        undoGroup->undo();
        QCOMPARE(secondary->findData(added), -1);
        QCOMPARE(secondary->currentData().toInt(), 0x54);
        QVERIFY(source->text().contains("removed preview class"));
        QCOMPARE(save->document()->serialize(), withAdded);
        save->loadBytes(battery);
        reset->click();
        QCOMPARE(save->document()->serialize(), battery);
        QCOMPARE(save->undoStack()->count(), 0);
        QCOMPARE(scenes.json(), before);
        QVERIFY(!window.isWindowModified());
    }
    void panelPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        for (quint32 preset : {33U, 3U, 1U, 2U, 34U}) {
            const auto id =
                scenes.createScene(QString("Panel %1 replay").arg(preset), source, false);
            QVector<quint32> words{0x80000007U, 47, 0x80000006U, 0,  0x3a, 2, 0,
                                   0,           0,  70,          70, 130,  8};
            for (int i = 0; i < 4; ++i)
                words << 0 << 0x80000000U;
            // Open only when the native owner query reports this slot absent.
            words << 0x56 << 0x6a << 0 << 0xfffffffbU << 4 << 0x62 << 4 << preset << 0x80000002U;
            for (int i = 0; i < 150; ++i)
                words << 0 << 0x80000000U;
            words << 0x6b << 4;
            for (int i = 0; i < 30; ++i)
                words << 0 << 0x80000000U;
            words << 0x80000001U << 0xff000004U;
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            const auto savedScenes = scenes.json();
            ScenePreview preview(rom);
            preview.resize(1200, 800);
            preview.show();
            preview.selectScene(scenes, id, 0);
            auto position = preview.findChild<QSpinBox *>("scenePosition");
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            auto reward = preview.findChild<QComboBox *>("sceneReward");
            QCOMPARE(reward->isVisible(), preset == 34);
            const auto seek = [&](int tick) {
                position->setValue(tick);
                QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
                QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            };
            seek(4);
            const auto before = preview.renderedFrame();
            seek(12);
            const auto opening = preview.renderedFrame();
            QVERIFY(opening != before);
            seek(20);
            const auto ready = preview.renderedFrame();
            QVERIFY(ready != opening);
            QVERIFY(status->text().contains("approximate frame"));
            if (preset == 3 || preset == 1 || preset == 2) {
                preview.findChild<QPushButton *>("panelDown")->click();
                QCOMPARE(preview.property("sceneTick").toInt(), 21);
                QVERIFY(preview.renderedFrame() != ready);
            }
            preview.findChild<QPushButton *>("panelConfirm")->click();
            const int acceptedTick = preview.property("sceneTick").toInt();
            const auto accepted = preview.renderedFrame();
            if (preset == 3)
                QVERIFY(preview.grab().save("scene-briefing-panel.png"));
            if (preset == 2)
                QVERIFY(preview.grab().save("scene-repeat-confirmation.png"));
            seek(40);
            QCOMPARE(preview.renderedFrame(), before);
            QVERIFY(!preview.findChild<QPushButton *>("panelConfirm")->isVisible());
            seek(124);
            QCOMPARE(preview.renderedFrame(), before);
            seek(12);
            QCOMPARE(preview.renderedFrame(), opening);
            seek(acceptedTick);
            QCOMPARE(preview.renderedFrame(), accepted);
            seek(160);
            QCOMPARE(preview.renderedFrame(), before);
            preview.findChild<QPushButton *>("sceneRestart")->click();
            seek(acceptedTick);
            QCOMPARE(preview.renderedFrame(), accepted);
            // A new input after rewinding replaces the recorded future.
            seek(20);
            preview.findChild<QPushButton *>("panelCancel")->click();
            seek(40);
            QCOMPARE(preview.renderedFrame(), before);
            if (preset == 34) {
                reward->setCurrentIndex(reward->findData(0x201));
                QCOMPARE(preview.property("sceneTick").toInt(), 0);
                seek(20);
                const auto changed = preview.renderedFrame();
                QVERIFY(changed != ready);
                QVERIFY(preview.grab().save("scene-reward-panel.png"));
                seek(124);
                QCOMPARE(preview.renderedFrame(), changed); // Changing reward clears prior input.
                seek(20);
                QCOMPARE(preview.renderedFrame(), changed);
                preview.findChild<QPushButton *>("panelConfirm")->click();
                seek(40);
                QCOMPARE(preview.renderedFrame(), before);
                preview.findChild<QPushButton *>("sceneRestart")->click();
                seek(40);
                QCOMPARE(preview.renderedFrame(), before);
            }
            QCOMPARE(scenes.json(), savedScenes);
        }
    }
    void irisPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto id = scenes.createScene("Iris close and open", source, false);
        QVector<quint32> words{0x80000007U, 47, 0x80000006U, 0, 0x3a, 2, 0, 0, 0, 70, 70, 130, 8};
        for (int i = 0; i < 4; ++i)
            words << 0 << 0x80000000U;
        words << 0x4d << 320 << 240 << 0 << 0 << 0 << 255 << 12 << 0 << 0;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x4d << 320 << 240 << 0 << 0 << 0 << 255 << 12 << 1 << 0;
        for (int i = 0; i < 30; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << 0xff000004U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1200, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(4);
        const auto before = preview.renderedFrame();
        seek(11);
        const auto middle = preview.renderedFrame();
        QVERIFY(middle != before);
        QVERIFY(status->text().contains("Iris: native layers"));
        QVERIFY(preview.grab().save("scene-iris.png"));
        seek(25);
        const auto closed = preview.renderedFrame();
        QVERIFY(closed != middle);
        seek(124);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(151);
        const auto opening = preview.renderedFrame();
        QVERIFY(opening != closed);
        seek(160);
        QCOMPARE(preview.renderedFrame(), before);
        seek(11);
        QCOMPARE(preview.renderedFrame(), middle);
        seek(151);
        QCOMPARE(preview.renderedFrame(), opening);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(25);
        QCOMPARE(preview.renderedFrame(), closed);
    }
    void screenCropPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Screen crop playback", source.key, false);
        QVector<quint32> words{0x80000006U, 0};
        for (int i = 0; i < 3; ++i)
            words << 0 << 0x80000000U;
        words << 0x3b << 0xffffffffU << 0xffffffffU << 80 << 70 << 4 << 1;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x3b << 80 << 70 << 24 << 24 << 4 << 0;
        for (int i = 0; i < 20; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(3);
        const auto before = preview.renderedFrame();
        QCOMPARE(before.pixelColor(160, 10), QColor(Qt::black));
        seek(5);
        const auto halfway = preview.renderedFrame();
        QVERIFY(halfway != before);
        seek(8);
        const auto closed = preview.renderedFrame();
        QVERIFY(closed != halfway);
        QCOMPARE(closed.pixelColor(160, 79), QColor(Qt::black));
        QCOMPARE(closed.pixelColor(160, 170), QColor(Qt::black));
        QVERIFY(closed.pixelColor(160, 110) != QColor(Qt::black));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), closed);
        seek(124);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(5);
        QCOMPARE(preview.renderedFrame(), halfway);
        seek(151);
        QCOMPARE(preview.renderedFrame(), before);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(8);
        QCOMPARE(preview.renderedFrame(), closed);
    }
    void displayControlPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Display blackout playback", source.key, false);
        QVector<quint32> words{0x80000006U, 0};
        for (int i = 0; i < 3; ++i)
            words << 0 << 0x80000000U;
        words << 0x84;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x85;
        for (int i = 0; i < 10; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(3);
        const auto visible = preview.renderedFrame();
        QVERIFY(visible.pixelColor(160, 120) != QColor(Qt::black));
        seek(4);
        const auto hidden = preview.renderedFrame();
        QCOMPARE(hidden.pixelColor(160, 120), QColor(Qt::black));
        QVERIFY(status->text().contains("blacked out"));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), hidden);
        preview.grab().save("display-blackout-preview.png");
        seek(140);
        QCOMPARE(preview.renderedFrame(), hidden);
        seek(144);
        QCOMPARE(preview.renderedFrame(), visible);
        seek(100);
        QCOMPARE(preview.renderedFrame(), hidden);
        seek(145);
        QCOMPARE(preview.renderedFrame(), visible);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(4);
        QCOMPARE(preview.renderedFrame(), hidden);
    }
    void titleRevealPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const SceneId id(scenes.original().selectors()[0]);
        QVector<quint32> words{0x3b, 0, 240, 80, 160, 4, 1};
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0xb8 << 0x3b << 160 << 320 << 0 << 480 << 4 << 0;
        for (int i = 0; i < 10; ++i)
            words << 0 << 0x80000000U;
        words << 0xb7 << 0 << 0x80000000U << 0x80000001U << 0xff000006U;
        QByteArray source(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(source, i * 4, words[i]);
        scenes.replace(id, source);
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(1);
        const auto first = preview.renderedFrame();
        QCOMPARE(first.pixelColor(160, 19), QColor(Qt::black));
        QCOMPARE(first.pixelColor(160, 20).alpha(), 0);
        seek(4);
        const auto closed = preview.renderedFrame();
        QCOMPARE(closed.pixelColor(160, 79), QColor(Qt::black));
        QCOMPARE(closed.pixelColor(160, 80).alpha(), 0);
        QCOMPARE(closed.pixelColor(160, 159).alpha(), 0);
        QCOMPARE(closed.pixelColor(160, 160), QColor(Qt::black));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), closed);
        seek(125);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(141);
        const auto high = preview.renderedFrame();
        QCOMPARE(high.pixelColor(160, 59), QColor(Qt::black));
        QCOMPARE(high.pixelColor(160, 60).alpha(), 0);
        QCOMPARE(high.pixelColor(319, 200), QColor(Qt::black));
        seek(145);
        const auto released = preview.renderedFrame();
        QCOMPARE(released.pixelColor(160, 0).alpha(), 0);
        QCOMPARE(released.pixelColor(160, 239).alpha(), 0);
        seek(1);
        QCOMPARE(preview.renderedFrame(), first);
        seek(141);
        QCOMPARE(preview.renderedFrame(), high);
        preview.grab().save("title-reveal-preview.png");
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(145);
        QCOMPARE(preview.renderedFrame(), released);
    }
    void launchImageSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto missing = scenes.createScene("Event-selected image", source, false);
        const auto explicitImage = scenes.createScene("Scene-selected image", source, false);
        QVector<quint32> words{0x3a, 1, 0, 0, 0, 70, 70, 130, 8,
                               0, 0x80000000U, 0, 0x80000000U, 0x80000001U, 0xff000004U};
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(missing, bytes);
        QByteArray selected(8, 0);
        put32(selected, 0, 0x80000007U);
        put32(selected, 4, 47);
        selected += bytes;
        scenes.replace(explicitImage, selected);
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, missing, 0);
        auto choice = preview.findChild<QComboBox *>("sceneLaunchImage");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto step = preview.findChild<QPushButton *>("sceneStep");
        QVERIFY(choice->isVisible());
        QVERIFY(!step->isEnabled());
        QVERIFY(status->text().contains("Choose the event's launch image"));
        QVERIFY(choice->findData(47) > 0);
        QCOMPARE(choice->findData(10), -1); // This retail row has no image selection.
        choice->setCurrentIndex(choice->findData(47));
        QVERIFY2(step->isEnabled(), qPrintable(status->text()));
        step->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QVERIFY(!first.isNull());
        int visible = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                visible += first.pixelColor(x, y).alpha() > 16;
        QVERIFY(visible > 1000);
        choice->setCurrentIndex(choice->findData(1));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        step->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
        preview.selectScene(scenes, explicitImage, 0);
        QCOMPARE(choice->currentData().toInt(), -1);
        step->click();
        QCOMPARE(preview.renderedFrame(), first);
        preview.selectScene(scenes, missing, 0);
        QVERIFY(!step->isEnabled());
        QCOMPARE(choice->currentData().toInt(), -1);
        QCOMPARE(scenes.json(), before);
    }
    void manualImageInputReplay() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Manual image input",source,false);
        // Retail row47 is288x276, exceeding both manual movement thresholds.
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0x50,0,1,0,0x6c,56};
        for(int i=0;i<170;++i)words<<0x55<<0<<0x80000000U;
        words<<0x6d<<0<<0x80000000U<<0<<0x80000000U<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        auto controls=preview.findChild<QWidget *>("sceneImageControls");auto view=preview.findChild<QGraphicsView *>("sceneCanvas");
        QVERIFY(!controls->isVisible());position->setValue(4);QTRY_COMPARE(preview.property("sceneTick").toInt(),4);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(controls->isVisible());
        const auto original=preview.renderedFrame();
        QTest::keyPress(view,Qt::Key_Right);QCOMPARE(preview.property("sceneTick").toInt(),5);
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QTest::keyRelease(view,Qt::Key_Right);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto moved=preview.renderedFrame();QVERIFY(moved!=original);
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),moved);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),moved);
        position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        QTest::mouseClick(preview.findChild<QPushButton *>("imagePanLeft"),Qt::LeftButton);
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QVERIFY(preview.renderedFrame()!=moved);
        const auto beforeZoom=preview.renderedFrame();
        QTest::mouseClick(preview.findChild<QPushButton *>("imageZoomOut"),Qt::LeftButton);
        position->setValue(75);QTRY_COMPARE(preview.property("sceneTick").toInt(),75);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(preview.renderedFrame()!=beforeZoom);
        const auto zoomed=preview.renderedFrame();position->setValue(124);QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        position->setValue(75);QTRY_COMPARE(preview.property("sceneTick").toInt(),75);QCOMPARE(preview.renderedFrame(),zoomed);
        QVERIFY(preview.grab().save("manual-image-controls.png"));
        position->setValue(171);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),171,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(!controls->isVisible());
    }
    void imagePresentationPlayback() {
        QFETCH(int,transition);
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Image presentation playback",source,false);
        QVector<quint32> words{0x80000007U,1,0x3a,1,0,0,0,70,70,130,8};
        if(transition<0)words<<0x76<<quint32(-1)<<quint32(-1)<<0<<50<<0<<0<<20;
        else words<<0<<0x80000000U<<0x50<<quint32(transition)<<20<<4;
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);
        scenes.replace(id,bytes);ScenePreview preview(rom);preview.resize(1000,750);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        const auto first=preview.renderedFrame();int visible=0;
        for(int y=0;y<first.height();++y)for(int x=0;x<first.width();++x)if(first.pixelColor(x,y).alpha()>16)++visible;
        QVERIFY(visible>1000);
        position->setValue(12);QTRY_COMPARE(preview.property("sceneTick").toInt(),12);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(preview.renderedFrame()!=first);
        const auto moved=preview.renderedFrame();
        QVERIFY(preview.grab().save(QString("scene-image-transition-%1.png").arg(transition)));
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        position->setValue(12);QTRY_COMPARE(preview.property("sceneTick").toInt(),12);QCOMPARE(preview.renderedFrame(),moved);
        position->setValue(1);QTRY_COMPARE(preview.property("sceneTick").toInt(),1);QCOMPARE(preview.renderedFrame(),first);
        preview.update();QCoreApplication::processEvents();QCOMPARE(preview.renderedFrame(),first);
        QVERIFY(preview.grab().save(QString("scene-image-presentation-%1.png").arg(transition)));
        preview.findChild<QPushButton *>("sceneRestart")->click();preview.findChild<QPushButton *>("sceneStep")->click();QCOMPARE(preview.renderedFrame(),first);
    }
    void scatteredPropsSeek_data() {
        QTest::addColumn<int>("environmentSelector");
        QTest::newRow("snow") << 35;
        QTest::newRow("scrolling-texture") << 31;
        QTest::newRow("dynamic-hall-props") << 34;
    }
    void scatteredPropsSeek() {
        QFETCH(int, environmentSelector);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Scattered props playback", source.key, false);
        QVector<quint32> words{0x80000006U, quint32(environmentSelector)};
        for (int i = 0; i < 140; ++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom); preview.resize(1100, 800); preview.show();
        preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame(); QVERIFY(!first.isNull());
        position->setValue(124); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 124, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        preview.update(); QCoreApplication::processEvents(); QCOMPARE(preview.renderedFrame(), first);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        preview.findChild<QPushButton *>("sceneStep")->click(); QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save(QString("scattered-props-preview-%1.png").arg(environmentSelector)));
    }
    void worldScenePanAndSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("World projection pan", source.key, false);
        QVector<quint32> words{0x46, 0, 30, 1, 2, 40, 20, 0, 100, 0,
                              0, 0x80000000U, 0x2c, 25000, quint32(-10000), 1000, 1000, 2};
        for (int i = 0; i < 6; ++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        auto save = [&] {
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
        };
        save();
        ScenePreview preview(rom);
        preview.resize(1100, 700); preview.show(); preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto first = preview.renderedFrame();
        auto center = [](const QImage &image) {
            QPointF sum; double weight = 0;
            // Measure the sprite inside the native scissor, excluding its black border.
            for (int y = 24; y < 215; ++y) for (int x = 0; x < 319; ++x) {
                const auto a = image.pixelColor(x,y).alpha(); sum += QPointF(x,y)*a; weight += a;
            }
            return weight ? sum / weight : QPointF(-10000,-10000);
        };
        QVERIFY(center(first).x() > 180);
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        const auto delta = center(preview.renderedFrame()) - center(first);
        QVERIFY(qAbs(delta.x()-25) < 1.1); QVERIFY(qAbs(delta.y()-10) < 1.1);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        words.prepend(57); words.prepend(0x80000006U); save();
        preview.selectScene(scenes, id, 2); preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto hall = preview.renderedFrame();
        QVERIFY(hall != first);
        position->setValue(4); QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        QVERIFY(preview.renderedFrame() != hall);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), hall);
        QVERIFY(preview.grab().save("world-scene-pan.png"));
    }
    void actorColorFocusPlayback() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Actor color focus",source.key,false);
        QVector<quint32> words{0x80000006U,2,0x73,0x14,0,30,1,2,quint32(-40000),0,0,0,0,0x1c,0,6,0x14,1,31,1,2,40000,0,0,0,0,0x1c,1,6,0,0x80000000U,0x80,0,0,0x80000000U,0x80,1};
        for(int i=0;i<130;++i) words<<0<<0x80000000U;
        words<<0x80000001U<<(0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]); scenes.replace(id,bytes);
        ScenePreview preview(rom); preview.resize(1100,800); preview.show(); preview.selectScene(scenes,id,0);
        auto next=preview.findChild<QPushButton*>("sceneStep"); auto position=preview.findChild<QSpinBox*>("scenePosition");
        next->click(); const auto sepia=preview.renderedFrame(); next->click(); const auto first=preview.renderedFrame(); QVERIFY(first!=sepia);
        next->click(); const auto second=preview.renderedFrame(); QVERIFY(second!=first);
        position->setValue(124); QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(),1); QCOMPARE(preview.renderedFrame(),sepia);
        position->setValue(2); QTRY_COMPARE(preview.property("sceneTick").toInt(),2); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(3); QTRY_COMPARE(preview.property("sceneTick").toInt(),3); QCOMPARE(preview.renderedFrame(),second);
        QVERIFY(preview.grab().save("actor-color-focus.png"));
    }
    void specialBlackPlayback() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Special black fade",source.key,false);
        QVector<quint32> words{0x80000006U,34,0x14,0,30,1,2,0,0,0,0,0,0xa9,0x80000008U,4,4};
        for(int i=0;i<130;++i) words<<0<<0x80000000U;
        words<<0x80000001U<<(0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]); scenes.replace(id,bytes);
        ScenePreview preview(rom); preview.selectScene(scenes,id,2); preview.findChild<QPushButton*>("sceneStep")->click();
        const auto first=preview.renderedFrame(); auto position=preview.findChild<QSpinBox*>("scenePosition");
        position->setValue(31); QTRY_COMPARE(preview.property("sceneTick").toInt(),31); const auto half=preview.renderedFrame(); QVERIFY(half!=first);
        position->setValue(61); QTRY_COMPARE(preview.property("sceneTick").toInt(),61);
        const auto black=preview.renderedFrame();
        for(int y=0;y<240;++y) for(int x=0;x<320;++x) QCOMPARE(black.pixelColor(x,y),QColor(Qt::black));
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(),1); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(31); QTRY_COMPARE(preview.property("sceneTick").toInt(),31); QCOMPARE(preview.renderedFrame(),half);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
    }
    void capturedScenePlayback_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("world") << 2; QTest::newRow("layered") << 0;
    }
    void capturedScenePlayback() {
        QFETCH(int,mode);
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Captured transitions",source.key,false);
        ScenePreview preview(rom); preview.resize(1100,800); preview.show();
        auto position=preview.findChild<QSpinBox*>("scenePosition"); auto next=preview.findChild<QPushButton*>("sceneStep");
        QVector<quint32> words{0x80000006U, mode ? 34U : 2U, 0,0x80000000U,0x80000008U,2,4};
        for(int i=0;i<5;++i) words << 0 << 0x80000000U;
        words << 0x80000008U << 3 << 4;
        for(int i=0;i<130;++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]);
        scenes.replace(id,bytes); preview.selectScene(scenes,id,mode); next->click();
        const auto normal=preview.renderedFrame(); next->click(); const auto first=preview.renderedFrame();
        QVERIFY(first!=normal);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(),5);
        const auto sepia=preview.renderedFrame(); QVERIFY(sepia!=first); QVERIFY(sepia!=normal);
        position->setValue(7); QTRY_COMPARE(preview.property("sceneTick").toInt(),7);
        const auto restoring=preview.renderedFrame(); QVERIFY(restoring!=sepia);
        position->setValue(10); QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        const auto restored=preview.renderedFrame(); QVERIFY(restored!=restoring);
        if(!mode) QCOMPARE(restored,normal); // World props have their own draw-driven poses.
        position->setValue(124); QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
        position->setValue(2); QTRY_COMPARE(preview.property("sceneTick").toInt(),2); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(),5); QCOMPARE(preview.renderedFrame(),sepia);
        QVERIFY(preview.grab().save(QString("captured-scene-%1.png").arg(mode)));
        position->setValue(10); QTRY_COMPARE(preview.property("sceneTick").toInt(),10); QCOMPARE(preview.renderedFrame(),restored);
    }
    void sceneColorPlayback_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("world-background-and-props") << 0;
        QTest::newRow("layered-background") << 1;
        QTest::newRow("sprite-channel-color") << 2;
        QTest::newRow("actor-material-independent") << 3;
    }
    void sceneColorPlayback() {
        QFETCH(int, kind);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Scene color playback", source.key, false);
        ScenePreview preview(rom); preview.resize(1100, 800); preview.show();
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const QVector<quint32> setup = kind < 2 ? QVector<quint32>{0x80000006U, kind ? 2U : 34U}
            : kind == 2 ? QVector<quint32>{0x46, 0, 30, 1, 2, 6, 0, 0, 100, 0}
            : QVector<quint32>{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0};
        auto select = [&](bool tint) {
            auto words = setup;
            if (tint) words << 0x33 << 255 << 255 << 255 << 0 << 255 << 0 << 4 << 0xffffffffU;
            for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
            words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes); preview.selectScene(scenes, id, kind == 0 ? 2 : 0);
            next->click();
        };
        select(false); position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        const auto untinted = preview.renderedFrame();
        select(true); const auto first = preview.renderedFrame();
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto last = preview.renderedFrame();
        if (kind == 3) QCOMPARE(last, untinted);
        else {
            QVERIFY(last != untinted);
            int visible = 0;
            for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
                const auto c = last.pixelColor(x, y);
                if (!c.alpha()) continue;
                ++visible; QCOMPARE(c.red(), 0); QCOMPARE(c.blue(), 0);
                if (kind == 2) QCOMPARE(c.green(), 0);
            }
            QVERIFY(visible > 30);
        }
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        QCOMPARE(preview.renderedFrame(), last);
        if (kind == 0) QVERIFY(preview.grab().save("scene-color-preview.png"));
    }
    void sceneRoomComposition() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Room composition", source.key, false);
        ScenePreview preview(rom);
        preview.resize(1100, 700);
        preview.show();
        auto run = [&](QVector<quint32> words) {
            // Explicit registered camera matches the Actor world scale0.1.
            // Keep the fixture's Actor crossing the table's occluding edge.
            words = QVector<quint32>{0x35, 0, 0, 0, 27200, 8000, 12700, 45000} + words;
            words << 0 << 0x80000000U << 0 << 0x80000000U << 0x80000001U
                  << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            preview.selectScene(scenes, id, 0);
            preview.findChild<QPushButton *>("sceneStep")->click();
            return preview.renderedFrame();
        };
        const QVector<quint32> room{0x80000006U, 2};
        const QVector<quint32> actor{0x14, 0, 30, 1, 2, 0, quint32(-40000), 0, 0, 0};
        const auto background = run(room);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto actorFrame = run(actor);
        auto expected = background.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, actorFrame); }
        const auto front = run(room + actor + QVector<quint32>{0x1c, 0, 6});
        QCOMPARE(front, expected);
        QVector<QPoint> frontPicks;
        for (int y = 24; y < 215; ++y)
            for (int x = 0; x < 319; ++x)
                if (preview.actorAt({x, y}) == 0)
                    frontPicks << QPoint(x, y);
        QVERIFY(!frontPicks.isEmpty());
        const auto behind = run(room + actor);
        QVERIFY(behind != front);
        QVERIFY(behind != background);
        int occluded = 0, visible = 0;
        for (const auto point : frontPicks) {
            if (preview.actorAt(point) < 0) {
                QCOMPARE(behind.pixelColor(point), background.pixelColor(point));
                ++occluded;
            } else {
                QCOMPARE(preview.actorAt(point), 0);
                ++visible;
            }
        }
        QVERIFY(occluded > 0 && visible > 0);
        QVERIFY(preview.grab().save("scene-room-composition.png"));
        const auto empty = run({0x80000006U, 23});
        QVERIFY(!preview.property("sceneStopped").toBool());
        QVERIFY(empty != background);
        for (int y = 0; y < empty.height(); ++y) for (int x = 0; x < empty.width(); ++x)
            if (x >= 319 || y < 24 || y >= 215)
                QCOMPARE(empty.pixelColor(x, y), QColor(Qt::black));
            else
                QCOMPARE(empty.pixelColor(x, y).alpha(), 0);
    }
    void spritePreviewSelection() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : programs.original().entries())
                if (entry.program.resourceClass() == 5 && programs.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = programs.createScene("Sprite selection", source, true);
            auto program = programs.program(id);
            const int point = SceneActions::positions(program).first().command;
            program = SceneActions::insert(program, point, 0x14);
            for (const auto &[field, text] :
                 QVector<QPair<int, QString>>{{1, "30"}, {2, "1"}, {3, "2"}, {4, "-60.000"}})
                program = SceneActions::edit(program, point, field, text);
            program = SceneActions::insert(program, point + 1, 0x46);
            program = SceneActions::edit(program, point + 1, 4, "48");
            program = SceneActions::insert(program, point + 2, 0xe, 30);
            programs.replace(id, program.source);
            project.setScenePrograms(programs.json());
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 1000);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                    scenes->setCurrentRow(i);
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            auto seek = preview->findChild<QSpinBox *>("scenePosition");
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(!preview->property("sceneStopped").toBool());
            auto objects = preview->findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(objects->count(), 2);
            QCOMPARE(objects->itemText(0), QString("Actor 0"));
            QCOMPARE(objects->itemText(1), QString("Sprite 0"));
            auto findPixel = [&] {
                for (int y = 0; y < 240; ++y)
                    for (int x = 0; x < 320; ++x)
                        if (preview->spriteAt({x, y}) == 0)
                            return QPoint(x, y);
                return QPoint(-1, -1);
            };
            const auto pixel = findPixel();
            QVERIFY(pixel.x() >= 0);
            QCOMPARE(preview->actorAt(pixel), -1);
            const auto frame = preview->renderedFrame();
            const auto before = project.scenePrograms()->json();
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            QCoreApplication::processEvents();
            const auto viewportPoint = view->mapFromScene(QPointF(pixel) + QPointF(.5, .5));
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(objects->currentText(), QString("Sprite 0"));
            QCOMPARE(objects->currentData(Qt::UserRole + 1).toInt(), int(SceneObjectKind::Sprite));
            QVERIFY(preview->findChild<QLabel *>("sceneActorState")->text().contains("X 48"));
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(project.scenePrograms()->json(), before);
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(preview->spriteAt(pixel), -1);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->spriteAt(pixel), 0);
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(objects->currentText(), QString("Sprite 0"));
            QSignalSpy selected(preview, &ScenePreview::findSpriteAction);
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(selected.count(), 1);
            auto actions = editor.findChild<SceneActionEditor *>();
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .contains("Create scene sprite"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("0"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField4")->text(), QString("48"));
            QCOMPARE(project.scenePrograms()->json(), before);
            pages->setCurrentWidget(preview);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-selection.png"));
            // Direct extended placement now navigates to its rotation Actions fields.
            auto extended = *project.scenePrograms();
            auto raw = extended.program(id);
            raw.commands[point + 1].opcode = 0x63;
            raw.commands[point + 1].operands = {0, 30,  1,           0,           48,
                                                0, 180, quint32(-1), quint32(-1), quint32(-1)};
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use extended sprite placement", extended);
            pages->setCurrentWidget(preview);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(!preview->property("sceneStopped").toBool());
            const auto rawPixel = findPixel();
            QVERIFY(rawPixel.x() >= 0);
            QCoreApplication::processEvents();
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                               view->mapFromScene(QPointF(rawPixel) + QPointF(.5, .5)));
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .contains("Create rotated scene sprite"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField6")->text(), QString("180"));
            QCOMPARE(project.scenePrograms()->program(id).source, raw.encode());
            // Path orientation now has its own fields; an unknown launch-selected
            // creation mode still routes to its exact source command.
            raw.commands[point + 1].operands[8] = 0;
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use path orientation", extended);
            preview->findSpriteAction(extended.resource(id).selectors.first(), 0);
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")->currentItem()->text()
                        .contains("Create sprite with path heading"));
            raw.commands[point + 1].operands[9] = 0x08880003;
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use launch creation mode", extended);
            preview->findSpriteAction(extended.resource(id).selectors.first(), 0);
            auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
            QCOMPARE(commands->currentRow(), point + 1);
            QVERIFY(pages->currentWidget()->isAncestorOf(commands));
            QCOMPARE(project.scenePrograms()->program(id).source, raw.encode());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageTransitionCompletionWaitControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Image completion controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          20,
                4,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 3;
            auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0x46);
            base = SceneActions::insert(base, point + 1, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 700);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x54));
            QVERIFY(insert->isEnabled());
            insert->click();
            const auto accepted = project.scenePrograms()->program(id).source;
            QVERIFY(!editor.findChild<QLineEdit *>("sceneActionField0"));
            QVERIFY(
                editor.findChild<QLabel *>("sceneActionStatus")->text().contains("both finished"));
            ScenePreview preview(project.rom());
            preview.resize(850, 700);
            preview.show();
            auto live = preview.findChild<QComboBox *>("sceneLiveActors");
            QVERIFY(live);
            auto sprite = [&] {
                for (int i = 0; i < live->count(); ++i)
                    if (live->itemData(i, Qt::UserRole + 1).toInt() ==
                            int(SceneObjectKind::Sprite) &&
                        live->itemData(i).toInt() == 0)
                        return true;
                return false;
            };
            auto select = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
            };
            select();
            QVERIFY(!sprite());
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(24);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 24);
            QVERIFY2(!preview.property("sceneStopped").toBool(),
                     qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
            QVERIFY(!sprite());
            const auto draining = preview.renderedFrame();
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            QVERIFY(sprite());
            const auto finished = preview.renderedFrame();
            seek->setValue(24);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 24);
            QVERIFY(!sprite());
            QCOMPARE(preview.renderedFrame(), draining);
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            QVERIFY(sprite());
            QCOMPARE(preview.renderedFrame(), finished);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            select();
            QVERIFY(sprite());
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            select();
            QVERIFY(!sprite());
            editor.selectScene(id, point);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-completion-wait.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageConstructionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Image construction controls", source, false);
            const QVector<quint32> words{0x80000007U, 47,          0x7d,       0x80000002U,
                                         0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 1;
            const auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 750);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x3a));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("1"));
            QCOMPARE(field(3)->text(), QString("70"));
            QCOMPARE(field(5)->text(), QString("130"));
            const auto once = project.scenePrograms()->program(id).source;
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).source, once);
            QVERIFY(editor.findChild<QLabel *>("sceneActionStatus")
                        ->text()
                        .contains("already constructs"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto initial = frame();
            undo.clear();
            field(1)->setText("60");
            const auto translated = frame();
            QVERIFY(translated != initial);
            undo.undo();
            QCOMPARE(frame(), initial);
            undo.redo();
            QCOMPARE(frame(), translated);
            editor.selectScene(id, point);
            undo.clear();
            field(3)->setText("-70");
            const auto mirrored = frame();
            QVERIFY(mirrored != translated);
            undo.undo();
            QCOMPARE(frame(), translated);
            undo.redo();
            QCOMPARE(frame(), mirrored);
            editor.selectScene(id, point);
            undo.clear();
            field(5)->setText("0");
            const auto transparent = frame();
            QVERIFY(transparent != mirrored);
            undo.undo();
            QCOMPARE(frame(), mirrored);
            undo.redo();
            QCOMPARE(frame(), transparent);
            editor.selectScene(id, point);
            field(5)->setText("210");
            field(6)->setText("0");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto accepted = project.scenePrograms()->program(id).source;
            field(0)->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-construction-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageTransitionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Image transition controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          1,
                0,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            // Establish a settled image so reverse motion has distinct endpoints.
            const auto ready = SceneActions::insert(DirectorProgram::decode(bytes), 3, 0xe, 3);
            const int point = 9;
            const auto base = SceneActions::insert(ready, point, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 750);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x50));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("0"));
            QCOMPARE(field(1)->text(), QString("20"));
            QCOMPARE(field(2)->text(), QString("4"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                for (int i = 0; i < 12; ++i)
                    preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto forward = frame();
            undo.clear();
            field(0)->setText("1");
            const auto reverse = frame();
            QVERIFY(reverse != forward);
            undo.undo();
            QCOMPARE(frame(), forward);
            undo.redo();
            QCOMPARE(frame(), reverse);
            editor.selectScene(id, point);
            undo.clear();
            field(1)->setText("40");
            const auto slower = frame();
            QVERIFY(slower != reverse);
            undo.undo();
            QCOMPARE(frame(), reverse);
            undo.redo();
            QCOMPARE(frame(), slower);
            editor.selectScene(id, point);
            undo.clear();
            field(2)->setText("0");
            QCOMPARE(project.scenePrograms()->program(id).commands[point].operands,
                     QVector<quint32>({1, 40, 0}));
            const auto plain = frame();
            undo.undo();
            QCOMPARE(frame(), slower);
            undo.redo();
            QCOMPARE(frame(), plain);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(1)->setText("256");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            seek->setValue(12);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 12);
            QCOMPARE(preview.renderedFrame(), plain);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-transition-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageMotionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Image motion controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          1,
                0,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            // The initial image transition establishes the native zoom state.
            const auto ready = SceneActions::insert(DirectorProgram::decode(bytes), 3, 0xe, 3);
            const int point = 9;
            auto base = SceneActions::insert(ready, point, 0xe, 40);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 850);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x76));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("-1"));
            QCOMPARE(field(1)->text(), QString("-1"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                for (int i = 0; i < 12; ++i)
                    preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto still = frame();
            field(2)->setText("50");
            field(5)->setText("20");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto panned = frame();
            QVERIFY(panned != still);
            undo.clear();
            field(4)->setText("1");
            const auto zoomed = frame();
            QVERIFY(zoomed != panned);
            undo.undo();
            QCOMPARE(frame(), panned);
            undo.redo();
            QCOMPARE(frame(), zoomed);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(5)->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(20);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 20);
            QVERIFY(preview.renderedFrame() != zoomed);
            seek->setValue(12);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 12);
            QCOMPARE(preview.renderedFrame(), zoomed);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-motion-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void ribbonActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Ribbon action controls", source, false);
            const QVector<quint32> words{0x80000007U, 47,          0x3a,        1,          0, 0,
                                         0,           70,          70,          130,        8, 0x7d,
                                         0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 2;
            auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0xe, 40);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 850);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            auto discard = editor.findChild<QPushButton *>("sceneActionDiscard");
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto background = frame();
            kind->setCurrentIndex(kind->findData(0x49));
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x49));
            field(7)->setText("1");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto first = frame();
            QVERIFY(first != background);
            undo.clear(); // Establish a baseline before testing one field's undo.
            field(6)->setText("2");
            const auto colored = frame();
            QVERIFY(colored != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), colored);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(4)->setText("2");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            discard->click();
            field(1)->setText("59");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            discard->click();
            field(1)->setText("0");
            field(2)->setText("6"); // Five source points in the original ROM.
            QVERIFY(!editor.property("invalidEdits").toBool());
            kind->setCurrentIndex(kind->findData(0x4a));
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            field(2)->setText("4");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto endpoint = project.scenePrograms()->program(id).source;
            field(2)->setText("5");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, endpoint);
            discard->click();
            editor.selectScene(id, point);
            field(2)->setText("0"); // This shorter path would invalidate the following point4.
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, endpoint);
            discard->click();
            editor.selectScene(id, point + 1);
            field(2)->setText("1");
            editor.selectScene(id, point);
            field(2)->setText("0");
            field(1)->setText("3");
            QVERIFY(!editor.property("invalidEdits").toBool());
            editor.selectScene(id, point + 1);
            kind->setCurrentIndex(kind->findData(0x4b));
            position->setCurrentIndex(position->findData(point + 2));
            insert->click();
            QCOMPARE(field(0)->text(), QString("0"));
            QCOMPARE(field(1)->text(), QString("1"));
            const auto faded = frame();
            QVERIFY(faded != background);
            field(1)->setText("0");
            QCOMPARE(frame(), background);
            undo.undo();
            QCOMPARE(frame(), faded);
            undo.redo();
            QCOMPARE(frame(), background);
            editor.selectScene(id, point + 2);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-ribbon-actions.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void pathSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Path heading actions", source, false);
            const QVector<quint32> words{
                0x80000007U, 47, 0x3a, 1,    0,           0,           0,           70,          70,
                130,         8,  0x49, 0,    3,           0,           90,          quint32(-1), 3,
                0,           0,  1,    0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            auto base = DirectorProgram::decode(bytes);
            const int point = 3;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(SceneActions::CreatePathSprite));
            insert->click();
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            QCOMPARE(field(6)->text(), QString("3"));
            QCOMPARE(field(7)->text(), QString("0"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto first = frame();
            field(6)->setText("4");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto other = frame();
            QVERIFY(other != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), other);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(6)->setText("59");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            field(7)->setText("999");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            kind->setCurrentIndex(kind->findData(SceneActions::OrientSprite));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            field(1)->setText("-1");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(frame(), first);
            const auto active = project.scenePrograms()->program(id).source;
            field(2)->setText("20");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, active);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            field(2)->setText("19");
            QVERIFY(!editor.property("invalidEdits").toBool());
            preview.selectScene(*project.scenePrograms(), id, 0);
            preview.findChild<QPushButton *>("sceneStep")->click();
            QVERIFY(preview.property("sceneStopped").toBool());
            QVERIFY(preview.findChild<QLabel *>("scenePlaybackStatus")
                        ->text()
                        .contains("existing sampled scene path"));
            field(2)->setText("0");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(frame(), first);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-path-heading-actions.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void rotatedSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Rotated sprite actions", source, true);
            auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x63));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x63));
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField6")->text(), QString("180"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto unrotated = frame();
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("270");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto rotated = frame();
            QVERIFY(rotated != unrotated);
            undo.undo();
            QCOMPARE(frame(), unrotated);
            undo.redo();
            QCOMPARE(frame(), rotated);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField2")->setText("800");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("32768");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            kind->setCurrentIndex(kind->findData(SceneActions::RotateSprite));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point + 1].operands[9],
                     quint32(0));
            QVERIFY(!editor.findChild<QLineEdit *>("sceneActionField2"));
            editor.findChild<QLineEdit *>("sceneActionField1")->setText("90");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto reusedFrame = frame();
            QVERIFY(reusedFrame != rotated);
            undo.undo();
            QCOMPARE(frame(), unrotated); // Undo only the reuse angle; default reuse is180.
            undo.redo();
            QCOMPARE(frame(), reusedFrame);
            editor.selectScene(id, point + 1);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-rotation.png"));
            // Preserve symbolic position when changing an unrelated direct rotation.
            auto symbolic = *project.scenePrograms();
            auto p = symbolic.program(id);
            p.commands[point].operands[4] = 0x08880004;
            symbolic.replace(id, p.encode());
            pushSceneProgramChange(&project, &undo, "Use launch position", symbolic);
            editor.selectScene(id, point);
            auto x = editor.findChild<QLineEdit *>("sceneActionField4");
            QVERIFY(x->isReadOnly());
            QCOMPARE(x->text(), QString("Launch value 4"));
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("90");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[point].operands[4],
                     quint32(0x08880004));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void sceneSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
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
            const auto id = scenes.createScene("Sprite actions", source, true);
            auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            QVERIFY(kind && insert);
            kind->setCurrentIndex(kind->findData(0x46));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x46));
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField0")->text(), QString("0"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto first = frame();
            QVERIFY(!first.isNull());
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("48");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(qint32(project.scenePrograms()->program(id).commands[point].operands[4]), 48);
            const auto moved = frame();
            QVERIFY(moved != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), moved);
            const auto accepted = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField2")->setText("800");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField2")->text(), QString("1"));
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("-1");
            const auto oneCoordinate = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField5")->setText("-1");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, oneCoordinate);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            // Consecutive field keystrokes intentionally merge within one second.
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("48");
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            // The enclosing Cutscene editor normally refreshes the selected command on Undo.
            editor.selectScene(id, point);
            kind->setCurrentIndex(kind->findData(0x64));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point + 1].opcode,
                     quint32(0x64));
            const auto inherited = project.scenePrograms()->program(id).source;
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField2")->text(), QString("-1"));
            editor.findChild<QLineEdit *>("sceneActionField1")->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, inherited);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!editor.property("invalidEdits").toBool());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void sceneSpriteComposition() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Sprite composition", source.key, false);
        ScenePreview preview(rom);
        preview.show();
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(next && position && status);
        auto run = [&](QVector<quint32> words) {
            for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
            words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            preview.selectScene(scenes, id, 0);
            next->click();
            return preview.renderedFrame();
        };
        const QVector<quint32> actor{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0};
        QVector<quint32> sprite{0x46, 0, 30, 1, 2, 6, 0, 0, 100, 0};
        const auto actorFrame = run(actor), spriteFrame = run(sprite);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        int overlap = 0;
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x)
            overlap += actorFrame.pixelColor(x, y).alpha() > 0 && spriteFrame.pixelColor(x, y).alpha() > 0;
        QVERIFY(overlap > 10);
        // Drawing a sprite also sets environment alpha to one. An offscreen
        // sprite establishes that same native state for the isolated Actor frame.
        auto offscreenSprite = sprite;
        offscreenSprite[5] = 1000;
        const auto actorAfterSprite = run(actor + offscreenSprite);
        auto expected = spriteFrame.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, actorAfterSprite); }
        const auto together = run(actor + sprite);
        QCOMPARE(together, expected); // Sprite pass is before Actor pass within a channel.
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), together);
        sprite[7] = 1; // A later sprite channel must instead appear over channel-zero Actors.
        const auto laterSprite = run(sprite);
        expected = actorFrame.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, laterSprite); }
        QCOMPARE(run(actor + sprite), expected);
        QVERIFY(preview.grab().save("scene-sprite-composition.png"));
    }
    void scenePlaybackPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Preview test", source.key, false);
        QVector<quint32> words{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0,
                              0x1e, 0, 255, 255, 255, 255, 0, 0, 4, 1};
        for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
        words << 0x80000006U << 0xffffffffU << 0x80000001U
              << (0xff000000U | quint32(source.program.resourceClass()));
        auto encode = [&] {
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            return bytes;
        };
        scenes.replace(id, encode());
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(next && position && status);
        next->click();
        QCOMPARE(preview.property("sceneTick").toInt(), 1);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QVERIFY(!first.isNull());
        int visible = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                visible += first.pixelColor(x, y).alpha() > 0;
        QVERIFY(visible > 10);
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        QVERIFY(preview.renderedFrame() != first);
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        preview.repaint();
        QCoreApplication::processEvents();
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save("scene-playback-preview.png"));
        position->setValue(10);
        QTRY_VERIFY(preview.property("sceneStopped").toBool());
        QVERIFY(status->text().contains("gameplay launch context"));
        QVERIFY(!next->isEnabled());
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(next->isEnabled());
        position->setValue(10000);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        QCoreApplication::processEvents();
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        words[5] = 30000; // Reloaded project edits change the command's Actor X.
        scenes.replace(id, encode());
        preview.selectScene(scenes, id, 0);
        next->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
    }
    void projectedActorPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        ActorPoseEditor editor(Rom::open(path));
        editor.resize(1250, 950);
        editor.show();
        auto banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto projected = editor.findChild<QCheckBox *>("actorProjected"),
             shadow = editor.findChild<QCheckBox *>("actorPlacementShadow");
        auto x = editor.findChild<QDoubleSpinBox *>("actorPlacement0"),
             yaw = editor.findChild<QDoubleSpinBox *>("actorCameraYaw");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        auto layers = editor.findChild<QListWidget *>("actorLayers");
        QVERIFY(banks && poses && projected && shadow && x && yaw && position && layers);
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        position->setValue(1);
        projected->setChecked(true);
        auto image = [&] { return editor.property("placementFrame").value<QImage>(); };
        auto center = [](const QImage &frame) {
            double sum = 0, weight = 0;
            for (int y = 0; y < frame.height(); ++y)
                for (int x = 0; x < frame.width(); ++x) {
                    const auto a = frame.pixelColor(x, y).alpha();
                    sum += x * a;
                    weight += a;
                }
            return weight > 0 ? sum / weight : -1.;
        };
        QVERIFY(!image().isNull());
        QVERIFY(center(image()) >= 0);
        QVERIFY(layers->count() > 1);
        const auto first = image();
        auto material = editor.findChild<QComboBox *>("actorMaterialMode");
        auto red = editor.findChild<QSpinBox *>("actorColor0");
        auto environment = editor.findChild<QSpinBox *>("actorEnvironment2");
        QVERIFY(material && red && environment);
        red->setValue(40);
        QVERIFY(image() != first);
        const auto multiplied = image();
        material->setCurrentIndex(1);
        QVERIFY(image() != multiplied);
        material->setCurrentIndex(2);
        const auto darkMaterial = image();
        environment->setValue(255);
        QVERIFY(image() != darkMaterial);
        material->setCurrentIndex(3);
        const auto coloredMaterial = image();
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), coloredMaterial);
        QVERIFY(editor.grab().save("actor-material-preview.png"));
        material->setCurrentIndex(0);
        red->setValue(255);
        environment->setValue(0);
        QCOMPARE(image(), first);
        const auto initialCenter = center(first);
        x->setValue(30);
        QVERIFY(center(image()) > initialCenter + 10);
        x->setValue(0);
        QCOMPARE(image(), first);
        yaw->setValue(35);
        QVERIFY(image() != first);
        yaw->setValue(0);
        QCOMPARE(image(), first);
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), first);
        const int withShadow = layers->count();
        shadow->setChecked(false);
        QCOMPARE(layers->count() * 2, withShadow);
        QVERIFY(image() != first);
        shadow->setChecked(true);
        QCOMPARE(image(), first);
        layers->setCurrentRow(0);
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto selectedLayer = view->scene()->selectedItems().first();
        layers->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!selectedLayer->isVisible());
        layers->item(0)->setCheckState(Qt::Checked);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("projected-actor-preview.png"));
        auto placementMode = editor.findChild<QComboBox *>("actorPlacementMode");
        QVERIFY(placementMode);
        placementMode->setCurrentIndex(1);
        const auto world = image();
        QVERIFY(!world.isNull());
        QVERIFY(center(world) >= 0);
        QVERIFY(world != first);
        QCOMPARE(layers->count(), withShadow * 3 / 2);
        x->setValue(30);
        QVERIFY(center(image()) > center(world) + 10);
        x->setValue(0);
        QCOMPARE(image(), world);
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), world);
        QVERIFY(editor.grab().save("world-actor-preview.png"));
        placementMode->setCurrentIndex(0);
        QCOMPARE(image(), first);
        projected->setChecked(false);
        QVERIFY(layers->count() > 0);
    }
    void equipmentBodyPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        ActorPoseEditor editor(Rom::open(path));
        editor.resize(1250,950); editor.show();
        auto kind = editor.findChild<QComboBox *>("actorSourceKind");
        auto bank = editor.findChild<QComboBox *>("actorBank");
        auto equipment = editor.findChild<QComboBox *>("actorEquipment");
        auto projected = editor.findChild<QCheckBox *>("actorProjected");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        QVERIFY(kind && bank && equipment && projected && view && position);
        QVERIFY(!equipment->isVisible());
        kind->setCurrentIndex(1); bank->setCurrentIndex(bank->findData(2));
        QVERIFY(equipment->isVisible());
        QCOMPARE(equipment->currentData().toInt(),-1);
        QVERIFY(!editor.findChild<QSpinBox *>("actorVariant")->isEnabled());
        auto flat = [&] {
            QImage image(320,240,QImage::Format_RGBA8888); image.fill(Qt::transparent);
            QPainter painter(&image); view->scene()->render(&painter); return image;
        };
        position->setValue(1);
        const auto defaultImage = flat();
        equipment->setCurrentIndex(equipment->findData(1));
        const auto first = flat(); const auto cursor = editor.property("poseCursor");
        int changedItem = -1;
        for (int item : {2,6,11,20,50,100}) {
            equipment->setCurrentIndex(equipment->findData(item));
            if (flat() != first) { changedItem = item; break; }
        }
        QVERIFY(changedItem >= 0);
        QCOMPARE(position->value(),1); QCOMPARE(editor.property("poseCursor"),cursor);
        equipment->setCurrentIndex(equipment->findData(1)); QCOMPARE(flat(),first);
        equipment->setCurrentIndex(0); QCOMPARE(flat(),defaultImage);
        equipment->setCurrentIndex(equipment->findData(1));
        projected->setChecked(true);
        const auto projectedFirst = editor.property("placementFrame").value<QImage>();
        QVERIFY(!projectedFirst.isNull());
        equipment->setCurrentIndex(equipment->findData(changedItem));
        QVERIFY(editor.property("placementFrame").value<QImage>() != projectedFirst);
        equipment->setCurrentIndex(equipment->findData(1));
        QCOMPARE(editor.property("placementFrame").value<QImage>(),projectedFirst);
        position->setValue(18); position->setValue(1);
        QCOMPARE(editor.property("placementFrame").value<QImage>(),projectedFirst);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("body-equipment-preview.png"));
        kind->setCurrentIndex(0); QVERIFY(!equipment->isVisible());
        QVERIFY(editor.findChild<QSpinBox *>("actorVariant")->isEnabled());
    }
    void actorPosePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        ActorPoseEditor editor(rom);
        editor.resize(1250, 900);
        editor.show();
        auto banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto position = editor.findChild<QSpinBox *>("actorPosition"),
             variant = editor.findChild<QSpinBox *>("actorVariant");
        auto next = editor.findChild<QPushButton *>("actorStep"),
             reset = editor.findChild<QPushButton *>("actorRestart");
        auto layers = editor.findChild<QListWidget *>("actorLayers");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QVERIFY(banks && poses && position && variant && next && reset && layers && view);
        QCOMPARE(banks->count(), 68);
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        ActorPosePlayer reference(rom, ActorArt::load(rom, 30), 12);
        for (int i = 1; i <= 24; ++i) {
            QCOMPARE(reference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(position->value(), i);
            QCOMPARE(editor.property("frameToken").toInt(), reference.actor().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), reference.actor().cursor);
            QCOMPARE(editor.property("poseDelay").toInt(), reference.actor().delay);
        }
        QVERIFY(layers->count() > 0);
        const auto finalFrame = editor.property("frameToken");
        reset->click();
        QCOMPARE(position->value(), 0);
        position->setValue(24);
        QCOMPARE(editor.property("frameToken"), finalFrame);
        position->setValue(1);
        QCOMPARE(position->value(), 1);
        variant->setValue(255);
        QCOMPARE(position->value(), 0);
        next->click();
        QVERIFY(layers->item(0)->toolTip().contains("fallback"));
        layers->setCurrentRow(0);
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto selected = view->scene()->selectedItems().first();
        layers->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!selected->isVisible());
        layers->item(0)->setCheckState(Qt::Checked);
        QVERIFY(selected->isVisible());
        variant->setValue(0);
        position->setValue(3);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-actor-preview.png"));
        auto kind = editor.findChild<QComboBox *>("actorSourceKind"),
             owner = editor.findChild<QComboBox *>("actorOwner"),
             flags = editor.findChild<QComboBox *>("actorBodyFlags");
        QVERIFY(kind && owner && flags);
        kind->setCurrentIndex(1);
        banks->setCurrentIndex(banks->findData(83));
        flags->setCurrentIndex(0);
        poses->setCurrentIndex(poses->findData(0));
        const auto body = ActorArt::loadBody(rom, 83, 83, 0, 0);
        QCOMPARE(variant->value(), body.bodyVariant());
        ActorPosePlayer bodyReference(rom, body, 0);
        bodyReference.setLinkedObjectKnownAbsent(true);
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(bodyReference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(editor.property("frameToken").toInt(), bodyReference.actor().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), bodyReference.actor().cursor);
        }
        QVERIFY(layers->count() > 0);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-body-preview.png"));
        layers->setCurrentRow(0);
        banks->setCurrentIndex(banks->findData(0));
        QCOMPARE(layers->count(), 0);
        QCOMPARE(view->scene()->items().size(), 0);
        QVERIFY(editor.findChild<QLabel *>("actorStatus")->text().contains("absent descriptor"));
        banks->setCurrentIndex(banks->findData(83));
        owner->setCurrentIndex(owner->findData(88));
        flags->setCurrentIndex(1);
        const auto alternate = ActorArt::loadBody(rom, 83, 88, 0, 1);
        QCOMPARE(variant->value(), alternate.bodyVariant());
        QCOMPARE(poses->count(), alternate.poses().size());
        kind->setCurrentIndex(0);
        QCOMPARE(banks->count(), 68);
    }
    void spritePosePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        ActorPoseEditor editor(rom);
        editor.resize(1250, 900);
        editor.show();
        auto kind = editor.findChild<QComboBox *>("actorSourceKind"),
             banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        auto next = editor.findChild<QPushButton *>("actorStep");
        auto sepia = editor.findChild<QCheckBox *>("actorSepia");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QVERIFY(kind && banks && poses && position && next && sepia && view);
        // This retail program contains opcode 12. It moves a 3D actor but is
        // deliberately a no-op in the 2D sprite sequencer.
        kind->setCurrentIndex(2);
        banks->setCurrentIndex(banks->findData(3));
        poses->setCurrentIndex(poses->findData(59));
        const auto art = ActorArt::load(rom, 3);
        SpritePosePlayer reference(rom, art, 59);
        for (int i = 1; i <= 16; ++i) {
            QCOMPARE(reference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(position->value(), i);
            QCOMPARE(editor.property("frameToken").toInt(), reference.sprite().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), reference.sprite().cursor);
            QCOMPARE(editor.property("poseX").toDouble(), reference.sprite().x);
            QCOMPARE(editor.property("poseY").toDouble(), reference.sprite().y);
        }
        const auto cursor = editor.property("poseCursor");
        position->setValue(0);
        position->setValue(16);
        QCOMPARE(editor.property("poseCursor"), cursor);
        sepia->setChecked(true);
        const auto frame = art.frame(rom, reference.sprite().frame, 0, true);
        QCOMPARE(view->scene()->items().size(), frame.size());
        for (auto raw : view->scene()->items()) {
            auto item = qgraphicsitem_cast<QGraphicsPixmapItem *>(raw);
            QVERIFY(item);
            const int layer = int(item->zValue());
            const auto &expected = frame[layer];
            const auto &r = expected.pixels.image;
            const QImage expectedImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                       r.height, r.width * 4, QImage::Format_RGBA8888);
            // QPixmap can premultiply alpha; compare the same Qt display format.
            QCOMPARE(item->pixmap().toImage(), QPixmap::fromImage(expectedImage).toImage());
            QCOMPARE(item->opacity(), reference.sprite().color[layer] / 255.);
        }
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        position->setValue(3);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-scene-sprite-preview.png"));
        kind->setCurrentIndex(0);
        QCOMPARE(position->value(), 0);
        sepia->setChecked(false);
        next->click();
        QCOMPARE(position->value(), 1);
    }
    void sceneEventBindingControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadSceneEvents();
        auto scenes = *project.scenePrograms();
        const auto added = scenes.createScene("A new journey", scenes.scenes().first(), false);
        scenes.renameScene(added, "Journey renamed");
        project.setScenePrograms(scenes.json());
        QUndoStack undo;
        int changed = 0;
        SceneEventEditor editor(&project, &undo, [&] { ++changed; });
        editor.resize(1100, 750);
        editor.show();
        auto table = editor.findChild<QTableWidget *>("sceneEventSites");
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        auto assign = editor.findChild<QPushButton *>("sceneEventAssign");
        auto restore = editor.findChild<QPushButton *>("sceneEventRestore");
        auto search = editor.findChild<QLineEdit *>("sceneEventSearch");
        QVERIFY(table && target && assign && restore && search);
        QCOMPARE(table->rowCount(), 1998);
        const QString site = "event-director:1:b0032";
        int row = -1;
        for (int i = 0; i < table->rowCount(); ++i)
            if (table->item(i, 0)->text() == site)
                row = i;
        QVERIFY(row >= 0);
        table->setCurrentCell(row, 0);
        QVERIFY(editor.findChild<QLabel *>("sceneEventStatus")->text().contains("Shared"));
        const auto selector = scenes.resource(added).selectors.first();
        const auto index = target->findData(selector);
        QVERIFY(index >= 0);
        QVERIFY(target->itemText(index).contains("Journey renamed"));
        target->setCurrentIndex(index);
        assign->click();
        QCOMPARE(changed, 1);
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QVERIFY(table->item(row, 2)->text().contains("Journey renamed"));
        QVERIFY(restore->isEnabled());
        undo.undo();
        QCOMPARE(project.sceneEvents()->target(site), 1);
        undo.redo();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        auto removed = *project.scenePrograms();
        removed.removeScene(added);
        QVERIFY_THROWS_EXCEPTION(Error, project.setScenePrograms(removed.json()));
        const auto historyCount = undo.count();
        QVERIFY_THROWS_EXCEPTION(
            Error, pushSceneProgramChange(&project, &undo, "Remove bound scene", removed));
        QCOMPARE(undo.count(), historyCount);
        target->setEditText("not a scene");
        assign->click();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QCOMPARE(changed, 1);
        search->setText(site);
        QVERIFY(!table->isRowHidden(row));
        int visible = 0;
        for (int i = 0; i < table->rowCount(); ++i)
            visible += !table->isRowHidden(i);
        QCOMPARE(visible, 1);
        restore->click();
        QCOMPARE(project.sceneEvents()->target(site), 1);
        QVERIFY(!restore->isEnabled());
        undo.undo();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QVERIFY(!table->isRowHidden(row));
        QTest::qWait(30);
        QVERIFY(editor.grab().save("native-scene-events.png"));
    }
    void sceneDraftSurvivesEventUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == 0x019aa5b6) {
                list->setCurrentRow(i);
                break;
            }
        editor.findChild<QTableWidget *>("cutsceneCommands")->setCurrentCell(0, 0);
        auto form = editor.findChild<SceneCommandEditor *>();
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("unfinished");
        QVERIFY(form->property("invalidEdits").toBool());
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        target->setCurrentIndex(target->currentIndex() == 0 ? 1 : 0);
        editor.findChild<QPushButton *>("sceneEventAssign")->click();
        QCOMPARE(field->text(), QString("unfinished"));
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(undo.canUndo());
        undo.undo();
        QCOMPARE(field->text(), QString("unfinished"));
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(!editor.findChild<QPushButton *>("sceneWipe")->isEnabled());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        QVERIFY(!form->property("invalidEdits").toBool());
    }
    void wipeSceneControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        auto wipe = editor.findChild<QPushButton *>("sceneWipe");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        QVERIFY(wipe);
        const quint32 key = 0x019aa5b6;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == key) {
                list->setCurrentRow(i);
                break;
            }
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toUInt(), key);
        const auto before = project.scenePrograms()->program(key);
        const auto expected = wipeSceneProgram(before).source;
        QVERIFY(wipe->isEnabled());
        wipe->click();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        QVERIFY(!wipe->isEnabled());
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, before.source);
        QVERIFY(wipe->isEnabled());
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        commands->setCurrentCell(0, 0);
        auto field =
            editor.findChild<SceneCommandEditor *>()->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("invalid");
        QVERIFY(!wipe->isEnabled());
        field->setText(QString::number(before.commands[0].operands[0]));
        QVERIFY(wipe->isEnabled());
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
    }
    void exportedSceneRemovalControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto programs = ScenePrograms::load(rom);
        const auto authored =
            programs.createScene("Remove after reopening", programs.scenes().first(), false);
        const auto image = SceneProgramResources::rebuild(rom, {}, programs.authoredPlan());
        auto project = Project::fromRom(Rom::fromBytes(image));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        auto remove = editor.findChild<QPushButton *>("sceneRemove");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1499, 10000);
        int selected = -1;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole + 1).toString() == authored.authoredId)
                selected = i;
        QVERIFY(selected >= 0);
        list->setCurrentRow(selected);
        QVERIFY(remove->isEnabled());
        remove->click();
        QCOMPARE(list->count(), 1498);
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        QCOMPARE(project.scenePrograms()->changedPrograms(), 1);
        undo.undo();
        QCOMPARE(list->count(), 1499);
        QCOMPARE(project.scenePrograms()->program(authored).source,
                 programs.program(authored).source);
        QVERIFY(!project.scenePrograms()->modified());
        undo.redo();
        QCOMPARE(list->count(), 1498);
        QVERIFY(project.scenePrograms()->modified());
    }
    void authoredSceneCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1498, 10000);
        auto create = editor.findChild<QPushButton *>("sceneCreate");
        auto duplicate = editor.findChild<QPushButton *>("sceneDuplicate");
        auto rename = editor.findChild<QPushButton *>("sceneRename");
        auto remove = editor.findChild<QPushButton *>("sceneRemove");
        QVERIFY(create && duplicate && rename && remove);
        QVERIFY(create->isEnabled());
        const auto key = scenes->currentItem()->data(Qt::UserRole).toUInt();
        const auto baseline = project.scenePrograms()->json();
        const auto selectors = project.scenePrograms()->availableSelectors();
        QTimer::singleShot(0, [] {
            if (auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue("My replacement scene"); dialog->accept();
            }
        });
        editor.findChild<QPushButton *>("sceneUseTemplate")->click();
        QCOMPARE(scenes->count(), 1498);
        QVERIFY(scenes->currentItem()->text().contains("My replacement scene"));
        QCOMPARE(project.scenePrograms()->displayName(key), "My replacement scene");
        QCOMPARE(project.scenePrograms()->availableSelectors(), selectors);
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        undo.undo(); QCOMPARE(project.scenePrograms()->json(), baseline);
        QVERIFY(!scenes->currentItem()->text().contains("My replacement scene"));
        undo.redo(); QVERIFY(scenes->currentItem()->text().contains("My replacement scene"));
        undo.undo();
        bool sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                sawDialog = name && choices && choices->count() > 0;
                if (choices)
                    for (int i = 0; i < choices->count(); ++i)
                        if (choices->itemText(i).startsWith("Room ·")) {
                            choices->setCurrentIndex(i);
                            break;
                        }
                if (name)
                    name->setText("A new adventure");
                dialog->accept();
            }
        });
        create->click();
        QVERIFY(sawDialog);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        QCOMPARE(scenes->count(), 1499);
        const auto first = project.scenePrograms()->authoredScenes().first();
        QCOMPARE(scenes->currentItem()->data(Qt::UserRole + 1).toString(), first.id);
        QVERIFY(scenes->currentItem()->text().contains("A new adventure"));
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        commands->setCurrentCell(2, 0);
        auto form = editor.findChild<SceneCommandEditor *>();
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("2222");
        QCOMPARE(
            project.scenePrograms()->program(SceneId::authored(first.id)).commands[2].operands[0],
            quint32(2222));
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(SceneId::authored(first.id)).source,
                 first.current.source);
        undo.redo();
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue("Adventure renamed");
                dialog->accept();
            }
        });
        rename->click();
        QCOMPARE(project.scenePrograms()->authoredScenes()[first.id].name,
                 QString("Adventure renamed"));
        undo.undo();
        QCOMPARE(project.scenePrograms()->authoredScenes()[first.id].name, first.name);
        undo.redo();
        sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                sawDialog = name != nullptr;
                if (name)
                    name->setText("Adventure copy");
                dialog->accept();
            }
        });
        duplicate->click();
        QVERIFY(sawDialog);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 2);
        const auto secondId = scenes->currentItem()->data(Qt::UserRole + 1).toString();
        QVERIFY(secondId != first.id);
        QCOMPARE(project.scenePrograms()->program(SceneId::authored(secondId)).source,
                 project.scenePrograms()->program(SceneId::authored(first.id)).source);
        remove->click();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        QCOMPARE(scenes->count(), 1499);
        undo.undo();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 2);
        undo.redo();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole + 1).toString() == first.id) {
                scenes->setCurrentRow(i);
                break;
            }
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentIndex(1);
        QVERIFY(editor.grab().save("native-new-scene.png"));
    }
    void blankEnvironmentCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        const SceneId source(0x1fa8bf4 - 0x594284);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == source.nativeKey)
                list->setCurrentRow(i);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toUInt(), source.nativeKey);
        const auto original = project.scenePrograms()->program(source).source;
        bool selectedEnvironment = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                selectedEnvironment = choices &&
                                      choices->currentData().toUInt() == source.nativeKey &&
                                      choices->currentText().startsWith("Environment ·");
                dialog->findChild<QLineEdit *>("newSceneName")->setText("A new world scene");
                dialog->accept();
            }
        });
        editor.findChild<QPushButton *>("sceneCreate")->click();
        QVERIFY(selectedEnvironment);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        const auto authored = project.scenePrograms()->authoredScenes().first();
        const auto id = SceneId::authored(authored.id);
        QCOMPARE(authored.current.source, original.first(8) + original.last(20));
        QCOMPARE(project.scenePrograms()->program(source).source, original);
        QCOMPARE(list->currentItem()->data(Qt::UserRole + 1).toString(), authored.id);
        QCOMPARE(undo.count(), 1);
        undo.undo();
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(id).source, authored.current.source);
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.scenePrograms()->program(id).source, authored.current.source);
    }
    void sceneCommandEditingAndPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        auto form = editor.findChild<SceneCommandEditor *>();
        QVERIFY(scenes && commands && form);
        QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 0, 10000);
        quint32 key = 0;
        int camera = -1, group = -1;
        for (const auto &scene : project.scenePrograms()->original().entries()) {
            if (scene.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < scene.program.commands.size(); ++i) {
                if (scene.program.commands[i].opcode == 0x35)
                    camera = i;
                if (scene.program.commands[i].opcode == 0x80000006)
                    group = i;
            }
            if (camera >= 0 && group >= 0) {
                key = scene.key;
                break;
            }
            camera = group = -1;
        }
        QVERIFY(key != 0);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole).toUInt() == key) {
                scenes->setCurrentRow(i);
                break;
            }
        const auto original = project.scenePrograms()->program(key);
        commands->setCurrentCell(camera, 0);
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        QCOMPARE(field->accessibleName(), QString("camera_target_x_milliunits"));
        field->setText("-1200");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[0],
                 quint32(qint32(-1200)));
        QVERIFY(commands->item(camera, 2)->text().contains("FFFFFB50"));
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, original.source);
        undo.redo();
        field = form->findChild<QLineEdit *>("sceneOperand0");
        QCOMPARE(field->text(), QString("-1200"));
        field->setText("0x1234");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[0],
                 quint32(0x1234));
        field->setText("0x");
        auto second = form->findChild<QLineEdit *>("sceneOperand1");
        second->setText("4321");
        QVERIFY(form->property("invalidEdits").toBool());
        field->setText("0x1234");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[1], quint32(4321));
        field->setText("0x");
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(!commands->isEnabled() && !scenes->isEnabled());
        auto catalogTabs = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
        catalogTabs->setCurrentIndex(1);
        catalogTabs->setCurrentIndex(0);
        QVERIFY(!editor.findChild<QPushButton *>("sceneCreate")->isEnabled());
        QVERIFY(!editor.findChild<QPushButton *>("sceneDuplicate")->isEnabled());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        QVERIFY(!form->property("invalidEdits").toBool());
        QVERIFY(commands->isEnabled() && scenes->isEnabled());
        const auto before = project.scenePrograms()->program(key).source;
        auto kind = form->findChild<QComboBox *>("sceneCommandKind");
        kind->setCurrentIndex(kind->findData(quint32(0x35)));
        form->findChild<QPushButton *>("sceneCommandInsert")->click();
        QCOMPARE(commands->rowCount(), original.commands.size() + 1);
        form->findChild<QPushButton *>("sceneCommandRemove")->click();
        QCOMPARE(project.scenePrograms()->program(key).source, before);
        commands->setCurrentCell(group, 0);
        field = form->findChild<QLineEdit *>("sceneOperand0");
        field->setText("22");
        auto editedScene = project.scenePrograms()->resource(key);
        editedScene.program = project.scenePrograms()->program(key);
        const auto expected = SceneImages::backgrounds(project.rom(), editedScene);
        auto requests = editor.findChild<QComboBox *>("cutsceneRequests");
        QVERIFY(requests);
        QCOMPARE(requests->count(), expected.requests.size());
        QVERIFY(requests->count() > 0);
        QVERIFY(requests->itemText(0).contains("selector 22"));
        QVERIFY(!expected.requests[0].images.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  expected.requests[0].images[0], 10000);
        form->findChild<QPushButton *>("sceneCommandRestore")->click();
        QVERIFY(!project.scenePrograms()->modified());
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).commands[group].operands[0], quint32(22));
        // Navigation cannot discard an invalid operand.
        commands->setCurrentCell(camera, 0);
        field = form->findChild<QLineEdit *>("sceneOperand0");
        field->setText("4294967296");
        QVERIFY(form->property("invalidEdits").toBool());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentIndex(1);
        QVERIFY(editor.grab().save("native-scene-command-editing.png"));
    }
    void dialogueEditingUndoAndNewEntry() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        int notifications = 0;
        DialogueInspector editor(&project, &undo, [&] { ++notifications; });
        editor.resize(1200, 850);
        editor.show();
        auto source = editor.findChild<QPlainTextEdit *>("dialogueSource");
        auto entries = editor.findChild<QListWidget *>("dialogueEntries");
        auto archives = editor.findChild<QComboBox *>("dialogueArchives");
        auto add = editor.findChild<QPushButton *>("dialogueAdd");
        auto restore = editor.findChild<QPushButton *>("dialogueRestore");
        auto discard = editor.findChild<QPushButton *>("dialogueDiscard");
        auto preview = editor.findChild<DialoguePreview *>();
        QVERIFY(source && entries && archives && add && restore && discard && preview);
        QVERIFY(!source->isReadOnly());
        const auto original = project.dialogues()->entry(Dialogues::Table::Primary, 0, 0);
        const QByteArray text = "@-@x@l3@r@S0@A@n@n@=Edited dialogue@s@c";
        source->setPlainText(QString::fromLatin1(text));
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), text);
        QVERIFY(notifications > 0);
        undo.undo();
        QCOMPARE(source->toPlainText().toLatin1(), original);
        undo.redo();
        QCOMPARE(source->toPlainText().toLatin1(), text);
        source->setPlainText(QString::fromLatin1(text) + QString::fromUtf8("\xe2\x98\x83"));
        QVERIFY(editor.property("invalidEdits").toBool());
        QVERIFY(!entries->isEnabled() && !archives->isEnabled() && !add->isEnabled());
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), text);
        editor.selectEntry(1, 0);
        QCOMPARE(editor.property("archiveSelector").toInt(), 0);
        discard->click();
        QVERIFY(!editor.property("invalidEdits").toBool());
        QCOMPARE(source->toPlainText().toLatin1(), text);
        source->setPlainText("Bytes " + QString(QChar(92)) + "x81" + QString(2, QChar(92)));
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0),
                 QByteArray("Bytes ") + char(-127) + char(92));
        editor.selectEntry(0, 1);
        editor.selectEntry(0, 0);
        QCOMPARE(source->toPlainText(),
                 "Bytes " + QString(QChar(92)) + "x81" + QString(2, QChar(92)));
        restore->click();
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), original);
        const auto originalCount = entries->count();
        add->click();
        QCOMPARE(entries->count(), originalCount + 1);
        QCOMPARE(entries->currentRow(), originalCount);
        QVERIFY(!restore->isEnabled());
        source->setPlainText(QString::fromLatin1(text));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("updateCount").toULongLong(), qulonglong(1),
                                  1000);
        auto step = preview->findChild<QPushButton *>("dialogueStep");
        auto status = preview->findChild<QLabel *>("dialoguePlaybackStatus");
        for (int i = 0; i < 200 && preview->property("dialogueState").toInt() != 4; ++i) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QCOMPARE(preview->property("dialogueState").toInt(), 4);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        project.saveFolder(folder.path());
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(folder.path()));
        QCOMPARE(reopened.dialogues()->json(), project.dialogues()->json());
        undo.undo(); // Added text.
        undo.undo(); // Added entry.
        QCOMPARE(entries->count(), originalCount);
        undo.redo();
        undo.redo();
        editor.selectEntry(0, originalCount);
        QCOMPARE(source->toPlainText().toLatin1(), text);
        editor.findChild<QTabWidget *>("dialogueDetailPages")->setCurrentWidget(preview);
        QVERIFY(editor.grab().save("native-dialogue-editing.png"));
    }
    void dialogueInvalidInputBlocksSave() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<DialogueInspector *>(), 10000);
        auto editor = window.findChild<DialogueInspector *>();
        auto source = editor->findChild<QPlainTextEdit *>("dialogueSource");
        QVERIFY(source && !source->isReadOnly());
        source->setPlainText(QString(QChar(92)) + "x00");
        QVERIFY(editor->property("invalidEdits").toBool());
        QVERIFY(window.isWindowModified());
        int checked = 0;
        for (auto action : window.findChildren<QAction *>()) {
            if (!action->text().startsWith("Save project") &&
                !action->text().startsWith("Save sharing") &&
                !action->text().startsWith("Build and"))
                continue;
            QString message;
            QTimer::singleShot(0, &window, [&] {
                if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    message = box->text();
                    box->accept();
                }
            });
            action->trigger();
            QVERIFY2(message.startsWith("Correct invalid text input"), qPrintable(message));
            ++checked;
        }
        QCOMPARE(checked, 3);
        editor->findChild<QPushButton *>("dialogueDiscard")->click();
        QVERIFY(!window.isWindowModified());
    }
    void dialoguePlaybackControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        DialogueInspector inspector(Rom::open(path));
        inspector.resize(1200, 850);
        inspector.show();
        auto pages = inspector.findChild<QTabWidget *>("dialogueDetailPages");
        auto preview = inspector.findChild<DialoguePreview *>();
        QVERIFY(pages && preview);
        pages->setCurrentWidget(preview);
        auto step = preview->findChild<QPushButton *>("dialogueStep");
        auto play = preview->findChild<QPushButton *>("dialoguePlay");
        auto restart = preview->findChild<QPushButton *>("dialogueRestart");
        auto resume = preview->findChild<QPushButton *>("dialogueResume");
        auto canvas = preview->findChild<QLabel *>("dialogueCanvas");
        auto status = preview->findChild<QLabel *>("dialoguePlaybackStatus");
        QVERIFY(step && play && restart && resume && canvas && status);
        QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(1));
        int updates = 0;
        for (; updates < 200 && preview->property("dialogueState").toInt() != 4; ++updates) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QVERIFY(updates < 200);
        QVERIFY(!canvas->pixmap().isNull());
        QVERIFY(inspector.grab().save("native-dialogue-playback.png"));
        resume->click();
        for (updates = 0; updates < 100 && preview->property("dialogueState").toInt() != -1;
             ++updates) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QVERIFY(updates < 100);
        QVERIFY(status->text().startsWith("Closed"));
        restart->click();
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(1));
        play->click();
        QTRY_VERIFY_WITH_TIMEOUT(preview->property("updateCount").toULongLong() > 1, 1000);
        play->click();
        QVERIFY(!play->isChecked());
        const auto paused = preview->property("updateCount").toULongLong();
        QTest::qWait(80);
        QCOMPARE(preview->property("updateCount").toULongLong(), paused);
        inspector.selectEntry(9, 0);
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(0));
        QVERIFY(canvas->pixmap().isNull());
        auto name = preview->findChild<QLineEdit *>("dialogueName0");
        name->setFocus();
        name->setText("Preview teardown");
    }
    void dialogueSourceSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        DialogueInspector inspector(rom);
        inspector.resize(1100, 650);
        inspector.show();
        const auto archives = inspector.findChild<QComboBox *>("dialogueArchives");
        const auto entries = inspector.findChild<QListWidget *>("dialogueEntries");
        const auto source = inspector.findChild<QPlainTextEdit *>("dialogueSource");
        const auto search = inspector.findChild<QLineEdit *>("dialogueSearch");
        const auto status = inspector.findChild<QLabel *>("dialogueStatus");
        QVERIFY(archives && entries && source && search && status);
        QCOMPARE(archives->count(), 349);
        QCOMPARE(entries->count(), 7);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 0);
        QCOMPARE(source->toPlainText(),
                 QString("@-@x@l3@r@S0@A@n@n@={T05}You're leaving tomorrow?@s@c"));
        QVERIFY(source->isReadOnly());
        search->setText("You're leaving tomorrow");
        QVERIFY(!entries->item(0)->isHidden());
        QVERIFY(entries->item(1)->isHidden());
        inspector.selectEntry(1, 0);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 1);
        QCOMPARE(inspector.property("entryIndex").toInt(), 0);
        QVERIFY(search->text().isEmpty());
        QVERIFY(source->toPlainText().startsWith("Archbishop Odiron@n"));
        QVERIFY(inspector.grab().save("native-dialogue-source.png"));
        inspector.selectEntry(9, 0);
        QCOMPARE(inspector.property("archiveSelector").toInt(), -1);
        QVERIFY(source->toPlainText().isEmpty());
        QVERIFY(status->text().contains("not present"));
        inspector.selectEntry(0, 99);
        QCOMPARE(inspector.property("entryIndex").toInt(), -1);
        QVERIFY(source->toPlainText().isEmpty());
        inspector.selectEntry(0, 1);
        QCOMPARE(inspector.property("entryIndex").toInt(), 1);
        QCOMPARE(source->toPlainText().toLatin1(), DialogueArchive::load(rom, 0).entry(1));
        inspector.selectEntry(0, 0x8000);
        QCOMPARE(inspector.property("archiveTable").toInt(), 1);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 0);
        QCOMPARE(inspector.property("entryIndex").toInt(), 0);
        QCOMPARE(source->toPlainText().toLatin1(),
                 DialogueArchive::load(rom, 0, DialogueArchive::Table::Alternate).entry(0));
        inspector.selectEntry(0, 0x4000);
        QVERIFY(source->toPlainText().isEmpty());
        QCOMPARE(inspector.property("archiveTable").toInt(), -1);
        QVERIFY(status->text().contains("unresolved"));
        inspector.selectEntry(0, 1);
        QCOMPARE(inspector.property("archiveTable").toInt(), 0);
        QCOMPARE(source->toPlainText().toLatin1(), DialogueArchive::load(rom, 0).entry(1));
    }
    void cutsceneCatalogSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto before = sha256(rom.bytes());
        CutsceneEditor editor(rom);
        editor.resize(1450, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        auto assets = editor.findChild<QListWidget *>("cutsceneAssets");
        auto tabs = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        auto requests = editor.findChild<QComboBox *>("cutsceneRequests");
        auto frames = editor.findChild<QComboBox *>("cutsceneFrames");
        auto layers = editor.findChild<QListWidget *>("cutsceneLayers");
        auto view = editor.findChild<QGraphicsView *>("cutsceneCanvas");
        QVERIFY(scenes && assets && tabs && commands && requests && frames && layers && view);
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1498, 10000);
        QCOMPARE(assets->count(), 366);
        auto selectScene = [&](quint32 key) {
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole).toUInt() == key) {
                    scenes->setCurrentRow(i);
                    return true;
                }
            return false;
        };
        auto selectAsset = [&](const QString &id) {
            for (int i = 0; i < assets->count(); ++i)
                if (assets->item(i)->data(Qt::UserRole).toString() == id) {
                    assets->setCurrentRow(i);
                    return true;
                }
            return false;
        };
        QVERIFY(selectScene(0x1a14970));
        QCOMPARE(commands->rowCount(), 282);
        QVERIFY(requests->currentText().contains("selector 34"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  "mode2-environment:000D62F6", 10000);
        QVERIFY(selectScene(0x19aa852)); // Selector 2, Graduation Ceremony.
        const auto dialogue = editor.findChild<DialogueInspector *>();
        const auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
        QVERIFY(dialogue && pages);
        const auto catalog = Directors::load(rom);
        const auto program = catalog.entries()[0x19aa852].program;
        bool foundDialogue = false;
        for (int i = 0; i < program.commands.size(); ++i) {
            const auto &cmd = program.commands[i];
            if (cmd.opcode != 0xbf)
                continue;
            commands->cellDoubleClicked(i, 1);
            QCOMPARE(pages->currentWidget(), dialogue);
            QCOMPARE(dialogue->property("archiveSelector").toInt(), int(quint16(cmd.operands[1])));
            QCOMPARE(dialogue->property("entryIndex").toInt(), int(quint16(cmd.operands[2])));
            foundDialogue = true;
            break;
        }
        QVERIFY(foundDialogue);
        bool foundTransform = false;
        const auto transform = editor.findChild<TransformPreview *>();
        QVERIFY(transform);
        for (const auto &resource : catalog.entries()) {
            if (foundTransform)
                break;
            if (resource.program.resourceClass() != 4 && resource.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < resource.program.commands.size(); ++i) {
                if (resource.program.commands[i].opcode != 8)
                    continue;
                QVERIFY(selectScene(resource.key));
                commands->cellDoubleClicked(i, 1);
                QCOMPARE(pages->currentWidget(), transform);
                QVERIFY2(!transform->property("channelRecord").toByteArray().isEmpty(),
                         qPrintable(transform->findChild<QLabel *>("transformStatus")->text()));
                foundTransform = true;
                break;
            }
        }
        QVERIFY(foundTransform);
        bool foundActor = false;
        const auto actorInspector = editor.findChild<ActorPoseEditor *>();
        QVERIFY(actorInspector);
        for (const auto &resource : catalog.entries()) {
            if (foundActor)
                break;
            if (resource.program.resourceClass() != 4 && resource.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < resource.program.commands.size(); ++i) {
                const auto &command = resource.program.commands[i];
                if (command.opcode != 0x14 || command.operands.size() != 9)
                    continue;
                const auto &args = command.operands;
                if (args[1] < 1 || args[1] > 68 || qint32(args[2]) < 0 || args[3] > 9)
                    continue;
                bool inRange = true;
                for (int axis = 0; axis < 3; ++axis)
                    inRange &= qAbs(double(qint32(args[4 + axis])) / 1000.) <= 10000;
                if (!inRange)
                    continue;
                QVERIFY(selectScene(resource.key));
                commands->cellDoubleClicked(i, 1);
                QCOMPARE(pages->currentWidget(), actorInspector);
                QVERIFY2(actorInspector->property("inspectedActorCommand").toInt() == i,
                         qPrintable(actorInspector->findChild<QLabel *>("actorStatus")->text()));
                QCOMPARE(actorInspector->findChild<QComboBox *>("actorBank")->currentData().toInt(),
                         qint32(args[1]));
                for (int axis = 0; axis < 3; ++axis)
                    QCOMPARE(
                        actorInspector
                            ->findChild<QDoubleSpinBox *>(QString("actorPlacement%1").arg(axis))
                            ->value(),
                        double(qint32(args[4 + axis])) / 1000.);
                QCOMPARE(actorInspector->findChild<QSpinBox *>("actorPlacementFacing")->value(),
                         qint32(args[3]));
                QVERIFY(actorInspector->findChild<QCheckBox *>("actorProjected")->isChecked());
                foundActor = true;
                break;
            }
        }
        QVERIFY(foundActor);
        QVERIFY(selectScene(0x19aa852));
        pages->setCurrentIndex(0);
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  "mode2-environment:00236B58", 10000);
        QCOMPARE(view->scene()->sceneRect().size(), QSizeF(495, 383));
        QVERIFY(editor.grab().save("cutscene-graduation.png"));
        tabs->setCurrentIndex(1);
        QVERIFY(selectAsset("archive:102"));
        QVERIFY(selectAsset("archive:636"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(), "archive:636",
                                  10000);
        QCOMPARE(frames->count(), 12);
        frames->setCurrentIndex(2);
        QCOMPARE(view->scene()->sceneRect().size(), QSizeF(256, 48));
        // Allow the earlier, more expensive compound decode to finish. It must not replace this
        // selection.
        QTest::qWait(200);
        QCOMPARE(editor.property("displayedImage").toString(), "archive:636");
        QVERIFY(selectAsset("archive:102"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(), "archive:102",
                                  10000);
        QCOMPARE(layers->count(), 6);
        QCOMPARE(view->scene()->sceneRect(), QRectF(-360, -320, 607, 511));
        layers->setCurrentRow(2);
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto chosen = view->scene()->selectedItems().first();
        layers->item(2)->setCheckState(Qt::Unchecked);
        QVERIFY(!chosen->isVisible());
        layers->item(2)->setCheckState(Qt::Checked);
        QVERIFY(chosen->isVisible());
        QVERIFY(editor.grab().save("cutscene-compound.png"));
        auto search = editor.findChild<QLineEdit *>("cutsceneAssetsSearch");
        search->setText("congra");
        int visible = 0;
        for (int i = 0; i < assets->count(); ++i)
            visible += !assets->item(i)->isHidden();
        QCOMPARE(visible, 1);
        QCOMPARE(sha256(rom.bytes()), before);
    }
    void scenarioMapLabelSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        auto scenarios = *project.scenarios();
        const int row = scenarios.addNode(1, 1, project.classData());
        auto node = scenarios.scenario(1).layout.nodes[row];
        put8(node, 4, 128); put8(node, 5, 10);
        scenarios.setNode(1, row, node, project.classData());
        project.setScenarios(scenarios.json());
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        const auto before = project.manifest();
        const auto target = [&] {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            return QPointF(canvas->width() / 2., canvas->height() / 2.) +
                   QPointF(0, -118) * side / 256. + QPointF(30, 0);
        };
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QTest::qWait(50);
        canvas->repaint();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QVERIFY(objects->currentItem());
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Waypoint));
        QCOMPARE(objects->currentItem()->data(Qt::UserRole + 1).toInt(), row);
        QCOMPARE(project.manifest(), before);
        canvas->repaint();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QTest::mouseMove(canvas, target().toPoint() + QPoint(30, 15), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint() + QPoint(30, 15));
        QCOMPARE(project.manifest(), before); // Labels select; only marker drags move.
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        auto orders = editor.findChild<QWidget *>("mapSquadOrders");
        auto command = orders->findChild<QComboBox *>("squadCommand");
        command->setCurrentIndex(command->findData(int(SquadBehavior::Move)));
        orders->findChild<QPushButton *>("squadCommandPick")->click();
        canvas->repaint();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Squad));
        QCOMPARE(project.manifest(), before);
        QVERIFY(selectMapObject(editor, MapObject::Waypoint, row));
        canvas->repaint();
        QVERIFY(editor.grab().save("map-label-selection.png"));
    }
    void treasureAutoApply() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path)); project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        QVERIFY(selectMapObject(editor, MapObject::Treasure, 0));
        QTest::qWait(30);
        const auto before = project.manifest();
        const auto original = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
        auto record = [&] { return ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0); };
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        table->setCurrentIndex(table->findData(1));
        item->setCurrentIndex(item->findData(277));
        QCOMPARE(u16(record(), 4), 277);
        const auto equipment = record();
        table->setCurrentIndex(table->findData(2));
        QCOMPARE(u8(record(), 3), 2); QCOMPARE(u16(record(), 4), 1);
        undo.undo(); QCOMPARE(record(), equipment);
        undo.redo();
        item->setCurrentIndex(item->findData(44));
        QCOMPARE(u16(record(), 4), 44);
        auto x = editor.findChild<QSpinBox *>("worldTreasureX");
        auto z = editor.findChild<QSpinBox *>("worldTreasureZ");
        x->setFocus(); x->selectAll(); QTest::keyClicks(x, "123");
        QCOMPARE(u8(record(), 1), u8(original, 1));
        z->setFocus(); QCOMPARE(u8(record(), 1), 123);
        z->setValue(234); QCOMPARE(u8(record(), 2), 234);
        QCOMPARE(u8(record(), 0), u8(original, 0));
        const auto after = project.manifest(); const int count = undo.count();
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        QVERIFY(selectMapObject(editor, MapObject::Treasure, 0));
        QCOMPARE(project.manifest(), after); QCOMPARE(undo.count(), count);
        QVERIFY(!editor.findChild<QPushButton *>("worldTreasureApply"));
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.setIndex(undo.count()); QCOMPARE(project.manifest(), after);
    }
    void scenarioSiteAutoApply() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        int row = -1, id = -1;
        for (int i = 0; i < project.world()->siteCount(1); ++i) {
            const int candidate = u16(project.world()->scenario(1).sites, 1 + i * 4);
            const auto town = project.world()->town(candidate);
            if (u8(town, 24) != 255 && !u8(town, 27) && !(u8(town, 25) & 3)) {
                row = i; id = candidate; break;
            }
        }
        QVERIFY(row >= 0);
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        QTest::qWait(50);
        const auto before = project.manifest();
        const auto original = project.world()->town(id);
        const int count = project.shops()->size();
        auto shop = editor.findChild<QCheckBox *>("worldFacility0");
        auto den = editor.findChild<QCheckBox *>("worldFacility1");
        auto inventory = editor.findChild<QSpinBox *>("worldShop");
        // Undo a new shop, reuse its former ID elsewhere, then enable this
        // site's shop again. The unrelated inventory must not be attached.
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), count);
        undo.undo();
        QCOMPARE(addShop(&project, &undo), count);
        shop->click();
        QCOMPARE(project.shops()->size(), count + 2);
        QCOMPARE(u8(project.world()->town(id), 27), count + 1);
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.clear();
        shop->click();
        QCOMPARE(project.shops()->size(), count + 1);
        QCOMPARE(u8(project.world()->town(id), 27), count);
        QCOMPARE(undo.count(), 1); // Stock creation and assignment undo together.
        undo.undo(); QCOMPARE(project.manifest(), before);
        undo.redo();
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), 0);
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), count);
        QCOMPARE(project.shops()->size(), count + 1); // Reuse the site's inventory.
        den->click();
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        QCOMPARE(inventory->value(), 0);
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 1);
        QCOMPARE(inventory->value(), count);
        QVERIFY(!den->isChecked());
        undo.undo(); QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        undo.redo();
        inventory->setValue(0); QVERIFY(!shop->isChecked());
        inventory->setValue(1); QVERIFY(shop->isChecked());
        QCOMPARE(u8(project.world()->town(id), 27), 1);
        QCOMPARE(u8(project.world()->town(id), 25) & 0xfc, u8(original, 25) & 0xfc);
        auto name = editor.findChild<QLineEdit *>("worldSiteName");
        name->setFocus(); name->selectAll();
        QTest::keyClicks(name, "Field Town");
        QCOMPARE(project.world()->townName(id), QString("Field Town"));
        name->setCursorPosition(5);
        QTest::keyClicks(name, "X");
        QCOMPARE(project.world()->townName(id), QString("FieldX Town"));
        QCOMPARE(name->cursorPosition(), 6);
        auto population = editor.findChild<QSpinBox *>("worldPopulation");
        const int oldPopulation = u16(project.world()->town(id), 22);
        population->setFocus(); population->selectAll();
        QTest::keyClicks(population, "1234");
        QCOMPARE(u16(project.world()->town(id), 22), oldPopulation);
        QTest::keyClick(population, Qt::Key_Return);
        QCOMPARE(u16(project.world()->town(id), 22), 1234);
        auto morale = editor.findChild<QSpinBox *>("worldMorale");
        morale->setValue(62);
        QCOMPARE(u8(project.world()->town(id), 24), 62);
        const auto valid = project.world()->town(id);
        const int edits = undo.index();
        morale->setValue(200);
        QCOMPARE(project.world()->town(id), valid);
        QCOMPARE(undo.index(), edits);
        QCOMPARE(morale->value(), 62);
        auto type = editor.findChild<QComboBox *>("worldSiteType");
        type->setCurrentIndex(type->findData(0x29));
        QCOMPARE(u8(project.world()->town(id), 26), 0x29);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(1));
        QCOMPARE(u16(project.world()->scenario(1).sites, 3 + row * 4), quint16(0x2012));
        // A real selection click commits typed numbers to the old site before
        // loading the new site's controls, without requiring Enter.
        population->setFocus(); population->selectAll();
        QTest::keyClicks(population, "2345");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        const int otherRow = (row + 1) % project.world()->siteCount(1);
        const int otherId = u16(project.world()->scenario(1).sites, 1 + otherRow * 4);
        const auto otherTown = project.world()->town(otherId);
        for (int i = 0; i < objects->count(); ++i) {
            auto item = objects->item(i);
            if (item->data(Qt::UserRole).toInt() != int(MapObject::Site) ||
                item->data(Qt::UserRole + 1).toInt() != otherRow) continue;
            objects->scrollToItem(item);
            QTest::mouseClick(objects->viewport(), Qt::LeftButton, Qt::NoModifier,
                              objects->visualItemRect(item).center());
            break;
        }
        QCOMPARE(u16(project.world()->town(id), 22), 2345);
        QCOMPARE(project.world()->town(otherId), otherTown);
        const auto after = project.manifest();
        const int undoCount = undo.count();
        QVERIFY(selectMapObject(editor, MapObject::Site, (row + 1) % project.world()->siteCount(1)));
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        QCOMPARE(name->text(), QString("FieldX Town"));
        QCOMPARE(population->value(), 2345);
        QCOMPARE(project.manifest(), after);
        QCOMPARE(undo.count(), undoCount);
        QVERIFY(!editor.findChild<QPushButton *>("worldSiteApply"));
        QVERIFY(!editor.findChild<QPushButton *>("worldShopAdd"));
        QVERIFY(!editor.findChild<QPushButton *>("worldShopRemove"));
        QVERIFY(editor.grab().save("site-auto-apply.png"));
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.setIndex(undo.count()); QCOMPARE(project.manifest(), after);
    }
    void scenarioFacilityExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 5) missions->setCurrentRow(i);
        const auto &world = *project.world();
        int row = -1, id = -1;
        for (int i = 0; i < world.siteCount(5); ++i) {
            const int candidate = u16(world.scenario(5).sites, 1 + i * 4);
            if (world.townName(candidate).startsWith("Quell")) { row = i; id = candidate; break; }
        }
        QVERIFY(row >= 0);
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        const auto before = project.manifest();
        editor.findChild<QCheckBox *>("worldFacility0")->setChecked(true);
        const int shop = u8(project.world()->town(id), 27);
        QVERIFY(shop > 0);
        QCOMPARE(project.shops()->stock(shop).consumables, QVector<quint16>({1, 2, 3, 4, 5, 6, 8}));
        editor.findChild<QCheckBox *>("worldFacility1")->setChecked(true);
        QVERIFY(!editor.findChild<QCheckBox *>("worldFacility0")->isChecked());
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        const int shopRow = row == 0 ? 1 : 0;
        const int shopTown = u16(project.world()->scenario(5).sites, 1 + shopRow * 4);
        QVERIFY(selectMapObject(editor, MapObject::Site, shopRow));
        editor.findChild<QCheckBox *>("worldObjective")->setChecked(false);
        editor.findChild<QSpinBox *>("worldShop")->setValue(shop);
        QCOMPARE(u8(project.world()->town(shopTown), 25) & 3, 1);
        const auto after = project.manifest();
        while (undo.canUndo()) undo.undo();
        QCOMPARE(project.manifest(), before);
        while (undo.canRedo()) undo.redo();
        QCOMPARE(project.manifest(), after);
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto restored = Project::fromRom(rom);
        restored.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(restored.world()->town(id), project.world()->town(id));
        const auto result = rebuild(restored, tools, work.path());
        const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(root));
        QTemporaryDir output(QDir(root).filePath("native-site-facilities-XXXXXX"));
        QVERIFY(output.isValid());
        const auto candidate = output.filePath(QFileInfo(path).fileName());
        exportRom(restored, result, candidate);
        const auto reopenedRom = Rom::open(candidate);
        const auto reopened = ScenarioWorld::load(reopenedRom);
        QCOMPARE(reopened.town(id), project.world()->town(id));
        QCOMPARE(u8(reopened.town(id), 25) & 3, 2);
        QCOMPARE(u8(reopened.town(shopTown), 25) & 3, 1);
        QCOMPARE(u8(reopened.town(id), 27), 0);
        QCOMPARE(u8(reopened.town(shopTown), 27), shop);
        QCOMPARE(Shops::load(reopenedRom).stock(shop), restored.shops()->stock(shop));
        auto sealed = reopenedRom.bytes();
        Rom::updateChecksum(sealed);
        QCOMPARE(sealed.mid(0x10, 8), reopenedRom.bytes().mid(0x10, 8));
        auto both = reopened.town(id);
        put8(both, 25, quint8(u8(both, 25) | 3));
        const auto example = [&](QString name, QByteArray town, int point, QJsonArray menu) {
            return QJsonObject{{"name", name}, {"town", QString::fromLatin1(town.toHex())},
                {"addend", u16(reopened.scenario(5).sites, 3 + point * 4)}, {"menu", menu}};
        };
        const auto fixture = work.filePath("facilities.json");
        QFile f(fixture); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonObject{{"rom", candidate}, {"cases", QJsonArray{
            example("Shop", reopened.town(shopTown), shopRow, {108}),
            example("Witch's den", reopened.town(id), row, {144}),
            example("Loaded conflicting flags", both, row, {108})}}}).toJson());
        f.close();
        QProcess probe;
        probe.start("node", {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("site-facilities-mips.cjs"), fixture});
        QVERIFY(probe.waitForFinished(30000));
        const auto errors = probe.readAllStandardError();
        QVERIFY2(probe.exitCode() == 0, errors.constData());
        qInfo().noquote() << probe.readAllStandardOutput();
        output.setAutoRemove(false);
        qInfo().noquote() << "Facility test ROM:" << candidate;
    }
    void scenarioTerrainRoute() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios(); project.loadMapEdits();
        auto s = *project.scenarios(); const auto classes = project.classData();
        auto d = s.scenario(5).layout.deployments[1]; put8(d, 3, 50); put8(d, 4, 100);
        s.setDeployment(5, 1, d, classes); s.setRouteGroup(5, 1, 0, 0, classes);
        auto members = squadMembers(s.scenario(5).squads[1]);
        for (auto &m : members) m.classId = 2;
        s.setSquad(5, 1, editSquadMembers(s.scenario(5).squads[1], members), classes);
        s.setSquadCommand(5, 1, {SquadBehavior::Move, 170, 100}, classes);
        project.setScenarios(s.json());
        auto maps = *project.mapEdits();
        maps.setBoundary(5, QRectF(QPointF(8,8), QPointF(248,248)));
        maps.paint(5, {{40,100},{180,100}}, 3, 25);
        maps.paint(5, {{110,72},{110,128}}, 8, 5);
        project.setMapEdits(maps.json());
        QUndoStack undo;
        ScenarioMapEditor editor(&project, &undo, [](const Scenarios &, const QString &) {}, [](MapObject, int) {});
        editor.resize(1250, 850); editor.show(); editor.refresh(5);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 1));
        auto route = editor.findChild<QLabel *>("mapTerrainRoute");
        QVERIFY(route && route->isVisible());
        QVERIFY2(route->text().contains("Terrain route shown"), qPrintable(route->text()));
        QTest::qWait(50); QVERIFY(editor.grab().save("terrain-route-preview.png"));
        // Changed orders and switching selection must refresh and clear the overlay.
        s.setSquadCommand(5, 1, {SquadBehavior::Guard}, classes);
        project.setScenarios(s.json()); editor.refresh(5);
        QVERIFY(route->text().contains("No fixed destination"));
        s.setSquadCommand(5, 1, {SquadBehavior::Move, 170, 100}, classes);
        project.setScenarios(s.json()); editor.refresh(5);
        QVERIFY(route->text().contains("Terrain route shown"));
        QVERIFY(selectMapObject(editor, MapObject::Site, 0)); QVERIFY(!route->isVisible());
    }
    void scenarioMapOrders() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
        pages->setCurrentIndex(0);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QTest::qWait(50);
        auto orders = editor.findChild<QWidget *>("mapSquadOrders");
        QVERIFY(orders && orders->isVisible());
        auto command = orders->findChild<QComboBox *>("squadCommand");
        auto pick = orders->findChild<QPushButton *>("squadCommandPick");
        auto apply = orders->findChild<QPushButton *>("squadCommandApply");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        const auto before = project.scenarios()->json();
        const auto deployment = project.scenarios()->scenario(1).layout.deployments[0];
        const int row = objects->currentRow();
        command->setCurrentIndex(command->findData(int(SquadBehavior::MoveAndAttack)));
        pick->click(); QVERIFY(pick->isChecked());
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, canvas->rect().center());
        QVERIFY(!pick->isChecked());
        QCOMPARE(objects->currentRow(), row);
        QCOMPARE(orders->findChild<QSpinBox *>("squadCommandX")->value(), 128);
        QCOMPARE(orders->findChild<QSpinBox *>("squadCommandZ")->value(), 128);
        QCOMPARE(project.scenarios()->json(), before);
        apply->click();
        const auto result = project.scenarios()->squadCommand(1, 0);
        QVERIFY(result);
        QCOMPARE(result->behavior, SquadBehavior::MoveAndAttack);
        QCOMPARE(result->x, 128); QCOMPARE(result->z, 128);
        QCOMPARE(project.scenarios()->scenario(1).layout.deployments[0].mid(3, 2), deployment.mid(3, 2));
        undo.undo(); QCOMPARE(project.scenarios()->json(), before);
        undo.redo();
        const auto applied = project.scenarios()->json();
        pick->click(); QVERIFY(pick->isChecked());
        QTest::keyClick(canvas, Qt::Key_Escape);
        QVERIFY(!pick->isChecked()); QCOMPARE(project.scenarios()->json(), applied);
        pick->click();
        editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(1);
        QVERIFY(!pick->isChecked()); QCOMPARE(project.scenarios()->json(), applied);
        editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(0);
        pick->click();
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        QVERIFY(!pick->isChecked()); QVERIFY(!orders->isVisible());
        QVERIFY(selectMapObject(editor, MapObject::Squad, 1));
        command->setCurrentIndex(command->findData(int(SquadBehavior::Guard)));
        apply->click();
        QCOMPARE(project.scenarios()->squadCommand(1, 0)->behavior, SquadBehavior::MoveAndAttack);
        QVERIFY(project.scenarios()->squadCommand(1, 1));
        QCOMPARE(project.scenarios()->squadCommand(1, 1)->behavior, SquadBehavior::Guard);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QVERIFY(editor.grab().save("scenario-map-orders.png"));
    }
    void scenarioAdvancedSmallWindow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1080, 720); editor.show();
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(2);
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(true);
        auto scroll = editor.findChild<QScrollArea *>("scenarioOrdersScroll");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QCOMPARE(editor.size(), QSize(1080, 720));
        for (int page = 0; page < 3; ++page) {
            tabs->setCurrentIndex(page);
            QTest::qWait(30);
            for (auto field : tabs->currentWidget()->findChildren<QComboBox *>()) {
                if (!field->isVisible()) continue;
                QVERIFY2(field->height() >= field->sizeHint().height(), qPrintable(field->objectName()));
            }
            for (auto field : tabs->currentWidget()->findChildren<QSpinBox *>()) {
                if (!field->isVisible()) continue;
                QVERIFY2(field->height() >= field->sizeHint().height(), qPrintable(field->objectName()));
            }
        }
        tabs->setCurrentIndex(0);
        auto apply = editor.findChild<QPushButton *>("routeNodeApply");
        scroll->ensureWidgetVisible(apply);
        QTest::qWait(50);
        QVERIFY(scroll->viewport()->rect().contains(apply->mapTo(scroll->viewport(), apply->rect().center())));
        QVERIFY(editor.grab().save("scenario-advanced-small.png"));
    }
    void mountainPeakEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        project.loadWorld();
        project.loadMapEdits();
        QUndoStack undo;
        ScenarioMapEditor editor(
            &project, &undo, [](const Scenarios &, const QString &) {}, [](MapObject, int) {});
        editor.resize(1300, 950);
        editor.refresh(5);
        editor.show();
        connect(&undo, &QUndoStack::indexChanged, &editor, [&] { editor.refresh(5); });
        auto tool = editor.findChild<QComboBox *>("mapTool");
        tool->setCurrentText("Mountain peaks");
        QTest::qWait(20);
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        auto height = editor.findChild<QDoubleSpinBox *>("mapPeakHeight");
        auto restore = editor.findChild<QPushButton *>("mapPeakRestore");
        QVERIFY(objects && canvas && height && restore);
        const auto peaks = project.mapEdits()->mountainPeaks(5);
        QCOMPARE(objects->count(), peaks.size());
        int selected = -1;
        for (int i = 0; i < peaks.size(); ++i)
            if (QRectF(70, 80, 130, 145).contains(peaks[i].position)) {
                selected = i;
                break;
            }
        QVERIFY(selected >= 0);
        const auto peak = peaks[selected];
        auto pixel = [&] {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            const double h = project.mapEdits()->mountainPeaks(5)[selected].height;
            return (QPointF(canvas->width() / 2., canvas->height() / 2.) +
                    (peak.position - QPointF(128, 128)) * side / 256. - QPointF(0, 12 + h * 24))
                .toPoint();
        };
        objects->setCurrentRow(selected);
        QVERIFY(height->isVisible());
        QVERIFY(!editor.findChild<QPushButton *>("mapApply")->isVisible());
        const auto original = project.mapEdits()->json();
        auto start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(25, -24), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(25, -24));
        QCOMPARE(undo.count(), 1);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position);
        QVERIFY(std::abs(project.mapEdits()->mountainPeaks(5)[selected].height - peak.height - 1) <
                .001);
        const auto raised = project.mapEdits()->json();
        start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(0, 48), 20);
        QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 48));
        QCOMPARE(project.mapEdits()->json(), raised);
        QCOMPARE(undo.count(), 1);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), original);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), raised);
        start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(0, 24), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 24));
        QCOMPARE(project.mapEdits()->json(), original);
        QCOMPARE(undo.count(), 2);
        height->setValue(.75);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, .75);
        restore->click();
        QCOMPARE(project.mapEdits()->json(), original);
        // At zero height the base and diamond are just 12 pixels apart. They
        // must still choose different operations, without a mode selector.
        height->setValue(0);
        const auto zeroHeight = project.mapEdits()->json();
        QTest::qWait(20);
        start = pixel() + QPoint(0, 12);
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(8, 4), 20);
        QCOMPARE(canvas->cursor().shape(), Qt::SizeAllCursor);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(8, 4));
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, 0.);
        QVERIFY(QLineF(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position)
                    .length() > 1);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        // The bottom of the zero-height diamond is closer to the base centre,
        // but grabbing the visible diamond must still edit height.
        start = pixel() + QPoint(0, 6);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(20, -12), 20);
        QCOMPARE(canvas->cursor().shape(), Qt::SizeVerCursor);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(20, -12));
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, .5);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        height->setValue(.75);
        const auto heightOnly = project.mapEdits()->json();
        auto add = editor.findChild<QPushButton *>("mapPeakAdd");
        auto remove = editor.findChild<QPushButton *>("mapPeakRemove");
        auto resetPeaks = editor.findChild<QPushButton *>("mapPeaksRestore");
        QVERIFY(add && remove && resetPeaks);
        QVERIFY(!editor.findChild<QComboBox *>("mapPeakDrag"));
        add->click();
        QTest::keyClick(canvas, Qt::Key_Escape);
        QVERIFY(!add->isChecked());
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        auto mapPixel = [&](QPointF grid) {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            return QPointF(canvas->width() / 2., canvas->height() / 2.) +
                   (grid - QPointF(128, 128)) * side / 256.;
        };
        QPoint place(-1, -1);
        const auto terrain = project.mapEdits()->source(5).visualTerrain;
        for (int y = 80; y < 210 && place.x() < 0; ++y)
            for (int x = 70; x < 200 && place.x() < 0; ++x) {
                bool inside = true;
                for (int dy = -4; dy <= 4 && inside; ++dy)
                    for (int dx = -4; dx <= 4; ++dx)
                        inside &= u8(terrain, (y + dy) * 256 + x + dx) == 7;
                for (const auto &p : peaks)
                    inside &= QLineF(p.position, QPointF(x, y)).length() > 5;
                if (inside)
                    place = {x, y};
            }
        QVERIFY(place.x() >= 0);
        add->click();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, mapPixel({-5, -5}).toPoint());
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        QVERIFY(add->isChecked());
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, mapPixel(place).toPoint());
        QVERIFY2(!add->isChecked(), qPrintable(editor.findChild<QLabel *>("mapNotice")->text()));
        QCOMPARE(objects->count(), peaks.size() + 1);
        QCOMPARE(objects->currentRow(), objects->count() - 1);
        const auto added = project.mapEdits()->mountainPeaks(5).last();
        QVERIFY(added.addedId > 0);
        const auto addedState = project.mapEdits()->json();
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), addedState);
        objects->setCurrentRow(objects->count() - 1);
        QTest::qWait(20);
        start = mapPixel(added.position).toPoint();
        const auto delta =
            (mapPixel(added.position + QPointF(3, 1)) - mapPixel(added.position)).toPoint();
        const int actions = undo.count();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + delta, 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + delta);
        QCOMPARE(undo.count(), actions + 1);
        const auto movedPeak = project.mapEdits()->mountainPeaks(5).last();
        QCOMPARE(movedPeak.addedId, added.addedId);
        QCOMPARE(movedPeak.height, added.height);
        QVERIFY(QLineF(movedPeak.position, added.position + QPointF(3, 1)).length() < .4);
        const auto movedState = project.mapEdits()->json();
        start = mapPixel(movedPeak.position).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(10, 10), 20);
        QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(10, 10));
        QCOMPARE(project.mapEdits()->json(), movedState);
        remove->click();
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        QCOMPARE(objects->count(), peaks.size());
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), movedState);
        objects->setCurrentRow(objects->count() - 1);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/mountain-peak-layout.png"));
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto restored = Project::fromRom(Rom::open(path));
        restored.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(restored.mapEdits()->json(), project.mapEdits()->json());
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        QVERIFY(!tools.isEmpty());
        const auto result = rebuild(restored, tools, work.path());
        const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(root));
        QTemporaryDir output(QDir(root).filePath("native-mountain-peaks-XXXXXX"));
        QVERIFY(output.isValid());
        const auto candidate = output.filePath(QFileInfo(path).fileName());
        exportRom(restored, result, candidate);
        const auto reopened = MapEdits::load(Rom::open(candidate));
        QCOMPARE(reopened.model(5), project.mapEdits()->model(5));
        QCOMPARE(reopened.source(5).geometry[1].data,
                 editFieldHeight(project.mapEdits()->source(5), project.mapEdits()->changes(5)));
        auto checksum = result.image;
        Rom::updateChecksum(checksum);
        QCOMPARE(checksum, result.image);
        output.setAutoRemove(false);
        qInfo().noquote() << "Peak edit export:" << candidate;
        resetPeaks->click();
        QCOMPARE(project.mapEdits()->json(), original);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), movedState);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), original);
        tool->setCurrentIndex(0);
        QVERIFY(!height->isVisible());
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
    }
    void scenarioMapEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            // Add a real waypoint so all three editable map object kinds are exercised.
            auto scenarios = *project.scenarios();
            const int waypoint = scenarios.addNode(1, 1, project.classData());
            project.setScenarios(scenarios.json());
            QUndoStack undo;
            ScenarioEditor editor(&project, &undo, [] {});
            editor.resize(1450, 950);
            editor.show();
            auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
            pages->setCurrentIndex(0);
            QTest::qWait(20);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            auto apply = editor.findChild<QPushButton *>("mapApply");
            auto fields = editor.findChild<QPushButton *>("mapEditFields");
            auto x = editor.findChild<QSpinBox *>("mapX"), z = editor.findChild<QSpinBox *>("mapZ");
            auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
            QVERIFY(objects && apply && fields && x && z && canvas);
            // Marker artwork follows the current records, including edits and Undo.
            QVERIFY(!objects->item(0)->icon().isNull());
            const auto leaderIcon = objects->item(0)->icon().pixmap(32, 32).toImage();
            QCOMPARE(leaderIcon.pixelColor(0, 0).alpha(), 0); // Circular crop, not a square portrait.
            QVERIFY(leaderIcon.pixelColor(16, 16).alpha() > 0);
            QSet<QRgb> portraitColors;
            for (int py = 2; py < 30; ++py)
                for (int px = 2; px < 30; ++px)
                    if (leaderIcon.pixelColor(px, py).alpha() == 255) portraitColors.insert(leaderIcon.pixel(px, py));
            QVERIFY(portraitColors.size() > 8); // The stock boss has an enemy portrait, not a blank badge.
            const auto beforeLeader = project.scenarios()->json();
            objects->setCurrentRow(0);
            fields->click();
            auto leaderClass = editor.findChild<QComboBox *>("scenarioClass_0");
            const int alternateClass = leaderClass->currentData().toInt() == 5 ? 6 : 5;
            leaderClass->setCurrentIndex(leaderClass->findData(alternateClass));
            editor.findChild<QPushButton *>("scenarioSquadApply")->click();
            QCOMPARE(u8(project.scenarios()->scenario(1).squads[0], 0), quint8(alternateClass));
            QVERIFY(objects->item(0)->icon().pixmap(32, 32).toImage() != leaderIcon);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), beforeLeader);
            QCOMPARE(objects->item(0)->icon().pixmap(32, 32).toImage(), leaderIcon);
            int treasureIconRow = -1;
            for (int row = 0; row < objects->count(); ++row)
                if (objects->item(row)->text().startsWith("Treasure 1 ·")) treasureIconRow = row;
            QVERIFY(treasureIconRow >= 0);
            QVERIFY(!objects->item(treasureIconRow)->icon().isNull());
            const auto treasureIcon = objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage();
            const auto beforeItem = project.world()->json();
            const auto firstTreasure = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(treasureIconRow);
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            auto itemTable = editor.findChild<QComboBox *>("worldTreasureTable");
            itemTable->setCurrentIndex(itemTable->findData(u8(firstTreasure, 3) == 1 ? 2 : 1));
            auto treasureChoice = editor.findChild<QComboBox *>("worldTreasureItem");
            treasureChoice->setCurrentIndex(treasureChoice->findData(1));
            QVERIFY(objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage() != treasureIcon);
            undo.undo();
            QCOMPARE(project.world()->json(), beforeItem);
            QCOMPARE(objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage(), treasureIcon);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            auto terrain = editor.findChild<QCheckBox *>("mapTerrain");
            QVERIFY(terrain);
            QVERIFY(terrain->isEnabled());
            QVERIFY(terrain->isChecked());
            QVERIFY(editor.findChild<QLabel *>("mapTerrainStatus")->text().contains("In-scenario battlefield"));
            const auto terrainImage = canvas->grab().toImage();
            const auto dataBeforeToggle = project.scenarios()->json();
            const auto undoBeforeToggle = undo.count();
            terrain->setChecked(false);
            const auto withoutTerrain = canvas->grab().toImage();
            QVERIFY(withoutTerrain != terrainImage);
            terrain->setChecked(true);
            QCOMPARE(canvas->grab().toImage(), terrainImage);
            QCOMPARE(project.scenarios()->json(), dataBeforeToggle);
            QCOMPARE(undo.count(), undoBeforeToggle);
            // Applying allegiance must repaint the actual building texture;
            // undo must also restore it, rather than leave a stale cached map.
            QVERIFY(selectMapObject(editor, MapObject::Site, 0));
            const auto beforeAllegiance = canvas->grab().toImage();
            auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
            allegiance->setCurrentIndex(allegiance->findData(2));
            QVERIFY(beforeAllegiance != canvas->grab().toImage());
            undo.undo();
            QCOMPARE(canvas->grab().toImage(), beforeAllegiance);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-battlefield.png"));
            const auto beforeTreasure = project.world()->json();
            const int treasuresBefore = ScenarioWorld::treasureCount(project.world()->scenario(1).treasures);
            editor.findChild<QPushButton *>("mapTreasureAdd")->click();
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(canvas->width() / 2, canvas->height() / 2));
            QCOMPARE(ScenarioWorld::treasureCount(project.world()->scenario(1).treasures), treasuresBefore + 1);
            const auto placed = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, treasuresBefore);
            QCOMPARE(u8(placed, 1), quint8(128));
            QCOMPARE(u8(placed, 2), quint8(128));
            const auto afterTreasure = project.world()->json();
            undo.undo(); QCOMPARE(project.world()->json(), beforeTreasure);
            undo.redo(); QCOMPARE(project.world()->json(), afterTreasure);
            // Keep the newly placed treasure for this workflow's project/export/reopen checks.
            QVERIFY(objects->count() > project.scenarios()->scenario(1).squads.size());
            const auto original = project.scenarios()->json(),
                       originalWorld = project.world()->json();
            objects->setCurrentRow(0);
            fields->click();
            QCOMPARE(pages->currentIndex(), 1);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioSquads")->currentRow(), 0);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            x->setValue(128);
            z->setValue(128);
            apply->click();
            const auto centered = project.scenarios()->json();
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 3), quint8(128));
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 4), quint8(128));
            undo.undo();
            QCOMPARE(project.scenarios()->json(), original);
            undo.redo();
            QCOMPARE(project.scenarios()->json(), centered);
            const QPoint center(canvas->width() / 2, canvas->height() / 2);
            const int shift = (std::min(canvas->width(), canvas->height()) - 72) / 4;
            const QPoint pan(24, -18), anchor = center + pan;
            QTest::mousePress(canvas, Qt::RightButton, Qt::NoModifier, center);
            QTest::mouseMove(canvas, anchor, 20);
            QTest::mouseRelease(canvas, Qt::RightButton, Qt::NoModifier, anchor);
            QWheelEvent wheel(anchor, canvas->mapToGlobal(anchor), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(canvas, &wheel);
            const int zoomedShift = qRound(shift * std::pow(1.0015, 120));
            const QPoint destination = anchor + QPoint(zoomedShift, -zoomedShift);
            const QPoint iconEdge(14, 0); // The visible portrait edge must hit and keep its grab offset.
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, anchor + iconEdge);
            QTest::mouseMove(canvas, destination + iconEdge, 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, destination + iconEdge);
            const auto moved = project.scenarios()->scenario(1).layout.deployments[0];
            QVERIFY(std::abs(int(u8(moved, 3)) - 192) <= 1);
            QVERIFY(std::abs(int(u8(moved, 4)) - 64) <= 1);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), centered);
            undo.redo();
            editor.findChild<QPushButton *>("mapFit")->click();
            int waypointItem = -1, treasureItem = -1, siteItem = -1;
            const int nodeId = u8(project.scenarios()->scenario(1).layout.nodes[waypoint], 0);
            const auto townName =
                project.world()->townName(u16(project.world()->scenario(1).sites, 1));
            for (int i = 0; i < objects->count(); ++i) {
                const auto label = objects->item(i)->text();
                if (label.startsWith(QString("Waypoint %1").arg(nodeId)))
                    waypointItem = i;
                if (label.startsWith("Treasure 1 ·"))
                    treasureItem = i;
                if (label == townName)
                    siteItem = i;
            }
            QVERIFY(waypointItem >= 0 && treasureItem >= 0 && siteItem >= 0);
            objects->setCurrentRow(waypointItem);
            x->setValue(55);
            z->setValue(66);
            apply->click();
            fields->click();
            QCOMPARE(pages->currentIndex(), 2);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioNodes")->currentRow(), waypoint);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(treasureItem);
            const int beforeTreasureMove = undo.index();
            editor.findChild<QSpinBox *>("worldTreasureX")->setValue(255);
            editor.findChild<QSpinBox *>("worldTreasureZ")->setValue(0);
            auto treasure =
                ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            QCOMPARE(u8(treasure, 1), quint8(255));
            QCOMPARE(u8(treasure, 2), quint8(0));
            const int afterTreasureMove = undo.index();
            undo.setIndex(beforeTreasureMove);
            QCOMPARE(project.world()->json(), originalWorld);
            undo.setIndex(afterTreasureMove);
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            QCOMPARE(editor.findChild<QSpinBox *>("worldTreasureX")->value(), 255);
            // A real map click clears the old inspector, then opens the hit treasure.
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
            QVERIFY(!editor.findChild<QWidget *>("mapWorldInspector")->isVisible());
            QTest::qWait(20);
            const qreal gridSize = std::min(canvas->width(), canvas->height()) - 72;
            const QPointF treasurePixel = QPointF(canvas->width() / 2., canvas->height() / 2.) +
                                          QPointF(127, -128) * gridSize / 256.;
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, treasurePixel.toPoint());
            QCOMPARE(objects->currentRow(), treasureItem);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            pages->setCurrentIndex(0);
            objects->setCurrentRow(siteItem);
            QVERIFY(apply->isEnabled());
            const auto originalMaps = project.mapEdits()->json();
            const auto sourceMap = ScenarioMap::load(rom, 1);
            const auto oldSite = sourceMap.points[0];
            const auto startGrid =
                QPointF((oldSite.x() - sourceMap.bounds.left()) / sourceMap.bounds.width() * 256.,
                        (oldSite.y() - sourceMap.bounds.top()) / sourceMap.bounds.height() * 256.);
            auto pixel = [&](QPointF grid) {
                const double side = std::min(canvas->width(), canvas->height()) - 72;
                return (QPointF(canvas->width() / 2., canvas->height() / 2.) +
                        (grid - QPointF(128, 128)) * side / 256.)
                    .toPoint();
            };
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel(startGrid));
            QTest::mouseMove(canvas, pixel(startGrid + QPointF(15, 8)), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                pixel(startGrid + QPointF(15, 8)));
            QVERIFY2(project.mapEdits()->json() != originalMaps,
                     qPrintable(editor.findChild<QLabel *>("mapNotice")->text()));
            const auto movedMap = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), originalMaps);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), movedMap);
            auto mapTool = editor.findChild<QComboBox *>("mapTool");
            QVERIFY(mapTool);
            mapTool->setCurrentIndex(2);
            auto brushSize = editor.findChild<QDoubleSpinBox *>("mapBrushSize");
            QVERIFY(brushSize);
            QCOMPARE(brushSize->singleStep(), .5);
            brushSize->setValue(1.5);
            QCOMPARE(brushSize->value(), 1.5);
            const auto beforeRoad = canvas->grab().toImage();
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({105, 120}));
            QTest::mouseMove(canvas, pixel({125, 130}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({125, 130}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 125 * 256 + 115), quint8(1));
            QVERIFY(beforeRoad != canvas->grab().toImage());
            const auto painted = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), movedMap);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), painted);
            mapTool->setCurrentIndex(1);
            auto paintType = editor.findChild<QComboBox *>("mapPaintType");
            paintType->setCurrentIndex(paintType->findData(7));
            brushSize->setValue(12);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({110, 155}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 155 * 256 + 110), quint8(7));
            // A second landform exercises the separate forest-border texture.
            paintType->setCurrentIndex(paintType->findData(5));
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({145, 160}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 160 * 256 + 145), quint8(5));
            paintType->setCurrentIndex(paintType->findData(2));
            brushSize->setValue(1.5);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({175, 68}));
            QTest::mouseMove(canvas, pixel({193, 55}), 20);
            QTest::mouseMove(canvas, pixel({211, 68}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({211, 68}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 68 * 256 + 193), quint8(2));
            QCOMPARE(u8(project.mapEdits()->changes(1).paint, 55 * 256 + 193), quint8(255));
            brushSize->setValue(12);
            const auto terrainPaint = project.mapEdits()->json();
            mapTool->setCurrentIndex(3);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({110, 155}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 155 * 256 + 110),
                     u8(project.mapEdits()->source(1).terrain[0].data, 155 * 256 + 110));
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), terrainPaint);
            editor.findChild<QPushButton *>("mapRestore")->click();
            QCOMPARE(project.mapEdits()->json(), originalMaps);
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), terrainPaint);
            // Visibility alone is not an edit; numeric edges and corner dragging are undoable.
            auto boundaryToggle = editor.findChild<QCheckBox *>("mapBoundary");
            QVERIFY(boundaryToggle && boundaryToggle->isChecked());
            const auto withBoundary = canvas->grab().toImage();
            const int beforeBoundaryUndo = undo.index();
            boundaryToggle->setChecked(false);
            QVERIFY(canvas->grab().toImage() != withBoundary);
            boundaryToggle->setChecked(true);
            QCOMPARE(undo.index(), beforeBoundaryUndo);
            const auto originalBoundary = project.mapEdits()->boundary(1);
            mapTool->setCurrentIndex(4);
            auto topBoundary = editor.findChild<QSpinBox *>("mapBoundaryTop");
            QVERIFY(topBoundary && topBoundary->isVisible());
            topBoundary->setValue(4);
            editor.findChild<QPushButton *>("mapBoundaryApply")->click();
            QCOMPARE(project.mapEdits()->boundary(1).top(), 4.);
            undo.undo();
            QCOMPARE(project.mapEdits()->boundary(1), originalBoundary);
            undo.redo();
            QCOMPARE(project.mapEdits()->boundary(1).top(), 4.);
            const auto numericBoundary = project.mapEdits()->boundary(1);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                              pixel(numericBoundary.bottomRight()));
            QTest::mouseMove(canvas, pixel({248, 249}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({248, 249}));
            QVERIFY(std::abs(project.mapEdits()->boundary(1).right() - 248) <= 1);
            QVERIFY(std::abs(project.mapEdits()->boundary(1).bottom() - 249) <= 1);
            const auto resizedBoundary = project.mapEdits()->boundary(1);
            undo.undo();
            QCOMPARE(project.mapEdits()->boundary(1), numericBoundary);
            undo.redo();
            QCOMPARE(project.mapEdits()->boundary(1), resizedBoundary);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                       "/scenario-map-boundary.png"));
            mapTool->setCurrentIndex(0);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/scenario-map-painting.png"));
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldSiteFields")->isVisible());
            QCOMPARE(editor.findChild<QLineEdit *>("worldSiteName")->text(), townName);
            // City scenery and paving use real map input, the shared Undo stack,
            // and the full save/compile/export/reopen checks below.
            const auto beforeCity = project.mapEdits()->json();
            mapTool->setCurrentIndex(5);
            QTest::qWait(20);
            auto cityType = editor.findChild<QComboBox *>("mapCityType");
            auto cityRotation = editor.findChild<QSpinBox *>("mapCityRotation");
            QVERIFY(cityType && cityRotation && cityType->isVisible());
            QCOMPARE(cityType->count(), 23);
            cityType->setCurrentIndex(cityType->findData(32));
            brushSize->setValue(7);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({140, 140}));
            QTest::mouseMove(canvas, pixel({164, 140}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({164, 140}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 140 * 256 + 152), quint8(1));
            const auto paving = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), beforeCity);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), paving);
            const int houseIndex = cityType->findData(QString("latium-house"));
            QVERIFY(houseIndex >= 0 && !cityType->itemIcon(houseIndex).isNull());
            cityType->setCurrentIndex(houseIndex);
            cityRotation->setValue(90);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({150, 140}));
            QCOMPARE(project.mapEdits()->changes(1).city.size(), 1);
            QCOMPARE(project.mapEdits()->changes(1).city[0].rotation, 90);
            const auto placedCity = project.mapEdits()->json();
            const int placedUndo = undo.index();
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({0, 0}));
            QCOMPARE(project.mapEdits()->json(), placedCity);
            QCOMPARE(undo.index(), placedUndo);
            QTest::qWait(20);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-palette.png"));
            mapTool->setCurrentIndex(0);
            QVERIFY(!cityType->isVisible());
            int cityItem = -1;
            for (int i = 0; i < objects->count(); ++i)
                if (objects->item(i)->data(Qt::UserRole).toInt() == int(MapObject::City))
                    cityItem = i;
            QVERIFY(cityItem >= 0);
            objects->setCurrentRow(cityItem);
            auto placedAngle = editor.findChild<QSpinBox *>("mapPlacedCityRotation");
            QVERIFY(placedAngle && placedAngle->isVisible());
            placedAngle->setValue(180);
            editor.findChild<QPushButton *>("mapCityRotate")->click();
            QCOMPARE(project.mapEdits()->changes(1).city[0].rotation, 180);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({150, 140}));
            QTest::mouseMove(canvas, pixel({155, 140}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({155, 140}));
            QVERIFY(QLineF(project.mapEdits()->changes(1).city[0].position,
                            sourceMap.world(155, 140)).length() < .2);
            const auto movedCity = project.mapEdits()->json();
            undo.undo();
            QVERIFY(project.mapEdits()->json() != movedCity);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), movedCity);
            editor.findChild<QPushButton *>("mapCityRemove")->click();
            QVERIFY(project.mapEdits()->changes(1).city.isEmpty());
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), movedCity);
            objects->setCurrentRow(cityItem);
            QTest::qWait(20);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-inspector.png"));
            editor.resize(1100, 800);
            mapTool->setCurrentIndex(5);
            QTest::qWait(20);
            QVERIFY(cityType->isVisible() && cityType->height() >= 42);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-small.png"));
            editor.resize(1450, 950);
            mapTool->setCurrentIndex(0);
            QTest::qWait(20);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            QTest::qWait(20);
            const auto squadSummary = editor.findChild<QLabel *>("mapSelectionSummary");
            QVERIFY(squadSummary->isVisible());
            QVERIFY(squadSummary->text().contains("Level"));
            QVERIFY(editor.grab().save("scenario-map.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(restored.world()->json(), project.world()->json());
            QCOMPARE(restored.mapEdits()->json(),project.mapEdits()->json());
            const auto result = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(root));
            QTemporaryDir output(QDir(root).filePath("native-map-edit-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadScenarios();
            reopened.loadWorld();
            QCOMPARE(reopened.scenarios()->scenario(1).layout.deployments[0].mid(3, 2),
                     moved.mid(3, 2));
            QCOMPARE(reopened.scenarios()->scenario(1).layout.nodes[waypoint].mid(4, 2),
                     QByteArray::fromHex("3742"));
            QCOMPARE(reopened.world()->scenario(1).treasures, project.world()->scenario(1).treasures);
            QCOMPARE(ScenarioWorld::treasureRecord(reopened.world()->scenario(1).treasures, 0),
                     treasure);
            auto reopenedMaps=MapEdits::load(reopened.rom());
            QCOMPARE(reopenedMaps.boundary(1), resizedBoundary);
            QCOMPARE(reopenedMaps.fineTerrain(1),project.mapEdits()->fineTerrain(1));
            QCOMPARE(reopenedMaps.model(1),project.mapEdits()->model(1));
            QCOMPARE(reopenedMaps.source(1).geometry[1].data,
                     editFieldHeight(project.mapEdits()->source(1), project.mapEdits()->changes(1)));
            QVERIFY(reopenedMaps.source(1).geometry[1].data != project.mapEdits()->source(1).geometry[1].data);
            QVERIFY(QLineF(reopenedMaps.points(1)[0],project.mapEdits()->points(1)[0]).length()<.00001);
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Map edit export:" << candidate;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void scenarioSiteCountEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            QUndoStack undo;
            ScenarioEditor editor(&project, &undo, [] {});
            editor.resize(1450, 950);
            editor.show();
            auto missions = editor.findChild<QListWidget *>("scenarioList");
            for (int i = 0; i < missions->count(); ++i)
                if (missions->item(i)->data(Qt::UserRole).toInt() == 64)
                    missions->setCurrentRow(i);
            auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
            pages->setCurrentIndex(0);

            auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
            QCOMPARE(project.world()->siteCount(64), 6);
            const auto original = project.world()->scenario(64).sites;
            QCOMPARE(original,
                     QByteArray::fromHex("010118200201190000011a2012011b0004011c0004011d0004"));
            auto expected = original;
            for (int row = 1; row < 6; ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Site, row));
                const int target = row == 2 ? 2 : 1;
                allegiance->setCurrentIndex(allegiance->findData(target));
                put16(expected, 3 + row * 4, target == 1 ? 0x2012 : 4);
                QCOMPARE(project.world()->scenario(64).sites, expected);
            }
            QCOMPARE(undo.count(), 5);
            while (undo.canUndo())
                undo.undo();
            QCOMPARE(project.world()->scenario(64).sites, original);
            while (undo.canRedo())
                undo.redo();
            QCOMPARE(project.world()->scenario(64).sites, expected);
            pages->setCurrentIndex(0);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            for (int row = 0; row < 6; ++row) {
                const auto name = project.world()->townName(u16(expected, 1 + row * 4));
                const auto matches = objects->findItems(name, Qt::MatchExactly);
                QCOMPARE(matches.size(), 1);
                objects->setCurrentItem(matches.first());
                QCOMPARE(editor.findChild<QLineEdit *>("worldSiteName")->text(), name);
                QVERIFY(editor.findChild<QWidget *>("worldSiteFields")->isVisible());
                pages->setCurrentIndex(0);
            }
            QVERIFY(editor.grab().save("scenario-64-sites.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.world()->scenario(64).sites, expected);
            const auto result = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(root));
            QTemporaryDir output(QDir(root).filePath("native-site-count-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, candidate);
            const auto reopenedRom = Rom::open(candidate);
            const auto reopened = ScenarioWorld::load(reopenedRom);
            QCOMPARE(reopened.siteCount(64), 6);
            QCOMPARE(reopened.scenario(64).sites, expected);
            const auto resources = WorldResources::load(reopenedRom);
            const auto allocation = resources.placement();
            QVERIFY(allocation);
            QVERIFY(quint64(allocation->offset) + allocation->allocation <=
                    quint64(reopenedRom.bytes().size()));
            QCOMPARE(resources.sites(reopenedRom, 64), expected);
            for (int key = 1; key < 64; ++key)
                QCOMPARE(reopened.scenario(key), project.world()->scenario(key));
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Site count export:" << candidate;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void scenarioWorldEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950);
        editor.show();
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(0);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto treasureCount = [&] { return ScenarioWorld::treasureCount(project.world()->scenario(52).treasures); };
        const auto base = project.world()->json();
        for (int i = 0; i < missions->count(); ++i) {
            if (!(missions->item(i)->flags() & Qt::ItemIsEnabled)) continue;
            missions->setCurrentRow(i);
            const int key = missions->currentItem()->data(Qt::UserRole).toInt();
            for (int row = 0; row < project.world()->siteCount(key); ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Site, row));
                QCOMPARE(project.world()->json(), base);
            }
            for (int row = 0; row < ScenarioWorld::treasureCount(project.world()->scenario(key).treasures); ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Treasure, row));
                QCOMPARE(project.world()->json(), base);
            }
        }
        QCOMPARE(undo.count(), 0);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 52)
                missions->setCurrentRow(i);
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        const int id = u16(project.world()->scenario(52).sites, 1);
        const auto beforeShop = project.manifest();
        const int shopCount = project.shops()->size();
        const int assigned = u8(project.world()->town(id), 27) == 1 ? 2 : 1;
        spin("worldShop")->setValue(assigned);
        QCOMPARE(project.shops()->size(), shopCount);
        QCOMPARE(u8(project.world()->town(id), 27), assigned);
        editor.findChild<QCheckBox *>("worldFacility0")->setChecked(false);
        QCOMPARE(u8(project.world()->town(id), 27), 0);
        undo.undo();
        QCOMPARE(u8(project.world()->town(id), 27), assigned);
        undo.undo();
        QCOMPARE(project.manifest(), beforeShop);
        auto den = editor.findChild<QCheckBox *>("worldFacility1");
        const auto oldFlags = u8(project.world()->town(id), 25);
        den->setChecked(!(oldFlags & 2));
        QCOMPARE(u8(project.world()->town(id), 25), quint8(oldFlags & 2 ? oldFlags & ~2 : (oldFlags & ~1) | 2));
        undo.undo();
        QCOMPARE(project.manifest(), beforeShop);
        const int beforePopulation = undo.index();
        spin("worldPopulation")->setValue(4567);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(2));
        QCOMPARE(u16(project.world()->town(id), 22), quint16(4567));
        QCOMPARE(u16(project.world()->scenario(52).sites, 3), quint16(4));
        const int afterAllegiance = undo.index();
        undo.setIndex(beforePopulation);
        QCOMPARE(project.world()->json(), base);
        undo.setIndex(afterAllegiance);
        auto objective = editor.findChild<QCheckBox *>("worldObjective");
        objective->setChecked(true);
        QCOMPARE(u8(project.world()->town(id), 24), quint8(255));
        QVERIFY(!spin("worldMorale")->isEnabled());
        editor.grab().save("scenario-sites.png");
        editor.resize(1100, 800);
        QTest::qWait(20);
        QVERIFY(editor.grab().save("map-inspector-site-1100.png"));
        editor.resize(1450, 950);
        QTest::qWait(20);
        const int count = treasureCount();
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        button("mapTreasureAdd")->click();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, canvas->rect().center());
        QCOMPARE(treasureCount(), count + 1);
        const int row = objects->currentItem()->data(Qt::UserRole + 1).toInt();
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Treasure));
        QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
        spin("worldTreasureX")->setValue(255);
        spin("worldTreasureZ")->setValue(0);
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        table->setCurrentIndex(table->findData(1));
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        item->setCurrentIndex(item->findData(277));
        const auto record =
            ScenarioWorld::treasureRecord(project.world()->scenario(52).treasures, row);
        QCOMPARE(u16(record, 4), quint16(277));
        QCOMPARE(u8(record, 1), quint8(255));
        editor.grab().save("scenario-treasure.png");
        button("worldTreasureRemove")->click();
        QCOMPARE(treasureCount(), count);
        undo.undo();
        QCOMPARE(treasureCount(), count + 1);
        while (undo.canUndo())
            undo.undo();
        QCOMPARE(project.world()->json(), base);
    }
    void scenarioRouteEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1500, 950);
        editor.show();
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(2);
        auto routes = editor.findChild<QWidget *>("scenarioRoutesEditor");
        QVERIFY(routes);
        auto button = [&](const char *name) { return routes->findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return routes->findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = routes->findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto nodes = editor.findChild<QListWidget *>("scenarioNodes");
        auto conditions = editor.findChild<QListWidget *>("scenarioConditions");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(true);
        const auto original = project.scenarios()->json();
        for (int mission = 0; mission < missions->count(); ++mission) {
            missions->setCurrentRow(mission);
            for (int row = 0; row < nodes->count(); ++row) {
                nodes->setCurrentRow(row);
                button("routeNodeApply")->click();
                QCOMPARE(project.scenarios()->json(), original);
            }
            for (int row = 0; row < conditions->count(); ++row) {
                conditions->setCurrentRow(row);
                button("routeConditionApply")->click();
                QCOMPARE(project.scenarios()->json(), original);
            }
        }
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 10)
                missions->setCurrentRow(i);
        button("routeConditionAdd")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.conditions.size(), 1);
        tabs->setCurrentIndex(1);
        spin("routePayload_6")->setValue(4);
        button("routeConditionApply")->click();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.conditions[0], 6), 4);
        tabs->setCurrentIndex(0);
        button("routeNodeAdd")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes.size(), 2);
        select("routePlacement", 1);
        spin("routeX")->setValue(80);
        spin("routeZ")->setValue(160);
        select("routeConditionA", 1);
        select("routeNext", 1);
        button("routeNodeApply")->click();
        const auto saved = project.scenarios()->json();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.nodes[1], 5), 160);
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.nodes[1], 10), 1);
        undo.undo();
        QVERIFY(project.scenarios()->json() != saved);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), saved);
        tabs->setCurrentIndex(1);
        button("routeConditionRemove")->click();
        QCOMPARE(project.scenarios()->json(), saved);
        QVERIFY(editor.findChild<QLabel *>("routeNotice")->text().contains("gates"));
        spin("routePayload_6")->setValue(31);
        button("routeConditionApply")->click();
        QCOMPARE(project.scenarios()->json(), saved);
        QVERIFY(editor.findChild<QLabel *>("routeNotice")->text().contains("30"));
        tabs->setCurrentIndex(2);
        select("routeGroupSquad", 1);
        spin("routeGroup")->setValue(7);
        select("routePattern", 5);
        button("routeGroupApply")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.deployments[0].right(2),
                 QByteArray::fromHex("0705"));
        tabs->setCurrentIndex(0);
        const auto beforeRoute = project.scenarios()->json();
        select("behaviorSquad", 0);
        const int nodeId = u8(project.scenarios()->scenario(10).layout.nodes.last(), 0);
        select("behaviorStart", nodeId);
        button("behaviorAssign")->click();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.deployments[0], 6), quint8(nodeId));
        QVERIFY(editor.findChild<QTextBrowser *>("behaviorSummary")->toPlainText().contains("X 80, Z 160"));
        undo.undo(); QCOMPARE(project.scenarios()->json(), beforeRoute);
        undo.redo();
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-routes.png"));
        const auto scripted = project.scenarios()->scenario(10).layout.nodes;
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(false);
        select("squadCommand", int(SquadBehavior::MoveAndAttack));
        select("squadCommandDestination", 0);
        spin("squadCommandX")->setValue(96);
        spin("squadCommandZ")->setValue(144);
        button("squadCommandApply")->click();
        const auto command = project.scenarios()->squadCommand(10, 0);
        QVERIFY(command);
        QCOMPARE(command->behavior, SquadBehavior::MoveAndAttack);
        QCOMPARE(command->x, 96);
        QCOMPARE(command->z, 144);
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes.first(scripted.size()), scripted);
        const auto commanded = project.scenarios()->json();
        undo.undo();
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes, scripted);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), commanded);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 1) missions->setCurrentRow(i);
        select("behaviorSquad", 0);
        select("squadCommand", int(SquadBehavior::MoveAndAttack));
        select("squadCommandDestination", 1);
        const auto destination = routes->findChild<QComboBox *>("squadCommandDestination");
        const int town = u16(project.world()->scenario(1).sites, 1);
        QCOMPARE(destination->currentText(), project.world()->townName(town));
        button("squadCommandApply")->click();
        const auto townCommand = project.scenarios()->squadCommand(1, 0);
        QVERIFY(townCommand);
        QCOMPARE(townCommand->x, 1);
        QCOMPARE(townCommand->z, 0);
        QTest::qWait(50);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-commands.png"));
    }
    void scenarioSquadEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QCOMPARE(project.appendClass(5, "Scenario fighter"), 165);
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1500, 950);
        editor.show();
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto combo = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const QString &name, int id) {
            auto box = editor.findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto list = editor.findChild<QListWidget *>("scenarioList");
        QCOMPARE(list->count(), 64);
        auto navigation = editor.findChild<QTabBar *>("scenarioNavigation");
        auto scenarioPages = editor.findChild<QStackedWidget *>("scenarioPages");
        QVERIFY(navigation && scenarioPages);
        const int selectedKey = list->currentItem()->data(Qt::UserRole).toInt();
        for (int page = 0; page < 3; ++page) {
            QTest::mouseClick(navigation, Qt::LeftButton, Qt::NoModifier, navigation->tabRect(page).center());
            QCOMPARE(scenarioPages->currentIndex(), page);
            QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), selectedKey);
            QVERIFY(list->isVisible());
            QVERIFY(editor.findChild<QLineEdit *>("scenarioSearch")->isVisible());
            QCOMPARE(spin("scenarioBase")->isVisible(), page == 1);
        }
        // Map's Edit fields action also changes pages programmatically.
        scenarioPages->setCurrentIndex(1);
        QCOMPARE(navigation->currentIndex(), 1);
        const auto beforeBase = project.scenarios()->json();
        const int originalBase = spin("scenarioBase")->value();
        spin("scenarioBase")->setValue(originalBase + 1);
        button("scenarioBaseApply")->click();
        QCOMPARE(project.scenarios()->scenario(selectedKey).layout.baseLevel(), originalBase + 1);
        undo.undo();
        QCOMPARE(project.scenarios()->json(), beforeBase);
        QCOMPARE(spin("scenarioBase")->value(), originalBase);
        const QList<int> devKeys{10, 22, 35, 54, 62, 63, 64};
        for (int i = 0; i < devKeys.size(); ++i) {
            const auto item = list->item(list->count() - devKeys.size() + i);
            QCOMPARE(item->data(Qt::UserRole).toInt(), devKeys[i]);
            QVERIFY(item->text().contains("Dev Key"));
            QCOMPARE(bool(item->flags() & Qt::ItemIsEnabled), devKeys[i] != 54);
        }
        const auto untouched = project.scenarios()->json();
        for (int i = 0; i < list->count(); ++i) {
            if (!(list->item(i)->flags() & Qt::ItemIsEnabled))
                continue;
            // Full record preservation is covered in the model check. Exercise
            // the controls on normal, shared, special-level and Dev deployments.
            if (!QList<int>{1, 10, 25, 64}.contains(list->item(i)->data(Qt::UserRole).toInt()))
                continue;
            list->setCurrentRow(i);
            auto squads = editor.findChild<QListWidget *>("scenarioSquads");
            for (int row = 0; row < squads->count(); ++row) {
                squads->setCurrentRow(row);
                button("scenarioSquadApply")->click();
                QCOMPARE(project.scenarios()->json(), untouched);
            }
        }
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toInt() == 10)
                list->setCurrentRow(i);
        const auto before = project.scenarios()->json();
        // Applying untouched controls must preserve raw drop encoding and all unknown bytes.
        button("scenarioSquadApply")->click();
        QCOMPARE(project.scenarios()->json(), before);
        select("scenarioClass_0", 165);
        button("scenarioRestoreSquad")->click();
        QCOMPARE(combo("scenarioClass_0")->currentData().toInt(), int(u8(project.scenarios()->original(10).squads[0],0)));
        QCOMPARE(project.scenarios()->json(), before);
        select("scenarioClass_0", 165);
        spin("scenarioMemberCount")->setValue(1);
        auto armyBoard = editor.findChild<FormationBoard *>("scenarioCell_Board");
        QVERIFY(armyBoard);
        const int originalCell = squadMembers(project.scenarios()->scenario(10).squads[0])[0].cell;
        const int destinationCell = (originalCell+1)%9;
        auto dragScenarioMember = [&](int from, int to) {
            auto source = armyBoard->cell(from);
            const auto start = source->mapFrom(armyBoard, armyBoard->cellCenter(from));
            const auto end = source->mapFrom(armyBoard, armyBoard->cellCenter(to));
            QTest::mousePress(source, Qt::LeftButton, Qt::NoModifier, start);
            QMouseEvent movement(QEvent::MouseMove, end, source->mapToGlobal(end),
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(source, &movement);
            QTest::mouseRelease(source, Qt::LeftButton, Qt::NoModifier, end);
        };
        dragScenarioMember(originalCell, destinationCell);
        QVERIFY(!armyBoard->cell(destinationCell)->icon().isNull());
        dragScenarioMember(destinationCell, originalCell);
        QVERIFY(!armyBoard->cell(originalCell)->icon().isNull());
        QCOMPARE(project.scenarios()->json(), before);
        select("scenarioEquipment_0_0", 0);
        for (const auto field : {"scenarioHealing", "scenarioFruit", "scenarioRevival"})
            select(field, 0);
        select("scenarioDrop", 0x8115);
        combo("scenarioPlacement")->setCurrentIndex(1);
        spin("scenarioX")->setValue(120);
        spin("scenarioZ")->setValue(160);
        button("scenarioSquadApply")->click();
        const auto after = project.scenarios()->json();
        QVERIFY(after != before);
        QCOMPARE(u8(project.scenarios()->scenario(10).squads[0], 0), 165);
        QCOMPARE(u16(project.scenarios()->scenario(10).squads[0], 2), 0);
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.deployments[0], 4), 160);
        QCOMPARE(project.scenarios()->scenario(22).json(),
                 project.scenarios()->original(22).json());
        undo.undo();
        QCOMPARE(project.scenarios()->json(), before);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), after);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), 10);
        // Five independent class/level rows, including followers formerly sharing a cohort.
        spin("scenarioMemberCount")->setValue(5);
        for (int i=0;i<5;++i) {
            select(QString("scenarioClass_%1").arg(i),5+i);
            editor.findChild<QSpinBox *>(QString("scenarioLevel_%1").arg(i))->setValue(i);
        }
        button("scenarioSquadApply")->click();
        const auto five=project.scenarios()->scenario(10).squads[0];
        QVERIFY(independentSquad(five));
        for(int i=0;i<5;++i) QCOMPARE(squadMembers(five)[i].classId,5+i);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/scenario-independent-squad.png"));
        select("scenarioClass_2",10);
        button("scenarioSquadApply")->click();
        const auto independent=project.scenarios()->scenario(10).squads[0];
        QCOMPARE(squadMembers(independent)[2].classId,10);
        QCOMPARE(squadMembers(independent)[1].classId,6);
        QCOMPARE(squadMembers(independent)[3].classId,8);
        undo.undo(); QCOMPARE(project.scenarios()->scenario(10).squads[0],five);
        undo.undo(); QCOMPARE(project.scenarios()->json(),after);
        spin("scenarioLevel_0")->setValue(127);
        button("scenarioSquadApply")->click();
        QCOMPARE(project.scenarios()->json(), after);
        QVERIFY(editor.findChild<QLabel *>("scenarioEditNotice")->text().contains("level 1–99"));
        spin("scenarioLevel_0")->setValue(0);
        button("scenarioAddSquad")->click();
        QCOMPARE(project.scenarios()->scenario(10).squads.size(),
                 project.scenarios()->original(10).squads.size() + 1);
        QVERIFY(button("scenarioRemoveSquad")->isEnabled());
        button("scenarioRemoveSquad")->click();
        QCOMPARE(project.scenarios()->json(), after);
        undo.undo();
        auto squads = editor.findChild<QListWidget *>("scenarioSquads");
        squads->setCurrentRow(squads->count() - 1);
        QVERIFY(button("scenarioRemoveSquad")->isEnabled());
        const auto beforeNew = project.scenarios()->json();
        button("scenarioNewSquad")->click();
        QCOMPARE(squadMembers(project.scenarios()->scenario(10).squads.last()).size(), 1);
        QCOMPARE(squadMembers(project.scenarios()->scenario(10).squads.last())[0].classId, 5);
        undo.undo(); QCOMPARE(project.scenarios()->json(), beforeNew);
        squads->setCurrentRow(0);
        button("scenarioRestoreSquad")->click();
        QCOMPARE(project.scenarios()->scenario(10).squads[0], project.scenarios()->original(10).squads[0]);
        undo.undo(); QCOMPARE(project.scenarios()->json(), beforeNew);
        squads->setCurrentRow(squads->count() - 1);
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-squads.png"));
        auto search = editor.findChild<QLineEdit *>("scenarioSearch");
        search->setText("54");
        QCOMPARE(list->currentRow(), -1); // The unsupported Dev Key stays visible but unselectable.
        QVERIFY(!list->item(list->count() - 4)->isHidden());
        search->setText("no matching mission");
        QCOMPARE(list->currentRow(), -1);
        search->clear();
        QVERIFY(list->currentRow() >= 0);
    }
    void customSquadEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QCOMPARE(project.appendClass(5, "New squad class"), 165);
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1400, 850);
        editor.show();
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("customSquadsPage"));
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto combo = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = combo(name);
            QVERIFY(box);
            const auto at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        button("customNew")->click();
        // Adding after moving a member into a formerly default cell must use free cells.
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(0, 0);
        button("customCell_4")->click();
        spin("customMemberCount")->setValue(5);
        button("customApply")->click();
        const auto five = project.encounters()->customSquads().profiles().first();
        QCOMPARE(five.members.size(), 5);
        QSet<int> occupied;
        for (const auto &member : five.members) occupied.insert(member.cell);
        QCOMPARE(occupied.size(), 5);
        QCOMPARE(five.members[0].cell, 4);
        undo.undo();
        button("customNew")->click();
        select("customScenario", 33);
        select("customTerrain", 9);
        editor.findChild<QLineEdit *>("customLabel")->setText("Raiding party");
        spin("customMemberCount")->setValue(3);
        select("customClass_0", 165);
        select("customClass_1", 71);
        spin("customLevel_1")->setValue(-10);
        QVERIFY(combo("customEquipment_0_0")->findData(2) >= 0); // Sword fits copied Knight.
        QVERIFY(combo("customEquipment_0_0")->findData(0x71) < 0); // Mace does not.
        select("customEquipment_0_0", 2);
        select("customEquipment_0_1", 1);
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(2, 0);
        button("customCell_0")->click();
        auto cell = button("customCell_0");
        auto board = editor.findChild<FormationBoard *>("customCell_Board");
        QVERIFY(board);
        // Known tile centers on the ROM board, including its opposite corners.
        QCOMPARE(board->cellAt(QPoint(228, 132)), 0);
        QCOMPARE(board->cellAt(QPoint(156, 284)), 8);
        QCOMPARE(board->cellAt(board->cellPolygon(0).boundingRect().topLeft().toPoint()), -1);
        QCOMPARE(board->cellAt(QPoint(410, 10)), -1);
        for (int i = 0; i < 9; ++i) {
            QCOMPARE(board->cellAt(board->cellCenter(i)), i);
            QCOMPARE(board->childAt(board->cellCenter(i)), board->cell(i));
        }
        QVERIFY(!cell->icon().isNull());
        QVERIFY(button("customCell_8")->mapToGlobal(QPoint(0, 0)).y() > cell->mapToGlobal(QPoint(0, 0)).y());
        QCOMPARE(editor.findChild<QTableWidget *>("customMembers")->columnCount(), 4);
        spin("customChance")->setValue(70);
        spin("customRetreat")->setValue(12);
        select("customBonusClass_0", 165);
        spin("customBonus_0")->setValue(25);
        select("customReward_0", 44);
        select("customReward_1", 0x8115);
        spin("customWeight_0")->setValue(3);
        spin("customWeight_1")->setValue(1);
        spin("customWeight_2")->setValue(2);
        button("customApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        const auto saved = project.encounters()->customSquads().profiles()[CustomSquads::key(33, 9)];
        QCOMPARE(saved.members.size(), 3);
        QCOMPARE(saved.members[0].classId, 165);
        QCOMPARE(saved.members[0].equipment[0], 2);
        QCOMPARE(saved.members[1].levelOffset, -10);
        QCOMPARE(saved.members[2].cell, 0);
        QCOMPARE(saved.chance, 70);
        QCOMPARE(saved.retreatHp, 12);
        QCOMPARE(saved.bonuses[0].second, 25);
        QCOMPARE(saved.rewards.outcomes[0], 44);
        const auto before = project.manifest();
        auto drag = [&](int from, int to) {
            const auto start = board->cellCenter(from);
            const auto end = to < 0 ? QPoint(410, 10) : board->cellCenter(to);
            const auto beforeDrag = board->grab().toImage();
            QTest::mousePress(board, Qt::LeftButton, Qt::NoModifier, start);
            QMouseEvent movement(QEvent::MouseMove, end, board->mapToGlobal(end),
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(board, &movement);
            QVERIFY(board->grab().toImage() != beforeDrag);
            if (from == 0 && to == 8)
                QVERIFY(board->grab().save(QCoreApplication::applicationDirPath()+"/army-board-drag.png"));
            QTest::mouseRelease(board, Qt::LeftButton, Qt::NoModifier, end);
        };
        drag(0, -1); // Releasing outside the angled board must leave the member in place.
        QVERIFY(button("customCell_0")->toolTip().startsWith("3 ·"));
        drag(0, 8); // Swap two ordinary members.
        QVERIFY(button("customCell_8")->toolTip().startsWith("3 ·"));
        drag(8, 0);
        drag(0, 4); // Adjacent to a large member: refuse without altering the draft.
        QVERIFY(editor.findChild<QLabel *>("customFormationNotice")->text().contains("adjacent"));
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        undo.undo();
        QVERIFY(!project.encounters()->customSquads().hasProfiles());
        undo.redo();
        QCOMPARE(project.manifest(), before);
        combo("customClass_0")->setEditText("No matching class");
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        select("customClass_0", 5);
        select("customClass_0", 165);
        editor.resize(1100, 640);
        QApplication::processEvents();
        // At a small window size the board scrolls intact; its angled targets retain identity.
        for (int i = 0; i < 9; ++i) QCOMPARE(board->cellAt(board->cellCenter(i)), i);
        QVERIFY(board->grab().save(QCoreApplication::applicationDirPath()+"/army-board-custom.png"));
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/custom-squad-members.png"));
        editor.findChild<QTabWidget *>("customDetailsTabs")->setCurrentIndex(1);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/custom-squad-battle.png"));
        button("customCopy")->click();
        QVERIFY(combo("customScenario")->currentData().toInt() != 33 ||
                combo("customTerrain")->currentData().toInt() != 9);
        button("customApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        button("customRemove")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        button("customRestore")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        select("customPersuasion", 0);
        QVERIFY(!spin("customChance")->isEnabled());
        QVERIFY(!combo("customBonusClass_0")->isEnabled());
        auto search = editor.findChild<QLineEdit *>("customSearch");
        search->setText("No matching squad");
        QVERIFY(!button("customApply")->isEnabled());
        QVERIFY(!button("customCopy")->isEnabled());
        search->clear();
        QVERIFY(button("customApply")->isEnabled());
    }
    void scenarioRateEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1350, 760);
        editor.show();
        auto keys = editor.findChild<QListWidget *>("scenarioRateKeys");
        keys->setCurrentRow(65);
        auto normal = editor.findChild<QComboBox *>("scenarioMode_1");
        auto alternate = editor.findChild<QComboBox *>("scenarioMode_0");
        auto divisor = editor.findChild<QDoubleSpinBox *>("scenarioDivisor_1");
        auto pass = editor.findChild<QDoubleSpinBox *>("scenarioPass_1");
        auto apply = editor.findChild<QPushButton *>("scenarioApply");
        const auto original = project.encounters()->json();
        normal->setCurrentIndex(1);
        divisor->setValue(4294967295.0);
        pass->setValue(2147483648.0);
        alternate->setCurrentIndex(2);
        apply->click();
        QCOMPARE(project.encounters()->scenarioRates().branch(65, true)->passCount,
                 quint32(0x80000000));
        QCOMPARE(project.encounters()->scenarioRates().branch(65, false)->passCount, quint32(0));
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        const auto before = project.manifest();
        divisor->setValue(1);
        apply->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(!editor.findChild<QLabel *>("scenarioNotice")->text().isEmpty());
        editor.findChild<QPushButton *>("scenarioRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        auto globalNormal = editor.findChild<QDoubleSpinBox *>("encounterNormalPass");
        globalNormal->setValue(777);
        editor.findChild<QPushButton *>("encounterApplyGlobal")->click();
        QCOMPARE(pass->value(), 777.0);
        QVERIFY(!pass->isEnabled());
        normal->setCurrentIndex(1);
        pass->setValue(100);
        apply->click();
        editor.findChild<QPushButton *>("scenarioInherit")->click();
        QVERIFY(!project.encounters()->scenarioRates().branch(65, true));
        undo.undo();
        QCOMPARE(project.encounters()->scenarioRates().branch(65, true)->passCount, quint32(100));
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("scenarioRatesPage"));
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-rates.png"));
        editor.findChild<QLineEdit *>("scenarioRateSearch")->setText("No matching scenario");
        QVERIFY(!keys->currentItem());
        QVERIFY(!apply->isEnabled());
    }
    void creatureRewardEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        project.appendClass(1, "New reward class");
        auto encounters = *project.encounters();
        encounters.setPair(1, 0, 5, 2, 166);
        CustomSquad custom;
        custom.members[0].classId = 165;
        encounters.setCustomSquad(custom, 166);
        custom.terrainSlot = 1;
        encounters.setCustomSquad(custom, 166);
        project.setEncounters(encounters.json());
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1250, 760);
        editor.show();
        auto classes = editor.findChild<QListWidget *>("rewardClasses");
        QCOMPARE(classes->count(), 167);
        QVERIFY(classes->item(0)->text().contains("New reward class Squad 1"));
        QVERIFY(classes->item(1)->text().contains("New reward class Squad 2"));
        QSet<int> used{165};
        for (int pool = 1; pool <= Encounters::poolCount; ++pool)
            for (int slot = 0; slot < Encounters::slotCount; ++slot)
                for (int side = 0; side < 2; ++side)
                    used.insert(project.encounters()->classAt(pool, slot, side));
        bool unusedSeen = false;
        for (int row = 0; row < classes->count(); ++row) {
            if (classes->item(row)->data(Qt::UserRole).toInt() < 0) continue;
            const bool active = used.contains(classes->item(row)->data(Qt::UserRole).toInt());
            QVERIFY(!active || !unusedSeen);
            unusedSeen |= !active;
        }
        const auto classReward = project.encounters()->rewards().row(165);
        classes->setCurrentRow(1);
        auto squadItem = editor.findChild<QComboBox *>("rewardItem_0");
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto choices = dialog->findChild<QListWidget *>("shopItemChoices");
            QVERIFY(choices);
            for (int i = 0; i < choices->count(); ++i)
                if (choices->item(i)->data(Qt::UserRole).toInt() == 277) {
                    QVERIFY(!choices->item(i)->icon().isNull());
                    choices->setCurrentRow(i); choices->item(i)->setSelected(true);
                }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        squadItem->showPopup();
        editor.findChild<QSpinBox *>("rewardWeight_0")->setValue(100);
        editor.findChild<QPushButton *>("rewardApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 1)).rewards.outcomes[0], 0x8115);
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 0)).rewards, CreatureReward{});
        QCOMPARE(project.encounters()->rewards().row(165), classReward);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 1)).rewards, CreatureReward{});
        editor.findChild<QLineEdit *>("rewardSearch")->setText("New reward class");
        for (int row = 0; row < classes->count(); ++row)
            if (classes->item(row)->data(Qt::UserRole).toInt() == 165) classes->setCurrentRow(row);
        QCOMPARE(classes->currentItem()->data(Qt::UserRole).toInt(), 165);
        auto item = editor.findChild<QComboBox *>("rewardItem_0");
        auto first = editor.findChild<QSpinBox *>("rewardWeight_0");
        auto second = editor.findChild<QSpinBox *>("rewardWeight_1");
        auto third = editor.findChild<QSpinBox *>("rewardWeight_2");
        auto apply = editor.findChild<QPushButton *>("rewardApply");
        const auto original = project.encounters()->json();
        item->setCurrentIndex(item->findData(0x8115));
        first->setValue(65535);
        second->setValue(0);
        third->setValue(1);
        apply->click();
        QCOMPARE(project.encounters()->rewards().row(165).outcomes[0], 0x8115);
        QCOMPARE(project.encounters()->rewards().row(165).weights[0], 65535);
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        QCOMPARE(first->value(), 65535);
        const auto before = project.manifest();
        first->setValue(0);
        third->setValue(0);
        apply->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(!editor.findChild<QLabel *>("rewardNotice")->text().isEmpty());
        editor.findChild<QPushButton *>("rewardRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        auto copy = editor.findChild<QComboBox *>("rewardCopySource");
        copy->setCurrentIndex(copy->findData(0x30));
        editor.findChild<QPushButton *>("rewardCopy")->click();
        QCOMPARE(project.encounters()->rewards().row(165).json(),
                 project.encounters()->rewards().row(0x30).json());
        editor.findChild<QPushButton *>("rewardClear")->click();
        QCOMPARE(project.encounters()->rewards().row(165).json(), CreatureReward{}.json());
        undo.undo();
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("creatureRewardsPage"));
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/creature-rewards.png"));
        editor.findChild<QLineEdit *>("rewardSearch")->setText("No class with this name");
        QVERIFY(!classes->currentItem());
        QVERIFY(!apply->isEnabled());
        editor.findChild<QLineEdit *>("rewardSearch")->clear();
        QVERIFY(classes->currentItem());
        QVERIFY(apply->isEnabled());
    }
    void encounterGlobalRateEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1200, 750);
        editor.show();
        auto divisor = editor.findChild<QDoubleSpinBox *>("encounterDivisor"),
             normal = editor.findChild<QDoubleSpinBox *>("encounterNormalPass"),
             alternate = editor.findChild<QDoubleSpinBox *>("encounterAlternatePass");
        auto apply = editor.findChild<QPushButton *>("encounterApplyGlobal");
        QCOMPARE(divisor->value(), 72000.0);
        QCOMPARE(normal->value(), 51.0);
        QCOMPARE(alternate->value(), 11.0);
        auto waits = editor.findChild<QTableWidget *>("encounterWaitEstimates");
        auto squads = editor.findChild<QSpinBox *>("encounterDeployed");
        const double single = waits->item(3, 1)->data(Qt::UserRole).toDouble();
        QVERIFY(single > waits->item(3, 2)->data(Qt::UserRole).toDouble());
        const double p = EncounterRate::passingFraction(100, 15);
        squads->setValue(10);
        const double ten = waits->item(3, 1)->data(Qt::UserRole).toDouble();
        QVERIFY(qAbs(ten - single * p / (1 - std::pow(1 - p, 10))) < 0.001);
        squads->setValue(0); QCOMPARE(waits->item(3, 1)->text(), QString("Never"));
        squads->setValue(10);
        const auto original = project.encounters()->json();
        divisor->setValue(1000000);
        normal->setValue(0);
        alternate->setValue(1000000);
        apply->click();
        QCOMPARE(project.encounters()->globalRate().normalPassCount(), quint32(0));
        QCOMPARE(project.encounters()->globalRate().alternatePassCount(), quint32(1000000));
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        QCOMPARE(normal->value(), 51.0);
        undo.redo();
        auto linked = editor.findChild<QCheckBox *>("encounterLinkRates");
        linked->setChecked(true);
        normal->setValue(700);
        QCOMPARE(alternate->value(), 700.0);
        QVERIFY(!alternate->isEnabled());
        apply->click();
        QCOMPARE(project.encounters()->globalRate().alternatePassCount(), quint32(700));
        const auto before = project.manifest();
        divisor->setValue(1);
        apply->click();
        QCOMPARE(project.manifest(), before);
        editor.findChild<QPushButton *>("encounterRestoreGlobal")->click();
        QCOMPARE(project.encounters()->json(), original);
        QVERIFY(!linked->isChecked());
        undo.undo();
        QCOMPARE(project.encounters()->globalRate().normalPassCount(), quint32(700));
        auto tabs = editor.findChild<QTabWidget *>();
        tabs->setCurrentIndex(2);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/global-rates.png"));
    }
    void encounterEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        project.appendClass(1, "Encounter tester");
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1350, 750);
        editor.show();
        auto list = editor.findChild<QListWidget *>("encounterPools");
        QCOMPARE(list->count(), 45);
        auto mapPairs = editor.findChild<QTableWidget *>("encounterPairs");
        auto mapTabs = editor.findChild<QTabWidget *>("encounterTabs");
        auto assignments = static_cast<TerrainAssignments *>(editor.findChild<QTableWidget *>("terrainAssignments"));
        auto tiles = editor.findChild<QListWidget *>("encounterPoolTiles");
        QCOMPARE(mapPairs->columnCount(), 4);
        QCOMPARE(mapTabs->tabText(0), QString("Map encounters"));
        const auto beforeSharing = project.encounters()->json();
        for (int row = 0; row < list->count(); ++row)
            if (project.encounters()->scenarioKeys(list->item(row)->data(Qt::UserRole).toInt()).contains(14))
                list->setCurrentRow(row);
        const int map = list->currentItem()->data(Qt::UserRole).toInt();
        QCOMPARE(mapPairs->item(2, 0)->text().section('\n', 0, 0), QString("Barrens"));
        const auto barrens = QString(project.record(Table::Classes, project.encounters()->classAt(map, 2, 0)).name).replace('\n', ' ');
        QVERIFY(tiles->item(2)->text().contains(barrens));
        QVERIFY(editor.findChild<QLabel *>("terrainMapContext")->text().contains("Sable Lowlands"));
        assignments->assign(-1, 3, 1); // Highway and Plain now share the same editable pair.
        assignments->assign(-1, 2, 1); // A zero-rate Bridge can share it without enabling encounters.
        QVERIFY(mapPairs->item(0, 0)->text().contains("Highway / Bridge / Plain"));
        auto sharedChance = editor.findChild<QLabel *>("encounterTerrainChance_0");
        QVERIFY(sharedChance->text().contains("Highway: 10%"));
        QVERIFY(sharedChance->text().contains("Bridge: 0%"));
        QVERIFY(sharedChance->text().contains("Plain: 15%"));
        editor.findChild<QPushButton *>("encounterTerrainRules_0")->click();
        QCOMPARE(mapTabs->currentIndex(), 1);
        QCOMPARE(assignments->currentRow(), 1);
        QCOMPARE(tiles->currentItem()->data(Qt::UserRole).toInt(), 1);
        auto highwayRate = editor.findChild<QSpinBox *>("encounterRate_1");
        highwayRate->setValue(0);
        auto plainRate = editor.findChild<QSpinBox *>("encounterRate_3");
        plainRate->setValue(0);
        QVERIFY(mapPairs->item(0, 0)->text().contains("Terrain chance is zero"));
        undo.undo();
        QVERIFY(!mapPairs->item(0, 0)->text().contains("Terrain chance is zero"));
        undo.undo();
        auto sharedCreature = editor.findChild<QComboBox *>("encounterClass_0_0");
        sharedCreature->setCurrentIndex(sharedCreature->findData(165));
        QVERIFY(tiles->item(0)->text().contains("Encounter tester"));
        for (int t : {1, 2, 3}) QVERIFY(assignments->item(t, 2)->text().contains("Encounter tester"));
        undo.undo(); undo.undo(); undo.undo();
        QCOMPARE(project.encounters()->json(), beforeSharing);
        auto emptyFirst = editor.findChild<QComboBox *>("encounterClass_0_0");
        auto emptySecond = editor.findChild<QComboBox *>("encounterClass_0_1");
        emptyFirst->setCurrentIndex(emptyFirst->findData(0));
        emptySecond->setCurrentIndex(emptySecond->findData(0));
        QVERIFY(mapPairs->item(0, 0)->text().contains("No creatures assigned"));
        assignments->assign(-1, 3, 1);
        assignments->assign(-1, 8, 2);
        editor.findChild<QPushButton *>("encounterBackToMap")->click();
        QCOMPARE(mapTabs->currentIndex(), 0);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/map-encounters.png"));
        mapTabs->setCurrentIndex(1);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/terrain-rules.png"));
        while (undo.index()) undo.undo();
        QCOMPARE(project.encounters()->json(), beforeSharing);
        list->setCurrentRow(44);
        const auto original = project.encounters()->json();
        auto first = editor.findChild<QComboBox *>("encounterClass_9_0");
        first->setCurrentIndex(first->findData(165));
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        QCOMPARE(first->currentData().toInt(), 165);
        editor.findChild<QComboBox *>("encounterCopySource")->setCurrentIndex(0);
        editor.findChild<QPushButton *>("encounterCopy")->click();
        for (int s = 0; s < 10; ++s)
            for (int side = 0; side < 2; ++side)
                QCOMPARE(project.encounters()->classAt(45, s, side),
                         project.encounters()->classAt(1, s, side));
        undo.undo();
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        editor.findChild<QPushButton *>("encounterRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        undo.undo();
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        auto table = static_cast<TerrainAssignments *>(editor.findChild<QTableWidget *>("terrainAssignments"));
        auto rate = editor.findChild<QSpinBox *>("encounterRate_31");
        QVERIFY(!rate->isEnabled());
        table->assign(-1, 31, 10);
        QVERIFY(rate->isEnabled()); rate->setValue(100);
        QCOMPARE(project.encounters()->rate(31), 100);
        table->assign(31, 2, 10);
        QCOMPARE(project.encounters()->lookup(2), 10);
        QCOMPARE(project.encounters()->lookup(31), 0);
        QCOMPARE(project.encounters()->rate(31), 0);
        undo.undo(); QCOMPARE(rate->value(), 100);
        table->setCurrentCell(24, 2);
        editor.findChild<QPushButton *>("encounterAddPool")->click();
        QCOMPARE(project.encounters()->slotsPerPool(), 11);
        QCOMPARE(project.encounters()->lookup(24), 11);
        auto pairs = editor.findChild<QTableWidget *>("encounterPairs");
        QVERIFY(pairs->item(10, 0)->text().contains("Malefic Woods"));
        table->assign(24, 3, 11); // Swap, preserving both pools.
        QCOMPARE(project.encounters()->lookup(3), 11);
        QCOMPARE(project.encounters()->lookup(24), 2);
        undo.undo(); undo.undo();
        QCOMPARE(project.encounters()->slotsPerPool(), 10);
        undo.redo(); QCOMPARE(pairs->rowCount(), 11);
        editor.findChild<QTabWidget *>()->setCurrentIndex(1);
        QApplication::processEvents();
        table->scrollToItem(table->item(2, 2));
        const QPoint target = table->visualItemRect(table->item(2, 2)).center();
        QMimeData mime; mime.setData(poolMime, "11:24");
        QDragEnterEvent enter(QPointF(target), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &enter); QVERIFY(enter.isAccepted());
        QDragMoveEvent move(QPointF(target), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &move);
        QDropEvent drop(target, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &drop);
        QCOMPARE(project.encounters()->lookup(2), 11);
        QVERIFY(pairs->item(10, 0)->text().contains("Bridge"));
        editor.findChild<QLineEdit *>("encounterSearch")->setText("Encounter tester");
        QVERIFY(!list->item(44)->isHidden());
        QApplication::processEvents();
        editor.grab().save(QCoreApplication::applicationDirPath() + "/encounters.png");
    }
    void consumableEffectEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        project.loadConsumableEffects();
        QUndoStack undo;
        ConsumableEditor editor(&project, &undo, [] {});
        editor.resize(410, 550);
        editor.show();
        editor.selectConsumable(3);
        const auto before = project.manifest();
        auto minimum = editor.findChild<QSpinBox *>("effectMinimum");
        auto maximum = editor.findChild<QSpinBox *>("effectMaximum");
        auto apply = editor.findChild<QPushButton *>("effectApply");
        minimum->setValue(333);
        apply->click();
        QCOMPARE(project.consumableEffects()->models()["healPack"].toObject()["magnitude"].toInt(),
                 333);
        QVERIFY(project.textBanks()["consumables"].text(3).contains("333HP"));
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(minimum->value(), 150);
        undo.redo();
        QCOMPARE(minimum->value(), 333);
        editor.selectConsumable(11);
        minimum->setValue(7);
        maximum->setValue(9);
        apply->click();
        editor.selectConsumable(16);
        QCOMPARE(minimum->value(), 7);
        QCOMPARE(maximum->value(), 9);
        const auto count = undo.count();
        minimum->setValue(12);
        maximum->setValue(10);
        apply->click();
        QCOMPARE(undo.count(), count);
        QCOMPARE(project.consumableEffects()
                     ->models()["sharedStatBoosters11To16"]
                     .toObject()["minimum"]
                     .toInt(),
                 7);
        editor.selectConsumable(44);
        QVERIFY(!apply->isEnabled());
        editor.selectConsumable(3);
        editor.findChild<QPushButton *>("effectRestore")->click();
        QCOMPARE(minimum->value(), 150);
        undo.undo();
        QCOMPARE(minimum->value(), 333);
        editor.grab().save(QCoreApplication::applicationDirPath() + "/consumable-effects.png");
    }
    void shopEditingAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        QFile catalog(":/catalog.json");
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        auto project =
            Project::fromRom(Rom::open(path), QJsonDocument::fromJson(catalog.readAll()).object());
        project.loadShops();
        const auto original = project.shops()->json();
        QUndoStack undo;
        ShopEditor editor(&project, &undo, [] {});
        editor.resize(1250, 740);
        editor.show();
        auto shops = editor.findChild<QListWidget *>("shopList");
        QCOMPARE(shops->count(), 35);
        QTest::qWait(20);
        shops->scrollToBottom();
        const int bottom = shops->verticalScrollBar()->value();
        shops->setCurrentRow(shops->count() - 2);
        QCOMPARE(shops->verticalScrollBar()->value(), bottom);
        auto townChoice = editor.findChild<QComboBox *>("shopStronghold");
        QSet<int> towns;
        for (int i = 0; i < townChoice->count(); ++i) towns.insert(townChoice->itemData(i).toInt());
        for (int id = 1; id <= ScenarioWorld::townCount; ++id) QVERIFY(towns.contains(id));
        shops->setCurrentRow(1);
        const int assignedTown = townChoice->currentData().toInt();
        QVERIFY(assignedTown > 0);
        QCOMPARE(u8(project.world()->town(assignedTown), 27), quint8(2));
        auto pick = [&](bool equipment, int id, bool accept = true) {
            QTimer::singleShot(0, &editor, [&, equipment, id, accept] {
                auto dialog = editor.findChild<QDialog *>("shopItemPicker");
                QVERIFY(dialog);
                auto list = dialog->findChild<QListWidget *>("shopItemChoices");
                QListWidgetItem *target = nullptr;
                for (int row = 0; row < list->count(); ++row)
                    if (list->item(row)->data(Qt::UserRole).toInt() == id) target = list->item(row);
                QVERIFY(target);
                if (equipment) {
                    const int type = u8(project.record(Table::Items, id).bytes, 4);
                    auto filter = dialog->findChild<QPushButton *>(QString("shopItemType_%1").arg(type));
                    QVERIFY(filter && !filter->icon().isNull());
                    filter->click();
                    QVERIFY(!target->isHidden());
                    for (int at = 0; at < list->count(); ++at)
                        QCOMPARE(!list->item(at)->isHidden(), list->item(at)->data(Qt::UserRole + 1).toInt() == type);
                }
                const auto image = project.iconArtwork()->image(id | (equipment ? 0 : 0x200))
                                       .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation);
                QCOMPARE(target->icon().pixmap(48, 48).toImage().convertToFormat(QImage::Format_RGBA8888),
                         image.convertToFormat(QImage::Format_RGBA8888));
                target->setSelected(true);
                if (equipment && !accept && !qEnvironmentVariable("LORDLY_SHOP_PICKER_SCREENSHOT").isEmpty()) {
                    QTest::qWait(20);
                    QVERIFY(dialog->grab().save(qEnvironmentVariable("LORDLY_SHOP_PICKER_SCREENSHOT")));
                }
                dialog->findChild<QDialogButtonBox *>()->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
            });
            editor.findChild<QPushButton *>(equipment ? "shopEquipmentAdd" : "shopConsumablesAdd")->click();
        };
        pick(true, 277, false);
        QCOMPARE(project.shops()->json(), original);
        pick(true, 277);
        QCOMPARE(project.shops()->stock(2).equipment.last(), quint16(277));
        auto inventory = editor.findChild<QListWidget *>("shopEquipmentList");
        QCOMPARE(inventory->currentItem()->data(Qt::UserRole).toInt(), 277);
        const auto added = project.shops()->json();
        editor.findChild<QPushButton *>("shopEquipmentUp")->click();
        QCOMPARE(
            project.shops()->stock(2).equipment[project.shops()->stock(2).equipment.size() - 2],
            quint16(277));
        undo.undo();
        QCOMPARE(project.shops()->json(), added);
        editor.findChild<QPushButton *>("shopEquipmentRemove")->click();
        QCOMPARE(project.shops()->json(), original);
        undo.undo();
        QCOMPARE(project.shops()->json(), added);
        pick(false, 44);
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        auto search = editor.findChild<QLineEdit *>("shopSearch");
        search->setText("Jadd");
        QVERIFY(!shops->item(1)->isHidden());
        QVERIFY(shops->item(34)->isHidden());
        search->clear();
        auto from = editor.findChild<QComboBox *>("shopCopySource");
        from->setCurrentIndex(from->findData(3));
        editor.findChild<QPushButton *>("shopCopy")->click();
        QCOMPARE(project.shops()->stock(2), project.shops()->stock(3));
        undo.undo();
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        QVERIFY(editor.findChild<QPushButton *>("shopEquipmentDown")->isEnabled());
        editor.findChild<QPushButton *>("shopRestore")->click();
        QCOMPARE(project.shops()->json(), original);
        undo.undo();
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        const auto beforeNew = project.manifest();
        editor.findChild<QPushButton *>("shopAdd")->click();
        QCOMPARE(project.shops()->size(), 37);
        QCOMPARE(shops->currentItem()->data(Qt::UserRole).toInt(), 36);
        auto town = editor.findChild<QComboBox *>("shopStronghold");
        int townId = 1;
        while (u8(project.world()->town(townId), 24) == 255) ++townId;
        town->setCurrentIndex(town->findData(townId));
        editor.findChild<QPushButton *>("shopAssign")->click();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(36));
        QVERIFY(u8(project.world()->town(townId), 25) & 1);
        editor.findChild<QPushButton *>("shopDetach")->click();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(0));
        undo.undo();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(36));
        undo.undo();
        undo.undo();
        QCOMPARE(project.manifest(), beforeNew);
        QTest::qWait(50);
        if (!qEnvironmentVariable("LORDLY_SHOP_SCREENSHOT").isEmpty())
            QVERIFY(editor.grab().save(qEnvironmentVariable("LORDLY_SHOP_SCREENSHOT")));
    }
    void consumableFieldsUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        const auto original = project.record(Table::Consumables, 1).bytes;
        QUndoStack undo;
        RecordModel model(&project, Table::Consumables, &undo);
        QCOMPARE(model.rowCount(), 45);
        QVERIFY(model.setData(model.index(1, 2), 65535, Qt::EditRole));
        auto expected = original;
        put16(expected, 6, 65535);
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        undo.undo();
        QCOMPARE(project.record(Table::Consumables, 1).bytes, original);
        undo.redo();
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        QVERIFY(!model.setData(model.index(1, 2), 65536, Qt::EditRole));
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        QVERIFY(!(model.flags(model.index(1, 1)) & Qt::ItemIsEditable));
    }
    void classNamesAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        const auto original = project.classNameBytes(164);
        const Field choiceField{"class", "Class", "", 0, 1, false, "CLASS_NAMES"};
        ValueEditor choice(choiceField, project.catalog(), [](int) {});
        connect(&model, &RecordModel::edited, &choice,
                [&] { choice.setChoices(project.catalog()); });
        QTableView view;
        view.setModel(&model);
        view.setItemDelegateForColumn(1, new ClassNameDelegate(&view));
        view.resize(600, 400);
        view.show();
        const auto index = model.index(164, 1);
        view.scrollTo(index);
        view.edit(index);
        QTRY_VERIFY(view.findChild<QPlainTextEdit *>());
        auto editor = view.findChild<QPlainTextEdit *>();
        QCOMPARE(editor->toPlainText(), QString("Death\nBahamut"));
        editor->setPlainText("Ancient\nBahamut");
        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
        QTRY_COMPARE(project.record(Table::Classes, 164).name, QString("Ancient\nBahamut"));
        auto combo = choice.findChild<QComboBox *>();
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Ancient\nBahamut"));
        undo.undo();
        QCOMPARE(project.classNameBytes(164), original);
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Death\nBahamut"));
        undo.redo();
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        QVERIFY(!model.setData(index, "", Qt::EditRole));
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
    }
    void addItemUndoRedo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        RecordModel model(&project, Table::Items, &undo);
        const auto before = project.manifest();
        QCOMPARE(model.appendItem(1, "Fresh blade"), 278);
        QCOMPARE(model.rowCount(), 279);
        const auto added = project.manifest();
        QVERIFY(model.setData(model.index(278, 1), "Named blade", Qt::EditRole));
        undo.undo();
        QCOMPARE(project.manifest(), added);
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(model.rowCount(), 278);
        undo.redo();
        QCOMPARE(project.manifest(), added);
        undo.redo();
        QCOMPARE(project.record(Table::Items, 278).name, QString("Named blade"));
        const auto current = project.manifest();
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        QCOMPARE(model.appendItem(0, "Invalid"), -1);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.manifest(), current);
        QFile save(QFileInfo(path).dir().filePath("tools/pj64_golden.sra"));
        QVERIFY(save.open(QIODevice::ReadOnly));
        SaveEditor saveEditor(&project);
        saveEditor.loadBytes(save.readAll());
        const int slot = saveEditor.document()->rows(SaveTable::Equipment).last();
        QCOMPARE(saveEditor.document()->value(SaveTable::Equipment, slot, "id").toInt(), 0);
        QVERIFY(saveEditor.edit(SaveTable::Equipment, slot, "id", 278));
        QVERIFY(saveEditor.edit(SaveTable::Equipment, slot, "owned", 3));
        QTemporaryDir output;
        saveEditor.exportFile(output.filePath("new-item.sra"));
        QFile saved(output.filePath("new-item.sra"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto readback = SaveDocument::open(project.rom(), saved.readAll());
        QCOMPARE(readback.value(SaveTable::Equipment, slot, "id").toInt(), 278);
        undo.undo();
        undo.undo();
        QCOMPARE(model.rowCount(), 278);
        QVERIFY_THROWS_EXCEPTION(Error, saveEditor.exportFile(output.filePath("missing-item.sra")));
        const auto beforeBlank = project.manifest();
        QCOMPARE(model.createItem("Empty accessory"), 278);
        auto expected = QByteArray(32, 0);
        put8(expected, 4, 25);
        QCOMPARE(project.record(Table::Items, 278).bytes, expected);
        const auto blankIcon = project.iconArtwork()->image(278);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) QCOMPARE(blankIcon.pixelColor(x, y).alpha(), 0);
        QCOMPARE(project.textBanks()["items"].entries[278].bytes, QByteArray());
        const auto blank = project.manifest();
        undo.undo();
        QCOMPARE(project.manifest(), beforeBlank);
        undo.redo();
        QCOMPARE(project.manifest(), blank);
    }
    void descriptionWorkspaceEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        const auto before = project.manifest();
        const auto action55 = project.textBanks()["actions"].entries[55].bytes;
        QVERIFY(!action55.isEmpty());
        QVERIFY(project.textBanks()["actions"].entries[56].bytes.isEmpty());
        QUndoStack undo;
        auto page = std::unique_ptr<QWidget>(createTextWorkspace(&project, &undo, [] {}));
        auto banks = page->findChild<QComboBox *>("descriptionBank");
        auto entries = page->findChild<QListWidget *>("descriptionEntries");
        auto editor = page->findChild<QPlainTextEdit *>("descriptionEditor");
        auto empty = page->findChild<QCheckBox *>("descriptionShowEmpty");
        auto search = page->findChild<QLineEdit *>("descriptionSearch");
        auto counts = page->findChild<QLabel *>("descriptionCounts");
        QCOMPARE(banks->currentData().toString(), QString("actions"));
        QCOMPARE(counts->text(), QString("102 with text · 57 empty entries"));
        QVERIFY(!entries->item(55)->isHidden());
        QVERIFY(entries->item(56)->isHidden());
        entries->setCurrentRow(55);
        QVERIFY(!editor->toPlainText().isEmpty());
        empty->setChecked(true);
        search->setText("Empty");
        entries->setCurrentRow(56);
        QVERIFY(editor->toPlainText().isEmpty());
        QVERIFY(editor->placeholderText().contains("No description text"));
        QVERIFY(entries->item(56)->text().contains("Empty"));
        QCOMPARE(project.manifest(), before); // Displaying empty slots never authors replacement text.
        editor->setPlainText("Authored action description.");
        QCOMPARE(project.textBanks()["actions"].text(56), QString("Authored action description."));
        QCOMPARE(entries->currentRow(), 56);
        QCOMPARE(editor->toPlainText(), QString("Authored action description."));
        QVERIFY(!entries->item(56)->isHidden()); // Keep typing even when the label no longer matches.
        QCOMPARE(project.textBanks()["actions"].entries[55].bytes, action55);
        QVERIFY(!entries->item(56)->text().contains("Empty"));
        QCOMPARE(counts->text(), QString("103 with text · 56 empty entries"));
        search->clear();
        empty->setChecked(false);
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(entries->currentRow(), 56);
        QVERIFY(editor->toPlainText().isEmpty());
        QVERIFY(!entries->item(56)->isHidden()); // Retain the edit target across Undo.
        undo.redo();
        QCOMPARE(project.textBanks()["actions"].text(56), QString("Authored action description."));
        search->setText("nothing matches this description");
        QCOMPARE(entries->currentRow(), -1);
        QVERIFY(!editor->isEnabled());
        QVERIFY(editor->toPlainText().isEmpty());
        search->clear();
        QVERIFY(editor->isEnabled());
        banks->setCurrentIndex(banks->findData("classes"));
        QCOMPARE(counts->text(), QString("188 with text · 37 empty entries"));
        QVERIFY(entries->item(165)->isHidden());
        QVERIFY(entries->item(165)->text().contains("Reserved slot"));
        QVERIFY(entries->item(200)->text().contains("Class-change help 2800"));
        RecordModel model(&project, Table::Classes, &undo);
        QCOMPARE(model.createClass("Blank hero"), 165);
        empty->setChecked(true);
        entries->setCurrentRow(225);
        QVERIFY(entries->item(225)->text().contains("Class 165 · Blank hero"));
        QVERIFY(editor->toPlainText().isEmpty());
        banks->setCurrentIndex(banks->findData("consumables"));
        QVERIFY(entries->item(1)->text().contains(project.record(Table::Consumables, 1).name));
    }
    void addClassUndoRedo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        project.loadPromotion();
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        const auto before = project.manifest();
        auto descriptions = std::unique_ptr<QWidget>(createTextWorkspace(&project, &undo, [] {}));
        descriptions->findChild<QComboBox *>()->setCurrentIndex(1); // classes follows actions
        QCOMPARE(model.appendClass(5, "New fighter"), 165);
        QCOMPARE(model.rowCount(), 166);
        const auto added = project.manifest();
        QVERIFY(model.setData(model.index(165, 1), "Renamed fighter", Qt::EditRole));
        undo.undo();
        QCOMPARE(project.manifest(), added);
        undo.undo();
        QCOMPARE(model.rowCount(), 165);
        QCOMPARE(project.manifest(), before);
        QCOMPARE(project.textBanks()["classes"].entries.size(), 225);
        QCOMPARE(project.promotion()->classCount(), 81);
        undo.redo();
        QCOMPARE(model.rowCount(), 166);
        QCOMPARE(project.manifest(), added);
        QCOMPARE(project.textBanks()["classes"].entries.size(), 226);
        undo.redo();
        QCOMPARE(project.record(Table::Classes, 165).name, QString("Renamed fighter"));
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        const auto current = project.manifest();
        QCOMPARE(model.appendClass(0, "Invalid"), -1);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.manifest(), current);
        // Fresh gameplay settings must not inherit even a modified starter's data.
        project.setText("classes", 5, "Do not copy this description");
        auto promotion = *project.promotion();
        promotion.setGate(5, 0, 123);
        promotion.setMask(5, 0x5a);
        project.setPromotion(promotion.json());
        auto record = project.record(Table::Classes, 5).bytes;
        put16(record, 8, 999);
        project.replaceBytes(Table::Classes, 5, record);
        const auto beforeFresh = project.manifest();
        QCOMPARE(model.createClass("Fresh class"), 166);
        const auto &fresh = project.record(Table::Classes, 166).bytes;
        QCOMPARE(u16(fresh, 8), 1U);
        QCOMPARE(fresh.mid(46, 15), QByteArray(15, 0)); // No equipment or actions.
        QCOMPARE(u8(fresh, 65), quint8(166)); // Own growth; no Fighter gameplay dependency.
        QVERIFY(project.textBanks()["classes"].entries[226].bytes.isEmpty());
        QCOMPARE(project.promotion()->gates.mid(166 * 8, 8), QByteArray::fromHex("0000000000000064"));
        QCOMPARE(u8(project.promotion()->masks, 166), quint8(1));
        QCOMPARE(project.promotion()->families["assignments"].toArray()[166].toInt(), 0);
        const auto freshManifest = project.manifest();
        undo.undo();
        QCOMPARE(project.manifest(), beforeFresh);
        undo.redo();
        QCOMPARE(project.manifest(), freshManifest);
        QCOMPARE(model.createClass(""), -1);
        QCOMPARE(project.manifest(), freshManifest);
    }
    void loadDesktopWorkspace() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        const auto baseline = Project::fromRom(Rom::open(path));
        const int classCount = int(baseline.records(Table::Classes).size());
        const int itemCount = int(baseline.records(Table::Items).size());
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QListWidget *>("encounterPools"), 10000);
        auto classes = window.findChild<QTableView *>("classesTable");
        auto items = window.findChild<QTableView *>("itemsTable");
        auto consumables = window.findChild<QTableView *>("consumablesTable");
        QVERIFY(classes);
        QVERIFY(items);
        QVERIFY(consumables);
        QCOMPARE(classes->model()->rowCount(), classCount);
        QCOMPARE(items->model()->rowCount(), itemCount);
        QCOMPARE(consumables->model()->rowCount(), 45);
        QCOMPARE(window.findChild<QListWidget *>("encounterPools")->count(), 45);
        QCOMPARE(window.findChild<QListWidget *>("scenarioList")->count(), 64);
        auto textBaseline = baseline;
        textBaseline.loadTexts();
        auto recordUndo = window.findChild<QUndoStack *>();
        auto editDescriptionFrom = [&](QPushButton *button, const QString &expected,
                                       const QString &text, bool save, bool invalid = false) {
            QVERIFY(button && button->isEnabled());
            QTimer::singleShot(0, &window, [&, expected, text, save, invalid] {
                auto dialog = window.findChild<QDialog *>("recordDescriptionDialog");
                QVERIFY(dialog);
                const auto close = qScopeGuard([dialog] { if (dialog->isVisible()) dialog->reject(); });
                auto draft = dialog->findChild<QPlainTextEdit *>("recordDescriptionText");
                auto buttons = dialog->findChild<QDialogButtonBox *>();
                QCOMPARE(draft->toPlainText(), expected);
                if (invalid) {
                    draft->setPlainText("One\nTwo\nThree\nFour\nFive");
                    QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
                }
                draft->setPlainText(text);
                QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
                buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
            });
            button->click();
        };
        // Follow the selected record's identity through sorted tables, not its visible row.
        for (const auto table : {Table::Classes, Table::Items, Table::Consumables}) {
            auto grid = table == Table::Classes ? classes : table == Table::Items ? items : consumables;
            const int id = table == Table::Classes ? 5 : 1;
            const auto kind = tableKey(table);
            grid->sortByColumn(1, Qt::DescendingOrder);
            for (int row = 0; row < grid->model()->rowCount(); ++row)
                if (grid->model()->index(row, 0).data(Qt::EditRole).toInt() == id)
                    grid->setCurrentIndex(grid->model()->index(row, 1));
            auto panel = window.findChild<QWidget *>(kind + "DescriptionPanel");
            auto preview = panel->findChild<QLabel *>("recordDescriptionPreview");
            auto button = panel->findChild<QPushButton *>("editRecordDescription");
            const auto original = textBaseline.textBanks()[kind].text(id);
            QCOMPARE(preview->text(), original);
            const int index = recordUndo->index();
            editDescriptionFrom(button, original, "Cancelled draft", false);
            QCOMPARE(recordUndo->index(), index);
            QCOMPARE(preview->text(), original);
            editDescriptionFrom(button, original, "Edited from " + kind, true, true);
            QCOMPARE(recordUndo->index(), index + 1);
            QCOMPARE(preview->text(), "Edited from " + kind);
            editDescriptionFrom(button, "Edited from " + kind, "Cancelled draft", false);
            recordUndo->undo();
            QCOMPARE(preview->text(), original);
            recordUndo->redo();
            QCOMPARE(preview->text(), "Edited from " + kind);
            recordUndo->undo();
            grid->sortByColumn(0, Qt::AscendingOrder);
        }
        const int frontAction = u8(textBaseline.record(Table::Classes, 5).bytes, 55);
        editDescriptionFrom(window.findChild<QPushButton *>("attack0Description"),
                            textBaseline.textBanks()["actions"].text(frontAction), "Action from class", true);
        editDescriptionFrom(window.findChild<QPushButton *>("attack0Description"),
                            "Action from class", "Cancelled draft", false);
        recordUndo->undo();
        auto addItem = window.findChild<QPushButton *>("addItem");
        QVERIFY(addItem);
        items->setCurrentIndex(items->model()->index(1, 1));
        QVERIFY(addItem->isEnabled());
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->setTextValue("Created item");
            dialog->accept();
        });
        addItem->click();
        QCOMPARE(items->model()->rowCount(), itemCount + 1);
        QCOMPARE(items->currentIndex().siblingAtColumn(1).data().toString(),
                 QString("Created item"));
        auto itemDescription = window.findChild<QWidget *>("itemsDescriptionPanel");
        editDescriptionFrom(itemDescription->findChild<QPushButton *>("editRecordDescription"),
                            textBaseline.textBanks()["items"].text(1), "New item description", true);
        QCOMPARE(itemDescription->findChild<QLabel *>("recordDescriptionPreview")->text(),
                 QString("New item description"));
        recordUndo->undo(); // Undo only the description, keeping the newly added item selected.
        for (const auto name : {"scenarioDrop", "rewardItem_0"}) {
            const auto box = window.findChild<QComboBox *>(name);
            QVERIFY(box);
            QVERIFY(box->findData(0x8000 | itemCount) >= 0);
        }
        const auto reward = window.findChild<QComboBox *>("sceneReward");
        QVERIFY(reward);
        QVERIFY(reward->findData(itemCount) >= 0);
        const auto equipment = window.findChild<QComboBox *>("actorEquipment");
        QVERIFY(equipment);
        QVERIFY(equipment->findData(itemCount) >= 0);
        window.findChild<QUndoStack *>()->undo();
        QCOMPARE(items->model()->rowCount(), itemCount);
        auto fresh = window.findChild<QPushButton *>("newClass");
        QVERIFY(fresh);
        // Scrolling over an unfocused field must not silently edit the ROM.
        auto untouchedSpin = window.findChild<QSpinBox *>();
        untouchedSpin->clearFocus();
        const int beforeWheel = untouchedSpin->value();
        QWheelEvent passiveWheel(QPointF(2, 2), QPointF(2, 2), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(untouchedSpin, &passiveWheel);
        QCOMPARE(untouchedSpin->value(), beforeWheel);
        classes->setCurrentIndex({}); // Creating a class needs no template selection.
        QVERIFY(fresh->isEnabled());
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->reject();
        });
        fresh->click();
        QCOMPARE(classes->model()->rowCount(), classCount);
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->setTextValue("Fresh class");
            dialog->accept();
        });
        fresh->click();
        QCOMPARE(classes->model()->rowCount(), classCount + 1);
        QCOMPARE(classes->currentIndex().siblingAtColumn(1).data().toString(), QString("Fresh class"));
        auto classDescription = window.findChild<QWidget *>("classesDescriptionPanel");
        editDescriptionFrom(classDescription->findChild<QPushButton *>("editRecordDescription"),
                            {}, "New class description", true);
        editDescriptionFrom(classDescription->findChild<QPushButton *>("editRecordDescription"),
                            "New class description", "Cancelled draft", false);
        recordUndo->undo();
        auto undo = window.findChild<QUndoStack *>();
        QVERIFY(undo);
        undo->undo();
        QCOMPARE(classes->model()->rowCount(), classCount);
        undo->redo();
        QCOMPARE(classes->model()->rowCount(), classCount + 1);
        undo->undo(); // Leave the window clean for teardown.
        auto screenshot = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window.grab().save(screenshot));
            const auto capturePrefix = QFileInfo(screenshot).absolutePath() + "/" + QFileInfo(screenshot).completeBaseName();
            auto workspaceTabs = qobject_cast<QTabWidget *>(window.centralWidget());
            for (int i = 0; i < workspaceTabs->count(); ++i)
                if (workspaceTabs->tabText(i) == "Consumables" || workspaceTabs->tabText(i) == "Items") {
                    workspaceTabs->setCurrentIndex(i);
                    window.resize(1440, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(capturePrefix + "-" + workspaceTabs->tabText(i).toLower() + ".png"));
                }
            for (const auto name : {"encounterEditor", "scenarioEditor"}) {
                workspaceTabs->setCurrentWidget(window.findChild<QWidget *>(name));
                for (int width : {1440, 1100}) {
                    window.resize(width, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(capturePrefix + QString("-%1-%2.png").arg(name).arg(width)));
                }
            }
            // Optional visual captures use the same loaded desktop, including its
            // advanced-tool navigation. No extra ROM or export fixture is needed.
            auto artwork = window.findChild<QTabWidget *>("artWorkspace");
            if (artwork) {
                auto workspaces = qobject_cast<QTabWidget *>(window.centralWidget());
                workspaces->setCurrentWidget(artwork);
                artwork->setCurrentIndex(artwork->count() - 1);
                const auto prefix = QFileInfo(screenshot).absolutePath() + "/" + QFileInfo(screenshot).completeBaseName();
                for (int width : {1440, 1100}) {
                    window.resize(width, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(prefix + QString("-advanced-%1.png").arg(width)));
                }
            }
        }
    }
    void categoryIconEditingAndNavigation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadPromotion();
            project.loadCategoryIcons();
            auto promotion = *project.promotion();
            const int family = promotion.createFamily("Test crest family");
            promotion.assign(1, family);
            project.setPromotion(promotion.json());
            QUndoStack undo;
            PromotionEditor families(&project, &undo, [] {});
            families.selectClass(1);
            CategoryIconEditor editor(&project, &undo, [] {});
            editor.resize(1000, 650);
            editor.show();
            QTest::qWait(20);
            QCOMPARE(editor.findChild<QListWidget *>("categoryIcons")->count(), 8);
            const auto before = *project.categoryIcons();
            auto canvas = editor.findChild<SpriteCanvas *>("categoryIconCanvas");
            QVERIFY(canvas);
            const int point = 5 * 14 + 5;
            const auto color = quint8(before.indices(1)[point]) ^ 1;
            canvas->setIndexColor(color);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, {110, 110});
            QCOMPARE(quint8(project.categoryIcons()->indices(1)[point]), color);
            QCOMPARE(project.categoryIcons()->indices(2), before.indices(2));
            undo.undo();
            QCOMPARE(project.categoryIcons()->json(), before.json());
            undo.redo();
            QTimer::singleShot(0, [] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto dialog = qobject_cast<QInputDialog *>(widget)) {
                        dialog->setTextValue("New crest");
                        dialog->accept();
                    }
            });
            editor.createIcon(family, true);
            const auto newIcon = editor.selectedCategory();
            QCOMPARE(newIcon, 9);
            QCOMPARE(project.categoryIcons()->familyIcon(family), 9);
            const auto beforeInvalid = project.categoryIcons()->json();
            QTimer::singleShot(0, [] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto dialog = qobject_cast<QInputDialog *>(widget)) {
                        dialog->setTextValue("   ");
                        dialog->accept();
                    }
            });
            editor.createIcon(family);
            QCOMPARE(project.categoryIcons()->json(), beforeInvalid);
            QVERIFY(!editor.findChild<QLabel *>("categoryIconNotice")->text().isEmpty());
            QCOMPARE(families.findChild<QComboBox *>("familyTabIcon")->currentData().toInt(), 9);
            undo.undo();
            QCOMPARE(project.categoryIcons()->iconIds().size(), 8);
            undo.redo();
            editor.selectCategory(9);
            QImage replacement(14, 14, QImage::Format_RGBA8888);
            replacement.fill(project.categoryIcons()->palette()[0]);
            editor.assignImage(replacement);
            QCOMPARE(project.categoryIcons()->image(9), replacement);
            auto picker = families.findChild<QComboBox *>("familyTabIcon");
            picker->setCurrentIndex(picker->findData(2));
            QCOMPARE(project.categoryIcons()->familyIcon(family), 2);
            undo.undo();
            QCOMPARE(project.categoryIcons()->familyIcon(family), 9);
            const auto capture = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
            if (!capture.isEmpty())
                QVERIFY(editor.grab().save(capture + "-category-icons.png"));
            MainWindow window;
            window.resize(1100, 780);
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<CategoryIconEditor *>() != nullptr, 30000);
            auto familyEditor = window.findChild<PromotionEditor *>();
            QVERIFY(familyEditor);
            familyEditor->findChild<QPushButton *>("editFamilyIcon")->click();
            auto workspaces = qobject_cast<QTabWidget *>(window.centralWidget());
            auto art = window.findChild<QTabWidget *>("artWorkspace");
            auto icons = window.findChild<QTabWidget *>("uiAndIcons");
            QCOMPARE(workspaces->currentWidget(), art);
            QCOMPARE(art->currentWidget(), icons);
            QCOMPARE(icons->currentWidget(), window.findChild<CategoryIconEditor *>());
            if (!capture.isEmpty())
                QVERIFY(window.grab().save(capture + "-category-workspace.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void promotionControlsAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadPromotion();
        const auto original = project.promotion()->json();
        QUndoStack undo;
        PromotionEditor editor(&project, &undo, [] {});
        editor.selectClass(5);
        auto equipment = editor.findChild<QCheckBox *>("standardEquipmentList");
        QVERIFY(equipment);
        const auto originalFlags = u8(project.promotion()->flags, 5);
        equipment->setChecked(!(originalFlags & 0x80));
        QCOMPARE(u8(project.promotion()->flags, 5), quint8(originalFlags ^ 0x80));
        undo.undo();
        QCOMPARE(project.promotion()->json(), original);
        QCOMPARE(equipment->isChecked(), bool(originalFlags & 0x80));
        undo.redo();
        QCOMPARE(u8(project.promotion()->flags, 5), quint8(originalFlags ^ 0x80));
        undo.undo();
        auto gate = editor.findChild<QSpinBox *>("promotionGate0");
        gate->setValue(90);
        QCOMPARE(u8(project.promotion()->gates, 40), quint8(90));
        undo.undo();
        QCOMPARE(project.promotion()->json(), original);
        undo.redo();
        QCOMPARE(gate->value(), 90);
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Order of Dawn");
        auto rules = editor.findChild<QComboBox *>("newFamilyRules");
        QVERIFY(rules);
        QCOMPARE(rules->currentData().toInt(), int(Promotion::FamilyRules::WithinFamily));
        const auto priorAssignments = project.promotion()->families["assignments"].toArray();
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[5].toInt(), 16);
        QCOMPARE(project.promotion()->families["families"].toArray().last().toObject()["behavior"].toInt(), 5);
        for (int id = 0; id < priorAssignments.size(); ++id)
            if (id != 5)
                QCOMPARE(project.promotion()->families["assignments"].toArray()[id], priorAssignments[id]);
        QCOMPARE(gate->value(), 90); // Creating a family does not overwrite class requirements.
        undo.undo();
        QCOMPARE(project.promotion()->families, project.promotion()->originalFamilies);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 16);
        auto links = editor.findChild<QListWidget *>("familyLinks");
        for (int row = 0; row < links->count(); ++row)
            if (links->item(row)->data(Qt::UserRole).toInt() == 1) links->item(row)->setCheckState(Qt::Checked);
        const auto unlinked = project.promotion()->json();
        editor.findChild<QPushButton *>("applyFamilyLinks")->click();
        QCOMPARE(project.promotion()->linkedFamilies(16), QSet<int>{1});
        QCOMPARE(project.promotion()->linkedFamilies(1), QSet<int>{16});
        QCOMPARE(u32(project.promotion()->decodedFamilies(), 648 + 5 * 4), quint32(0x50001));
        undo.undo(); QCOMPARE(project.promotion()->json(), unlinked);
        undo.redo();
        editor.selectClass(100);
        QVERIFY(!gate->isEnabled());
        QVERIFY(!equipment->isEnabled());
        RecordModel model(&project, Table::Classes, &undo);
        connect(&model, &RecordModel::edited, &editor,
                [&] { editor.selectClass(model.rowCount() > 165 ? 165 : 100); });
        QCOMPARE(model.appendClass(5, "New promotion class"), 165);
        QVERIFY(gate->isEnabled());
        QVERIFY(equipment->isEnabled());
        QCOMPARE(gate->value(), 90);
        gate->setValue(120);
        QCOMPARE(u8(project.promotion()->gates, 165 * 8), quint8(120));
        undo.undo();
        QCOMPARE(gate->value(), 90);
        undo.undo();
        QVERIFY(!gate->isEnabled());
        QCOMPARE(project.promotion()->classCount(), 81);
        undo.redo();
        QVERIFY(gate->isEnabled());
        QCOMPARE(project.promotion()->classCount(), 166);
        const QJsonValue previousFamily = project.promotion()->families["assignments"].toArray()[165];
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Added class family");
        rules->setCurrentIndex(rules->findData(int(Promotion::FamilyRules::WithLich)));
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), 17);
        QCOMPARE(project.promotion()->families["families"].toArray().last().toObject()["behavior"].toInt(), 2);
        undo.undo();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165], previousFamily);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 17);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
