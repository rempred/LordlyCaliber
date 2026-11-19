#include "app/main_window.h"
#include "app/scene_preview.h"
#include "app/theme.h"
#include "engine/project.h"
#include "engine/scene_actions.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace lordly;

namespace {
QPushButton *button(QWidget *root, const char *name) {
    return root->findChild<QPushButton *>(name);
}
void click(QWidget *w) {
    QVERIFY(w && w->isVisible() && w->isEnabled());
    QTest::mouseClick(w, Qt::LeftButton);
}
void ok(QDialog *d) {
    click(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok));
}
void tab(QTabWidget *tabs, const QString &name) {
    QVERIFY(tabs);
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == name) {
            tabs->setCurrentIndex(i);
            return;
        }
    QFAIL(qPrintable("Missing workspace tab " + name));
}
void chooseFile(const QString &path) {
    auto d = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
    QVERIFY(d);
    // Use the same filename field and acceptance path as the desktop dialog.
    d->selectFile(path);
    QMetaObject::invokeMethod(d, "accept", Qt::QueuedConnection);
}
QAction *fileAction(MainWindow &w, const QString &name) {
    for (auto action : w.findChildren<QAction *>())
        if (action->text().remove('&').startsWith(name))
            return action;
    return nullptr;
}
} // namespace

class StoryAuthoringTests : public QObject {
    Q_OBJECT
  private slots:
    void lastFerry() {
        const auto input = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (input.isEmpty())
            QSKIP("US Rev 0 ROM required");
        QTemporaryDir temporary;
        QDir out(qEnvironmentVariable("LORDLY_STORY_OUTPUT", temporary.path()));
        QVERIFY(QDir().mkpath(out.filePath("project")));
        const auto archive = out.filePath("The Last Ferry.lcproject");
        const auto filename = out.filePath(QFileInfo(input).fileName());
        QVERIFY2(!QFileInfo::exists(filename), "Use a fresh output folder");
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QStandardPaths::setTestModeEnabled(true);
        applyEditorTheme(*qApp);
        QTimer dismissFailedDialog;
        connect(&dismissFailedDialog, &QTimer::timeout, [] {
            if (QTest::currentTestFailed())
                if (auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                    dialog->reject();
        });
        dismissFailedDialog.start(200);
        MainWindow window;
        window.resize(1520, 1000);
        window.show();
        window.loadRom(input);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTabWidget *>("storylineTabs"), 30000);
        auto campaign = window.findChild<QWidget *>("campaignEditor");
        tab(qobject_cast<QTabWidget *>(window.centralWidget()), "Campaign");
        tab(campaign->findChild<QTabWidget *>("campaignTabs"), "Storyline");
        tab(campaign->findChild<QTabWidget *>("storylineTabs"), "Cutscenes");
        auto scenes = campaign->findChild<QListWidget *>("cutsceneScenes");
        QTRY_VERIFY_WITH_TIMEOUT(scenes && scenes->count() > 1000, 30000);
        auto editor = campaign;
        QTimer::singleShot(0, [&] {
            auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            d->findChild<QLineEdit *>("newSceneName")->setText("The Last Ferry");
            ok(d);
        });
        click(button(editor, "sceneCreate"));
        QCOMPARE(scenes->currentItem()->text(), "The Last Ferry");
        auto stage = campaign->findChild<ScenePreview *>();
        auto canvas = stage->findChild<QGraphicsView *>("sceneCanvas");
        auto tick = stage->findChild<QSpinBox *>("scenePosition");
        auto cast = stage->findChild<QComboBox *>("sceneLiveActors");
        auto seek = [&](int at) {
            tick->setValue(at);
            QTRY_COMPARE_WITH_TIMEOUT(stage->property("sceneTick").toInt(), at, 10000);
        };
        auto stageClick = [&](QPoint at) {
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                              canvas->mapFromScene(at));
        };
        auto place = [&](int appearance, int slot, QPoint point) {
            click(button(editor, "sceneAddCast"));
            auto gallery = editor->findChild<QListWidget *>("sceneArtworkGallery");
            auto findTile = [&]() -> QListWidgetItem * {
                for (int i = 0; i < gallery->count(); ++i) {
                    auto t = gallery->item(i);
                    if (t->data(Qt::UserRole).toInt() == 3 &&
                        t->data(Qt::UserRole + 1).toInt() == appearance)
                        return t;
                }
                return nullptr;
            };
            QTRY_VERIFY_WITH_TIMEOUT(findTile(), 15000);
            gallery->scrollToItem(findTile());
            QApplication::processEvents();
            QTest::mouseClick(gallery->viewport(), Qt::LeftButton, Qt::NoModifier,
                              gallery->visualItemRect(findTile()).center());
            click(button(editor, "sceneUseSelectedKey"));
            stageClick(point);
            QTRY_COMPARE_WITH_TIMEOUT(stage->selectedObject().slot, slot, 10000);
            QTRY_VERIFY_WITH_TIMEOUT(!stage->selectedObjectRecord().isEmpty(), 10000);
            QVERIFY(QLineF(stage->selectedOrigin(), point).length() < 2);
        };
        place(0, 0, {68, 146});
        place(6, 1, {236, 146});
        auto move = [&](int slot, int start, int duration, QPoint point, bool wait) {
            seek(start);
            cast->setCurrentIndex(cast->findData(slot));
            click(button(editor, "sceneMove"));
            editor->findChild<QSpinBox *>("sceneMoveDuration")->setValue(duration);
            editor->findChild<QCheckBox *>("sceneMoveWait")->setChecked(wait);
            click(button(editor, "sceneChooseDestination"));
            stageClick(point);
            QTRY_COMPARE_WITH_TIMEOUT(stage->property("sceneTick").toInt(), start, 10000);
        };
        move(0, 12, 24, {129, 144}, false);
        move(1, 17, 27, {174, 144}, true);
        seek(55);
        for (auto [slot, at] : {qMakePair(0, QPoint(129, 144)), qMakePair(1, QPoint(174, 144))}) {
            cast->setCurrentIndex(cast->findData(slot));
            QVERIFY2(QLineF(stage->selectedOrigin(), at).length() < 2,
                     "Both actors must reach their meeting points");
        }
        auto step = [&](QString kind, QString where, int slot, QString first, QString second,
                        bool cancel = false) {
            click(button(editor, "sceneComposeStep"));
            auto kindBox = editor->findChild<QComboBox *>("sceneWorkspaceAction");
            const int k = kindBox->findText(kind, Qt::MatchStartsWith);
            QVERIFY2(k >= 0, qPrintable(kind));
            kindBox->setCurrentIndex(k);
            auto position = editor->findChild<QComboBox *>("sceneWorkspaceInsertWhere");
            const int index = position->findText(where, Qt::MatchContains);
            QVERIFY2(index >= 0, qPrintable(where));
            position->setCurrentIndex(index);
            bool opened = false;
            QTimer::singleShot(0, [&] {
                auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(d);
                opened = true;
                if (kind == "Dialogue") {
                    d->findChild<QLineEdit *>("sceneDialogueSpeaker")->setText(first);
                    d->findChild<QPlainTextEdit *>("sceneDialogueText")->setPlainText(second);
                    auto who = d->findChild<QComboBox *>("sceneDialogueCast");
                    const int entry = who->findData(slot);
                    QVERIFY(entry >= 0);
                    who->setCurrentIndex(entry);
                    auto faces = d->findChild<QComboBox *>("sceneDialoguePortrait");
                    QVERIFY(faces);
                    const int face = slot == 1 ? faces->findData(84)
                                               : faces->findText("Knight ·", Qt::MatchStartsWith);
                    QVERIFY2(face >= 0, qPrintable(faces->toolTip()));
                    QVERIFY(!faces->itemIcon(face).isNull());
                    faces->setCurrentIndex(face);
                } else {
                    d->findChild<QLineEdit *>("sceneChoiceOption1")->setText(first);
                    d->findChild<QLineEdit *>("sceneChoiceOption2")->setText(second);
                }
                if (cancel)
                    d->reject();
                else
                    ok(d);
            });
            click(button(editor, "sceneWorkspaceAdd"));
            QVERIFY(opened);
            if (!cancel)
                QVERIFY2(
                    stage->findChild<QLabel *>("scenePlacementHelp")->text().startsWith("Added"),
                    qPrintable(stage->findChild<QLabel *>("scenePlacementHelp")->text()));
        };
        step("Dialogue", "End of scene", 0, "Gatekeeper",
             "The east gate is barred. Which way do we go?");
        step("Dialogue", "End of scene", 1, "Dio",
             "The ferry still runs. Or we can raise the gate.");
        step("Choice", "End of scene", -1, "Raise the east gate", "Take the ferry");
        step("Dialogue", "Take the ferry", 1, "Dio", "This canceled line must never appear.", true);
        step("Dialogue", "Take the ferry", 1, "Dio",
             "The boat is waiting. Follow me to the river.");
        step("Dialogue", "Raise the east gate", 0, "Gatekeeper",
             "I have the key. The east gate is yours.");
        auto timeline = editor->findChild<QGraphicsView *>("sceneTimeline");
        QVERIFY(timeline);
        auto clipHasText = [&](const QString &text) {
            for (auto item : timeline->scene()->items())
                if (item->toolTip().contains(text))
                    return true;
            return false;
        };
        QVERIFY(clipHasText("Gatekeeper: The east gate is barred."));
        QVERIFY(clipHasText("Raise the east gate / Take the ferry"));
        QVERIFY(clipHasText("If “Take the ferry”\nDio: The boat is waiting."));
        QVERIFY(clipHasText("If “Raise the east gate”\nGatekeeper: I have the key."));

        auto rows = editor->findChild<QComboBox *>("cutsceneEventRow");
        const auto rowName = rows->itemText(rows->findData(4));
        QTimer::singleShot(0, [&] {
            auto row = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(row);
            row->setTextValue(rowName);
            row->accept();
            QTimer::singleShot(0, [&] {
                auto rule = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(rule);
                auto target = rule->findChild<QComboBox *>("eventTreeTarget");
                int site = target->findText("Mulsuk", Qt::MatchContains);
                QVERIFY2(site >= 0, qPrintable(target->currentText()));
                target->setCurrentIndex(site);
                ok(rule);
            });
        });
        click(button(editor, "sceneCreateTrigger"));
        QCOMPARE(rows->currentData().toInt(), 4);
        auto file = [&](QString action, QString path) {
            auto a = fileAction(window, action);
            QVERIFY(a);
            QTimer::singleShot(0, [path] { chooseFile(path); });
            a->trigger();
        };
        file("Save project folder", out.filePath("project"));
        QVERIFY(QFileInfo::exists(out.filePath("project/project.json")));
        file("Save sharing archive", archive);
        QVERIFY(QFileInfo::exists(archive));
        file("Open project", archive);
        auto exportAction = fileAction(window, "Build and export ROM");
        QVERIFY(exportAction);
        QTimer::singleShot(0, [&] {
            auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            auto confirm = button(d, "confirmRomExport");
            QTRY_VERIFY_WITH_TIMEOUT(confirm && confirm->isEnabled(), 60000);
            QTimer::singleShot(0, [filename] { chooseFile(filename); });
            click(confirm);
        });
        exportAction->trigger();
        auto done = button(&window, "romBuildClose");
        QTRY_VERIFY_WITH_TIMEOUT(done && done->isEnabled(), 120000);
        QVERIFY2(QFileInfo::exists(filename),
                 qPrintable(window.findChild<QPlainTextEdit *>("romBuildLog")->toPlainText()));
        click(done);

        // Read the exported artifact independently of the widgets which authored it.
        const auto manifest = Project::readProject(archive);
        auto saved = Project::fromRom(Rom::open(input));
        saved.applyManifest(manifest);
        auto exported = Project::fromRom(Rom::open(filename));
        exported.loadScenePrograms();
        exported.loadDialogues();
        const auto authored = saved.scenePrograms()->authoredScenes();
        QCOMPARE(authored.size(), 1);
        const auto id = SceneId::authored(authored.first().id);
        const auto program = exported.scenePrograms()->program(id);
        QCOMPARE(program.source, saved.scenePrograms()->program(id).source);
        const auto choices = SceneActions::choices(program);
        QCOMPARE(choices.size(), 1);
        const auto &question = program.commands[choices.first().command].operands;
        const auto answers = exported.dialogues()->entry(Dialogues::Table::Primary, question[1], question[2]);
        QVERIFY(!answers.contains("{T05}"));
        auto olderAnswers = answers;
        olderAnswers.replace("1. ", "1. {T05}").replace("2. ", "2. {T05}");
        QCOMPARE(SceneActions::choiceOptions(olderAnswers), SceneActions::choiceOptions(answers));
        int moves = 0;
        QStringList lines;
        for (int i = 0; i < program.commands.size(); ++i) {
            const auto &c = program.commands[i];
            if (c.opcode == 7 && c.operands[5] > 0)
                ++moves;
            if (c.opcode != 0xbf || i == choices.first().command)
                continue;
            const auto line = Dialogues::spokenFields(exported.dialogues()->entry(
                Dialogues::Table::Primary, c.operands[1], c.operands[2]));
            QVERIFY(line);
            lines << line->second;
            QCOMPARE(c.operands[3], line->first == "Dio" ? 1U : 0U);
            if (line->first == "Dio")
                QCOMPARE(c.operands[10], 84U);
            else
                QCOMPARE(saved.record(Table::Classes, int(c.operands[10])).name, QString("Knight"));
            QCOMPARE(c.operands[12], 0U);
            if (line->second.contains("key"))
                QCOMPARE(choices.first().option(i), 1);
            if (line->second.contains("boat"))
                QCOMPARE(choices.first().option(i), 2);
        }
        QCOMPARE(moves, 2);
        QCOMPARE(lines.size(), 4);
        QVERIFY(!lines.join(" ").contains("canceled"));
        auto bytes = exported.rom().bytes();
        Rom::updateChecksum(bytes);
        QCOMPARE(bytes, exported.rom().bytes());
    }
};
QTEST_MAIN(StoryAuthoringTests)
#include "story_authoring_tests.moc"
