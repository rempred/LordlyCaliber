#include "app/table_transfer.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class TableTransferTests : public QObject {
    Q_OBJECT
  private slots:
    void csvViewAndEscaping() {
        QStandardItemModel model(3, 4);
        model.setHorizontalHeaderLabels({"Name", "HP", "Growth", "Hidden"});
        const QStringList names{"Élite, \"Knight\"", "Mage\nsecond line", "Filtered out"};
        for (int row = 0; row < 3; ++row) {
            model.setData(model.index(row, 0), names[row]);
            model.setData(model.index(row, 1), 100 + row);
            model.setData(model.index(row, 2), 4 + row);
            model.setData(model.index(row, 3), "secret");
        }
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        proxy.setFilterRegularExpression("Knight|Mage");
        proxy.sort(1, Qt::DescendingOrder);
        QTableView table;
        table.setModel(&proxy);
        table.verticalHeader()->hide();
        table.hideColumn(3);
        table.horizontalHeader()->moveSection(2, 1);
        const auto expected = QByteArray::fromHex("efbbbf") +
                              QString("Name,Growth,HP\r\n\"Mage\nsecond line\",5,101\r\n\"Élite, "
                                      "\"\"Knight\"\"\",4,100\r\n")
                                  .toUtf8();
        QCOMPARE(tableCsv(table), expected);
        QTemporaryDir work;
        const auto path = work.filePath("data.csv");
        writeTableCsv(table, path);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), expected);
        table.hideRow(0);
        QVERIFY(!tableCsv(table).contains("Mage"));
        proxy.setFilterFixedString("no results");
        QCOMPARE(tableCsv(table), QByteArray::fromHex("efbbbf") + "Name,Growth,HP\r\n");
    }
    void copyGridAndEditor() {
        QStandardItemModel model(3, 3);
        model.setHorizontalHeaderLabels({"Name", "HP", "Growth"});
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                model.setData(model.index(row, col), row * 10 + col);
        QTableView table;
        table.setModel(&model);
        table.verticalHeader()->hide();
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        installTableTransfer(table);
        installTableTransfer(table);
        QCOMPARE(table.findChildren<QAction *>("tableCopySelection").size(), 1);
        table.show();
        table.setFocus();
        QTRY_VERIFY(table.hasFocus());
        table.selectionModel()->select(QItemSelection(model.index(0, 1), model.index(2, 2)),
                                       QItemSelectionModel::ClearAndSelect);
        QCOMPARE(tableSelectionText(table), QString("1\t2\r\n11\t12\r\n21\t22"));
        QTest::keyClick(&table, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), tableSelectionText(table));
        table.findChild<QAction *>("tableCopyWithHeaders")->trigger();
        QCOMPARE(QApplication::clipboard()->text(),
                 QString("HP\tGrowth\r\n1\t2\r\n11\t12\r\n21\t22"));
        table.selectionModel()->select(model.index(1, 1), QItemSelectionModel::Deselect);
        QCOMPARE(tableSelectionText(table), QString("1\t2\r\n\t12\r\n21\t22"));
        table.edit(model.index(0, 0));
        auto editor = table.findChild<QLineEdit *>();
        QVERIFY(editor);
        editor->setText("editing text");
        editor->selectAll();
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("editing text"));
        QCOMPARE(model.index(0, 0).data().toInt(), 0);
    }
    void widgetCellsAndRowNames() {
        QTableWidget table(2, 1);
        table.setHorizontalHeaderLabels({"Affinity"});
        table.setVerticalHeaderLabels({"Male", "Female"});
        auto spin = new QSpinBox;
        spin->setValue(42);
        table.setCellWidget(0, 0, spin);
        table.setItem(1, 0, new QTableWidgetItem("7"));
        QCOMPARE(tableCsv(table),
                 QByteArray::fromHex("efbbbf") + "Row,Affinity\r\nMale,42\r\nFemale,7\r\n");
    }
};
QTEST_MAIN(TableTransferTests)
#include "table_transfer_tests.moc"
