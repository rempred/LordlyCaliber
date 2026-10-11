#pragma once
#include <QByteArray>
#include <QMap>
namespace lordly {
// Current project pixels, keyed by ROM descriptor key and member's art ordinal.
// Runtime pose/metadata selection remains unchanged.
using ActorArtworkEdits = QMap<quint32, QMap<int, QByteArray>>;
} // namespace lordly
