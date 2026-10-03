#pragma once
#include "engine/resource.h"
#include <QJsonObject>

namespace lordly {
quint32 crc32(const QByteArray &bytes);
QJsonObject promotionOwners();
class Promotion {
  public:
    static Promotion load(const Rom &rom);
    QJsonObject families;
    QByteArray gates, masks;
    QJsonObject originalFamilies;
    QByteArray originalGates, originalMasks;
    Resource retailResource;
    bool expanded = false;
    void setGate(int classId, int field, int value);
    void setMask(int classId, int value);
    void assign(int classId, int familyId);
    int addFamily(const QString &name, int templateId);
    static QJsonObject validateFamilies(const QJsonObject &value);
    void apply(const QJsonObject &value);
    QJsonObject json() const;
    bool modified() const;
    QByteArray decodedFamilies() const;
    QByteArray familyContainer(const QByteArray &compiled) const;
};
} // namespace lordly
