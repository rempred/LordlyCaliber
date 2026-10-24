#pragma once
#include <QByteArray>
#include <QString>
class QTableView;
namespace lordly {
QString tableSelectionText(const QTableView &, bool headers = false);
QByteArray tableCsv(const QTableView &);
void writeTableCsv(const QTableView &, const QString &path);
void chooseTableCsv(QTableView &);
void installTableTransfer(QTableView &);
} // namespace lordly
