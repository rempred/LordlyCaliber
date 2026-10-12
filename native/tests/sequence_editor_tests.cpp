#include "app/animation_sequence_editor.h"
#include "app/combat_animation_editor.h"
#include "app/sprite_editor.h"
#include "app/theme.h"
#include "engine/actor_art.h"
#include "engine/actor_pose.h"
#include "engine/animation_transfer.h"
#include "engine/sprite_object.h"
#include "rebuild/compiler.h"
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
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
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
    void combatAnimationChoices() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            CombatAnimationEditor editor(&project, &undo, [] {});
            auto classes = editor.findChild<QComboBox *>("combatAnimationClass");
            auto side = editor.findChild<QComboBox *>("combatAnimationSide");
            auto variant = editor.findChild<QComboBox *>("combatAnimationVariant");
            auto choices = editor.findChild<QComboBox *>("combatAnimationChoice");
            QVERIFY(classes && side && variant && choices);
            const auto baseline = project.manifest();
            for (int id : {2, 10, 15, 30, 123}) {
                classes->setCurrentIndex(classes->findData(id));
                const auto record = project.record(Table::Classes, id).bytes;
                const int copy = u8(record, 69), owner = copy ? copy : id;
                auto selectors = project.animationRoutes()->entry(id).selectors;
                if (copy != owner || selectors.isEmpty())
                    selectors = project.animationRoutes()->entry(owner).selectors;
                QSet<int> combat{0, 5, 11, 17};
                for (int at = 0; at + 3 < selectors.size(); at += 4)
                    for (int mode = 1; mode <= 3; ++mode)
                        combat.insert(u8(selectors, at + mode));
                for (int route = 0; route < 4; ++route) {
                    side->setCurrentIndex(route & 1);
                    variant->setCurrentIndex(route >> 1);
                    for (int row = 0; row < 3; ++row) {
                        int action = u8(record, 55 + row * 2);
                        if (action == 255)
                            action = u8(project.record(Table::Classes, owner).bytes, 55 + row * 2);
                        if (action != 255)
                            for (int mode = 0; mode < 3; ++mode)
                                combat.insert(project.bodyAnimationData()->selector(
                                    id, owner, action, mode, route));
                    }
                    QVERIFY(choices->count() > 0);
                    for (int row = 0; row < choices->count(); ++row)
                        QVERIFY2(combat.contains(choices->itemData(row).toInt()),
                                 qPrintable(choices->itemText(row)));
                    QCOMPARE(choices->findData(38), -1);
                    QCOMPARE(choices->findData(88), -1);
                }
            }
            QCOMPARE(project.manifest(),
                     baseline); // Filtering and previewing never rewrite programs.
            classes->setCurrentIndex(classes->findData(2));
            const auto art =
                ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            const auto bank = AnimationDescriptor::load(project.rom(), art.descriptorKey()).poses();
            QVERIFY(choices->count() < bank.programs().size());
            // Newly assigned private attack sequences remain combat choices.
            const int action = u8(project.record(Table::Classes, 2).bytes, 55);
            auto sequences = *project.animationSequences();
            sequences.create(2, 0, *project.animationRoutes(),
                             project.combatArtwork()->previewEdits());
            project.setAnimationSequences(sequences.json());
            const int assigned = project.assignPrivateAttack(2, 0, action, false, 5);
            CombatAnimationEditor edited(&project, &undo, [] {});
            auto editedClasses = edited.findChild<QComboBox *>("combatAnimationClass");
            editedClasses->setCurrentIndex(editedClasses->findData(2));
            QVERIFY(edited.findChild<QComboBox *>("combatAnimationChoice")->findData(assigned) >=
                    0);
            edited.resize(1440, 950);
            edited.show();
            QTest::qWait(30);
            QVERIFY(edited.grab().save(QCoreApplication::applicationDirPath() +
                                       "/combat-only-choices.png"));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void visualCombatFrames() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            applyEditorTheme(*qApp);
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            CombatAnimationEditor editor(&project, &undo, [] {});
            editor.resize(1100, 780); editor.show();
            auto classes = editor.findChild<QComboBox *>("combatAnimationClass");
            classes->setCurrentIndex(classes->findData(2));
            auto choice = editor.findChild<QComboBox *>("combatAnimationChoice");
            QVERIFY(choice->findData(40) >= 0); choice->setCurrentIndex(choice->findData(40));
            auto sequence = editor.findChild<AnimationSequenceEditor *>();
            auto frames = editor.findChild<QListWidget *>("sequenceFrames");
            auto ticks = editor.findChild<QSpinBox *>("sequenceDuration");
            auto preview = editor.findChild<QWidget *>("sequencePreview");
            const auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
            const auto art = ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            const auto original = AnimationDescriptor::load(project.rom(), art.descriptorKey());
            const auto originalRecords = original.poses().effective(40).records;
            QVector<int> frameRows;
            const auto token = [](const PoseRecord &r) { return u8(r.operands, 0) + (r.opcode == 21 ? 256 * u8(r.operands, 1) : 0); };
            for (int i = 0; i < originalRecords.size(); ++i)
                if (originalRecords[i].opcode == 1 || originalRecords[i].opcode == 21) frameRows << i;
            QCOMPARE(frames->count(), frameRows.size()); QVERIFY(frames->count() > 2);
            QTest::qWait(30);
            QVERIFY(frames->isVisible()); QVERIFY(!editor.findChild<QTableWidget *>("sequenceRecords")->isVisible());
            QVERIFY(editor.grab().save("native/build/combat-frames-initial.png"));
            const auto initial = project.animationSequences()->json();
            for (int i = 0; i < frames->count(); ++i) {
                frames->scrollToItem(frames->item(i));
                QTest::mouseClick(frames->viewport(), Qt::LeftButton, Qt::NoModifier, frames->visualItemRect(frames->item(i)).center());
                QCOMPARE(frames->selectedItems().size(), 1);
                QCOMPARE(preview->property("frameToken").toInt(), token(originalRecords[frameRows[i]]));
                QCOMPARE(ticks->value(), int(u8(originalRecords[frameRows[i]].operands, originalRecords[frameRows[i]].operands.size() - 1)));
                QVERIFY(!frames->item(i)->icon().isNull());
            }
            QCOMPARE(project.animationSequences()->json(), initial); QCOMPARE(undo.count(), 0);
            frames->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
            // A click is navigation only, even before this class has a private copy.
            auto untouchedCanvas = editor.findChild<QGraphicsView *>("actorCanvas");
            QTest::mouseClick(untouchedCanvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                              untouchedCanvas->viewport()->rect().center());
            QCOMPARE(project.animationSequences()->json(), initial); QCOMPARE(undo.count(), 0);
            // Cancel the first painting operation: even automatic isolation must be atomic.
            bool painterOpened = false;
            QTimer::singleShot(30, [&] {
                if (auto dialog = editor.findChild<QDialog *>("sequenceSpriteDialog")) { painterOpened = true; dialog->reject(); }
            });
            button("sequencePaint")->click(); QVERIFY(painterOpened);
            QCOMPARE(project.animationSequences()->json(), initial); QCOMPARE(undo.count(), 0);
            const int editedTicks = (ticks->value() + 5) % 255 + 1;
            ticks->setValue(editedTicks);
            QVERIFY2(project.animationSequences()->find(2, 0), qPrintable(editor.findChild<QLabel *>("sequenceNotice")->text()));
            QCOMPARE(undo.count(), 1);
            QVERIFY(!project.animationSequences()->find(3, 0)); QVERIFY(!project.animationSequences()->find(2, 1));
            const auto afterTiming = project.animationSequences()->json();
            auto records = project.animationSequences()->find(2, 0)->poses().effective(40).records;
            QCOMPARE(int(u8(records[frameRows[0]].operands, records[frameRows[0]].operands.size() - 1)), editedTicks);
            for (int i = 0; i < records.size(); ++i) if (i != frameRows[0]) {
                QCOMPARE(records[i].opcode, originalRecords[i].opcode); QCOMPARE(records[i].operands, originalRecords[i].operands);
            }
            undo.undo(); QCOMPARE(project.animationSequences()->json(), initial);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), afterTiming);
            frames->setCurrentRow(0, QItemSelectionModel::ClearAndSelect); frames->item(1)->setSelected(true);
            QCOMPARE(frames->selectedItems().size(), 2);
            QCOMPARE(ticks->value(), editedTicks);
            ticks->setFocus(); frames->setFocus();
            QCOMPARE(project.animationSequences()->json(), afterTiming);
            ticks->setFocus();
            ticks->findChild<QLineEdit *>()->selectAll();
            QTest::keyClicks(ticks, QString::number(editedTicks));
            QTest::keyClick(ticks, Qt::Key_Return);
            QCOMPARE(undo.count(), 2); QCOMPARE(frames->selectedItems().size(), 2);
            records = project.animationSequences()->find(2, 0)->poses().effective(40).records;
            for (int i : {0, 1}) QCOMPARE(int(u8(records[frameRows[i]].operands, records[frameRows[i]].operands.size() - 1)), editedTicks);
            const auto afterBatch = project.animationSequences()->json();
            undo.undo(); QCOMPARE(project.animationSequences()->json(), afterTiming);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), afterBatch);
            frames->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
            const QPoint secondCard = frames->visualItemRect(frames->item(1)).center();
            QTest::mouseClick(frames->viewport(), Qt::LeftButton, Qt::ControlModifier, secondCard);
            QTest::mouseClick(frames->viewport(), Qt::LeftButton, Qt::ControlModifier, secondCard);
            QCOMPARE(frames->selectedItems().size(), 1);
            QCOMPARE(frames->currentRow(), 0);
            QVERIFY(button("sequencePaint")->isEnabled());
            QCOMPARE(project.animationSequences()->json(), afterBatch);
            const int count = frames->count();
            button("sequenceDuplicate")->click(); QCOMPARE(frames->count(), count + 1);
            QCOMPARE(frames->currentRow(), 1);
            const auto beforeMove = project.animationSequences()->json();
            QVector<int> order;
            for (int i = 0; i < frames->count(); ++i) order << frames->item(i)->data(Qt::UserRole + 2).toInt();
            sequence->moveVisibleFrame(1, count); order.move(1, count);
            for (int i = 0; i < frames->count(); ++i) QCOMPARE(frames->item(i)->data(Qt::UserRole + 2).toInt(), order[i]);
            QCOMPARE(frames->currentRow(), count);
            undo.undo(); QCOMPARE(project.animationSequences()->json(), beforeMove); undo.redo();
            button("sequenceDelete")->click(); QCOMPARE(frames->count(), count);
            frames->setCurrentRow(1, QItemSelectionModel::ClearAndSelect);
            const int row = frames->currentItem()->data(Qt::UserRole).toInt();
            const auto beforeNudge = *project.animationSequences()->find(2, 0);
            const int oldToken = token(beforeNudge.poses().effective(40).records[row]);
            const auto oldFrame = beforeNudge.frame(oldToken);
            // Non-default layer scales use scene-space displacement without changing the
            // existing advanced editor's native-coordinate translation behavior.
            auto scaled = beforeNudge;
            auto scaledFrame = oldFrame;
            put16(scaledFrame, 14, 512); put16(scaledFrame, 16, 2048);
            const int scaledToken = scaled.appendFrame(scaledFrame);
            auto scaledBank = scaled.poses(); scaledBank.setFrame(40, row, scaledToken, 4);
            scaled.setPoses(scaledBank);
            const auto scaledResult = translateAnimationFrames(scaled, {{40, {row}}}, {3, -2}, -1, true);
            const auto resultFrame = scaledResult.frame(token(scaledResult.poses().effective(40).records[row]));
            QCOMPARE(qint16(u16(resultFrame, 4)), qint16(u16(scaledFrame, 4)) + 6);
            QCOMPARE(qint16(u16(resultFrame, 6)), qint16(u16(scaledFrame, 6)) - 1);
            auto canvas = editor.findChild<QGraphicsView *>("actorCanvas");
            const auto artPoint = [&](int onlyLayer = -1) {
                for (auto item : canvas->scene()->items()) {
                    auto image = dynamic_cast<QGraphicsPixmapItem *>(item);
                    if (!image || !(image->flags() & QGraphicsItem::ItemIsSelectable) ||
                        (onlyLayer >= 0 && image->zValue() != onlyLayer)) continue;
                    const auto pixels = image->pixmap().toImage();
                    for (int y = 0; y < pixels.height(); ++y)
                        for (int x = 0; x < pixels.width(); ++x) if (pixels.pixelColor(x, y).alpha() > 0) {
                            const auto point = canvas->mapFromScene(image->mapToScene(QPointF(x + .5, y + .5)));
                            if (canvas->viewport()->rect().contains(point)) return point;
                        }
                }
                return QPoint(-1, -1);
            };
            canvas->setTransform(QTransform::fromScale(4, 4));
            const QPoint start = artPoint(); QVERIFY(start.x() >= 0);
            const auto beforeDrag = project.animationSequences()->json();
            const int undoIndex = undo.index();
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
            QCOMPARE(project.animationSequences()->json(), beforeDrag);
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(canvas->viewport(), start + QPoint(12, -8));
            QCOMPARE(project.animationSequences()->json(), beforeDrag); // Only a live preview so far.
            QTest::keyClick(canvas, Qt::Key_Escape);
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start + QPoint(12, -8));
            QCOMPARE(project.animationSequences()->json(), beforeDrag); QCOMPARE(undo.index(), undoIndex);
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(canvas->viewport(), start + QPoint(12, -8));
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start + QPoint(12, -8));
            QCOMPARE(undo.index(), undoIndex + 1);
            QCOMPARE(canvas->transform().m11(), 4.);
            const auto nudged = *project.animationSequences()->find(2, 0);
            const auto newFrame = nudged.frame(token(nudged.poses().effective(40).records[row]));
            QCOMPARE(nudged.frame(oldToken), oldFrame);
            for (int i = 0; i < u16(oldFrame, 0); ++i) {
                const int at = 2 + i * 16;
                QCOMPARE(qint16(u16(newFrame, at + 2)), qint16(u16(oldFrame, at + 2)) + qRound(3. * 1024 / u16(oldFrame, at + 12)));
                QCOMPARE(qint16(u16(newFrame, at + 4)), qint16(u16(oldFrame, at + 4)) + qRound(-2. * 1024 / u16(oldFrame, at + 14)));
            }
            const auto afterDrag = project.animationSequences()->json();
            undo.undo(); QCOMPARE(project.animationSequences()->json(), beforeDrag);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), afterDrag);
            // A second zoom level must produce the same world-space move, and multi-selection
            // must move each selected frame rather than a shared frame used elsewhere.
            frames->item(2)->setSelected(true);
            canvas->setTransform(QTransform::fromScale(2, 2));
            const auto start2 = artPoint(); QVERIFY(start2.x() >= 0);
            const auto batchBefore = *project.animationSequences()->find(2, 0);
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start2);
            QTest::mouseMove(canvas->viewport(), start2 + QPoint(-6, 4));
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start2 + QPoint(-6, 4));
            QCOMPARE(canvas->transform().m11(), 2.);
            const auto batchAfter = *project.animationSequences()->find(2, 0);
            for (int selected : {1, 2}) {
                const int record = frames->item(selected)->data(Qt::UserRole).toInt();
                const auto previous = batchBefore.frame(token(batchBefore.poses().effective(40).records[record]));
                const auto moved = batchAfter.frame(token(batchAfter.poses().effective(40).records[record]));
                for (int i = 0; i < u16(previous, 0); ++i) {
                    const int at = 2 + i * 16;
                    QCOMPARE(qint16(u16(moved, at + 2)), qint16(u16(previous, at + 2)) + qRound(-3. * 1024 / u16(previous, at + 12)));
                    QCOMPARE(qint16(u16(moved, at + 4)), qint16(u16(previous, at + 4)) + qRound(2. * 1024 / u16(previous, at + 14)));
                }
            }
            frames->setCurrentRow(1, QItemSelectionModel::ClearAndSelect);
            auto onion = editor.findChild<QCheckBox *>("sequenceOnionSkin"); onion->setChecked(true);
            auto scene = editor.findChild<QGraphicsView *>("actorCanvas")->scene();
            bool ghost = false; for (auto item : scene->items()) ghost |= qFuzzyCompare(item->opacity(), .25);
            QVERIFY(ghost);
            auto positionLayer = editor.findChild<QComboBox *>("actorPositionLayer");
            QVERIFY(positionLayer && positionLayer->isVisible());
            QCOMPARE(positionLayer->currentData().toInt(), -1);
            QVERIFY(positionLayer->findData(1) >= 0);
            positionLayer->setCurrentIndex(positionLayer->findData(1));
            button("actorFit")->click();
            const auto origin = canvas->mapFromScene(QPointF(0, 0));
            QVERIFY(canvas->viewport()->rect().contains(origin));
            const auto originColor = canvas->viewport()->grab().toImage().pixelColor(origin);
            QVERIFY(originColor.red() > 220 && originColor.green() > 190);
            const auto layerBefore = *project.animationSequences()->find(2, 0);
            const auto layerFrame = layerBefore.frame(token(layerBefore.poses().effective(40).records[row]));
            canvas->setTransform(QTransform::fromScale(4, 4));
            const auto fixedOrigin = canvas->mapFromScene(QPointF(0, 0));
            const auto layerStart = artPoint(1); QVERIFY(layerStart.x() >= 0);
            QMap<QGraphicsItem *, QPointF> positions;
            for (auto item : canvas->scene()->items()) positions[item] = item->pos();
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, layerStart);
            QTest::mouseMove(canvas->viewport(), layerStart + QPoint(16, 8));
            for (auto item : canvas->scene()->items())
                if (item->zValue() != 1) QCOMPARE(item->pos(), positions[item]);
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, layerStart + QPoint(16, 8));
            QCOMPARE(canvas->mapFromScene(QPointF(0, 0)), fixedOrigin);
            const auto layerAfter = *project.animationSequences()->find(2, 0);
            const auto movedLayerFrame = layerAfter.frame(token(layerAfter.poses().effective(40).records[row]));
            for (int i = 0; i < u16(layerFrame, 0); ++i) {
                const int at = 2 + i * 16;
                if (i != 1) QCOMPARE(movedLayerFrame.mid(at, 16), layerFrame.mid(at, 16));
                else {
                    QCOMPARE(qint16(u16(movedLayerFrame, at + 2)), qint16(u16(layerFrame, at + 2)) + qRound(4. * 1024 / u16(layerFrame, at + 12)));
                    QCOMPARE(qint16(u16(movedLayerFrame, at + 4)), qint16(u16(layerFrame, at + 4)) + qRound(2. * 1024 / u16(layerFrame, at + 14)));
                }
            }
            undo.undo();
            // Compare the descriptor: the surrounding project contains other animation routes.
            QCOMPARE(project.animationSequences()->find(2, 0)->json(), layerBefore.json());
            undo.redo(); QCOMPARE(project.animationSequences()->find(2, 0)->json(), layerAfter.json());
            positionLayer->setCurrentIndex(0);
            button("actorFit")->click();
            QTest::qWait(30);
            QCOMPARE(editor.size(), QSize(1100, 780));
            QVERIFY(frames->viewport()->height() > 150);
            QVERIFY(editor.grab().save("native/build/combat-frames-1100.png"));
            editor.resize(1440, 900); QTest::qWait(30);
            QVERIFY(editor.grab().save("native/build/combat-frames-1440.png"));
            frames->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
            const auto beforePlayback = project.animationSequences()->json();
            button("actorRestart")->click(); button("actorStep")->click();
            bool highlighted = false;
            for (int i = 0; i < frames->count(); ++i) highlighted |= frames->item(i)->data(Qt::UserRole + 1).toBool();
            QVERIFY(highlighted); QCOMPARE(frames->currentRow(), 0);
            QCOMPARE(project.animationSequences()->json(), beforePlayback);
            // Selecting every card cannot delete the complete playable animation.
            frames->selectAll(); const auto beforeDeleteAll = project.animationSequences()->json();
            button("sequenceDelete")->click(); QCOMPARE(project.animationSequences()->json(), beforeDeleteAll);
            QVERIFY(editor.findChild<QLabel *>("sequenceNotice")->text().contains("at least one frame"));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void guidedCombatWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            applyEditorTheme(*qApp);
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            CombatAnimationEditor editor(&project, &undo, [] {}); editor.resize(1440, 960); editor.show();
            const auto combo = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
            const auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
            auto classes = combo("combatAnimationClass"), actions = combo("combatAnimationAction"),
                 choice = combo("combatAnimationChoice"), side = combo("combatAnimationSide"), mode = combo("combatAnimationMode");
            classes->setCurrentIndex(classes->findData(2));
            const int action = u8(project.record(Table::Classes, 2).bytes, 55);
            QVERIFY(actions->findData(action) >= 0);
            actions->setCurrentIndex(actions->findData(action)); side->setCurrentIndex(1); mode->setCurrentIndex(1);
            const auto initial = project.animationSequences()->json(), overrides = project.animationOverrides()->json();
            const auto body = project.bodyAnimationData();
            const int old = body->selector(2, 2, action, 2, 1), normal = body->selector(2, 2, action, 0, 1), player = body->selector(2, 2, action, 2, 0);
            QCOMPARE(choice->currentData().toInt(), old);
            const int replacement = old == 0 ? 40 : 0;
            QVERIFY(choice->findData(replacement) >= 0); choice->setCurrentIndex(choice->findData(replacement));
            QCOMPARE(project.animationSequences()->json(), initial); QCOMPARE(project.animationOverrides()->json(), overrides); QCOMPARE(undo.count(), 0);
            const auto classText = classes->currentText(); classes->setEditText("unfinished class search");
            QVERIFY(!button("combatAnimationAssign")->isEnabled());
            QVERIFY(!editor.findChild<AnimationSequenceEditor *>()->isEnabled());
            QCOMPARE(project.animationSequences()->json(), initial);
            classes->setEditText(classText);
            button("combatAnimationAssign")->click();
            QVERIFY2(project.animationSequences()->find(2, 1), qPrintable(editor.findChild<QLabel *>("combatAnimationNotice")->text()));
            QCOMPARE(undo.count(), 1);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, action, 2, 1), replacement);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, action, 0, 1), normal);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, action, 2, 0), player);
            const auto assigned = project.animationSequences()->json();
            undo.undo(); QCOMPARE(project.animationSequences()->json(), initial); QCOMPARE(project.animationOverrides()->json(), overrides);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), assigned);
            auto records = editor.findChild<QTableWidget *>("sequenceRecords");
            int selected = -1;
            for (int i = 0; i < records->rowCount(); ++i) if (records->item(i, 0)->text() == "Frame") { selected = i; break; }
            QVERIFY(selected >= 0); records->selectRow(selected);
            const auto original = *project.animationSequences()->find(2, 1);
            const auto command = original.poses().effective(replacement).records[selected];
            const auto frameToken = [](const PoseRecord &r) { return u8(r.operands, 0) + (r.opcode == 21 ? 256 * u8(r.operands, 1) : 0); };
            const auto frame = original.frame(frameToken(command)); const int artId = u16(frame, 2);
            const auto sprite = SpriteObject::decode(original.sprite(artId));
            const auto config = original.controls()[2];
            const auto palette = original.controls()[3].mid(u8(config, u32(config, 0x1c) + artId) * 512, 512);
            const auto pixels = sprite.frame(0, palette).image;
            QPoint painted(-1, -1);
            for (int y = 0; y < pixels.height && painted.x() < 0; ++y)
                for (int x = 0; x < pixels.width; ++x) if (u8(pixels.rgba, (y * pixels.width + x) * 4 + 3)) { painted = {x, y}; break; }
            QVERIFY(painted.x() >= 0);
            QString problem;
            const auto respond = [&](bool accept) {
                QTimer::singleShot(50, [&editor, &problem, painted, accept] {
                    auto dialog = editor.findChild<QDialog *>("sequenceSpriteDialog");
                    if (!dialog) { problem = "Missing sprite painter"; return; }
                    QTimer::singleShot(5000, dialog, &QDialog::reject);
                    auto canvas = dialog->findChild<SpriteCanvas *>("sequenceSpriteCanvas");
                    if (!canvas || !canvas->isEnabled()) { problem = "Sprite canvas is unavailable"; dialog->reject(); return; }
                    canvas->setTool(SpriteCanvas::Tool::Eraser);
                    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, painted * canvas->zoom() + QPoint(2, 2));
                    auto ok = dialog->findChild<QDialogButtonBox *>("sequenceSpriteButtons")->button(QDialogButtonBox::Ok);
                    if (!ok->isEnabled()) { problem = "Painting produced no editable frame"; dialog->reject(); return; }
                    dialog->grab().save("native/build/combat-sprite-painter.png");
                    if (accept) ok->click(); else dialog->reject();
                });
            };
            respond(false); button("sequenceEditSprite")->click();
            QVERIFY2(problem.isEmpty(), qPrintable(problem)); QCOMPARE(project.animationSequences()->json(), assigned); QCOMPARE(undo.count(), 1);
            respond(true); button("sequenceEditSprite")->click();
            QVERIFY2(problem.isEmpty(), qPrintable(problem)); QCOMPARE(undo.count(), 2);
            const auto changed = *project.animationSequences()->find(2, 1);
            const auto changedFrame = changed.frame(frameToken(changed.poses().effective(replacement).records[selected]));
            QCOMPARE(editor.findChild<QWidget *>("sequencePreview")->property("frameToken").toInt(),
                     frameToken(changed.poses().effective(replacement).records[selected]));
            const int newArt = u16(changedFrame, 2); QVERIFY(newArt != artId);
            QCOMPARE(changed.sprite(artId), original.sprite(artId));
            const auto changedSprite = SpriteObject::decode(changed.sprite(newArt));
            QVERIFY(changedSprite.pixels(0) != sprite.pixels(0));
            for (int i = 1; i < sprite.children().size(); ++i) QCOMPARE(changedSprite.pixels(i), sprite.pixels(i));
            const auto after = project.animationSequences()->json();
            undo.undo(); QCOMPARE(project.animationSequences()->json(), assigned);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), after);
            QTest::qWait(30); QVERIFY(editor.grab().save("native/build/combat-animation-workflow.png"));
            button("actorStep")->click();
            QVERIFY(!editor.findChild<QLabel *>("actorStatus")->text().startsWith("Selected frame"));
            QCOMPARE(project.animationSequences()->json(), after);
            // A linked class has two pose programs for every fixed combat motion.
            classes->setCurrentIndex(classes->findData(10)); side->setCurrentIndex(0);
            actions->setCurrentIndex(actions->findData(-1)); choice->setCurrentIndex(choice->findData(5));
            const auto linkedArt = ActorArt::loadBody(project.rom(), 10, 10, 0, 0, project.bodyAnimationData());
            const auto linkedSource = AnimationDescriptor::load(project.rom(), linkedArt.descriptorKey());
            button("combatAnimationAssign")->click();
            QVERIFY2(project.animationSequences()->find(10, 0), qPrintable(editor.findChild<QLabel *>("combatAnimationNotice")->text()));
            const auto linkedTarget = *project.animationSequences()->find(10, 0);
            const auto targetArt = linkedArt.withDescriptor(linkedTarget);
            const auto appearance = linkedArt.defaultAppearance(project.rom());
            for (int offset : {0, 50}) {
                const auto from = linkedSource.poses().effective(5 + offset).records;
                const auto to = linkedTarget.poses().effective(offset).records;
                QCOMPARE(from.size(), to.size());
                for (int i = 0; i < from.size(); ++i) if (from[i].opcode == 1 || from[i].opcode == 21) {
                    const auto expected = linkedArt.bodyFrame(project.rom(), frameToken(from[i]), appearance);
                    const auto actual = targetArt.bodyFrame(project.rom(), frameToken(to[i]), appearance);
                    QCOMPARE(actual.size(), expected.size());
                    for (int j = 0; j < actual.size(); ++j) {
                        QCOMPARE(actual[j].pixels.image.rgba, expected[j].pixels.image.rgba);
                        QCOMPARE(actual[j].metadata.origin, expected[j].metadata.origin);
                    }
                }
            }
            const auto finalSequences = project.animationSequences()->json();
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopened.animationSequences()->json(), finalSequences);
            const auto built = rebuild(reopened, tools, work.path());
            QTemporaryDir candidates(QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-guided-combat-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName()); exportRom(reopened, built, output);
            const auto exported = Rom::open(output); const auto runtime = BodyAnimationData::load(exported);
            QCOMPARE(runtime->selector(2, 2, action, 2, 1), replacement);
            QCOMPARE(runtime->selector(2, 2, action, 0, 1), normal); QCOMPARE(runtime->selector(2, 2, action, 2, 0), player);
            const auto exportedArt = ActorArt::loadBody(exported, 2, 2, 0, 1, runtime);
            const auto exportedDescriptor = AnimationDescriptor::load(exported, exportedArt.descriptorKey());
            QCOMPARE(exportedDescriptor.sprite(newArt), changed.sprite(newArt));
            qInfo().noquote() << "Guided combat candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void completeSequenceReplacement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            auto asset = SpriteAsset::blank("Two distinct frames", {3, 2});
            asset.anchor = {1, 2};
            asset.frames[0].ticks = 3;
            asset.frames[0].layers[0].image.fill(Qt::red);
            auto second = asset.frames[0];
            second.id = SpriteLibrary::newId(); second.ticks = 7;
            second.layers[0].id = SpriteLibrary::newId(); second.layers[0].image.fill(Qt::blue);
            asset.frames << second;
            const auto before = project.animationSequences()->json();
            const auto respond = [](bool accept) {
                QTimer::singleShot(0, [accept] {
                    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    QVERIFY(dialog);
                    QCOMPARE(dialog->objectName(), "sequenceBatchPreparation");
                    dialog->findChild<QSpinBox *>("sequenceBatchFrame")->setValue(2);
                    QVERIFY(dialog->findChild<QLabel *>("sequenceBatchStatus")->text().contains("7 ticks"));
                    dialog->findChild<QDialogButtonBox *>("sequenceBatchButtons")
                        ->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
                });
            };
            respond(false); editor.importSequence(asset);
            QCOMPARE(project.animationSequences()->json(), before); QCOMPARE(undo.count(), 0);
            respond(true); editor.importSequence(asset);
            QCOMPARE(undo.count(), 1);
            const auto after = project.animationSequences()->json();
            const auto descriptor = *project.animationSequences()->find(2, 0);
            const auto original = AnimationDescriptor::load(project.rom(), descriptor.key());
            QCOMPARE(descriptor.poses().effective(0).source, original.poses().effective(0).source);
            const auto verify = [&](const Rom &rom, const ActorArt &art) {
                int ordinal = 0;
                for (const auto &record : art.poses()[40].records) {
                    if (record.opcode != 1 && record.opcode != 21) continue;
                    QVERIFY(ordinal < 2);
                    QCOMPARE(u8(record.operands, record.operands.size() - 1), ordinal ? 7 : 3);
                    const int token = u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
                    const auto layers = art.bodyFrame(rom, token, art.defaultAppearance(rom));
                    QCOMPARE(layers.size(), 1);
                    QCOMPARE(layers[0].metadata.origin, QPoint(-1, -2));
                    QByteArray expected;
                    for (int pixel = 0; pixel < 6; ++pixel) expected += QByteArray::fromHex(ordinal ? "0000ffff" : "ff0000ff");
                    QCOMPARE(layers[0].pixels.image.rgba, expected);
                    ++ordinal;
                }
                QCOMPARE(ordinal, 2);
            };
            verify(project.rom(), ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData()));
            undo.undo(); QCOMPARE(project.animationSequences()->json(), before);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), after);
            // Too many replacement records must not leave appended resources or another Undo entry.
            auto tooLong = asset;
            while (tooLong.frames.size() < 255) {
                auto frame = tooLong.frames.first(); frame.id = SpriteLibrary::newId();
                frame.layers[0].id = SpriteLibrary::newId(); tooLong.frames << frame;
            }
            respond(true);
            QVERIFY_THROWS_EXCEPTION(Error, editor.importSequence(tooLong));
            QCOMPARE(project.animationSequences()->json(), after); QCOMPARE(undo.count(), 1);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationSequences()->json(), after);
            const auto built = rebuild(restored, tools, work.path());
            QTemporaryDir candidates(QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-sequence-batch-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, output);
            const auto exported = Rom::open(output);
            verify(exported, ActorArt::loadBody(exported, 2, 2, 0, 0));
            qInfo().noquote() << "Complete sequence candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void copyCompareAndTranslate() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            const auto source = ActorArt::loadBody(project.rom(), 3, 3, 0, 0, project.bodyAnimationData());
            const auto sourceBytes = source.contentIdentity();
            editor.copySequence(3, 0, 40);
            QCOMPARE(undo.count(), 1);
            auto target = ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            const auto token = [](const PoseRecord &r) { return u8(r.operands, 0) + (r.opcode == 21 ? 256 * u8(r.operands, 1) : 0); };
            const auto expectedRecords = AnimationDescriptor::load(project.rom(), source.descriptorKey()).poses().effective(40).records;
            const auto copied = *project.animationSequences()->find(2, 0);
            const auto records = copied.poses().effective(40).records;
            QCOMPARE(records.size(), expectedRecords.size());
            QSet<int> frameRows;
            for (int row = 0; row < records.size(); ++row) {
                if (expectedRecords[row].opcode != 1 && expectedRecords[row].opcode != 21) continue;
                frameRows.insert(row);
                for (int equipment : {1, 8}) {
                    const BodyAppearance appearance{quint16(equipment), quint8(0)};
                    const auto a = source.bodyFrame(project.rom(), token(expectedRecords[row]), appearance);
                    const auto b = target.bodyFrame(project.rom(), token(records[row]), appearance);
                    QCOMPARE(a.size(), b.size());
                    for (int layer = 0; layer < a.size(); ++layer) {
                        QCOMPARE(a[layer].pixels.image.rgba, b[layer].pixels.image.rgba);
                        QCOMPARE(a[layer].metadata.origin, b[layer].metadata.origin);
                    }
                }
            }
            QVERIFY(frameRows.size() > 1);
            // A donor can transfer into another pose. Copying must follow that
            // control graph without changing an existing destination pose.
            auto dependent = AnimationDescriptor::load(project.rom(), source.descriptorKey());
            auto dependencyBank = dependent.poses();
            const int child = dependencyBank.appendProgram(QByteArray(1, 0));
            const auto shown = expectedRecords[*frameRows.cbegin()];
            const QByteArray jump(1, char(child)), high(1, char(child >> 8));
            dependencyBank.setProgram(child, PoseBank::encodeProgram({shown, {22, jump + high}}));
            dependencyBank.setProgram(40, PoseBank::encodeProgram({{22, jump + high}}));
            dependent.setPoses(dependencyBank);
            const auto donor = source.withDescriptor(dependent);
            const auto transferred = copyAnimationPose(project.rom(), copied, 40, dependent, donor, 40, true);
            const auto targetArt = target.withDescriptor(transferred);
            ActorPosePlayer sourcePlayer(project.rom(), donor, 40), targetPlayer(project.rom(), targetArt, 40);
            sourcePlayer.setLinkedObjectKnownAbsent(true); targetPlayer.setLinkedObjectKnownAbsent(true);
            for (int tick = 0; tick < 6; ++tick) {
                QCOMPARE(sourcePlayer.advance(), int(LORDLY_POSE_OK)); QCOMPARE(targetPlayer.advance(), int(LORDLY_POSE_OK));
                const BodyAppearance appearance{1, quint8(0)};
                const auto a = donor.bodyFrame(project.rom(), sourcePlayer.actor().frame, appearance);
                const auto b = targetArt.bodyFrame(project.rom(), targetPlayer.actor().frame, appearance);
                QCOMPARE(a.size(), b.size());
                for (int i = 0; i < a.size(); ++i) QCOMPARE(a[i].pixels.image.rgba, b[i].pixels.image.rgba);
            }
            QCOMPARE(transferred.poses().effective(0).source, copied.poses().effective(0).source);
            auto sorted = frameRows.values(); std::sort(sorted.begin(), sorted.end());
            const int selected = sorted.first(), untouched = sorted.last();
            const auto before = project.animationSequences()->json();
            editor.translateFrames({{40, {selected}}}, {5, -2});
            QCOMPARE(undo.count(), 2);
            const auto translated = *project.animationSequences()->find(2, 0);
            const auto translatedRecords = translated.poses().effective(40).records;
            QCOMPARE(token(translatedRecords[untouched]), token(records[untouched]));
            auto expected = copied.frame(token(records[selected]));
            for (int i = 0; i < u16(expected, 0); ++i) {
                const int at = 2 + i * 16;
                put16(expected, at + 2, quint16(qint16(u16(expected, at + 2)) + 5));
                put16(expected, at + 4, quint16(qint16(u16(expected, at + 4)) - 2));
            }
            QCOMPARE(translated.frame(token(translatedRecords[selected])), expected);
            QCOMPARE(ActorArt::loadBody(project.rom(), 3, 3, 0, 0, project.bodyAnimationData()).contentIdentity(), sourceBytes);
            const auto after = project.animationSequences()->json();
            undo.undo(); QCOMPARE(project.animationSequences()->json(), before);
            undo.redo(); QCOMPARE(project.animationSequences()->json(), after);
            QVERIFY_THROWS_EXCEPTION(Error, editor.translateFrames({{40, frameRows}}, {1, 1}, 4095));
            QCOMPARE(project.animationSequences()->json(), after); QCOMPARE(undo.count(), 2);
            auto canvas = editor.findChild<QGraphicsView *>("actorCanvas");
            QVERIFY(!editor.findChild<QCheckBox *>("actorAutomaticFit")->isChecked());
            editor.findChild<QSpinBox *>("actorPosition")->setValue(1);
            const auto origin = canvas->mapFromScene(QPointF(0, 0));
            const auto scale = canvas->transform();
            editor.findChild<QSpinBox *>("actorPosition")->setValue(12);
            QCOMPARE(canvas->mapFromScene(QPointF(0, 0)), origin); QCOMPARE(canvas->transform(), scale);
            editor.findChild<QTableWidget *>("sequenceRecords")->selectRow(selected);
            editor.findChild<QSpinBox *>("sequenceTicks")->setValue(99);
            editor.findChild<QPushButton *>("sequenceApplyFrame")->click();
            const auto copyOne = [&](const char *button) {
                QTimer::singleShot(0, [] {
                    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
                    auto classes = dialog->findChild<QComboBox *>("sequenceSourceClass");
                    classes->setCurrentIndex(classes->findData(3));
                    auto frame = dialog->findChild<QComboBox *>("sequenceSourceFrame");
                    QVERIFY(frame && frame->count() > 0); frame->setCurrentIndex(0);
                    dialog->findChild<QDialogButtonBox *>("sequenceSourceButtons")->button(QDialogButtonBox::Ok)->click();
                });
                editor.findChild<QPushButton *>(button)->click();
                const auto notice = editor.findChild<QLabel *>("sequenceNotice");
                QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            };
            copyOne("sequenceCopyFrameArt");
            QCOMPARE(u8(project.animationSequences()->find(2, 0)->poses().effective(40).records[selected].operands.right(1), 0), 99);
            copyOne("sequenceCopyFrameTiming");
            QCOMPARE(project.animationSequences()->find(2, 0)->poses().effective(40).records[selected].operands.right(1), expectedRecords[selected].operands.right(1));
            undo.undo(); undo.undo(); undo.undo();
            QCOMPARE(project.animationSequences()->json(), after);
            QTimer::singleShot(0, [] {
                auto sourceDialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(sourceDialog);
                sourceDialog->findChild<QDialogButtonBox *>("sequenceSourceButtons")->button(QDialogButtonBox::Ok)->click();
                QTimer::singleShot(20, [] {
                    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    QVERIFY(dialog); QCOMPARE(dialog->objectName(), "sequenceComparison");
                    auto left = dialog->findChild<QGraphicsView *>("sequenceCompareLeft");
                    auto right = dialog->findChild<QGraphicsView *>("sequenceCompareRight");
                    QCOMPARE(left->sceneRect(), right->sceneRect()); QCOMPARE(left->transform(), right->transform());
                    dialog->findChild<QSlider *>("sequenceCompareTick")->setValue(2);
                    QCOMPARE(left->property("frameIndex"), right->property("frameIndex"));
                    QVERIFY(dialog->grab().save(QDir::current().filePath("sequence-comparison.png")));
                    dialog->reject();
                });
            });
            editor.findChild<QPushButton *>("sequenceCompare")->click();
            QVERIFY2(editor.findChild<QLabel *>("sequenceNotice")->text().isEmpty(), qPrintable(editor.findChild<QLabel *>("sequenceNotice")->text()));
            QCOMPARE(project.animationSequences()->json(), after); QCOMPARE(undo.index(), 2);
            QTemporaryDir work;
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/native-sequence-copy-XXXXXX"));
            QVERIFY(candidates.isValid()); candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            const auto exported = Rom::open(output);
            const auto reopened = ActorArt::loadBody(exported, 2, 2, 0, 0);
            const auto reopenedRecords = AnimationSequences::load(exported).find(2, 0)->poses().effective(40).records;
            QCOMPARE(reopenedRecords[selected].operands, translatedRecords[selected].operands);
            const auto liveArt = ActorArt::loadBody(project.rom(), 2, 2, 0, 0, project.bodyAnimationData());
            const auto live = liveArt.bodyFrame(project.rom(), token(translatedRecords[selected]), liveArt.defaultAppearance(project.rom()));
            const auto disk = reopened.bodyFrame(exported, token(reopenedRecords[selected]), reopened.defaultAppearance(exported));
            QCOMPARE(disk.size(), live.size());
            for (int i = 0; i < disk.size(); ++i) {
                QCOMPARE(disk[i].metadata.origin, live[i].metadata.origin);
                QCOMPARE(disk[i].pixels.image.rgba, live[i].pixels.image.rgba);
            }
            qInfo().noquote() << "Copied sequence candidate:" << output;
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void copyFramesToLibrary() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationSequenceEditor editor(&project, &undo, [] {});
            editor.resize(1400, 1000); editor.show();
            const auto copy = editor.findChild<QPushButton *>("sequenceCopyToLibrary");
            QVERIFY(copy && copy->isEnabled());
            const auto sequences = project.animationSequences()->json();
            copy->click();
            auto notice = editor.findChild<QLabel *>("sequenceNotice");
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(project.sprites().assets().size(), 1);
            const auto retail = project.sprites().assets().first();
            QVERIFY(retail.frames.size() > 1);
            QCOMPARE(project.animationSequences()->json(), sequences);
            undo.undo(); QVERIFY(project.sprites().assets().isEmpty());
            undo.redo(); QCOMPARE(project.sprites().assets().first(), retail);
            editor.findChild<QPushButton *>("sequenceCreate")->click();
            auto changed = *project.animationSequences();
            auto descriptor = *changed.find(2, 0);
            auto bank = descriptor.poses();
            const auto records = bank.effective(40).records;
            for (int i = 0; i < records.size(); ++i) if (records[i].opcode == 1) {
                bank.setFrame(40, i, u8(records[i].operands, 0), 37); break;
            }
            descriptor.setPoses(bank); changed.setDescriptor(2, 0, descriptor);
            project.setAnimationSequences(changed.json());
            copy->click();
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(project.sprites().assets().size(), 2);
            const auto edited = project.sprites().assets().last();
            QCOMPARE(edited.frames.first().ticks, 37);
            QCOMPARE(edited.composite(0), retail.composite(0));
            QVERIFY(edited.id != retail.id);
            QCOMPARE(editor.findChild<QComboBox *>("sequenceLibrary")->currentData().toString(), edited.id);
            undo.undo(); QCOMPARE(project.sprites().assets().size(), 1);
            QCOMPARE(project.animationSequences()->json(), changed.json());
            undo.redo();
            QTemporaryDir work; project.saveFolder(work.path());
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.sprites().json(), project.sprites().json());
            QVERIFY(editor.grab().save(QDir::current().filePath("sequence-library-copy.png")));
        } catch (const Error &e) { QFAIL(e.what()); }
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
                QCOMPARE(project.animationSequences()->groups().value(id * 4).pairedPoses.value(100, -2), -1);
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
            const auto respond = [](bool accept, bool dither = false) {
                QTimer::singleShot(50, [accept, dither] {
                    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    if (!dialog)
                        return;
                    auto check = dialog->findChild<QCheckBox *>("sequenceImportDither");
                    QVERIFY(check);
                    QVERIFY(!check->isChecked());
                    check->setChecked(dither);
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
            respond(true, true);
            editor.findChild<QPushButton *>("sequenceImportLibrary")->click();
            auto notice = editor.findChild<QLabel *>("sequenceNotice");
            QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            QCOMPARE(project.animationSequences()->find(2, 0)->members().size(), 113);
            QCOMPARE(layers->count(), 1);
            QVERIFY(layers->item(0)->text().contains("art 108"));
            const auto expected = SpriteObject::fromImage(
                {3, 2,
                 QByteArray(reinterpret_cast<const char *>(input.constBits()),
                            input.sizeInBytes())},
                true);
            QCOMPARE(project.animationSequences()->find(2, 0)->sprite(108), expected.source());
            QVERIFY(expected.source() != project.animationSequences()->find(2, 0)->sprite(107));
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
