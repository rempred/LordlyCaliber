#include "app/consumable_editor.h"
#include "app/theme.h"
#include "engine/consumable_tree.h"
#include "engine/consumable_tree_runtime.h"
#include "engine/consumable_visual.h"
#include "rebuild/compiler.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>
using namespace lordly;
class ConsumableTreeTests : public QObject {
    Q_OBJECT
    static bool dropEffect(QTreeWidget *tree, QTreeWidgetItem *source, QTreeWidgetItem *target) {
        std::unique_ptr<QMimeData> mime(tree->model()->mimeData({tree->indexFromItem(source)}));
        if (!mime) return false;
        tree->scrollToItem(target);
        const QPoint point = tree->visualItemRect(target).center();
        QDragEnterEvent enter(QPointF(point), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &enter);
        if (!enter.isAccepted()) return false;
        QDragMoveEvent move(QPointF(point), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &move);
        if (!move.isAccepted()) {
            QDragLeaveEvent leave;
            QApplication::sendEvent(tree->viewport(), &leave);
            return false;
        }
        QDropEvent drop(point, Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &drop);
        return drop.isAccepted();
    }
    static QJsonArray example() {
        return {QJsonObject{{"type", "if"},
                            {"condition", QJsonObject{{"type", "class"}, {"value", 5}}},
                            {"then", QJsonArray{QJsonObject{
                                         {"type", "heal"}, {"minimum", 40}, {"maximum", 60}}}},
                            {"else", QJsonArray{QJsonObject{
                                         {"type", "strength"}, {"minimum", 2}, {"maximum", 3}}}}},
                QJsonObject{{"type", "fatigue"}, {"minimum", 10}, {"maximum", 10}}};
    }
  private slots:
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        qApp->setFont(QFont("Segoe UI", 10));
        applyEditorTheme(*qApp);
    }
    void retailAndValidation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto p = Project::fromRom(Rom::open(path));
        p.loadConsumableEffects();
        const auto before = p.manifest();
        auto e = *p.consumableEffects();
        for (int id = 1; id < 20; id++)
            if (ConsumableTree::supported(id)) {
                const auto t = e.tree(id);
                QVERIFY(!t.isEmpty());
                ConsumableTree::validate(t);
                e.setTree(id, t);
            }
        p.setConsumableEffects(e.json());
        QCOMPARE(p.manifest(), before);
        QVERIFY(!e.hasTrees());
        QCOMPARE(e.tree(6)[0].toObject()["then"].toArray()[0].toObject()["type"].toString(),
                 QString("cure"));
        QCOMPARE(e.tree(11)[0].toObject()["then"].toArray()[0].toObject()["type"].toString(),
                 QString("strength"));
        QCOMPARE(e.tree(16)[0].toObject()["then"].toArray()[0].toObject()["type"].toString(),
                 QString("dexterity"));
        QVERIFY_THROWS_EXCEPTION(Error,
                                 ConsumableTree::validate({QJsonObject{{"type", "attack"}}}));
        auto highClass = example();
        auto branch = highClass[0].toObject();
        branch["condition"] = QJsonObject{{"type", "class"}, {"value", 255}};
        highClass[0] = branch;
        ConsumableTree::validate(highClass);
        branch["condition"] = QJsonObject{{"type", "class"}, {"value", 256}};
        highClass[0] = branch;
        QVERIFY_THROWS_EXCEPTION(Error, ConsumableTree::validate(highClass));
        e.setTree(1, example());
        p.setConsumableEffects(e.json());
        const auto changed = p.manifest();
        auto restored = Project::fromRom(p.rom());
        restored.applyManifest(changed);
        restored.loadConsumableEffects();
        QCOMPARE(restored.consumableEffects()->json(), e.json());
    }
    void editorUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto p = Project::fromRom(Rom::open(path));
        p.loadConsumableEffects();
        QUndoStack undo;
        ConsumableEditor editor(&p, &undo, [] {});
        editor.selectConsumable(1);
        const auto before = p.manifest();
        QTimer::singleShot(0, [&] {
            auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            auto tree = d->findChild<QTreeWidget *>("itemEffectTree");
            tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0)->child(0));
            d->findChild<QSpinBox *>("itemEffectMinimum")->setValue(250);
            d->findChild<QSpinBox *>("itemEffectMaximum")->setValue(250);
            d->grab().save(QCoreApplication::applicationDirPath() + "/consumable-tree-dialog.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        editor.findChild<QPushButton *>("editConsumableTree")->click();
        QVERIFY(p.consumableEffects()->hasTrees());
        const auto changed = p.manifest();
        undo.undo();
        QCOMPARE(p.manifest(), before);
        undo.redo();
        QCOMPARE(p.manifest(), changed);
    }
    void editOnlyClickedBranch() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadConsumableEffects();
        auto effects = *project.consumableEffects();
        const auto heal = QJsonObject{{"type", "heal"}, {"minimum", 50}, {"maximum", 50}};
        const auto branch =
            QJsonObject{{"type", "if"},
                        {"condition", QJsonObject{{"type", "hpPercent"}, {"value", 50}}},
                        {"then", QJsonArray{heal}},
                        {"else", QJsonArray{heal}}};
        effects.setTree(3, {branch});
        project.setConsumableEffects(effects.json());
        const auto before = project.manifest();
        QUndoStack undo;
        ConsumableEditor editor(&project, &undo, [] {});
        editor.resize(600, 650);
        editor.selectConsumable(3);
        editor.show();
        auto overview = editor.findChild<QTreeWidget *>("consumableTreeOverview");
        auto second = overview->topLevelItem(0)->child(1)->child(1)->child(0);
        QVERIFY(!overview->visualItemRect(second).isEmpty());
        bool correctBranch = false;
        QTimer::singleShot(0, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto tree = dialog->findChild<QTreeWidget *>("itemEffectTree");
            correctBranch = tree->currentItem() == tree->topLevelItem(0)->child(0)->child(1)->child(0);
            dialog->findChild<QCheckBox *>("itemEffectRandomRange")->setChecked(true);
            dialog->findChild<QSpinBox *>("itemEffectMinimum")->setValue(10);
            dialog->findChild<QSpinBox *>("itemEffectMaximum")->setValue(25);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        QTest::mouseClick(overview->viewport(), Qt::LeftButton, Qt::NoModifier,
                          overview->visualItemRect(second).center());
        QVERIFY(correctBranch);
        const auto edited = project.consumableEffects()->tree(3).at(0).toObject();
        QCOMPARE(edited["then"].toArray(), QJsonArray{heal});
        QCOMPARE(edited["condition"], branch["condition"]);
        const auto other = edited["else"].toArray().at(0).toObject();
        QCOMPARE(other["minimum"].toInt(), 10);
        QCOMPARE(other["maximum"].toInt(), 25);
        const auto after = project.manifest();
        undo.undo();
        QCOMPARE(project.manifest(), before);
        undo.redo();
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(project.manifest());
        QCOMPARE(reopened.manifest(), after);
    }
    void effectAndConditionLabels() {
        const auto cases = QMap<QString, QPair<QString, QString>>{
            {"luck", {"Change luck by -3 to 2", "IF Luck is at least 65"}},
            {"alignment", {"Change alignment by -3 to 2", "IF Alignment is at least 65"}},
            {"fatigue", {"Reduce fatigue by 3 to 5", "IF Fatigue is at least 65"}}};
        for (auto it = cases.begin(); it != cases.end(); ++it) {
            const bool fatigue = it.key() == "fatigue";
            const QJsonObject effect{{"type", it.key()}, {"minimum", fatigue ? 3 : -3},
                                     {"maximum", fatigue ? 5 : 2}};
            const QJsonObject branch{{"type", "if"},
                                     {"condition", QJsonObject{{"type", it.key()}, {"value", 65}}},
                                     {"then", QJsonArray{effect}}, {"else", QJsonArray{}}};
            QCOMPARE(ConsumableTree::label(branch), it.value().second);
            QCOMPARE(ConsumableTree::label(effect), it.value().first);
        }
    }
    void addAtSelectedBranch() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadConsumableEffects();
        const auto original = project.consumableEffects()->tree(19)[0].toObject();
        const auto luck = original["then"].toArray()[0].toObject();
        QUndoStack undo;
        ConsumableEditor editor(&project, &undo, [] {});
        editor.selectConsumable(19);
        auto overview = editor.findChild<QTreeWidget *>("consumableTreeOverview");
        const auto displayed = overview->topLevelItem(0)->child(1)->child(0)->child(0)->text(0);
        QVERIFY(displayed.startsWith("Change luck by "));
        QTimer::singleShot(0, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            const auto dismiss = qScopeGuard([&] { if (dialog->isVisible()) dialog->reject(); });
            auto tree = dialog->findChild<QTreeWidget *>("itemEffectTree");
            auto add = dialog->findChild<QPushButton *>("itemAddEffect");
            auto addIf = dialog->findChild<QPushButton *>("itemAddIf");
            auto amount = dialog->findChild<QSpinBox *>("itemEffectMinimum");
            auto root = tree->topLevelItem(0);
            auto condition = root->child(0);
            auto then = condition->child(0);
            QCOMPARE(then->child(0)->text(0), displayed);
            QCOMPARE(tree->currentItem(), condition);
            QCOMPARE(add->text(), QString("Add effect to THEN"));
            add->click();
            amount->setValue(33);
            tree->setCurrentItem(then->child(0));
            QCOMPARE(add->text(), QString("Add effect below in THEN"));
            add->click();
            amount->setValue(17);
            tree->setCurrentItem(then);
            QCOMPARE(addIf->text(), QString("Add IF / ELSE to THEN"));
            addIf->click();
            auto nested = tree->currentItem();
            add->click();
            amount->setValue(45);
            tree->setCurrentItem(nested->child(1));
            QCOMPARE(add->text(), QString("Add effect to ELSE"));
            add->click();
            amount->setValue(60);
            tree->setCurrentItem(root);
            QCOMPARE(add->text(), QString("Add effect at top level"));
            add->click();
            amount->setValue(8);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        editor.findChild<QPushButton *>("editConsumableTree")->click();
        const auto result = project.consumableEffects()->tree(19);
        QCOMPARE(result.size(), 2);
        const auto condition = result[0].toObject();
        QCOMPARE(condition["condition"], original["condition"]);
        QCOMPARE(condition["else"], original["else"]);
        const auto then = condition["then"].toArray();
        QCOMPARE(then.size(), 4);
        QCOMPARE(then[0].toObject(), luck);
        QCOMPARE(then[1].toObject()["minimum"].toInt(), 17);
        QCOMPARE(then[2].toObject()["minimum"].toInt(), 33);
        const auto nested = then[3].toObject();
        QCOMPARE(nested["then"].toArray().at(0).toObject()["minimum"].toInt(), 45);
        QCOMPARE(nested["else"].toArray().at(0).toObject()["minimum"].toInt(), 60);
        QCOMPARE(result[1].toObject()["minimum"].toInt(), 8);
        undo.undo();
        QCOMPARE(project.consumableEffects()->tree(19), QJsonArray{original});
        undo.redo();
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(project.manifest());
        QCOMPARE(reopened.consumableEffects()->tree(19), result);
    }
    void dragExistingEffectAcrossBranches() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadConsumableEffects();
        const QJsonObject heal{{"type","heal"}, {"minimum",12}, {"maximum",37}, {"percent",true}};
        const auto strength = ConsumableTree::action("strength");
        const QJsonObject nested{{"type","if"}, {"condition",ConsumableTree::condition("alive")},
                                 {"then",QJsonArray{strength}}, {"else",QJsonArray{}}};
        const QJsonObject outer{{"type","if"}, {"condition",ConsumableTree::condition("ordinary")},
                                {"then",QJsonArray{nested}}, {"else",QJsonArray{}}};
        const QJsonArray initial{heal,outer};
        auto effects = *project.consumableEffects();
        effects.setTree(19, initial);
        project.setConsumableEffects(effects.json());
        QUndoStack undo;
        ConsumableEditor editor(&project, &undo, [] {});
        editor.resize(690, 820);
        editor.selectConsumable(19);
        editor.show();
        auto tree = editor.findChild<QTreeWidget *>("consumableTreeOverview");
        auto when = tree->topLevelItem(0);
        auto origin = when->child(1);  // TARGET precedes the effects.
        auto nestedRow = when->child(2)->child(0)->child(0);
        QVERIFY(!dropEffect(tree, nestedRow, when));
        QVERIFY(!dropEffect(tree, origin, nestedRow->child(0)->child(0)));
        QCOMPARE(undo.count(), 0);
        QVERIFY(dropEffect(tree, origin, nestedRow->child(1)));  // Empty nested ELSE.
        auto withEffect = [&](bool inElse) {
            auto inner = nested;
            inner[inElse ? "else" : "then"] = inElse ? QJsonArray{heal} : QJsonArray{strength, heal};
            auto condition = outer;
            condition["then"] = QJsonArray{inner};
            return QJsonArray{condition};
        };
        QCOMPARE(project.consumableEffects()->tree(19), withEffect(true));
        QCOMPARE(undo.count(), 1);
        undo.undo();
        QCOMPARE(project.consumableEffects()->tree(19), initial);
        undo.redo();
        for (bool accept : {false, true}) {
            QTimer::singleShot(0, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                const auto dismiss = qScopeGuard([&] { if (dialog->isVisible()) dialog->reject(); });
                auto editTree = dialog->findChild<QTreeWidget *>("itemEffectTree");
                auto inner = editTree->topLevelItem(0)->child(0)->child(0)->child(0);
                auto effect = inner->child(1)->child(0);
                QVERIFY(dropEffect(editTree, effect, inner->child(0)));
                QCOMPARE(editTree->currentItem(), effect);
                QCOMPARE(effect->parent(), inner->child(0));
                auto buttons = dialog->findChild<QDialogButtonBox *>();
                buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
            });
            editor.findChild<QPushButton *>("editConsumableTree")->click();
            QCOMPARE(project.consumableEffects()->tree(19), withEffect(!accept));
        }
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(project.manifest());
        QCOMPARE(reopened.consumableEffects()->tree(19), withEffect(false));
        when = tree->topLevelItem(0);
        nestedRow = when->child(1)->child(0)->child(0);
        QVERIFY(dropEffect(tree, nestedRow->child(0)->child(1), when));
        QCOMPARE(project.consumableEffects()->tree(19), (QJsonArray{outer, heal}));
        // Applying a pending native amount rebuilds the overview during the drop.
        editor.selectConsumable(1);
        editor.setAdvancedVisible(true);
        editor.findChild<QSpinBox *>("effectMinimum")->setValue(123);
        auto condition = tree->topLevelItem(0)->child(1);
        QVERIFY(dropEffect(tree, condition->child(0)->child(0), condition->child(1)));
        const auto moved = project.consumableEffects()->tree(1).at(0).toObject();
        QVERIFY(moved["then"].toArray().isEmpty());
        QCOMPARE(moved["else"].toArray().size(), 1);
        QCOMPARE(moved["else"].toArray().at(0).toObject()["minimum"].toInt(), 123);
        QCOMPARE(moved["else"].toArray().at(0).toObject()["maximum"].toInt(), 123);
    }
    void visualConversion() {
        QImage input(64, 32, QImage::Format_RGBA8888);
        input.fill(Qt::red);
        const auto fit = ConsumableVisual::fit(input, false);
        QCOMPARE(fit.size(), QSize(32, 32));
        QCOMPARE(fit.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(fit.pixelColor(0, 8), QColor(Qt::red));
        QCOMPARE(fit.pixelColor(31, 23), QColor(Qt::red));
        QCOMPARE(fit.pixelColor(0, 24).alpha(), 0);
        QVector<QImage> frames;
        for (int n = 0; n < 4; n++) {
            auto image = input;
            image.fill(n % 2 ? Qt::blue : Qt::red);
            frames << image;
        }
        const auto visual = ConsumableVisual::fromImages("Independent effect", frames, false);
        QCOMPARE(visual.pixels.size(), 2048);
        QCOMPARE(visual.palette.size(), 32);
        QCOMPARE(visual.frame(0).pixelColor(16, 16), QColor(Qt::red));
        QCOMPARE(visual.frame(1).pixelColor(16, 16), QColor(Qt::blue));
        QCOMPARE(ConsumableVisual::fromJson(visual.json()).json(), visual.json());
        auto bad = visual.json();
        bad["pixels"] = "AAAA";
        QVERIFY_THROWS_EXCEPTION(Error, ConsumableVisual::fromJson(bad));
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto p = Project::fromRom(Rom::open(path));
        p.loadConsumableEffects();
        auto e = *p.consumableEffects();
        e.setVisual(2, visual.json());
        e.setVisual(3, visual.json());
        auto renamed = visual;
        renamed.name = "Changed copy";
        e.setVisual(3, renamed.json());
        QCOMPARE(e.visuals()["2"].toObject(), visual.json());
        p.setConsumableEffects(e.json());
        auto reopened = Project::fromRom(p.rom());
        reopened.applyManifest(p.manifest());
        reopened.loadConsumableEffects();
        QCOMPARE(reopened.consumableEffects()->json(), e.json());
        auto old = e.json();
        old["version"] = 2;
        QVERIFY_THROWS_EXCEPTION(Error, e.apply(old));
    }
    void visualEditorUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto p = Project::fromRom(Rom::open(path));
        p.loadConsumableEffects();
        QUndoStack undo;
        ConsumableEditor editor(&p, &undo, [] {});
        editor.selectConsumable(2);
        const auto before = p.manifest();
        auto open = editor.findChild<QPushButton *>("editConsumableVisual");
        QVERIFY(open);
        QTimer::singleShot(0, [&] {
            auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        open->click();
        QCOMPARE(p.manifest(), before);
        QTimer::singleShot(0, [&] {
            auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            QTimer::singleShot(0, [&] {
                auto picker = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(picker);
                QVERIFY(picker->findChild<QListWidget *>("effectLibrarySources")->isVisible());
                QTimer::singleShot(0, [&] {
                    auto rom = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    QVERIFY(rom);
                    QCOMPARE(rom->objectName(), QString("romArtImport"));
                    rom->findChild<QTabWidget *>()->setCurrentIndex(1);
                    rom->findChild<QComboBox *>("romArtCategory")->setCurrentIndex(1);
                    rom->findChild<QListWidget *>("romArtList")->setCurrentRow(0);
                    rom->findChild<QPushButton *>("romArtImportSelected")->click();
                });
                picker->findChild<QPushButton *>("itemUseOtherRomArt")->click();
            });
            d->findChild<QPushButton *>("itemVisualImportSequence")->click();
            QCOMPARE(p.manifest(), before);
            d->findChild<QLineEdit *>("itemVisualName")->setText("Imported item artwork");
            d->grab().save(QCoreApplication::applicationDirPath() +
                           "/consumable-visual-dialog.png");
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        open->click();
        QVERIFY(p.consumableEffects()->hasVisuals());
        QVERIFY(!p.consumableEffects()->hasTrees());
        const auto changed = p.manifest();
        undo.undo();
        QCOMPARE(p.manifest(), before);
        undo.redo();
        QCOMPARE(p.manifest(), changed);
        editor.findChild<QPushButton *>("restoreConsumableVisual")->click();
        QCOMPARE(p.manifest(), before);
    }
    void exportAndInstructions_data() {
        QTest::addColumn<bool>("expanded");
        QTest::addColumn<bool>("authoredTrees");
        QTest::newRow("ordinary") << false << true;
        QTest::newRow("expanded-armies") << true << true;
        QTest::newRow("visual-only") << false << false;
    }
    void exportAndInstructions() {
        QFETCH(bool, expanded);
        QFETCH(bool, authoredTrees);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM/toolchain not supplied");
        auto p = Project::fromRom(Rom::open(path));
        p.loadConsumableEffects();
        auto e = *p.consumableEffects();
        e.setTree(1, example());
        e.setTree(7, {QJsonObject{
                          {"type", "revive"}, {"minimum", 50}, {"maximum", 50}, {"percent", true}},
                      QJsonObject{{"type", "heal"}, {"minimum", 25}, {"maximum", 25}}});
        p.setConsumableEffects(e.json());
        e.setTree(3, {QJsonObject{{"type", "heal"}, {"minimum", 90}, {"maximum", 90}},
                      QJsonObject{{"type", "fatigue"}, {"minimum", 5}, {"maximum", 5}}});
        p.setConsumableEffects(e.json());
        if (!authoredTrees)
            for (const int id : {1, 3, 7})
                e.restoreTree(id);
        QImage art(64, 32, QImage::Format_RGBA8888);
        art.fill(Qt::red);
        e.setVisual(
            2, ConsumableVisual::fromImages("Red item effect", {art, art, art, art}, false).json());
        art.fill(Qt::blue);
        e.setVisual(
            3,
            ConsumableVisual::fromImages("Blue item effect", {art, art, art, art}, false).json());
        p.setConsumableEffects(e.json());
        if (expanded) {
            p.loadScenarios();
            auto scenarios = *p.scenarios();
            while (scenarios.scenario(1).squads.size() < 30)
                scenarios.addSquad(1, 0, p.classData());
            p.setScenarios(scenarios.json());
            QVERIFY(p.expandedArmies());
        }
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const auto built = rebuild(p, tools, work.path());
        auto reopened = Project::fromRom(Rom::fromBytes(built.image));
        reopened.loadConsumableEffects();
        QCOMPARE(reopened.consumableEffects()->json(), p.consumableEffects()->json());
        const auto placement = ConsumableTreePlacement::read(reopened.rom());
        QVERIFY(placement.has_value());
        auto target = qEnvironmentVariable("LORDLY_TREE_TEST_OUTPUT", work.filePath("tree.z64"));
        if (expanded || !authoredTrees)
            target = QFileInfo(target).absoluteDir().filePath(
                QString(expanded ? "expanded/" : "visual-only/") + QFileInfo(target).fileName());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile f(target);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(built.image);
        f.close();
        QJsonObject retailPrograms;
        const auto retailEffects = ConsumableEffects::load(p.rom());
        for (int id = 1; id < 20; id++)
            if (ConsumableTree::supported(id))
                retailPrograms[QString::number(id)] =
                    QString::fromLatin1(ConsumableTree::compile(retailEffects.tree(id)).toHex());
        QFile programs(work.filePath("retail-programs.json"));
        QVERIFY(programs.open(QIODevice::WriteOnly));
        programs.write(QJsonDocument(retailPrograms).toJson());
        programs.close();
        if (authoredTrees) {
            QProcess node;
            node.setProgram("node");
            node.setArguments({QString(LORDLY_SOURCE_DIR) + "/tests/consumable-tree-mips.cjs",
                               target, programs.fileName()});
            node.start();
            QVERIFY(node.waitForFinished(120000));
            QVERIFY2(node.exitCode() == 0,
                     qPrintable(QString::fromUtf8(node.readAllStandardError()) +
                                QString::fromUtf8(node.readAllStandardOutput())));
        }
        QFile visualFixture(work.filePath("visual-fixture.json"));
        QVERIFY(visualFixture.open(QIODevice::WriteOnly));
        visualFixture.write(
            QJsonDocument(
                QJsonObject{{"pixels",
                             QString::fromLatin1(ConsumableVisual::retailPixels(p.rom()).toHex())},
                            {"palettes", QString::fromLatin1(
                                             ConsumableVisual::retailPalettes(p.rom()).toHex())}})
                .toJson());
        visualFixture.close();
        QProcess node;
        node.setProgram("node");
        node.setArguments({QString(LORDLY_SOURCE_DIR) + "/tests/consumable-visual-mips.cjs", target,
                           visualFixture.fileName()});
        node.start();
        QVERIFY(node.waitForFinished(120000));
        QVERIFY2(node.exitCode() == 0, qPrintable(QString::fromUtf8(node.readAllStandardError()) +
                                                  QString::fromUtf8(node.readAllStandardOutput())));
        const auto second = rebuild(reopened, tools, work.filePath("again"));
        QCOMPARE(second.image, built.image);
    }
};
QTEST_MAIN(ConsumableTreeTests)
#include "consumable_tree_tests.moc"
