#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <optional>
namespace lordly {
QJsonObject customCallerCatalog();
QStringList customRuntimeEntries();
struct CustomPlacement {
    static constexpr int descriptorOffset = 0x27c4300, descriptorSize = 128;
    static constexpr int indexBytes = 256 * 10 * 4, profileBytes = 128, stateBytes = 144;
    quint32 dataOffset = 0, dataSize = 0, profileCount = 0;
    quint32 codeAddress = 0, codeBytes = 0, residentBytes = 0, stateAddress = 0;
    QMap<QString, quint32> entries;
    static std::optional<CustomPlacement> read(const Rom &rom);
    QByteArray descriptor(const QByteArray &data, const QByteArray &resident) const;
    QByteArray canonicalReferences(const QByteArray &owner, qsizetype start) const;
};
void verifyCustomCallers(const Rom &rom, const std::optional<CustomPlacement> &placement);
} // namespace lordly
