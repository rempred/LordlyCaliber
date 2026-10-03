#pragma once
#include "engine/resource.h"
#include <QMap>
#include <QVector>

namespace lordly {
struct TextSpec {
    QString id, label;
    int relativeOffset = 0, capacity = 0, allocation = 0, count = 0;
    bool paged = false;
};
struct TextEntry {
    QByteArray original, bytes;
};
struct TextBank {
    TextSpec spec;
    Resource resource;
    QVector<TextEntry> entries;
    static QMap<QString, TextBank> load(const Rom &rom);
    QString text(int id) const;
    void setText(int id, const QString &text);
    QByteArray serialized() const;
    bool modified() const;
};
QString decodeText(const QByteArray &bytes);
QByteArray encodeText(const QString &text, bool paged);
} // namespace lordly
