#include "app/actor_pose_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/dialogue_inspector.h"
#include "app/dialogue_preview.h"
#include "app/encounter_editor.h"
#include "app/main_window.h"
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/scenario_editor.h"
#include "app/scene_command_editor.h"
#include "app/shop_editor.h"
#include "app/text_workspace.h"
#include "app/value_editor.h"
#include "rebuild/compiler.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
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
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace lordly;
class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        // The offscreen Qt platform does not enumerate Windows system fonts.
        QFontDatabase::addApplicationFont(
            QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));
        QApplication::setFont(QFont("Segoe UI", 10));
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
        bool sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                sawDialog = name && choices && choices->count() > 0;
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
            auto pages = editor.findChild<QTabWidget *>("scenarioPages");
            pages->setCurrentIndex(3);
            QTest::qWait(20);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            auto apply = editor.findChild<QPushButton *>("mapApply");
            auto fields = editor.findChild<QPushButton *>("mapEditFields");
            auto x = editor.findChild<QSpinBox *>("mapX"), z = editor.findChild<QSpinBox *>("mapZ");
            auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
            QVERIFY(objects && apply && fields && x && z && canvas);
            QVERIFY(objects->count() > project.scenarios()->scenario(1).squads.size());
            const auto original = project.scenarios()->json(),
                       originalWorld = project.world()->json();
            objects->setCurrentRow(0);
            fields->click();
            QCOMPARE(pages->currentIndex(), 0);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioSquads")->currentRow(), 0);
            pages->setCurrentIndex(3);
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
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, center);
            QTest::mouseMove(canvas, center + QPoint(shift, -shift), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                center + QPoint(shift, -shift));
            const auto moved = project.scenarios()->scenario(1).layout.deployments[0];
            QVERIFY(std::abs(int(u8(moved, 3)) - 192) <= 1);
            QVERIFY(std::abs(int(u8(moved, 4)) - 64) <= 1);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), centered);
            undo.redo();
            int waypointItem = -1, treasureItem = -1, siteItem = -1;
            const int nodeId = u8(project.scenarios()->scenario(1).layout.nodes[waypoint], 0);
            const auto townName =
                project.world()->townName(u16(project.world()->scenario(1).sites, 1));
            for (int i = 0; i < objects->count(); ++i) {
                const auto label = objects->item(i)->text();
                if (label.startsWith(QString("Waypoint %1").arg(nodeId)))
                    waypointItem = i;
                if (label.startsWith("Treasure 1 Â·"))
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
            QCOMPARE(pages->currentIndex(), 1);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioNodes")->currentRow(), waypoint);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(treasureItem);
            x->setValue(255);
            z->setValue(0);
            apply->click();
            auto treasure =
                ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            QCOMPARE(u8(treasure, 1), quint8(255));
            QCOMPARE(u8(treasure, 2), quint8(0));
            undo.undo();
            QCOMPARE(project.world()->json(), originalWorld);
            undo.redo();
            fields->click();
            QCOMPARE(pages->currentIndex(), 2);
            QCOMPARE(editor.findChild<QTabWidget *>("worldPages")->currentIndex(), 1);
            QCOMPARE(editor.findChild<QListWidget *>("worldTreasures")->currentRow(), 0);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(siteItem);
            QVERIFY(!apply->isEnabled());
            fields->click();
            QCOMPARE(editor.findChild<QTabWidget *>("worldPages")->currentIndex(), 0);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(0);
            QVERIFY(editor.grab().save("scenario-map.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(restored.world()->json(), project.world()->json());
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
            QCOMPARE(ScenarioWorld::treasureRecord(reopened.world()->scenario(1).treasures, 0),
                     treasure);
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
        editor.findChild<QTabWidget *>("scenarioPages")->setCurrentIndex(2);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto sites = editor.findChild<QListWidget *>("worldSites");
        auto treasures = editor.findChild<QListWidget *>("worldTreasures");
        const auto base = project.world()->json();
        for (int i = 0; i < missions->count(); ++i) {
            missions->setCurrentRow(i);
            for (int row = 0; row < sites->count(); ++row) {
                sites->setCurrentRow(row);
                button("worldSiteApply")->click();
                QCOMPARE(project.world()->json(), base);
            }
            for (int row = 0; row < treasures->count(); ++row) {
                treasures->setCurrentRow(row);
                button("worldTreasureApply")->click();
                QCOMPARE(project.world()->json(), base);
            }
        }
        QCOMPARE(undo.count(), 0);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 52)
                missions->setCurrentRow(i);
        sites->setCurrentRow(0);
        const int id = sites->currentItem()->data(Qt::UserRole).toInt();
        spin("worldPopulation")->setValue(4567);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(2));
        button("worldSiteApply")->click();
        QCOMPARE(u16(project.world()->town(id), 22), quint16(4567));
        QCOMPARE(u16(project.world()->scenario(52).sites, 3), quint16(4));
        undo.undo();
        QCOMPARE(project.world()->json(), base);
        undo.redo();
        auto objective = editor.findChild<QCheckBox *>("worldObjective");
        objective->setChecked(true);
        button("worldSiteApply")->click();
        QCOMPARE(u8(project.world()->town(id), 24), quint8(255));
        QVERIFY(!spin("worldMorale")->isEnabled());
        editor.grab().save("scenario-sites.png");
        editor.findChild<QTabWidget *>("worldPages")->setCurrentIndex(1);
        const int count = treasures->count();
        button("worldTreasureAdd")->click();
        QCOMPARE(treasures->count(), count + 1);
        const int row = treasures->currentRow();
        spin("worldTreasureX")->setValue(255);
        spin("worldTreasureZ")->setValue(0);
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        table->setCurrentIndex(table->findData(1));
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        item->setCurrentIndex(item->findData(277));
        button("worldTreasureApply")->click();
        const auto record =
            ScenarioWorld::treasureRecord(project.world()->scenario(52).treasures, row);
        QCOMPARE(u16(record, 4), quint16(277));
        QCOMPARE(u8(record, 1), quint8(255));
        editor.grab().save("scenario-treasure.png");
        button("worldTreasureRemove")->click();
        QCOMPARE(treasures->count(), count);
        undo.undo();
        QCOMPARE(treasures->count(), count + 1);
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
        editor.findChild<QTabWidget *>("scenarioPages")->setCurrentIndex(1);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = editor.findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto nodes = editor.findChild<QListWidget *>("scenarioNodes");
        auto conditions = editor.findChild<QListWidget *>("scenarioConditions");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
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
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-routes.png"));
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
        QCOMPARE(list->count(), 63);
        const auto untouched = project.scenarios()->json();
        for (int i = 0; i < list->count(); ++i) {
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
        for (int c = 1; c < 3; ++c)
            for (int p = 0; p < 3; ++p)
                select(QString("scenarioPosition_%1_%2").arg(c).arg(p), 0);
        select("scenarioGear_0_0", 277);
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
        QCOMPARE(u16(project.scenarios()->scenario(10).squads[0], 2), 277);
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.deployments[0], 4), 160);
        QCOMPARE(project.scenarios()->scenario(22).json(),
                 project.scenarios()->original(22).json());
        undo.undo();
        QCOMPARE(project.scenarios()->json(), before);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), after);
        spin("scenarioLevel_0")->setValue(127);
        button("scenarioSquadApply")->click();
        QCOMPARE(project.scenarios()->json(), after);
        QVERIFY(editor.findChild<QLabel *>("scenarioEditNotice")->text().contains("1â€“99"));
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
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-squads.png"));
        auto search = editor.findChild<QLineEdit *>("scenarioSearch");
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
        select("customScenario", 33);
        select("customTerrain", 9);
        editor.findChild<QLineEdit *>("customLabel")->setText("Raiding party");
        spin("customMemberCount")->setValue(3);
        select("customClass_0", 165);
        select("customClass_1", 71);
        spin("customLevel_1")->setValue(-10);
        select("customEquipment_0_0", 277);
        select("customEquipment_0_1", 1);
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(2, 0);
        button("customCell_0")->click();
        QCOMPARE(combo("customPosition_2")->currentData().toInt(), 0);
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
        const auto saved = project.encounters()->customSquads().profiles()[339];
        QCOMPARE(saved.members.size(), 3);
        QCOMPARE(saved.members[0].classId, 165);
        QCOMPARE(saved.members[0].equipment[0], 277);
        QCOMPARE(saved.members[1].levelOffset, -10);
        QCOMPARE(saved.members[2].cell, 0);
        QCOMPARE(saved.chance, 70);
        QCOMPARE(saved.retreatHp, 12);
        QCOMPARE(saved.bonuses[0].second, 25);
        QCOMPARE(saved.rewards.outcomes[0], 44);
        const auto before = project.manifest();
        select("customPosition_2", 6);
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(editor.findChild<QLabel *>("customNotice")->text().contains("share"));
        undo.undo();
        QVERIFY(!project.encounters()->customSquads().hasProfiles());
        undo.redo();
        QCOMPARE(project.manifest(), before);
        combo("customClass_0")->setEditText("No matching class");
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        select("customClass_0", 5);
        select("customClass_0", 165);
        QApplication::processEvents();
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
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1250, 760);
        editor.show();
        auto classes = editor.findChild<QListWidget *>("rewardClasses");
        QCOMPARE(classes->count(), 165);
        editor.findChild<QLineEdit *>("rewardSearch")->setText("New reward class");
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
        auto slot = editor.findChild<QComboBox *>("encounterSlot_31");
        auto rate = editor.findChild<QSpinBox *>("encounterRate_31");
        QVERIFY(!rate->isEnabled());
        slot->setCurrentIndex(10);
        QVERIFY(rate->isEnabled());
        rate->setValue(100);
        QCOMPARE(project.encounters()->rate(31), 100);
        slot->setCurrentIndex(0);
        QCOMPARE(project.encounters()->rate(31), 0);
        QVERIFY(!rate->isEnabled());
        undo.undo();
        QCOMPARE(rate->value(), 100);
        QCOMPARE(slot->currentData().toInt(), 10);
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
        shops->setCurrentRow(1);
        auto choice = editor.findChild<QComboBox *>("shopEquipmentChoice");
        QVERIFY(choice->findData(277) >= 0);
        choice->setCurrentIndex(choice->findData(277));
        editor.findChild<QPushButton *>("shopEquipmentAdd")->click();
        QCOMPARE(project.shops()->stock(2).equipment.last(), quint16(277));
        QCOMPARE(choice->findData(277), -1);
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
        auto consumable = editor.findChild<QComboBox *>("shopConsumablesChoice");
        consumable->setCurrentIndex(consumable->findData(44));
        QVERIFY(!consumable->currentText().startsWith("Consumable "));
        editor.findChild<QPushButton *>("shopConsumablesAdd")->click();
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
    }
    void loadDesktopWorkspace() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
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
        QCOMPARE(classes->model()->rowCount(), 165);
        QCOMPARE(items->model()->rowCount(), 278);
        QCOMPARE(consumables->model()->rowCount(), 45);
        QCOMPARE(window.findChild<QListWidget *>("encounterPools")->count(), 45);
        QCOMPARE(window.findChild<QListWidget *>("scenarioList")->count(), 63);
        auto screenshot = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window.grab().save(screenshot));
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
        editor.findChild<QComboBox *>("familyTemplate")->setCurrentIndex(0);
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[5].toInt(), 16);
        undo.undo();
        QCOMPARE(project.promotion()->families, project.promotion()->originalFamilies);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 16);
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
        const auto previousFamily = project.promotion()->families["assignments"].toArray()[165];
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Added class family");
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), 17);
        undo.undo();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165], previousFamily);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 17);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
