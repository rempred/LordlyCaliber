#include "app/consumable_editor.h"
#include "app/encounter_editor.h"
#include "app/main_window.h"
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/shop_editor.h"
#include "app/text_workspace.h"
#include "app/value_editor.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
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
