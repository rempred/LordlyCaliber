#include "app/campaign_editor.h"
#include "app/cutscene_editor.h"
#include "app/scene_action_editor.h"
#include "app/scene_key_picker.h"
#include "app/scene_preview.h"
#include "app/scene_timeline.h"
#include "app/event_flow_editor.h"
#include "app/theme.h"
#include "engine/director_session.h"
#include "engine/event_authoring.h"
#include "engine/scene_background_edit.h"
#include "engine/pose_preview.h"
#include "engine/project.h"
#include "engine/save_document.h"
#include "engine/scene_images.h"
#include "engine/scene_import.h"
#include "engine/scene_sprites.h"
#include "rebuild/scene_logic.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtTest>
#include <bit>
using namespace lordly;
class SceneWorkspaceTests : public QObject {
    Q_OBJECT
    Rom rom()const{return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));}
  private slots:
    void openingClassActors_data() {
        QTest::addColumn<int>("selector");
        QTest::addColumn<int>("slot");
        QTest::addColumn<int>("body");
        QTest::newRow("Ischka graduation") << 2 << 6 << 32;
        QTest::newRow("Dio challenges Magnus") << 11 << 1 << 84;
    }
    void openingClassActors() {
        QFETCH(int, selector);
        QFETCH(int, slot);
        QFETCH(int, body);
        auto project = Project::fromRom(rom());
        project.loadSceneEvents();
        project.loadDialogues();
        const auto &scenes = *project.scenePrograms();
        const auto key = scenes.original().selectors().at(selector);
        ScenePreview preview(project.rom());
        preview.show();
        preview.selectScene(scenes, key, SceneImages::backgrounds(project.rom(), scenes.resource(key), -1).mode,
                            *project.dialogues(), *project.sceneEvents());
        preview.selectEventLaunch(selector == 2 ? 1 : 4, selector == 2 ? 82 : 62);
        auto *actors = preview.findChild<QComboBox *>("sceneLiveActors");
        auto *status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto *confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto *step = preview.findChild<QPushButton *>("sceneStep");
        QVERIFY(actors && status && confirm && step);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 10000);
        bool appeared = false, spoke = false;
        for (int i = 0; i < 3000 && !preview.property("sceneStopped").toBool(); ++i) {
            const int index = actors->findText(QString("Actor %1").arg(slot));
            if (!appeared && index >= 0) {
                actors->setCurrentIndex(index);
                const auto record = preview.selectedObjectRecord();
                appeared = record.size() >= 0x150 && u8(record,0x13d) && u32(record,0xe8) == quint32(body);
            }
            const int state = preview.property("sceneFocusedDialogueState").toInt();
            spoke |= state == 3 || state == 6 || state == 7;
            if (i % 20 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
        }
        QVERIFY2(appeared && spoke, qPrintable(status->text()));
        QVERIFY2(status->text().contains("Scene released") || status->text().contains("returned to the game")
                 || status->text().contains("requested a scene"), qPrintable(status->text()));
    }
    void theodricusBriefingMenu() {
        auto project = Project::fromRom(rom());
        project.loadSceneEvents();
        project.loadDialogues();
        const auto &scenes = *project.scenePrograms();
        const auto key = scenes.original().selectors().at(10);
        ScenePreview preview(project.rom());
        preview.show();
        preview.selectScene(scenes, key, SceneImages::backgrounds(project.rom(), scenes.resource(key), -1).mode,
                            *project.dialogues(), *project.sceneEvents());
        preview.selectEventLaunch(4, 50);
        auto *status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto *confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto *down = preview.findChild<QPushButton *>("panelDown");
        auto *step = preview.findChild<QPushButton *>("sceneStep");
        QVERIFY(status && confirm && step && down);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 10000);
        int menus = 0, menuStart = -1;
        bool replayed = false, closing = false;
        for (int n = 0; n < 13000 && !preview.property("sceneStopped").toBool(); ++n) {
            const int word = preview.activeWord();
            if (word >= 1100 && word <= 1755 && menus == 1) replayed = true;
            if (word >= 2392) closing = true;
            // The scheduler waits after the menu input query at word 1948.
            if (menuStart < 0 && menus < 2 && (menus == 0 || replayed) &&
                status->text().section('\n', 0, 0).endsWith("word 1949")) {
                menuStart = n;
                ++menus;
            }
            if (menuStart >= 0) {
                const int age = n - menuStart;
                if (menus == 2 && age == 40) down->click();
                else if (age == 42) confirm->click();
                else step->click();
                if (age == 43) menuStart = -1;
            } else if (n % 20 == 0 && confirm->isVisible()) {
                confirm->click();
            } else {
                step->click();
            }
        }
        QVERIFY2(replayed && menus == 2 && closing, qPrintable(status->text()));
        QVERIFY2(status->text().contains("returned to the game") || status->text().contains("Scene released"),
                 qPrintable(status->text()));
    }
    void storyDecisionHandoff_data() {
        QTest::addColumn<int>("question");
        QTest::addColumn<bool>("second");
        QTest::addColumn<bool>("swapped");
        QTest::addColumn<int>("reaction");
        QTest::newRow("Wentinus orders") << 180 << false << false << 181;
        QTest::newRow("Wentinus peace") << 180 << true << false << 182;
        QTest::newRow("Wentinus edited orders") << 180 << false << true << 182;
        QTest::newRow("Wentinus edited peace") << 180 << true << true << 181;
        QTest::newRow("Sheen accepts") << 159 << false << false << 160;
        QTest::newRow("Sheen refuses") << 159 << true << false << 161;
        QTest::newRow("Liedel accepts") << 121 << false << false << 122;
        QTest::newRow("Liedel returns") << 121 << true << false << 123;
        QTest::newRow("Let Paul leave") << 289 << false << false << 291;
        QTest::newRow("Confront Paul") << 289 << true << false << 290;
    }
    void storyDecisionHandoff() {
        QFETCH(int, question);
        QFETCH(bool, second);
        QFETCH(bool, swapped);
        QFETCH(int, reaction);
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("US Rev 0 ROM required");
        auto project = Project::fromRom(rom());
        project.loadSceneEvents();
        project.loadScenePrograms();
        project.loadDialogues();
        const auto &scenes = *project.scenePrograms();
        auto events = *project.sceneEvents();
        if (swapped)
            events.editParameter(22, 562, "value", 1, scenes);
        const bool wentinus = question == 180;
        const bool liedel = question == 121;
        const bool paul = question == 289;
        ScenePreview preview(project.rom());
        preview.resize(1250, 950);
        preview.show();
        const auto key = scenes.original().selectors()[question];
        const auto &resource = scenes.original().entries()[key];
        preview.selectScene(scenes, key, SceneImages::backgrounds(project.rom(), resource, -1).mode,
                            *project.dialogues(), events);
        const QMap<int, QPair<int, int>> launches{{180,{22,544}}, {121,{16,394}}, {159,{20,202}}, {289,{35,158}}};
        const auto [row, request] = launches.value(question);
        preview.selectEventLaunch(row, request);
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto down = preview.findChild<QPushButton *>("panelDown");
        auto outcome = preview.findChild<QPushButton *>("sceneAnswerOutcome");
        QVERIFY(status && confirm && step && down && outcome);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 15000);
        int choices = 0;
        bool moved = false;
        for (int n = 0;
             n < 4000 && !preview.property("sceneStopped").toBool() && !outcome->isVisible(); ++n) {
            const int state = preview.property("sceneFocusedDialogueState").toInt();
            if (state == 7 && second && !moved) {
                down->click();
                moved = true;
            } else if (state == 7) {
                confirm->click();
                ++choices;
            } else if (n % 20 == 0 && confirm->isVisible()) {
                confirm->click();
                moved = false;
            } else {
                step->click();
                if (state != 7)
                    moved = false;
            }
        }
        QVERIFY2(choices > 0, qPrintable(status->text()));
        QVERIFY2(outcome->isVisible(), qPrintable(status->text()));
        QVERIFY2(status->text().contains(QString("selector %1 ·").arg(reaction)),
                 qPrintable(status->text()));
        bool sawReactionPage = false;
        int briefingMenu = -1;
        for (int n = 0; n < 2200 && !preview.property("sceneStopped").toBool(); ++n) {
            const int state = preview.property("sceneFocusedDialogueState").toInt();
            if (n > 8 && (state == 3 || state == 6))
                sawReactionPage = true;
            // The orders reaction opens the Repeat / End menu at this barrier.
            if (reaction == 181 && briefingMenu < 0 &&
                status->text().section('\n', 0, 0).endsWith("word 1039"))
                briefingMenu = n;
            if (briefingMenu >= 0 && n - briefingMenu <= 43) {
                if (n - briefingMenu == 40)
                    down->click();
                else if (n - briefingMenu == 42)
                    confirm->click();
                else
                    step->click();
            } else if (n % 20 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
        }
        QVERIFY2(sawReactionPage, qPrintable(status->text()));
        if (reaction == 181)
            QVERIFY2(briefingMenu >= 0, qPrintable(status->text()));
        QVERIFY2(preview.property("sceneStopped").toBool() &&
                     (status->text().contains("Scene released") ||
                      status->text().contains("returned to the game")),
                 qPrintable(status->text()));
        QString effects;
        QTimer::singleShot(0, &preview, [&] {
            if (auto dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                effects = dialog->text();
                dialog->accept();
            }
        });
        outcome->click();
        if (wentinus) {
            QVERIFY(effects.contains("Flag 119"));
            QVERIFY(effects.contains(reaction == 182 ? "scenario 21" : "Flag 19"));
        } else if (paul) {
            QVERIFY2(effects.contains("202"), qPrintable(effects));
            QCOMPARE(effects.contains("203"), reaction == 290);
            if (reaction == 290)
                QVERIFY2(status->text().contains("Sound gain 44"), qPrintable(status->text()));
        } else if (liedel) {
            if (reaction == 122)
                QVERIFY2(effects.contains("Recruit character"), qPrintable(effects));
            else
                QVERIFY2(effects.contains("Central Division", Qt::CaseInsensitive) || effects.contains("return", Qt::CaseInsensitive), qPrintable(effects));
        } else
            QVERIFY(effects.contains(reaction == 160 ? "Recruit character" : "Refuse Sheen"));
    }
    void openTownVisitFromCampaign_data() {
        QTest::addColumn<bool>("redirect");
        QTest::newRow("retail Mulsuk") << false;
        QTest::newRow("edited Jadd") << true;
    }
    void openTownVisitFromCampaign() {
        QFETCH(bool, redirect);
        auto project = Project::fromRom(rom());
        project.loadSceneEvents();
        if (redirect) {
            auto events = *project.sceneEvents();
            events.editParameter(4, 0xaa, "value", 3, *project.scenePrograms());
            events.editParameter(4, 0xb0, "value", 611, *project.scenePrograms());
            project.setSceneEvents(events.json());
        }
        QUndoStack undo;
        CampaignEditor campaign(&project, &undo, [] {});
        campaign.resize(1400, 900);
        campaign.show();
        campaign.selectEvent(4, 0xa8);
        QApplication::processEvents();
        auto tabs = campaign.findChild<QTabWidget *>("storylineTabs");
        QVERIFY(tabs);
        const auto buttons = tabs->currentWidget()->findChildren<QPushButton *>();
        auto play = std::find_if(buttons.cbegin(), buttons.cend(), [](const auto *button) {
            return button->isVisible() && button->text() == "Edit cutscene…";
        });
        QVERIFY(play != buttons.cend());
        (*play)->click();
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QString("Cutscenes"));
        auto context = tabs->currentWidget()->findChild<QComboBox *>("sceneLaunchContext");
        QVERIFY(context);
        QCOMPARE(context->currentData().toString(), QString("event-director:4:b00B4:invocation:168:0"));
        const auto sceneList = tabs->currentWidget()->findChild<QListWidget *>("cutsceneScenes");
        QVERIFY(sceneList && sceneList->currentItem());
        const auto key = project.scenePrograms()->original().selectors().at(redirect ? 611 : 610);
        QCOMPARE(sceneList->currentItem()->data(Qt::UserRole).toUInt(), key);
        QCOMPARE(undo.count(), 0);
    }
    void openDecisionFromCampaign() {
        auto project = Project::fromRom(rom());
        QUndoStack undo;
        CampaignEditor campaign(&project, &undo, [] {});
        campaign.resize(1400, 900);
        campaign.show();
        campaign.selectEvent(22, 190);
        auto tabs = campaign.findChild<QTabWidget *>("storylineTabs");
        QVERIFY(tabs);
        auto decisions = tabs->currentWidget()->findChild<QPushButton *>("storyPlayerDecisions");
        QVERIFY(decisions && decisions->isVisible());
        QTimer::singleShot(0, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(5000, dialog, &QDialog::reject);
            auto version = dialog->findChild<QComboBox *>("decisionVersion");
            auto open = dialog->findChild<QPushButton *>("previewStoryDecision");
            QVERIFY(version && open);
            version->setCurrentIndex(3);
            open->click();
        });
        decisions->click();
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QString("Cutscenes"));
        auto context = tabs->currentWidget()->findChild<QComboBox *>("sceneLaunchContext");
        QVERIFY(context);
        const auto &launches = SceneEvents::contexts();
        const auto launch = std::find_if(launches.cbegin(), launches.cend(), [&](const auto &c) {
            return c.id == context->currentData().toString();
        });
        QVERIFY2(launch != launches.cend(), qPrintable(context->currentText()));
        const auto &site = SceneEvents::site(launch->site);
        QCOMPARE(site.row, 22);
        QCOMPARE(site.offset, 544);
        QCOMPARE(undo.count(), 0);
        auto preview = tabs->currentWidget()->findChild<ScenePreview *>();
        QVERIFY(preview);
        preview->selectEventLaunch(22, 8); // An occurrence absent from the launch catalog.
        QCOMPARE(context->currentData().toString(), QString("event:22:8"));
        const int count = context->count();
        preview->selectEventLaunch(22, 8);
        QCOMPARE(context->count(), count);
        preview->selectEventLaunch(22, 544);
        QCOMPARE(context->currentData().toString(), launch->id);
    }
    void storyDecisionUsesSelectedEvent() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("US Rev 0 ROM required");
        auto project = Project::fromRom(rom());
        project.loadScenePrograms();
        project.loadSceneEvents();
        auto &scenes = *project.scenePrograms();
        const auto &events = *project.sceneEvents();
        for (int selector : {171, 174, 177, 180}) {
            auto plans = SceneLaunchPlan::choices(scenes.runtimeDirectors(), events, selector);
            SceneLaunchPlan::addAnswerRoutes(plans, project.rom(), scenes, events, selector);
            bool story = false, other = false;
            for (const auto &plan : plans) {
                if (plan.site.isEmpty())
                    continue;
                const int row = SceneEvents::site(plan.site).row;
                if (row == 22) {
                    story = true;
                    QVERIFY(plan.decision);
                    QCOMPARE(plan.decision->reaction(255).selector, selector + 2);
                    QCOMPARE(plan.decision->reaction(1).selector, selector + 1);
                }
                if (row == 66) {
                    other = true;
                    QVERIFY(!plan.decision);
                }
            }
            QVERIFY(story);
            QVERIFY(other);
        }
    }
    void hugoReportTips_data() {
        QTest::addColumn<bool>("upperRdram");
        QTest::newRow("retail capture") << false;
        QTest::newRow("report script in Expansion Pak RAM") << true;
    }
    void hugoReportTips() {
        const auto path = qEnvironmentVariable("LORDLY_REPORT_RAM");
        if (path.isEmpty())
            QSKIP("Requires the native RAM dump of the retail Hugo Report menu-open reference");
        QFile capture(path);
        QVERIFY(capture.open(QIODevice::ReadOnly));
        const auto game = rom();
        const auto programs = ScenePrograms::load(game);
        const auto &original = programs.original();
        const auto key = original.selectors()[0];
        QCOMPARE(programs.displayName(key), QString("Hugo's Report"));
        const auto bytes = original.entries()[key].program.encode();
        auto edited = original.entries()[key];
        auto help =
            std::find_if(edited.program.commands.begin(), edited.program.commands.end(),
                         [](const DirectorCommand &command) { return command.opcode == 0xae; });
        QVERIFY(help != edited.program.commands.end());
        help->operands[1] = 1;
        QCOMPARE(SceneImages::backgrounds(game, edited, -1).mode, 2);
        QCOMPARE(
            SceneImages::backgrounds(game, original.entries()[original.selectors()[1078]], -1).mode,
            0);
        auto input = capture.readAll();
        QFETCH(bool, upperRdram);
        if (upperRdram) {
            QCOMPARE(input.size(), 0x400000);
            const auto originalScript = input.mid(u32(input, 0x22a958) & 0x1fffffff, bytes.size());
            input.append(QByteArray(0x400000, 0));
            input.replace(0x500000, originalScript.size(), originalScript);
            put32(input, 0x22a958, 0x80500000u);
        }
        const auto save = SaveDocument::open(game, input);
        const auto &ram = save.memory();
        QCOMPARE(u32(ram, 0x22a950), 30u);
        QCOMPARE(ram.mid(u32(ram, 0x22a958) & 0x1fffffff, bytes.size()), bytes);
        QCOMPARE(u8(ram, 0x193686) & 2, 0);
        ScenePreview preview(game);
        preview.setSaveProvider([&] { return &save; });
        preview.selectScene(programs, key, 2);
        auto click = [&](const char *name) {
            auto b = preview.findChild<QPushButton *>(name);
            QVERIFY(b && b->isEnabled());
            b->click();
        };
        for (int n = 0; n < 250; ++n) {
            if (n == 22 || n == 29 || (n >= 90 && n < 138 && !(n % 2)))
                click("panelDown");
            else if (n == 45 || n == 152 || n == 197)
                click("panelConfirm");
            else if (n == 218)
                click("panelCancel");
            else
                click("sceneStep");
            QVERIFY2(!preview.property("sceneStopped").toBool(),
                     qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
            if (n == 180)
                QVERIFY(preview.commandTimes().contains(524)); // detail page is waiting for input
        }
        QVERIFY(preview.commandTimes().contains(539)); // Back rebuilds the list
    }
    void zemioBriefingVisit() {
        auto project = Project::fromRom(rom());
        project.loadScenePrograms();
        project.loadSceneEvents();
        project.loadDialogues();
        const auto key = project.scenePrograms()->original().selectors()[1493];
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.show();
        auto catalog = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_VERIFY_WITH_TIMEOUT(catalog && catalog->count() > 1000, 10000);
        for (int i = 0; i < catalog->count(); ++i)
            if (catalog->item(i)->data(Qt::UserRole).toUInt() == key) {
                catalog->setCurrentRow(i);
                break;
            }
        auto playback = editor.findChild<ScenePreview *>();
        QVERIFY(playback);
        ScenePreview &preview = *playback;
        for (auto pages : editor.findChildren<QTabWidget *>())
            if (pages->indexOf(playback) >= 0) pages->setCurrentWidget(playback);
        QVERIFY(preview.isVisible());
        QCOMPARE(preview.playbackAssetId(), QString("rom-director:01FBDA40"));
        QFile inputs(QFINDTESTDATA("fixtures/zemio-briefing-inputs.json"));
        QVERIFY(inputs.open(QIODevice::ReadOnly));
        preview.importPlaybackInputs(inputs.readAll());
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto down = preview.findChild<QPushButton *>("panelDown");
        auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(step && confirm && down && objects && status);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 15000);

        // Project64 observed these three slots in both rooms immediately after 0xC2.
        auto checkRoom = [&] {
            QVector<int> slotsSeen;
            for (int i = 0; i < objects->count(); ++i) {
                if (objects->itemData(i, Qt::UserRole + 1).toInt() != int(SceneObjectKind::Actor))
                    continue;
                objects->setCurrentIndex(i);
                const auto record = preview.selectedObjectRecord();
                const int slot = objects->itemData(i).toInt();
                slotsSeen << slot;
                QCOMPARE(record.size(), 336);
                if (slot == 5) {
                    QCOMPARE(u32(record, 0xe8), 81u);
                    QCOMPARE(u32(record, 0xec), 81u);
                    QCOMPARE(u16(record, 0x138), 13);
                    QCOMPARE(u8(record, 0x146), 0);
                    QCOMPARE(u8(record, 0x147), 0); // visiting Magnus, roster row zero
                    QCOMPARE(u8(record, 0x149), 0); // first component
                } else {
                    QCOMPARE(u32(record, 0xe8), slot == 0 ? 42u : 45u);
                    QCOMPARE(u8(record, 0x147), 255);
                }
            }
            QCOMPARE(slotsSeen, QVector<int>({0, 1, 5}));
        };

        // Advance ordinary dialogue, then operate the same two choice panels as the game.
        auto advanceUntil = [&](auto reached, bool advanceDialogue = true) {
            for (int n = 0; n < 3000 && !preview.property("sceneStopped").toBool(); ++n) {
                if (reached()) return true;
                if (advanceDialogue && n % 45 == 0 && confirm->isVisible() && confirm->isEnabled()) confirm->click();
                else step->click();
            }
            return reached();
        };
        auto observed = [&](int word) { return preview.commandTimes().contains(word); };
        auto choicePanel = [&] {
            return observed(1098) && preview.activeWord() >= 1114 && preview.activeWord() <= 1141;
        };
        auto settlePanel = [&] { for (int i = 0; i < 40; ++i) step->click(); };
        QVERIFY2(advanceUntil([&] { return observed(108); }), qPrintable(status->text()));
        checkRoom();
        QVERIFY2(advanceUntil([&] { return observed(1098); }), qPrintable(status->text()));
        settlePanel();
        confirm->click(); // Repeat, then Yes
        QVERIFY2(advanceUntil([&] { return observed(1119); }, false), qPrintable(status->text()));
        settlePanel();
        confirm->click();
        QVERIFY2(advanceUntil([&] { return preview.activeWord() >= 294 && preview.activeWord() < 400; }),
                 qPrintable(status->text()));
        QVERIFY(!observed(1305));
        QVERIFY2(advanceUntil(choicePanel), qPrintable(status->text()));
        settlePanel();
        down->click(); // End, then Yes
        step->click();
        confirm->click();
        QVERIFY2(advanceUntil([&] { return observed(1128); }, false), qPrintable(status->text()));
        settlePanel();
        confirm->click();
        QVERIFY2(advanceUntil([&] { return observed(1305); }, false), qPrintable(status->text()));
        checkRoom();
        QVERIFY2(advanceUntil([&] { return preview.property("sceneStopped").toBool(); }), qPrintable(status->text()));
        QVERIFY2(observed(1374) && status->text().contains("released", Qt::CaseInsensitive), qPrintable(status->text()));
    }
    void briefingTimeline() {
        try {
            auto project = Project::fromRom(rom());
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const SceneId scene(0x01fbda40 - 0x594284);
            auto source = programs.program(scene);
            int command = -1;
            for (int i = 0; i < source.commands.size(); ++i)
                if (source.commands[i].opcode == 0xc2) { command = i; break; }
            QVERIFY(command >= 0);
            QCOMPARE(source.commands[command].operands, QVector<quint32>({5, 13}));
            source.commands[command].operands = {0x12340005, 0xabcd000d};
            const auto before = source.encode();
            programs.replace(scene, before);
            project.setScenePrograms(programs.json());
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 1000, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole).toUInt() == scene.nativeKey) {
                    scenes->setCurrentRow(i); break;
                }
            auto timeline = editor.findChild<SceneTimeline *>();
            auto actions = editor.findChild<SceneActionEditor *>();
            QGraphicsItem *clip = nullptr;
            bool actorLane = false;
            for (auto item : timeline->scene()->items()) {
                if (item->toolTip() == "Actor 5") actorLane = true;
                if (item->data(0).toInt() == command && item->toolTip().startsWith("Start briefing actors"))
                    clip = item;
            }
            QVERIFY(actorLane);
            QVERIFY(clip);
            timeline->centerOn(clip);
            QTest::mouseClick(timeline->viewport(), Qt::LeftButton, Qt::NoModifier,
                              timeline->mapFromScene(clip->sceneBoundingRect().center()));
            auto anchor = actions->findChild<QLineEdit *>("sceneActionField0");
            auto pose = actions->findChild<QLineEdit *>("sceneActionField1");
            QVERIFY(anchor && pose);
            QCOMPARE(anchor->accessibleName(), QString("Anchor actor slot"));
            QCOMPARE(anchor->text(), QString("5"));
            QCOMPARE(pose->text(), QString("13"));
            anchor->setText("3");
            auto expected = before;
            put32(expected, 4 * (source.commands[command].word + 1), 0x12340003);
            QCOMPARE(project.scenePrograms()->program(scene).source, expected);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(scene).source, before);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(scene).source, expected);
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty()) QSKIP("N64 toolchain is required for the briefing export");
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(scene).source, expected);
            const auto built = rebuild(restored, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() + "/Test ROMs/native-briefing-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, destination);
            auto reopened = Project::fromRom(Rom::open(destination));
            reopened.loadScenePrograms();
            const auto &directors = reopened.scenePrograms()->original();
            const auto selectors = programs.resource(scene).selectors;
            QVERIFY(!selectors.isEmpty());
            for (int selector : selectors)
                QCOMPARE(directors.entries()[directors.selectors()[selector]].program.source, expected);
            reopened.scenePrograms()->validateReferences(reopened.rom());
            const auto placement = SceneProgramResources::placement(reopened.rom());
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <= quint64(reopened.rom().bytes().size()));
            auto checksum = reopened.rom().bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, reopened.rom().bytes());
            QVERIFY(Rom::open(path).bytes() == project.rom().bytes());
            qInfo().noquote() << "Briefing export:" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void keyVisibility(){
        QWidget host;auto layout=new QVBoxLayout(&host);auto picker=new SceneKeyPicker(rom());
        layout->addWidget(picker,1);layout->addWidget(picker->auditionWidget());host.resize(650,850);host.show();
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank"),kind=picker->findChild<QComboBox *>("sceneKeyKind"),variants=picker->findChild<QComboBox *>("sceneKeyAppearance");
        auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");auto image=picker->auditionWidget()->findChild<QLabel *>("sceneKeyPreviewImage");
        for(int bank=1;bank<=ActorArt::bankCount(rom());++bank){banks->setCurrentIndex(banks->findData(bank));QVERIFY2(variants->count()>0,qPrintable(QString("Bank %1 appearances missing").arg(bank)));QCOMPARE(grid->count(),ActorArt::load(rom(),bank).selections().size());}
        for(int mode:{0,1}){
            kind->setCurrentIndex(mode);banks->setCurrentIndex(banks->findData(7));QCOMPARE(variants->currentData().toInt(),1);
            int checked=0;for(int row=0;row<grid->count();++row){auto item=grid->item(row);const int key=item->data(Qt::UserRole+1).toInt();if(key<401||key>408)continue;
                QVERIFY2(!item->icon().isNull(),qPrintable(item->text()));grid->setCurrentRow(row);QVERIFY(!image->pixmap().isNull());++checked;
            }QCOMPARE(checked,8);
            // Source pose 401 has four distinct frames. Real timer/VM playback must change visible pixels.
            for(int row=0;row<grid->count();++row)if(grid->item(row)->data(Qt::UserRole+1).toInt()==401)grid->setCurrentRow(row);
            const auto first=image->pixmap().toImage();QTRY_VERIFY_WITH_TIMEOUT(!image->pixmap().isNull()&&image->pixmap().toImage()!=first,2000);
            variants->setCurrentIndex(variants->findData(0));
            for(int row=0;row<grid->count();++row)if(grid->item(row)->data(Qt::UserRole+1).toInt()==401){QVERIFY(grid->item(row)->text().contains("Empty appearance"));QVERIFY(grid->item(row)->icon().isNull());}
        }
        // Original scene "Hugo assigns Magnus and Dio..." creates Dio in bank3 / appearance6.
        auto programs=ScenePrograms::load(rom());QVector<quint32> dio;
        for(auto id:programs.scenes())if(programs.displayName(id)=="Hugo assigns Magnus and Dio to clear Theodricus Mine")for(auto c:programs.program(id).commands)if(c.opcode==0x14&&c.operands[1]==3&&c.operands[8]==6){dio=c.operands;break;}
        QVERIFY(!dio.isEmpty());kind->setCurrentIndex(0);banks->setCurrentIndex(banks->findData(dio[1]));QVERIFY(variants->count()>6);
        variants->setCurrentIndex(variants->findData(dio[8]));
        int row=-1;for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole+1).toUInt()==dio[2]&&grid->item(i)->data(Qt::UserRole+2).toUInt()==dio[3])row=i;
        QVERIFY(row>=0);grid->setCurrentRow(row);QVERIFY(!image->pixmap().isNull());QCOMPARE(picker->selectedKey()[4],6);
        QVERIFY(host.grab().save("scene-key-dio.png"));
        banks->setCurrentIndex(banks->findData(7));for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole+1).toInt()==401){grid->setCurrentRow(i);grid->scrollToItem(grid->item(i),QAbstractItemView::PositionAtTop);break;}
        QTest::qWait(100);QVERIFY(host.grab().save("scene-key-bank7.png"));
    }
    void initTestCase(){if(qEnvironmentVariable("LORDLY_TEST_ROM").isEmpty())QSKIP("Pinned private ROM required");QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));applyEditorTheme(*qApp);}
    void reportedPlaybackAndLayout(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1280,850);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toUInt()==0x19aa5b6)list->setCurrentRow(i);
        auto preview=editor.findChild<ScenePreview *>();auto status=preview->findChild<QLabel *>("scenePlaybackStatus");
        auto play=preview->findChild<QPushButton *>("scenePlay");play->click();
        QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>3,qPrintable(status->text()),5000);play->setChecked(false);
        preview->seekToUpdate(220);QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=220||preview->property("sceneStopped").toBool(),qPrintable(status->text()),20000);
        QVERIFY2(!preview->property("sceneStopped").toBool(),qPrintable(status->text()));
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QVERIFY(canvas);
        for(const auto size:{QSize(1280,850),QSize(1600,1050)})for(bool advanced:{false,true}){
            editor.resize(size);editor.findChild<QCheckBox *>("cutsceneRawToggle")->setChecked(advanced);QTest::qWait(100);
            QVERIFY(canvas->viewport()->width()>=640);QVERIFY(canvas->viewport()->height()>=480);
            QVERIFY2(canvas->transform().m11()>=1.99,"The scene image must be at least 640x480");
            const auto bottom=canvas->mapTo(preview,canvas->rect().bottomLeft()).y();
            if(status->mapTo(preview,QPoint()).y()<=bottom){editor.grab().save("scene-workspace-layout-failure.png");qInfo()<<"geometry"<<size<<advanced<<preview->geometry()<<preview->minimumSizeHint()<<canvas->geometry()<<status->geometry()<<preview->parentWidget()->geometry()<<preview->parentWidget()->minimumSizeHint();}
            QVERIFY2(status->mapTo(preview,QPoint()).y()>bottom,"Status text overlaps the scene");
            auto live=preview->findChild<QComboBox *>("sceneLiveActors");QVERIFY(live->mapTo(preview,QPoint()).y()>bottom);
        }
        editor.findChild<QCheckBox *>("cutsceneRawToggle")->setChecked(false);editor.resize(1500,1000);QTest::qWait(100);
        QVERIFY(editor.grab().save("scene-workspace-fixed.png"));
        preview->seekToUpdate(6000);QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=6000||preview->property("sceneStopped").toBool(),qPrintable(status->text()),90000);
        QVERIFY2(status->text().contains("Scene released"),qPrintable(status->text()));
    }
    void keyFrameCounts(){
        SceneKeyPicker picker(rom());picker.show();const auto art=ActorArt::load(rom(),1);
        auto grid=picker.findChild<QListWidget *>("sceneKeyGrid");auto kind=picker.findChild<QComboBox *>("sceneKeyKind");
        for(int mode:{0,1}){kind->setCurrentIndex(mode);int checked=0;
            for(int i=0;i<grid->count();++i){auto item=grid->item(i);const int state=item->data(Qt::UserRole).toInt();QSet<int> expected;bool simple=true;
                for(int record=0;record<1024;++record){const auto r=art.poseRecord(state,record);if(r.opcode==0||r.opcode==4)break;if(r.opcode==5||r.opcode==22){simple=false;break;}if(r.opcode==1||r.opcode==21)expected<<u8(r.operands,0)+(r.opcode==21?256*u8(r.operands,1):0);}
                if(simple&&!expected.isEmpty()){QCOMPARE(item->data(Qt::UserRole+3).toInt(),expected.size());QVERIFY(item->text().contains(QString::number(expected.size())+" frame"));++checked;}
            }QVERIFY(checked>=8);
        }
    }
    void originalYawSetter(){
        SceneActors actors(rom(),0,0);int value=0;const QVector<int> actorSlots{0,1,27};QJsonArray initial,indices,cases;
        for(int slot:actorSlots){actors.service(0x14,{quint32(slot),30,0,0,0,0,0,0,0},value);initial<<QString::fromLatin1(actors.actor(slot)->record().toHex());indices<<slot;}
        for(quint32 selector:{0xffffffffU,1U,26U,0x10001U,0x7fffU})for(int angle:{-50000,200000,125})cases<<QJsonObject{{"selector",double(selector)},{"degrees",double(float(angle)/1000.f)},{"word",angle}};
        QTemporaryDir work;QFile input(work.filePath("input.json"));QVERIFY(input.open(QIODevice::WriteOnly));input.write(QJsonDocument(QJsonObject{{"slots",indices},{"initial",initial},{"cases",cases}}).toJson());input.close();
        QProcess process;process.start("node",{QFINDTESTDATA("actor-yaw-mips.cjs"),qEnvironmentVariable("LORDLY_TEST_ROM"),input.fileName(),work.filePath("result.json")});QVERIFY(process.waitForFinished(30000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());
        QFile output(work.filePath("result.json"));QVERIFY(output.open(QIODevice::ReadOnly));
        for(const auto &v:QJsonDocument::fromJson(output.readAll()).object()["cases"].toArray()){
            auto candidate=actors;const auto c=v.toObject();candidate.service(0x22,{quint32(c["selector"].toInteger()),0x7fffffffU,quint32(c["word"].toInt()),0x80000000U},value);
            for(int i=0;i<actorSlots.size();++i)QCOMPARE(candidate.actor(actorSlots[i])->record(),QByteArray::fromHex(c["records"].toArray()[i].toString().toLatin1()));
        }
    }
    void authoringAndUndo(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.canCreateBlank(id)){source=id;break;}
        QVERIFY(source);const auto id=programs.createScene("Workspace test",source,true);
        project.setScenePrograms(programs.json());const auto baseline=programs.program(id).source;
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);
        editor.resize(1500,1000);editor.show();
        auto scenes=editor.findChild<QListWidget *>("cutsceneScenes");QVERIFY(scenes);
        QTRY_VERIFY_WITH_TIMEOUT(scenes->count()>1000,10000);
        for(int i=0;i<scenes->count();++i)if(scenes->item(i)->data(Qt::UserRole+1).toString()==id.authoredId)scenes->setCurrentRow(i);
        auto pages=editor.findChild<QTabWidget *>("cutsceneDetailPages");QVERIFY(pages);
        QCOMPARE(pages->currentWidget()->objectName(),"cutsceneWorkspace");
        auto picker=editor.findChild<SceneKeyPicker *>();QVERIFY(picker);
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank");auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");
        banks->setCurrentIndex(banks->findData(3));QVERIFY(grid->count()>0);
        auto variants=picker->findChild<QComboBox *>("sceneKeyAppearance");variants->setCurrentIndex(variants->findData(6));
        QCOMPARE(grid->count(),ActorArt::load(rom(),3).selections().size());
        grid->setCurrentRow(0);QTest::qWait(100);
        QCOMPARE(project.scenePrograms()->program(id).source,baseline); // Audition never edits.
        auto actions=editor.findChild<SceneActionEditor *>();QVERIFY(actions);
        actions->addAtPlayhead(0xe);
        auto preview=editor.findChild<ScenePreview *>();QVERIFY(preview);preview->seekToUpdate(1);
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),1,5000);
        grid->setCurrentRow(1);grid->setCurrentRow(0);
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QVERIFY(canvas);
        QSignalSpy placed(preview,&ScenePreview::assetPlaced);
        QTest::mouseClick(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,canvas->viewport()->rect().center());
        QCOMPARE(placed.count(),1);
        QCOMPARE(placed.first().last().toInt(),6);
        bool dio=false;for(auto c:project.scenePrograms()->program(id).commands)if(c.opcode==0x14&&c.operands[1]==3){QCOMPARE(c.operands[8],6U);dio=true;}QVERIFY(dio);
        auto placedActions=SceneActions::inspect(project.scenePrograms()->program(id));
        QVERIFY(std::any_of(placedActions.begin(),placedActions.end(),[](const auto &a){return a.name=="Create Actor";}));
        QTRY_VERIFY_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=2,5000);
        QPoint actorPoint(-1,-1);
        for(int y=2;y<238&&actorPoint.x()<0;y+=3)for(int x=2;x<318;x+=3)if(preview->actorAt({x,y})==0&&preview->actorAt({x+1,y+1})==0&&preview->actorAt({x-1,y-1})==0){actorPoint={x,y};break;}
        QVERIFY2(actorPoint.x()>=0,"Placed Actor must be visible and selectable");
        const auto beforeMove=project.scenePrograms()->program(id).source;
        actions->setPlacementMode(true,12,false);
        const auto from=canvas->mapFromScene(actorPoint),to=from+QPoint(30,15);
        QSignalSpy dragged(preview,&ScenePreview::actorPlacedAtPlayhead);
        QVERIFY(canvas->viewport()->rect().contains(from));
        QTest::mousePress(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(canvas->viewport(),to,20);QTest::mouseRelease(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(dragged.count(),1);
        QVERIFY2(project.scenePrograms()->program(id).source!=beforeMove,qPrintable(editor.findChild<QLabel *>("sceneActionStatus")->text()));
        bool moved=false;for(const auto &c:project.scenePrograms()->program(id).commands)if(c.opcode==7&&c.operands[0]==0&&c.operands[5]==12)moved=true;QVERIFY(moved);
        undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,beforeMove);undo.redo();
        preview->seekToUpdate(0);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),0,5000);
        actions->addAtPlayhead(SceneActions::SetStoryFlag);
        QVERIFY(needsSceneLogic(project));QVERIFY(project.scenePrograms()->program(id).source!=baseline);
        auto flags=actions->findChild<QComboBox *>("sceneStoryFlag");QVERIFY(flags);
        auto state=actions->findChild<QComboBox *>("sceneStoryFlagState");QVERIFY(state);
        state->setCurrentIndex(state->findData(0));
        const auto edited=project.scenePrograms()->program(id).source;
        auto actual=SceneActions::inspect(project.scenePrograms()->program(id));
        QVERIFY(std::any_of(actual.begin(),actual.end(),[](const auto &a){return a.name=="Set story flag";}));
        QTemporaryDir folder;project.saveFolder(folder.path());
        auto reopened=Project::fromRom(rom());reopened.applyManifest(Project::readProject(folder.path()));
        QCOMPARE(reopened.scenePrograms()->program(id).source,edited);
        while(undo.canUndo())undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,baseline);
        while(undo.canRedo())undo.redo();QCOMPARE(project.scenePrograms()->program(id).source,edited);
        preview->seekToUpdate(3);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),3,5000);
        QVERIFY(editor.grab().save("scene-workspace.png"));
    }
    void filterCreationAndTrigger(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1280,850);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        auto rows=editor.findChild<QComboBox *>("cutsceneEventRow");QVERIFY(rows);rows->setCurrentIndex(rows->findData(4));
        bool mulsuk=false;for(int i=0;i<list->count();++i){auto item=list->item(i);if(item->toolTip().contains("selectors 610")){QVERIFY(!item->isHidden());list->setCurrentRow(i);mulsuk=true;break;}}QVERIFY(mulsuk);
        auto search=editor.findChild<QLineEdit *>("cutsceneScenesSearch");search->setText("not-a-scene-unique-test");
        auto pages=editor.findChild<QTabWidget *>("cutsceneDetailPages");QVERIFY(pages->isHidden());QVERIFY(!list->currentItem());
        search->clear();QVERIFY(!pages->isHidden());
        QTimer::singleShot(0,&editor,[&]{auto d=editor.findChild<QDialog *>("sceneCreateDialog");QVERIFY(d);d->findChild<QLineEdit *>("newSceneName")->setText("Filtered new scene");d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
        editor.findChild<QPushButton *>("sceneCreate")->click();
        QCOMPARE(rows->currentData().toInt(),-1);QVERIFY(list->currentItem());QVERIFY(!list->currentItem()->isHidden());QCOMPARE(list->currentItem()->text(),"Filtered new scene");
        const auto id=SceneId::authored(list->currentItem()->data(Qt::UserRole+1).toString());QVERIFY(id);
        const int selector=project.scenePrograms()->resource(id).selectors.first();
        const auto eventsBefore=project.sceneEvents()->json();
        QTimer::singleShot(0,&editor,[&]{auto choose=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(choose);choose->setTextValue(rows->itemText(rows->findData(4)));choose->accept();
            QTimer::singleShot(0,&editor,[]{auto d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);auto target=d->findChild<QComboBox *>("eventTreeTarget");QVERIFY(target);target->setCurrentIndex(target->findData(2));d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});});
        editor.findChild<QPushButton *>("sceneCreateTrigger")->click();
        const auto trees=project.sceneEvents()->flagEdits()["4"].toObject()["trees"].toArray();QVERIFY(!trees.isEmpty());
        const auto trigger=trees.last().toObject();QCOMPARE(trigger["group"].toInt(),1);QCOMPARE(trigger["target"].toInt(),2);QCOMPARE(trigger["nodes"].toArray().first().toObject()["scene"].toInt(),selector);
        QCOMPARE(rows->currentData().toInt(),4);QVERIFY(!list->currentItem()->isHidden());QCOMPARE(list->currentItem()->text(),"Filtered new scene");
        undo.undo();QCOMPARE(project.sceneEvents()->json(),eventsBefore);undo.redo();QVERIFY(project.sceneEvents()->json()!=eventsBefore);
        QCOMPARE(editor.size(),QSize(1280,850));QVERIFY(editor.grab().save("scene-workspace-small.png"));
    }
    void spriteAuthoring(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610))source=id;
        const auto id=programs.createScene("Sprite placement",source,true);project.setScenePrograms(programs.json());
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1500,1000);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()==id.authoredId)list->setCurrentRow(i);
        auto actions=editor.findChild<SceneActionEditor *>();auto preview=editor.findChild<ScenePreview *>();actions->addAtPlayhead(0xe);preview->seekToUpdate(1);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),1,5000);
        auto picker=editor.findChild<SceneKeyPicker *>();picker->findChild<QComboBox *>("sceneKeyKind")->setCurrentIndex(1);
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank");banks->setCurrentIndex(banks->findData(7));auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");grid->setCurrentRow(0);
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QSignalSpy placed(preview,&ScenePreview::assetPlaced);
        QTest::mouseClick(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,canvas->viewport()->rect().center());QCOMPARE(placed.count(),1);
        QCOMPARE(placed.first().last().toInt(),1);
        bool appearance=false;for(auto c:project.scenePrograms()->program(id).commands)if(c.opcode==0x46&&c.operands[1]==7){QCOMPARE(c.operands[8],1U);appearance=true;}QVERIFY(appearance);
        QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()==2,qPrintable(editor.findChild<QLabel *>("sceneActionStatus")->text()+" / "+preview->findChild<QLabel *>("scenePlaybackStatus")->text()),5000);
        QPoint point(-1,-1);for(int y=2;y<238&&point.x()<0;y+=3)for(int x=2;x<318;x+=3)if(preview->spriteAt({x,y})==0&&preview->spriteAt({x+1,y+1})==0&&preview->spriteAt({x-1,y-1})==0){point={x,y};break;}QVERIFY(point.x()>=0);
        const auto before=project.scenePrograms()->program(id).source;
        editor.findChild<QDoubleSpinBox *>("sceneMoveSeconds")->setValue(12);editor.findChild<QComboBox *>("scenePlacementTool")->setCurrentIndex(1);
        const auto from=canvas->mapFromScene(point),to=from+QPoint(30,15);QSignalSpy dragged(preview,&ScenePreview::spritePlacedAtPlayhead);
        QTest::mousePress(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(canvas->viewport(),to,20);QTest::mouseRelease(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(dragged.count(),1);QVERIFY(project.scenePrograms()->program(id).source!=before);
        bool moved=false;const auto p=project.scenePrograms()->program(id);for(const auto &a:SceneActions::inspect(p))if(a.name=="Move sprite"){QCOMPARE(p.commands[a.command+2].operands[1],12U);moved=true;}QVERIFY(moved);
        undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,before);undo.redo();
    }
    void compiledFlagDispatcher(){
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");QVERIFY(!tools.isEmpty());
        QTemporaryDir work;Compiler compiler(tools);
        const auto module=buildSceneLogicRuntime(compiler,work.path(),0x80360000,0x80000100,0x80000110);
        QFile binary(work.filePath("code.bin"));QVERIFY(binary.open(QIODevice::WriteOnly));binary.write(module.bytes);binary.close();
        QProcess process;process.start("node",{QFINDTESTDATA("scene-flags-mips.cjs"),qEnvironmentVariable("LORDLY_TEST_ROM"),binary.fileName(),QString::number(module.entries.value("func_00284288"))});
        QVERIFY(process.waitForFinished(60000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());
        qInfo().noquote()<<process.readAllStandardOutput();
    }
    void spriteMovement(){
        SceneSprites edited(rom()),control(rom());int value=0;
        const QVector<quint32> create{0,30,1,2,10,20,0,100,0};
        edited.service(0x46,create,value);control.service(0x46,create,value);
        edited.service(0x8e,{0x4c4d0000,50},value);edited.service(0x8e,{0x4c4e0000,60},value);edited.service(0x8e,{0x4c540000,4},value);
        for(int i=1;i<=4;++i){
            QVERIFY(edited.advancePoses());QVERIFY(control.advancePoses());auto expected=control.sprite(0)->record();
            put32(expected,0x24,u32(expected,0x24)+10*i);put32(expected,0x28,u32(expected,0x28)-10*i);
            QCOMPARE(edited.sprite(0)->record(),expected);
            edited.service(0x8f,{0,0,0x4c4d0000},value);QCOMPARE(value,i<4?1:0);
        }
        edited.service(0x8e,{0x4c500000,quint32(-7)},value);edited.service(0x8e,{0x4c510000,9},value);
        QCOMPARE(qint32(u32(edited.sprite(0)->record(),0x24)),-7);QCOMPARE(qint32(u32(edited.sprite(0)->record(),0x28)),-9);
        edited.service(0x8e,{0x4c4d0000,50},value);edited.service(0x8e,{0x4c4e0000,60},value);edited.service(0x8e,{0x4c540000,4},value);
        edited.service(0x66,{0},value);edited.service(0x46,create,value);control.service(0x46,create,value);
        edited.advancePoses();control.advancePoses();QCOMPARE(edited.sprite(0)->record(),control.sprite(0)->record());
    }
    void choiceBranchFlags(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();auto scenes=*project.scenePrograms();SceneId source;
        for(auto id:scenes.scenes())if(scenes.canCreateBlank(id)){source=id;break;}QVERIFY(source);
        const auto id=scenes.createScene("Choice branches",source,true);project.setScenePrograms(scenes.json());
        QUndoStack undo;SceneActionEditor editor(&project,&undo,[]{});editor.selectScene(id,-1);
        QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("sceneChoiceDialog");QVERIFY(dialog);dialog->findChild<QLineEdit *>("sceneChoiceOption1")->setText("Help the town");dialog->findChild<QLineEdit *>("sceneChoiceOption2")->setText("Leave it");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
        editor.addAtPlayhead(SceneActions::Choice);
        auto choices=SceneActions::choices(project.scenePrograms()->program(id));QCOMPARE(choices.size(),1);
        editor.selectCommand(choices.first().command);editor.addFromWorkspace(SceneActions::SetStoryFlag,3);
        auto create=editor.findChild<QPushButton *>("sceneCreateFlag");QVERIFY(create);
        QTimer::singleShot(0,&editor,[]{auto d=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue("Town help accepted");d->accept();});
        create->click();const auto flags=project.sceneEvents()->customFlags();QCOMPARE(flags.size(),1);const int flag=flags.first().toObject()["id"].toInt();
        QCOMPARE(project.sceneEvents()->flagName(flag),QString("Flag %1 (Town help accepted)").arg(flag));
        choices=SceneActions::choices(project.scenePrograms()->program(id));editor.selectCommand(choices.first().command);editor.addFromWorkspace(SceneActions::SetStoryFlag,4);
        auto flagBox=editor.findChild<QComboBox *>("sceneStoryFlag");QVERIFY(flagBox);flagBox->setCurrentIndex(flagBox->findData(flag));
        auto state=editor.findChild<QComboBox *>("sceneStoryFlagState");QVERIFY(state);state->setCurrentIndex(state->findData(0));
        const auto program=project.scenePrograms()->program(id);choices=SceneActions::choices(program);QCOMPARE(choices.size(),1);
        for(int option:{1,255}){
            DirectorSession session(project.scenePrograms()->runtimeDirectors(),project.scenePrograms()->resource(id).selectors.first(),0);
            QMap<int,int> written;
            auto service=[&](quint32 opcode,const QVector<quint32> &args,int &value){
                if(opcode==0x25)value=option;
                if(opcode==0x10)value=1;
                if(opcode==0x8e&&(args[0]&0xffff0000U)==SceneActions::storyFlagTag)written[args[0]&0xffff]=args[1];
                return true;
            };
            for(int i=0;i<30;++i){const auto status=session.dispatch(service);if(status==LORDLY_DIRECTOR_END||status==LORDLY_DIRECTOR_RELEASE)break;QVERIFY2(status==LORDLY_DIRECTOR_YIELD,qPrintable(session.reason()));}
            QCOMPARE(written.size(),1);QCOMPARE(written.value(flag,-1),option==1?1:0);
        }
        QTemporaryDir folder;project.saveFolder(folder.path());auto opened=Project::fromRom(rom());opened.applyManifest(Project::readProject(folder.path()));QCOMPARE(opened.scenePrograms()->program(id).source,program.source);QCOMPARE(opened.sceneEvents()->flagName(flag),QString("Flag %1 (Town help accepted)").arg(flag));
    }
    void appearanceExport(){
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),run=qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT");QVERIFY(!tools.isEmpty()&&!run.isEmpty());QDir().mkpath(run);
        auto project=Project::fromRom(rom());QCOMPARE(rebuild(project,tools,QDir(run).filePath("unedited")).image,rom().bytes());
        project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610)){source=id;break;}QVERIFY(source);
        const auto id=programs.createScene("Appearance placement test",source,true);project.setScenePrograms(programs.json());
        QUndoStack undo;SceneActionEditor actions(&project,&undo,{});actions.selectScene(id,SceneActions::positions(programs.program(id)).first().command);
        actions.addAsset(true,3,0,1,{-30,0,0},6);actions.addAsset(true,7,401,0,{40,0,0},1);
        auto p=project.scenePrograms()->program(id);int count=0;for(auto c:p.commands)if(c.opcode==0x46){QCOMPARE(c.operands[8],c.operands[1]==3?6U:1U);++count;}QCOMPARE(count,2);
        auto events=*project.sceneEvents();const int start=events.createFlag("Appearance test started"),done=events.createFlag("Appearance test finished");
        const int at=SceneActions::positions(p).first().command;p=SceneActions::insert(p,at,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at,0,QString::number(start));
        p=SceneActions::insert(p,at+3,0xe,240);p=SceneActions::insert(p,at+9,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at+9,0,QString::number(done));
        programs=*project.scenePrograms();programs.replace(id,p.source);project.setScenePrograms(programs.json());
        events.setTrees(4,QJsonArray{QJsonObject{{"id","appearance-test"},{"name","Appearance test"},{"group",1},{"target",2},{"nodes",QJsonArray{QJsonObject{{"type","scene"},{"scene",programs.resource(id).selectors.first()}}}}}},programs);project.setSceneEvents(events.json());
        project.saveFolder(QDir(run).filePath("project"));auto saved=Project::fromRom(rom());saved.applyManifest(Project::readProject(QDir(run).filePath("project")));QCOMPARE(saved.scenePrograms()->program(id).source,p.source);
        auto build=rebuild(project,tools,QDir(run).filePath("build"));const auto path=QDir(run).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");exportRom(project,build,path);
        const auto output=Rom::open(path);auto reopened=ScenePrograms::load(output);bool found=false;for(auto candidate:reopened.scenes())if(reopened.program(candidate).source==p.source)found=true;QVERIFY(found);
        const auto placement=ClassPlacement::read(output);QVERIFY(placement);QFile meta(QDir(run).filePath("probe.json"));QVERIFY(meta.open(QIODevice::WriteOnly));
        meta.write(QJsonDocument(QJsonObject{{"flagsAddress",double(placement->layout.customFlags)},{"startFlag",start},{"doneFlag",done},{"scene",programs.resource(id).selectors.first()}}).toJson());
    }
    void rebuildGraduationInPlace() {
        try {
            auto project = Project::fromRom(rom());
            project.loadSceneEvents();
            project.loadDialogues();
            const int selector = 2;
            const auto key = project.scenePrograms()->original().selectors().at(selector);
            const auto original = project.scenePrograms()->program(key);
            const auto blank = wipeSceneProgram(original);
            const auto event = project.sceneEvents()->program(1);
            QUndoStack undo;
            {
                CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
                editor.show();
                auto *list = editor.findChild<QListWidget *>("cutsceneScenes");
                auto *wipe = editor.findChild<QPushButton *>("sceneWipe");
                QVERIFY(list && wipe);
                QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1000, 10000);
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->data(Qt::UserRole).toUInt() == key) {
                        list->setCurrentRow(i);
                        break;
                    }
                }
                QCOMPARE(list->currentItem()->data(Qt::UserRole).toUInt(), key);
                QVERIFY(wipe->isEnabled());
                wipe->click();
                QCOMPARE(project.scenePrograms()->program(key).source, blank.source);
                undo.undo();
                QCOMPARE(project.scenePrograms()->program(key).source, original.source);
                undo.redo();
                QCOMPARE(project.scenePrograms()->program(key).source, blank.source);
            }
            int end = 0;
            while (end < blank.commands.size() && blank.commands[end].opcode != 0x7d &&
                   blank.commands[end].opcode != 0x39)
                ++end;
            QVERIFY(end < blank.commands.size());
            auto lines = *project.dialogues();
            const auto text = Dialogues::spokenText("Registrar", "Magnus, step forward and give your name.");
            const int entry = lines.appendEntry(Dialogues::Table::Primary, 5, text);
            auto replacement = SceneActions::insert(blank, end, 0xbf);
            replacement = SceneActions::edit(replacement, end, 2, QString::number(entry));
            auto scenes = *project.scenePrograms();
            scenes.replace(key, replacement.source);
            // Files written by the older editor closed this window and
            // immediately released the scene. Reopen that saved performance.
            auto legacyProgram = replacement;
            legacyProgram.commands.remove(end + 7, 9);
            const auto legacyBytes = legacyProgram.encode();
            QCOMPARE(SceneActions::edit(DirectorProgram::decode(legacyBytes), end, 2,
                                        QString::number(entry)).source, replacement.source);
            auto oldManifest = scenes.json();
            oldManifest["version"] = 1;
            oldManifest["programs"] = QJsonObject{{QString::number(key), QString::fromLatin1(legacyBytes.toHex())}};
            auto restored = *project.scenePrograms();
            restored.apply(oldManifest);
            QCOMPARE(restored.program(key).source, replacement.source);
            project.setDialogues(lines.json());
            project.setScenePrograms(scenes.json());
            QTemporaryDir temporary;
            QDir out(qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT", temporary.path()));
            QVERIFY(out.mkpath("."));
            project.saveFolder(out.filePath("project"));
            auto saved = Project::fromRom(rom());
            saved.applyManifest(Project::readProject(out.filePath("project")));
            auto built = rebuild(saved, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), out.filePath("build"));
            const auto filename = out.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(saved, built, filename);
            auto reopened = Project::fromRom(Rom::open(filename));
            reopened.loadSceneEvents();
            reopened.loadDialogues();
            const auto &library = *reopened.scenePrograms();
            const auto current = library.original().selectors().at(selector);
            QCOMPARE(library.program(current).source, replacement.source);
            QCOMPARE(reopened.sceneEvents()->program(1), event);
            ScenePreview playback(reopened.rom());
            playback.show();
            playback.selectScene(library, current, SceneImages::backgrounds(reopened.rom(), library.resource(current), -1).mode,
                                 *reopened.dialogues(), *reopened.sceneEvents());
            playback.selectEventLaunch(1, 82);
            QTRY_VERIFY_WITH_TIMEOUT(!playback.property("scenePreparing").toBool(), 10000);
            auto *step = playback.findChild<QPushButton *>("sceneStep");
            auto *confirm = playback.findChild<QPushButton *>("panelConfirm");
            auto *status = playback.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY(step && confirm && status);
            bool spoke = false;
            for (int i = 0; i < 600 && !playback.property("sceneStopped").toBool(); ++i) {
                const int state = playback.property("sceneFocusedDialogueState").toInt();
                spoke |= state == 3 || state == 6;
                if (i % 20 == 0 && confirm->isVisible()) confirm->click();
                else step->click();
            }
            QVERIFY2(spoke, qPrintable(status->text()));
            QVERIFY2(status->text().contains("returned to the game") || status->text().contains("Scene released") ||
                     status->text().contains("requested a scene"), qPrintable(status->text()));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void originalStoryVisits() {
        try {
            auto project = Project::fromRom(rom());
            project.loadSceneEvents();
            project.loadDialogues();
            auto scenes = *project.scenePrograms();
            auto events = *project.sceneEvents();
            auto dialogue = *project.dialogues();
            const SceneId source(scenes.original().selectors().at(610));
            const auto offer = scenes.createScene("The sealed dispatch", source, true);
            const auto sent = scenes.createScene("Dispatch delivered", source, true);
            const auto held = scenes.createScene("Dispatch held", source, true);
            const int seen = events.createFlag("Heard the courier");
            const int accepted = events.createFlag("Took the dispatch");
            const int delivered = events.createFlag("Courier confirmed delivery");
            const int kept = events.createFlag("Courier kept the dispatch");
            auto blank = scenes.program(offer);
            const auto backdrop = BackgroundEdit::inspect(project.rom(), blank);
            QVERIFY2(backdrop.editable(), qPrintable(backdrop.reason));
            blank = backdrop.select(project.rom(), blank, 58);
            auto exitAt = [](const DirectorProgram &p) {
                for (int i = 0; i < p.commands.size(); ++i)
                    if (p.commands[i].opcode == 0x7d) return i;
                throw Error("Missing scene exit");
            };
            // Pick two places on the actual background using the same stage tool as the editor.
            scenes.replace(offer, SceneActions::insert(blank, exitAt(blank), 0xe, 100).source);
            ScenePreview stage(project.rom());
            stage.resize(960, 760);
            stage.show();
            const auto mode = SceneImages::backgrounds(project.rom(), scenes.resource(offer), -1).mode;
            stage.selectScene(scenes, offer, mode, dialogue, events);
            stage.seekToUpdate(1);
            QTRY_COMPARE_WITH_TIMEOUT(stage.property("sceneTick").toInt(), 1, 5000);
            auto canvas = stage.findChild<QGraphicsView *>("sceneCanvas");
            QVERIFY(canvas);
            QVector<SceneVector> positions;
            connect(&stage, &ScenePreview::assetPlaced, &stage,
                    [&](bool,int,int,int,SceneVector position,int){ positions << position; });
            for (const auto x : {104, 183}) {
                stage.armAssetPlacement(false, 3, 0, 0, 6);
                const auto point = canvas->mapFromScene(QPointF(x, 139));
                QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, point);
            }
            QCOMPARE(positions.size(), 2);
            auto setFlag = [](DirectorProgram p, int at, int flag) {
                p = SceneActions::insert(p, at, SceneActions::SetStoryFlag);
                return SceneActions::edit(p, at, 0, QString::number(flag));
            };
            auto addText = [&](DirectorProgram p, quint32 kind, QByteArray text) {
                const int entry = dialogue.appendEntry(Dialogues::Table::Primary, 5, text);
                const int at = exitAt(p);
                p = SceneActions::insert(p, at, kind);
                return SceneActions::edit(p, at, 2, QString::number(entry));
            };
            auto opening = blank;
            int at = exitAt(opening);
            opening = SceneActions::insert(opening, at, 0x14);
            opening = SceneActions::edit(opening, at, 1, "3");
            opening = SceneActions::edit(opening, at, 8, "6");
            opening = SceneActions::edit(opening, at, 4, QString::number(positions[0].x, 'f', 3));
            opening = SceneActions::edit(opening, at, 5, QString::number(positions[0].y, 'f', 3));
            opening = SceneActions::edit(opening, at, 6, QString::number(positions[0].z, 'f', 3));
            at = exitAt(opening);
            opening = SceneActions::insert(opening, at, 7);
            opening = SceneActions::edit(opening, at, 3, QString::number(positions[1].x, 'f', 3));
            opening = SceneActions::edit(opening, at, 4, QString::number(positions[1].z, 'f', 3));
            opening = SceneActions::edit(opening, at, 5, "120");
            opening = SceneActions::insert(opening, exitAt(opening), 0xe, 24);
            opening = addText(opening, 0xbf, Dialogues::spokenText("Courier", "Magnus, a sealed dispatch from Zemio."));
            opening = addText(opening, SceneActions::Choice, SceneActions::choiceText("Take the dispatch", "Leave it here"));
            const auto choices = SceneActions::choices(opening);
            QCOMPARE(choices.size(), 1);
            opening = setFlag(opening, choices.first().firstEnd, accepted);
            scenes.replace(offer, opening.source);
            auto follow = [&](SceneId id, int marker, QString line) {
                auto p = setFlag(blank, exitAt(blank), marker);
                scenes.replace(id, addText(p, 0xbf, Dialogues::spokenText("Courier", line)).source);
            };
            follow(sent, delivered, "Your dispatch reached the captain.");
            follow(held, kept, "I kept the dispatch here, as ordered.");
            const QJsonObject finish{{"type","return"}};
            auto play = [&](SceneId id) { return QJsonObject{{"type","scene"},{"scene",scenes.resource(id).selectors.first()}}; };
            auto ifFlag = [](int flag, int value, QJsonArray yes, QJsonArray no) {
                return QJsonObject{{"type","if"},{"condition",QJsonObject{{"type","flag"},{"flag",flag},{"value",value}}},
                                   {"then",yes},{"else",no}};
            };
            const auto revisit = ifFlag(accepted, 1, {play(sent)}, {play(held)});
            const QJsonObject markVisit{{"type","flag"},{"flag",seen},{"value",1}};
            const auto visit = ifFlag(seen, 0, {play(offer), markVisit}, {revisit});
            const QJsonObject rule{{"id","zemio-courier"},{"name","Visit the Zemio courier"},
                                   {"group",2},{"target",1},{"after",false},{"nodes",QJsonArray{visit,finish}}};
            events.setTrees(4, {rule}, scenes);
            project.setDialogues(dialogue.json());
            project.setScenePrograms(scenes.json());
            project.setSceneEvents(events.json());
            QTemporaryDir temporary;
            const auto run = qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT", temporary.path());
            QDir out(run);
            QVERIFY(out.mkpath("."));
            project.saveFolder(out.filePath("project"));
            auto saved = Project::fromRom(rom());
            saved.applyManifest(Project::readProject(out.filePath("project")));
            auto build = rebuild(saved, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), out.filePath("build"));
            const auto filename = out.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(saved, build, filename);
            auto reopened = Project::fromRom(Rom::open(filename));
            reopened.loadSceneEvents(); reopened.loadDialogues();
            QCOMPARE(reopened.sceneEvents()->program(4), events.program(4));
            for (const auto &id : {offer, sent, held})
                QCOMPARE(reopened.scenePrograms()->program(id).source, scenes.program(id).source);
            const auto requests = EventAuthoring::sceneRequests(rule);
            QCOMPARE(requests.size(), 3);
            const int entry = EventAuthoring::entries(events.authoredBase(4), 4, {rule}, {}).value("zemio-courier", -1);
            QVERIFY(entry >= 0);
            QUndoStack undo;
            EventFlowEditor logic(&reopened, &undo, []{});
            QMap<int,int> links;
            logic.editCutscene = [&](int id,int row,int pc){ QCOMPARE(row,4); links[pc]=id; };
            logic.setRow(4); logic.selectOffset(entry); logic.show();
            QApplication::processEvents();
            auto outline = logic.findChild<QScrollArea *>("storyEventDetails")->widget()->findChild<QTreeWidget *>("storyAuthoredTree");
            QVERIFY(outline);
            for(auto button : outline->findChildren<QPushButton *>())
                if(button->text()=="Edit cutscene…") button->click();
            QCOMPARE(links.size(), 3);
            const auto bytes = reopened.sceneEvents()->program(4);
            for (const auto &offset : requests) {
                const int pc = entry + offset;
                QCOMPARE(u16(bytes, pc), 0x1000);
                QCOMPARE(links.value(pc,-1), int(u8(bytes,pc-3) | (u8(bytes,pc-1) << 8)));
            }
            ScenePreview playback(reopened.rom());
            playback.resize(1060, 820);
            playback.show();
            auto *step = playback.findChild<QPushButton *>("sceneStep");
            auto *confirm = playback.findChild<QPushButton *>("panelConfirm");
            auto *down = playback.findChild<QPushButton *>("panelDown");
            auto *status = playback.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY(step && confirm && down && status);
            for (bool take : {true, false}) {
                for (auto id : {offer, take ? sent : held}) {
                    const auto &library = *reopened.scenePrograms();
                    const auto resource = library.resource(id);
                    playback.selectScene(library, id, SceneImages::backgrounds(reopened.rom(), resource, -1).mode,
                                         *reopened.dialogues(), *reopened.sceneEvents());
                    playback.selectEventLaunch(4, links.key(resource.selectors.first(), -1));
                    QTRY_VERIFY_WITH_TIMEOUT(!playback.property("scenePreparing").toBool(), 10000);
                    bool chose = false, pressedDown = false, sawSpeech = false;
                    bool sawStart = false, sawDestination = false;
                    for (int n = 0; n < 800 && !playback.property("sceneStopped").toBool(); ++n) {
                        const int state = playback.property("sceneFocusedDialogueState").toInt();
                        sawSpeech |= state == 3 || state == 6;
                        if (id == offer && n % 4 == 0 && (!sawStart || !sawDestination)) {
                            for (int y : {115, 123, 131, 137}) {
                                sawStart |= playback.actorAt({108, y}) == 0;
                                sawDestination |= playback.actorAt({181, y}) == 0;
                            }
                        }
                        if (state == 7 && !take && !pressedDown) {
                            down->click();
                            pressedDown = true;
                        } else if (state == 7 && !chose) {
                            confirm->click();
                            chose = true;
                        } else if ((id != offer || n >= 140) && n % 20 == 0 && confirm->isVisible()) {
                            confirm->click();
                        } else {
                            step->click();
                        }
                    }
                    QVERIFY2(sawSpeech, qPrintable(status->text()));
                    QVERIFY2(status->text().contains("returned to the game") || status->text().contains("Scene released"),
                             qPrintable(status->text()));
                    if (id == offer) {
                        QVERIFY(chose);
                        QVERIFY(sawStart && sawDestination);
                        QCOMPARE(status->text().contains("Took the dispatch set"), take);
                    } else {
                        QVERIFY(!chose);
                        QVERIFY2(status->text().contains(take ? "Courier confirmed delivery set" : "Courier kept the dispatch set"),
                                 qPrintable(status->text()));
                    }
                }
            }
        } catch(const std::exception &e) { QFAIL(e.what()); }
    }
    void exportRoundTrip(){
        try {
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");QVERIFY(!tools.isEmpty());
        const auto run=qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT");QVERIFY2(!run.isEmpty(),"Run-specific local export folder required");
        QDir().mkpath(run);auto project=Project::fromRom(rom());
        auto unedited=rebuild(project,tools,QDir(run).filePath("unedited-build"));QCOMPARE(unedited.image,rom().bytes());
        project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610)){source=id;break;}QVERIFY(source);
        QVERIFY(programs.canCreateBlank(source));const auto id=programs.createScene("Scene authoring test",source,true);
        auto events=*project.sceneEvents();const int flag=events.createFlag("Scene choice accepted");project.setSceneEvents(events.json());
        const int done=events.createFlag("Scene movement finished");
        auto p=programs.program(id);int at=SceneActions::positions(p).first().command;
        p=SceneActions::insert(p,at,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at,0,QString::number(flag));
        p=SceneActions::insert(p,at+1,0x46);p=SceneActions::insert(p,at+2,SceneActions::MoveSprite,30);
        p=SceneActions::edit(p,at+2,1,"40");p=SceneActions::edit(p,at+2,2,"30");
        p=SceneActions::insert(p,at+5,0xe,90);
        p=SceneActions::insert(p,at+11,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at+11,0,QString::number(done));
        programs.replace(id,p.source);project.setScenePrograms(programs.json());
        const QJsonArray nodes{QJsonObject{{"type","scene"},{"scene",programs.resource(id).selectors.first()}}};
        const QJsonObject trigger{{"id","cutscene-workspace-test"},{"name","Cutscene workspace test"},{"group",1},{"target",2},{"nodes",nodes}};
        events.setTrees(4,QJsonArray{trigger},programs);
        project.setSceneEvents(events.json());project.saveFolder(QDir(run).filePath("project"));
        auto build=rebuild(project,tools,QDir(run).filePath("edited-build"));
        const auto filename=QDir(run).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");exportRom(project,build,filename);
        const auto output=Rom::open(filename);QVERIFY(hasSceneLogicRuntime(output));
        const auto placement=ClassPlacement::read(output);QVERIFY(placement);
        QFile meta(QDir(run).filePath("probe.json"));QVERIFY(meta.open(QIODevice::WriteOnly));meta.write(QJsonDocument(QJsonObject{{"flagsAddress",double(placement->layout.customFlags)},{"startFlag",flag},{"doneFlag",done},{"scene",programs.resource(id).selectors.first()}}).toJson());
        auto reopened=Project::fromRom(output);reopened.loadScenePrograms();reopened.loadSceneEvents();
        bool found=false;for(auto candidate:reopened.scenePrograms()->scenes())if(reopened.scenePrograms()->program(candidate).source==p.source)found=true;QVERIFY(found);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_MAIN(SceneWorkspaceTests)
#include "scene_workspace_tests.moc"
