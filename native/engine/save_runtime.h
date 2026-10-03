#pragma once
#include "engine/rom.h"
#include <QJsonObject>
namespace lordly {
QJsonObject saveRuntimeCatalog();
quint32 saveRuntimeOwnerChecksum(const QByteArray &rom, quint32 version);
struct SavePlacement {
    quint32 codeBytes = 0, codeChecksum = 0, ownerChecksum = 0;
    quint32 version = 0;
    static constexpr quint32 romAddress = 0x5de00, ramAddress = 0x80187f00;
    static constexpr quint32 capacity = 0x8018aa70 - ramAddress;
    bool installed() const {
        return codeBytes != 0;
    }
    quint32 endAddress() const {
        return ramAddress + ((codeBytes + 15U) & ~15U);
    }
    void validate() const;
    void verify(const QByteArray &rom) const;
};
} // namespace lordly
