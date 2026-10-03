#include "app/main_window.h"
#include "app/record_model.h"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
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
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
