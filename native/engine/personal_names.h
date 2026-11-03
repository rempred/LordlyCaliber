#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QVector>
namespace lordly {
// Identity-class assignments, separate from class labels and saved character names.
class PersonalNames {
public:
    static constexpr quint32 ownerOffset = 0x41c20, ownerSize = 608;
    static constexpr int headerSize = 64, pointerOffset = 64;
    static PersonalNames retail(const Rom &);
    static PersonalNames read(const QByteArray &, quint32 address);
    static void verifyOwner(const Rom &, const QByteArray &metadata = {});
    QByteArray build(quint32 address) const;
    QJsonObject json() const;
    void apply(const QJsonObject &);
    int create(const QString &);
    void rename(int id,const QString &);
    void assign(int classId, int nameId);
    int assignment(int classId) const;
    const QVector<QByteArray> &names() const { return names_; }
private:
    QVector<QByteArray> names_;
    QVector<int> assignments_ = QVector<int>(256, -1);
};
}
