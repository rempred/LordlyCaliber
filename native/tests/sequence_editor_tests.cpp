#include "app/animation_sequence_editor.h"
#include "engine/actor_art.h"
#include "engine/sprite_object.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
class SequenceEditorTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    }
    void attackAssignmentsAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1400, 1000);
            editor.show();
            const auto button = [&](const char *key) {
                return editor.findChild<QPushButton *>(key);
            };
            button("sequenceCreate")->click();
            auto actions = editor.findChild<QComboBox *>("sequenceAttack");
            actions->setCurrentIndex(actions->findData(4));
            const auto before = project.animationOverrides()->json();
            button("sequenceAssignAttack")->click();
            auto notice = editor.findChild<QLabel *>("sequenceNotice");
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(editor.findChild<QComboBox *>("sequencePose")->currentData().toInt(), 100);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 0, 0), 100);
            undo.undo();
            QCOMPARE(project.animationOverrides()->json(), before);
            undo.redo();
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 0, 0), 100);
            editor.findChild<QComboBox *>("sequenceAttackMode")->setCurrentIndex(1);
            button("sequenceAssignAttack")->click();
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 4, 2, 0), 101);
            const auto assigned = project.animationSequences()->json(),
                       overrides = project.animationOverrides()->json();
            button("sequenceRestoreAttack")->click();
            QCOMPARE(project.animationOverrides()->json(), before);
            undo.undo();
            QCOMPARE(project.animationOverrides()->json(), overrides);
            button("sequenceRemoveCopy")->click();
            QVERIFY(!project.animationSequences()->find(2, 0));
            QCOMPARE(project.animationOverrides()->json(), before);
            undo.undo();
            QCOMPARE(project.animationSequences()->json(), assigned);
            QCOMPARE(project.animationOverrides()->json(), overrides);
            QCOMPARE(editor.findChild<QComboBox *>("sequencePose")->currentData().toInt(), 101);
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QVERIFY(editor.grab().save(QDir::current().filePath("private-attack-assignment.png")));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void linkedCompanionEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1400, 1000); editor.show();
            auto classes = editor.findChild<QComboBox *>("sequenceClass");
            classes->setCurrentIndex(classes->findData(10));
            editor.findChild<QPushButton *>("sequenceCreate")->click();
            auto poses = editor.findChild<QComboBox *>("sequencePose");
            poses->setCurrentIndex(poses->findData(40));
            auto actions = editor.findChild<QComboBox *>("sequenceAttack");
            actions->setCurrentIndex(actions->findData(4));
            editor.findChild<QPushButton *>("sequenceAssignAttack")->click();
            const auto notice = editor.findChild<QLabel *>("sequenceNotice");
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(poses->currentData().toInt(), 100);
            QVERIFY(editor.findChild<QLabel *>("sequenceAssignmentSummary")->text().contains("linked pose 256"));
            poses->setCurrentIndex(poses->findData(256));
            QCOMPARE(poses->currentData().toInt(), 256);
            const auto before = project.animationSequences()->json();
            auto rows = editor.findChild<QTableWidget *>("sequenceRecords");
            int frame = 0;
            const auto program = project.animationSequences()->find(10, 0)->poses().effective(256);
            while (frame < program.records.size() && program.records[frame].opcode != 1 && program.records[frame].opcode != 21) ++frame;
            QVERIFY(frame < program.records.size()); rows->selectRow(frame);
            auto ticks = editor.findChild<QSpinBox *>("sequenceTicks");
            ticks->setValue(ticks->value() == 19 ? 20 : 19);
            editor.findChild<QPushButton *>("sequenceApplyFrame")->click();
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QVERIFY(project.animationSequences()->json() != before);
            const auto bank = project.animationSequences()->find(10, 0)->poses();
            QCOMPARE(bank.effective(90).source, program.source);
            QCOMPARE(project.bodyAnimationData()->routes().privatePose(10, 10, 0, 100), 256);
            undo.undo(); QCOMPARE(project.animationSequences()->json(), before);
            undo.redo();
            editor.findChild<QSpinBox *>("actorPosition")->setValue(1);
            QApplication::processEvents();
            QVERIFY(editor.grab().save(QDir::current().filePath("linked-private-attack.png")));
            editor.findChild<QPushButton *>("sequenceRemoveCopy")->click();
            QVERIFY(!project.animationSequences()->find(10, 0));
            undo.undo();
            QCOMPARE(project.bodyAnimationData()->routes().privatePose(10, 10, 0, 100), 256);
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void fixedOwnerPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1400, 1000); editor.show();
            auto classes = editor.findChild<QComboBox *>("sequenceClass");
            for (int id : {135, 161, 160}) {
                classes->setCurrentIndex(classes->findData(id));
                editor.findChild<QPushButton *>("sequenceCreate")->click();
                auto poses = editor.findChild<QComboBox *>("sequencePose");
                poses->setCurrentIndex(poses->findData(0));
                auto actions = editor.findChild<QComboBox *>("sequenceAttack");
                actions->setCurrentIndex(actions->findData(4));
                editor.findChild<QPushButton *>("sequenceAssignAttack")->click();
                const auto notice = editor.findChild<QLabel *>("sequenceNotice");
                QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
                QCOMPARE(poses->currentData().toInt(), 100);
                editor.findChild<QSpinBox *>("actorPosition")->setValue(1);
                const auto status = editor.findChild<QLabel *>("actorStatus")->text();
                QVERIFY2(status.contains("state 100"), qPrintable(status));
                QCOMPARE(editor.findChild<QComboBox *>("actorOwner")->currentData().toInt(), id == 160 ? 19 : -1);
                undo.undo();
                QVERIFY(project.animationSequences()->groups().value(id * 4).pairedPoses.isEmpty());
                undo.redo();
                QCOMPARE(project.animationSequences()->groups().value(id * 4).pairedPoses.value(100, -2), id == 160 ? -2 : -1);
            }
            editor.findChild<QSpinBox *>("actorPosition")->setValue(1);
            QApplication::processEvents();
            QVERIFY(editor.grab().save(QDir::current().filePath("fixed-private-attack.png")));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void importedFrameAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1300, 880);
            editor.show();
            editor.findChild<QPushButton *>("sequenceCreate")->click();
            auto rows = editor.findChild<QTableWidget *>("sequenceRecords");
            rows->selectRow(0);
            const auto before = project.animationSequences()->json();
            const int beforeUndo = undo.count(), beforeRows = rows->rowCount();
            QTemporaryDir folder;
            const auto file = folder.filePath("new.png");
            QImage input(3, 2, QImage::Format_RGBA8888);
            input.fill(QColor(171, 205, 239, 128));
            QVERIFY(input.save(file));
            const auto respond = [](bool accept) {
                QTimer::singleShot(50, [accept] {
                    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    if (!dialog)
                        return;
                    auto buttons = dialog->findChild<QDialogButtonBox *>();
                    buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)
                        ->click();
                });
            };
            respond(false);
            editor.importImage(file);
            QCOMPARE(project.animationSequences()->json(), before);
            QCOMPARE(undo.count(), beforeUndo);
            respond(true);
            editor.importImage(file);
            QCOMPARE(undo.count(), beforeUndo + 1);
            QCOMPARE(rows->rowCount(), beforeRows + 1);
            QCOMPARE(rows->item(0, 1)->text(), "32");
            QCOMPARE(rows->item(1, 1)->text(), "17");
            const auto *value = project.animationSequences()->find(2, 0);
            QCOMPARE(value->members().size(), 112);
            QCOMPARE(u16(value->frame(32), 2), 107);
            QCOMPARE(SpriteObject::decode(value->sprite(107)).frame(0).image.rgba,
                     QByteArray::fromHex("adceef80adceef80adceef80adceef80adceef80adceef80"));
            const auto imported = project.animationSequences()->json();
            editor.findChild<QSpinBox *>("actorPosition")->setValue(1);
            auto layers = editor.findChild<QListWidget *>("actorLayers");
            QCOMPARE(layers->count(), 1);
            QVERIFY(layers->item(0)->text().contains("art 107"));
            undo.undo();
            QCOMPARE(project.animationSequences()->json(), before);
            undo.redo();
            QCOMPARE(project.animationSequences()->json(), imported);
            auto library = project.sprites();
            library.replace(SpriteAsset::fromImage("Imported example", input));
            project.setSprites(library);
            undo.undo();
            undo.redo(); // Refresh the asset choices through the shared undo path.
            respond(true);
            editor.findChild<QPushButton *>("sequenceImportLibrary")->click();
            auto notice = editor.findChild<QLabel *>("sequenceNotice");
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(project.animationSequences()->find(2, 0)->members().size(), 113);
            QCOMPARE(layers->count(), 1);
            QVERIFY(layers->item(0)->text().contains("art 108"));
            project.saveFolder(folder.filePath("project"));
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(folder.filePath("project")));
            QCOMPARE(reopened.animationSequences()->json(), project.animationSequences()->json());
            QVERIFY(editor.grab().save(QDir::current().filePath("private-frame-import.png")));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void markerLayerPreviewAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadAnimationRoutes();
            auto sequences = *project.animationSequences();
            sequences.create(2, 0, *project.animationRoutes());
            auto snapshot = *sequences.find(2, 0);
            auto bank = snapshot.poses();
            bank.setProgram(38, PoseBank::encodeProgram({{1, QByteArray::fromHex("0002")},
                                                         {1, QByteArray::fromHex("1102")}}));
            snapshot.setPoses(bank);
            sequences.setDescriptor(2, 0, snapshot);
            project.setAnimationSequences(sequences.json());
            const auto art =
                ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            const auto expected =
                art.bodyFrame(project.rom(), 17, art.defaultAppearance(project.rom()));
            QVERIFY(!expected.isEmpty());
            const auto before = sequences.json();
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1300, 880);
            editor.show();
            auto poses = editor.findChild<QComboBox *>("sequencePose");
            poses->setCurrentIndex(poses->findData(38));
            QVERIFY(editor.findChild<QLabel *>("sequenceSummary")->text().contains("warm tint"));
            editor.findChild<QTableWidget *>("sequenceRecords")->selectRow(1);
            QString error;
            QTimer::singleShot(50, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) {
                    error = "Missing substitution-layer dialog";
                    return;
                }
                try {
                    auto x = dialog->findChild<QSpinBox *>("layerField0_1");
                    x->setValue(x->value() + 7);
                    auto scene = dialog->findChild<QGraphicsView *>("layerPreview")->scene();
                    int layer = 0;
                    for (auto item : scene->items(Qt::AscendingOrder)) {
                        auto pixels = qgraphicsitem_cast<QGraphicsPixmapItem *>(item);
                        if (!pixels)
                            continue;
                        if (layer >= expected.size())
                            throw Error("Unexpected substituted layer");
                        const auto &want = expected[layer++];
                        const auto &image = want.pixels.image;
                        const QImage expectedImage(
                            reinterpret_cast<const uchar *>(image.rgba.constData()), image.width,
                            image.height, image.width * 4, QImage::Format_RGBA8888);
                        if (pixels->pixmap().toImage() !=
                            QPixmap::fromImage(expectedImage).toImage())
                            throw Error("Draft pixels differ from the substituted body frame");
                        if (pixels->pos() !=
                            QPointF(want.metadata.origin.x() * want.metadata.scaleX / 1024.,
                                    want.metadata.origin.y() * want.metadata.scaleY / 1024.))
                            throw Error("Draft position differs from the substituted body frame");
                    }
                    if (layer != expected.size())
                        throw Error("Missing substituted layer");
                    dialog->findChild<QDialogButtonBox *>("layerDialogButtons")
                        ->button(QDialogButtonBox::Ok)
                        ->click();
                } catch (const std::exception &e) {
                    error = QString::fromUtf8(e.what());
                    dialog->reject();
                }
            });
            editor.findChild<QPushButton *>("sequenceEditLayers")->click();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(undo.count(), 1);
            const auto *edited = project.animationSequences()->find(2, 0);
            QCOMPARE(edited->poses().effective(38).records.last().operands,
                     QByteArray::fromHex("2002"));
            const auto updated = art.withDescriptor(*edited);
            QCOMPARE(updated.bodyFrame(project.rom(), 32, updated.defaultAppearance(project.rom()))
                         .first()
                         .pixels.image.rgba,
                     expected.first().pixels.image.rgba);
            undo.undo();
            QCOMPARE(project.animationSequences()->json(), before);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void editPreviewAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            int changes = 0;
            AnimationSequenceEditor editor(&project, &undo, [&] { ++changes; });
            editor.resize(1300, 880);
            editor.show();
            QTest::qWait(50);
            const auto box = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
            const auto button = [&](const char *name) {
                return editor.findChild<QPushButton *>(name);
            };
            const auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
            auto notice = editor.findChild<QLabel *>("sequenceNotice");
            auto rows = editor.findChild<QTableWidget *>("sequenceRecords");
            QVERIFY(rows);
            QVERIFY(notice);
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(box("sequenceClass")->currentData().toInt(), 2);
            QCOMPARE(box("sequencePose")->currentData().toInt(), 40);
            QCOMPARE(rows->item(0, 1)->text(), "17");
            QCOMPARE(rows->item(0, 2)->text(), "6");
            QVERIFY(!button("sequenceApplyFrame")->isEnabled());
            QTest::mouseClick(button("sequenceCreate"), Qt::LeftButton);
            QVERIFY2(project.animationSequences()->find(2, 0), qPrintable(notice->text()));
            QCOMPARE(undo.count(), 1);
            spin("actorPosition")->setValue(2);
            QCOMPARE(spin("actorPosition")->value(), 2);
            rows->selectRow(0);
            spin("sequenceTicks")->setValue(2);
            QTest::mouseClick(button("sequenceApplyFrame"), Qt::LeftButton);
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(undo.count(), 2);
            QCOMPARE(rows->item(0, 2)->text(), "2");
            QCOMPARE(spin("actorPosition")->value(), 2);
            QCOMPARE(project.animationSequences()
                         ->find(2, 0)
                         ->poses()
                         .effective(40)
                         .records.first()
                         .operands,
                     QByteArray::fromHex("1102"));
            undo.undo();
            QCOMPARE(rows->item(0, 2)->text(), "6");
            QCOMPARE(spin("actorPosition")->value(), 2);
            undo.redo();
            QCOMPARE(rows->item(0, 2)->text(), "2");
            const int count = rows->rowCount();
            QTest::mouseClick(button("sequenceDuplicateFrame"), Qt::LeftButton);
            QCOMPARE(rows->rowCount(), count + 1);
            QCOMPARE(rows->item(1, 1)->text(), "17");
            rows->selectRow(1);
            QTest::mouseClick(button("sequenceMoveDown"), Qt::LeftButton);
            QCOMPARE(rows->item(1, 1)->text(), "18");
            QCOMPARE(rows->item(2, 1)->text(), "17");
            rows->selectRow(2);
            QTest::mouseClick(button("sequenceRemoveFrame"), Qt::LeftButton);
            QCOMPARE(rows->rowCount(), count);
            QCOMPARE(rows->item(1, 1)->text(), "18");
            rows->selectRow(2);
            const int retained = undo.count();
            const auto safe = project.animationSequences()->json();
            QTest::mouseClick(button("sequenceRemoveFrame"), Qt::LeftButton);
            QCOMPARE(undo.count(), retained);
            QCOMPARE(project.animationSequences()->json(), safe);
            QVERIFY(!notice->text().isEmpty());
            box("sequenceClass")->setEditText("Not a class");
            QTest::mouseClick(button("sequenceRemoveCopy"), Qt::LeftButton);
            QCOMPARE(undo.count(), retained);
            QCOMPARE(project.animationSequences()->json(), safe);
            box("sequenceClass")
                ->setCurrentText(box("sequenceClass")->itemText(box("sequenceClass")->findData(2)));
            QTest::mouseClick(button("sequenceRemoveCopy"), Qt::LeftButton);
            QVERIFY(!project.animationSequences()->find(2, 0));
            QVERIFY(!button("sequenceApplyFrame")->isEnabled());
            undo.undo();
            QVERIFY(project.animationSequences()->find(2, 0));
            box("sequenceRoute")->setCurrentIndex(1);
            QVERIFY(button("sequenceCreate")->isEnabled());
            QVERIFY(!button("sequenceRemoveCopy")->isEnabled());
            box("sequenceRoute")->setCurrentIndex(0);
            QTemporaryDir folder;
            project.saveFolder(folder.path());
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(reopened.animationSequences()->json(), project.animationSequences()->json());
            QCOMPARE(reopened.appendClass(2, "Private clone"), 165);
            QCOMPARE(reopened.animationSequences()->find(165, 0)->controls(),
                     project.animationSequences()->find(2, 0)->controls());
            QVERIFY(changes >= 6);
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            rows->selectRow(0);
            const auto sourceFrame = project.animationSequences()->find(2, 0)->frame(17);
            const int x = qint16(u16(sourceFrame, 4)), oldFlags = u16(sourceFrame, 12);
            QString dialogError;
            QTimer::singleShot(50, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) {
                    dialogError = "Missing layer dialog";
                    return;
                }
                try {
                    auto table = dialog->findChild<QTableWidget *>("frameLayers");
                    auto view = dialog->findChild<QGraphicsView *>("layerPreview");
                    if (!table || !view)
                        throw Error("Missing layer controls");
                    const auto image = view->grab().toImage();
                    dialog->findChild<QSpinBox *>("layerField0_1")->setValue(x + 9);
                    dialog->findChild<QCheckBox *>("layerFlip0_0")->toggle();
                    dialog->findChild<QDoubleSpinBox *>("layerScale0_0")->setValue(.5);
                    if (image == view->grab().toImage())
                        throw Error("Layer preview did not change");
                    const int count = table->rowCount();
                    for (const char *name : {"layerDuplicate", "layerMoveDown", "layerRemove",
                                             "layerAdd", "layerMoveUp", "layerRemove"})
                        dialog->findChild<QPushButton *>(name)->click();
                    if (table->rowCount() != count)
                        throw Error("Layer editing lost an original layer");
                    if (!dialog->findChild<QLabel *>("layerNotice")->text().isEmpty())
                        throw Error(dialog->findChild<QLabel *>("layerNotice")->text());
                    if (!dialog->grab().save(QDir::current().filePath("private-frame-layers.png")))
                        throw Error("Layer screenshot failed");
                    dialog->findChild<QDialogButtonBox *>("layerDialogButtons")
                        ->button(QDialogButtonBox::Ok)
                        ->click();
                } catch (const std::exception &e) {
                    dialogError = QString::fromUtf8(e.what());
                    dialog->reject();
                }
            });
            QTest::mouseClick(button("sequenceEditLayers"), Qt::LeftButton);
            QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            const auto *edited = project.animationSequences()->find(2, 0);
            QCOMPARE(edited->frame(17), sourceFrame);
            QCOMPARE(edited->poses().effective(40).records.first().operands,
                     QByteArray::fromHex("2002"));
            const auto privateFrame = edited->frame(32);
            QCOMPARE(qint16(u16(privateFrame, 4)), x + 9);
            QCOMPARE(u16(privateFrame, 12), oldFlags ^ 1);
            QCOMPARE(u16(privateFrame, 14), 512);
            const auto afterLayers = project.animationSequences()->json();
            const int afterCount = undo.count();
            QTimer::singleShot(50, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) {
                    dialogError = "Missing validation dialog";
                    return;
                }
                dialog->findChild<QSpinBox *>("layerField0_3")->setValue(65535);
                dialog->findChild<QSpinBox *>("layerField0_4")->setValue(65535);
                dialog->findChild<QDialogButtonBox *>("layerDialogButtons")
                    ->button(QDialogButtonBox::Ok)
                    ->click();
                if (!dialog->isVisible() ||
                    dialog->findChild<QLabel *>("layerNotice")->text().isEmpty())
                    dialogError = "Oversized layer was accepted";
                dialog->reject();
            });
            QTest::mouseClick(button("sequenceEditLayers"), Qt::LeftButton);
            QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
            QCOMPARE(project.animationSequences()->json(), afterLayers);
            QCOMPARE(undo.count(), afterCount);
            undo.undo();
            QCOMPARE(project.animationSequences()->find(2, 0)->frameCount(), 32);
            undo.redo();
            QCOMPARE(project.animationSequences()->json(), afterLayers);
            QVERIFY(editor.grab().save(QDir::current().filePath("private-sequence-editor.png")));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_MAIN(SequenceEditorTests)
#include "sequence_editor_tests.moc"
