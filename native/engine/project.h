#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <map>

namespace lordly {
enum class Table { Classes, Items };
struct Field {
    QString key, label, group;
    int offset=0, width=1;
    bool signedValue=false;
    QString choices;
    int minimum() const { return signedValue ? -128 : 0; }
    int maximum() const { return signedValue ? 127 : width==2 ? 65535 : 255; }
};
struct Record {
    int id=0;
    QString name;
    QByteArray original, bytes;
    qsizetype romOffset=0;
};
QString tableKey(Table table);
const QVector<Field>& fields(Table table);
class Project {
public:
    static Project fromRom(Rom rom, const QJsonObject& catalog = {});
    const Rom& rom() const { return rom_; }
    const QVector<Record>& records(Table table) const { return tables_.at(table); }
    const Record& record(Table table,int row) const;
    int value(Table table,int row,int field) const;
    void setValue(Table table,int row,int field,int value);
    void replaceBytes(Table table,int row,const QByteArray& bytes);
    int changedRecords() const;
    QJsonObject manifest() const;
    void applyManifest(const QJsonObject& object);
    void importLegacy(const QJsonObject& object);
    void saveFolder(const QString& folder) const;
    static QJsonObject readProject(const QString& path);
    void saveArchive(const QString& path) const;
    const QJsonObject& catalog() const { return catalog_; }
    const QStringList& importWarnings() const { return importWarnings_; }
    QByteArray rebuildRecords() const;
private:
    Rom rom_;
    QJsonObject catalog_, retained_, legacy_;
    std::map<Table,QVector<Record>> tables_;
    QStringList importWarnings_;
};
void atomicWrite(const QString& path,const QByteArray& data);
}
