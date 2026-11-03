#pragma once
#include "engine/resource.h"
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <array>
#include <functional>
namespace lordly {
// The game's three compressed random-name lists and resident consumable names.
// Other names remain owned by their existing domain models.
class NameTables {
  public:
    explicit NameTables(const Rom &);
    static constexpr int randomCapacity=1024;
    int randomCount(int pool) const;
    bool expandedRandom() const;
    QString randomName(int pool,int id) const;
    QString consumableName(int id) const;
    int consumableCapacity(int id) const;
    QVector<int> sharingConsumableName(int id) const;
    void setRandomName(int pool,int id,const QString &);
    int addRandomName(int pool,const QString &);
    void removeRandomName(int pool,int id);
    void setConsumableName(int id,const QString &);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    bool modified() const {return names_!=originalNames_ || !consumableEdits_.isEmpty();}
    QByteArray rebuild(QByteArray image,const std::function<QByteArray(const QByteArray &,const QString &)> &compile) const;
  private:
    std::array<Resource,3> resources_;
    std::array<QStringList,3> names_,originalNames_;
    std::array<QByteArray,3> tails_;
    QMap<int,QByteArray> consumableEdits_,consumableOriginal_;
    QMap<int,quint32> consumableOffsets_;
    QMap<quint32,int> consumableSizes_;
};
}
