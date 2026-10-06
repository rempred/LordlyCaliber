#pragma once
#include "engine/rom.h"
#include "engine/save_runtime.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <optional>

namespace lordly {
// Source-built owners may use different instruction locations for the same
// class data. Select their references only after authenticating the full owner.
QJsonArray classOwnerReferences(const QJsonObject &owner, const QByteArray &bytes,
                                const QMap<QString, quint32> &bases,
                                const QJsonObject &originalBases);
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
struct ClassPlacement {
    ClassDataLayout layout;
    quint32 dataRomOffset = 0, codeBytes = 0;
    quint32 bootEntry = 0, heapEntry = 0, secondaryEntry = 0;
    quint32 descriptionEntry = 0;
    quint32 directory = 0;
    int directoryCapacity = 0;
    SavePlacement saves;
    quint32 residentAddress = 0, residentBytes = 0, residentChecksum = 0;
    static constexpr quint32 residentBegin = 0x80187f00, residentEnd = 0x8018aa70;
    // Proven permanent DMA segment ROM40E80..66E10 -> RAM8016AF80..80190F10.
    static constexpr quint32 residentDelta = 0x8012a100;
    quint32 instructionEnd() const {
        if (residentBytes)
            return residentAddress + residentBytes;
        return saves.installed() ? SavePlacement::ramAddress + saves.codeBytes
                                 : codeRamAddress + codeBytes;
    }
    // Validated V2 promotion metadata, loaded from its separate ROM container.
    // Not part of the resident class descriptor or game RAM.
    QByteArray runtimeInventory;
    static constexpr quint32 descriptorRomOffset = 0x5dc80, descriptorSize = 128;
    static constexpr quint32 codeRomOffset = 0x5db14, codeRamAddress = 0x80187c14;
    static constexpr quint32 codeCapacity = descriptorRomOffset - codeRomOffset;
    static std::optional<ClassPlacement> read(const Rom &rom);
    QByteArray descriptor(const QByteArray &data, const QByteArray &code) const;
    QByteArray canonicalReferences(const QByteArray &owner, qsizetype romStart) const;
    QByteArray placedReferences(const QByteArray &owner, qsizetype romStart) const;
};
// Authored class data is independent of ROM/RAM placement. Runtime references,
// loader code, menu capacity and save migration are separate export owners.
class ClassData {
  public:
    // Native records include the twelve-byte header. Modder B59 names the
    // final body byte, at full-record offset 71, not full-record offset 59.
    static constexpr int unitSizeOffset = 4, itemCapacityOffset = 71;
    static ClassData load(const Rom &rom);
    static ClassData loadRetail(const Rom &rom);
    const QVector<ClassAsset> &entries() const {
        return entries_;
    }
    const QByteArray &prefix() const {
        return prefix_;
    }
    int append(int templateId, const QByteArray &encodedName);
    void set(int id, const ClassAsset &asset);
    int unitSize(int primary, int secondary) const;
    ClassDataImage build(quint32 ramBase, quint32 ramEnd = 0x80400000U) const;

  private:
    static void validate(const ClassAsset &asset);
    QByteArray prefix_;
    QVector<ClassAsset> entries_;
};
} // namespace lordly
