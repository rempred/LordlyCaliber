#include "app/main_window.h"
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/text_workspace.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QJsonArray>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
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
    void loadDesktopWorkspace() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_COMPARE_WITH_TIMEOUT(window.findChildren<QTableView *>().size(), 2, 10000);
        auto classes = window.findChild<QTableView *>("classesTable");
        auto items = window.findChild<QTableView *>("itemsTable");
        QVERIFY(classes);
        QVERIFY(items);
        QCOMPARE(classes->model()->rowCount(), 165);
        QCOMPARE(items->model()->rowCount(), 278);
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
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
