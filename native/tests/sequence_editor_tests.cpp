#include "app/animation_sequence_editor.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGraphicsView>
#include <QLabel>
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
