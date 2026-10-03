#pragma once
#include "engine/rom.h"
#include <QVector>

namespace lordly {
struct ClassAsset {
    QByteArray record, appearance, name;
    quint8 namedAlias = 0;
};
struct ClassDataLayout {
    quint32 indexBase = 0, records = 0, appearances = 0, aliases = 0, names = 0;
    quint32 discovery = 0, end = 0;
    int count = 0;
};
struct ClassDataImage {
    ClassDataLayout layout;
    QByteArray bytes;
};
// Authored class data is independent of ROM/RAM placement. Runtime references,
// loader code, menu capacity and save migration are separate export owners.
class ClassData {
  public:
    static ClassData loadRetail(const Rom &rom);
    const QVector<ClassAsset> &entries() const {
        return entries_;
    }
    const QByteArray &prefix() const {
        return prefix_;
    }
    int append(int templateId, const QByteArray &encodedName);
    void set(int id, const ClassAsset &asset);
    ClassDataImage build(quint32 ramBase, quint32 ramEnd = 0x80400000U) const;

  private:
    static void validate(const ClassAsset &asset);
    QByteArray prefix_;
    QVector<ClassAsset> entries_;
};
} // namespace lordly
