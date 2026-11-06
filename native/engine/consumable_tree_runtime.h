#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <optional>
namespace lordly {
struct ConsumableTreePlacement {
    static constexpr int descriptorOffset=0x27c5400, descriptorSize=64;
    quint32 offset=0,address=0,size=0;
    QByteArray data;
    QJsonObject model;
    quint32 directory=0,count=0;
    static std::optional<ConsumableTreePlacement> read(const Rom &rom);
    QByteArray remove(const Rom &rom) const;
    QByteArray descriptor() const;
};
}
