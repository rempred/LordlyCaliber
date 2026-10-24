#include "app/table_transfer.h"
#include "engine/rom.h"
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSaveFile>
#include <QTableView>
#include <algorithm>
namespace lordly {
namespace {
QString escaped(QString value, QChar separator) {
    if (value.contains(separator) || value.contains('"') || value.contains('\n') ||
        value.contains('\r')) {
        value.replace("\"", "\"\"");
        return '"' + value + '"';
    }
    return value;
}
QString value(const QTableView &table, const QModelIndex &index) {
    if (auto widget = table.indexWidget(index)) {
        if (auto spin = qobject_cast<QAbstractSpinBox *>(widget))
            return spin->text();
        if (auto combo = qobject_cast<QComboBox *>(widget))
            return combo->currentText();
        if (auto edit = qobject_cast<QLineEdit *>(widget))
            return edit->text();
        if (auto check = qobject_cast<QCheckBox *>(widget))
            return check->isChecked() ? "Yes" : "No";
        if (auto label = qobject_cast<QLabel *>(widget))
            return label->text();
    }
    return index.data(Qt::DisplayRole).toString();
}
QVector<int> visibleSections(const QHeaderView &header) {
    QVector<int> result;
    for (int visual = 0; visual < header.count(); ++visual) {
        const int logical = header.logicalIndex(visual);
        if (!header.isSectionHidden(logical))
            result << logical;
    }
    return result;
}
QString serialize(const QTableView &table, QChar separator, bool selection, bool headers) {
    const auto model = table.model();
    if (!model || (selection && !table.selectionModel()))
        return {};
    auto rows = visibleSections(*table.verticalHeader());
    auto cols = visibleSections(*table.horizontalHeader());
    if (selection) {
        const auto selected = table.selectionModel()->selectedIndexes();
        int firstRow = rows.size(), lastRow = -1, firstCol = cols.size(), lastCol = -1;
        for (const auto &index : selected) {
            const int row = rows.indexOf(index.row()), col = cols.indexOf(index.column());
            if (row < 0 || col < 0)
                continue;
            firstRow = std::min(firstRow, row);
            lastRow = std::max(lastRow, row);
            firstCol = std::min(firstCol, col);
            lastCol = std::max(lastCol, col);
        }
        if (lastRow < 0)
            return {};
        rows = rows.mid(firstRow, lastRow - firstRow + 1);
        cols = cols.mid(firstCol, lastCol - firstCol + 1);
    }
    if (cols.isEmpty())
        return {};
    const bool rowNames =
        !table.verticalHeader()->isHidden() && std::any_of(rows.begin(), rows.end(), [&](int row) {
            const auto name = model->headerData(row, Qt::Vertical).toString();
            return !name.isEmpty() && name != QString::number(row + 1);
        });
    QStringList lines;
    if (headers) {
        QStringList cells;
        if (rowNames)
            cells << "Row";
        for (int col : cols)
            cells << escaped(model->headerData(col, Qt::Horizontal).toString(), separator);
        lines << cells.join(separator);
    }
    for (int row : rows) {
        QStringList cells;
        if (rowNames)
            cells << escaped(model->headerData(row, Qt::Vertical).toString(), separator);
        for (int col : cols) {
            const auto index = model->index(row, col, table.rootIndex());
            cells << (selection && !table.selectionModel()->isSelected(index)
                          ? QString{}
                          : escaped(value(table, index), separator));
        }
        lines << cells.join(separator);
    }
    return lines.join("\r\n");
}
} // namespace
QString tableSelectionText(const QTableView &table, bool headers) {
    return serialize(table, '\t', true, headers);
}
QByteArray tableCsv(const QTableView &table) {
    return QByteArray::fromHex("efbbbf") + serialize(table, ',', false, true).toUtf8() + "\r\n";
}
void writeTableCsv(const QTableView &table, const QString &path) {
    const auto data = tableCsv(table);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw Error("Cannot write CSV: " + file.errorString());
}
void chooseTableCsv(QTableView &table) {
    auto path = QFileDialog::getSaveFileName(&table, "Export visible table to CSV",
                                             table.objectName() + ".csv", "CSV files (*.csv)");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".csv", Qt::CaseInsensitive))
        path += ".csv";
    try {
        writeTableCsv(table, path);
    } catch (const std::exception &e) {
        QMessageBox::warning(&table, "CSV export", QString::fromUtf8(e.what()));
    }
}
void installTableTransfer(QTableView &table) {
    if (table.property("tableTransferInstalled").toBool())
        return;
    table.setProperty("tableTransferInstalled", true);
    for (bool headers : {false, true}) {
        auto copy = new QAction(headers ? "Copy with column names" : "Copy selection", &table);
        copy->setObjectName(headers ? "tableCopyWithHeaders" : "tableCopySelection");
        copy->setShortcut(headers ? QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)
                                  : QKeySequence::Copy);
        copy->setShortcutContext(Qt::WidgetShortcut);
        QObject::connect(copy, &QAction::triggered, &table, [&table, headers] {
            if (table.selectionModel() && table.selectionModel()->hasSelection())
                QApplication::clipboard()->setText(tableSelectionText(table, headers));
        });
        table.addAction(copy);
    }
    auto csv = new QAction("Export CSV…", &table);
    csv->setObjectName("tableExportCsv");
    csv->setToolTip("Export every filtered row and visible column in the current display order.");
    QObject::connect(csv, &QAction::triggered, &table, [&table] { chooseTableCsv(table); });
    table.addAction(csv);
    if (table.contextMenuPolicy() == Qt::DefaultContextMenu)
        table.setContextMenuPolicy(Qt::ActionsContextMenu);
}
} // namespace lordly
