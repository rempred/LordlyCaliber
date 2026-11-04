#pragma once
#include "engine/rom.h"
#include <QPoint>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <optional>
namespace lordly {
struct CampaignPlacement {
    static constexpr quint32 sourceStart = 0x79730, sourceEnd = 0x87200;
    static constexpr quint32 ramStart = 0x8019a7a0, codeAddress = 0x801a8270;
    quint32 descriptor = 0, overlayOffset = 0, overlaySize = 0, roadAddress = 0, dotAddress = 0;
    QByteArray roads, dots, roadFlags;
    int version = 0;
    int bendCapacity = 7;
    QByteArray locationIcons;
    quint32 displayDirectory = 0, displayPoints = 0;
    QVector<QVector<QPoint>> displayDots;
    QMap<int,QString> locationNames;
    QByteArray background, artworkAssignments;
    QJsonObject artworkIcons;
    quint32 backgroundOffset = 0, backgroundAllocation = 0;
    static std::optional<CampaignPlacement> read(const QByteArray &);
};
} // namespace lordly
