#pragma once
#include "engine/resource.h"
#include <QJsonObject>

namespace lordly {
quint32 crc32(const QByteArray &bytes);
QJsonObject promotionOwners();
struct PromotionData {
    int classCount = 0;
    qsizetype familyOffset = 0, maskOffset = 0, flagsOffset = 0, catalogOffset = 0;
    QByteArray bytes;
};
class Promotion {
  public:
    static Promotion load(const Rom &rom);
    QJsonObject families;
    QByteArray gates, masks, flags;
    QJsonObject originalFamilies;
    QByteArray originalGates, originalMasks, originalFlags;
    Resource retailResource;
    bool expanded = false;
    bool directGates = false;
    void setGate(int classId, int field, int value);
    void setMask(int classId, int value);
    void setEquipmentSetListed(int classId, bool listed);
    void assign(int classId, int familyId);
    int addFamily(const QString &name, int templateId);
    int classCount() const;
    void growToClassCount(int count);
    int appendClass(int templateId);
    PromotionData buildExpandedData() const;
    void applyExpandedData(const QByteArray &bytes, int count);
    static QJsonObject validateFamilies(const QJsonObject &value);
    void apply(const QJsonObject &value);
    QJsonObject json() const;
    bool modified() const;
    QByteArray decodedFamilies() const;
    QByteArray familyContainer(const QByteArray &compiled) const;
};
} // namespace lordly
