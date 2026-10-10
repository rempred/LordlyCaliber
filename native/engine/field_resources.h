#pragma once
#include "engine/archive.h"
#include <QVector>
namespace lordly {
// Compact native resource envelopes, independent of the named LHA archives.
struct FieldMember {
    QByteArray original, data;
    int offset = 0;
};
QByteArray fieldResource(const Rom &, quint32 key);
QVector<FieldMember> fieldMembers(const QByteArray &);
QByteArray encodeFieldMembers(const QVector<FieldMember> &);
quint32 fieldChild(const Rom &, quint32 root, int index);
void replaceFieldChild(QByteArray &rom, quint32 root, int index, const QByteArray &payload);
void replaceFieldMemberInPlace(QByteArray &rom, quint32 key, int index, const QByteArray &data);
} // namespace lordly
