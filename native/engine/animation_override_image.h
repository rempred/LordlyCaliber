#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <array>
namespace lordly {
// Binary ownership only. Keep this independent of ClassData: class-owner
// authentication itself needs to recognize the selector-return instruction.
class AnimationOverrideImage {
  public:
    using Row = std::array<quint8, 8>;
    static AnimationOverrideImage inspect(const Rom &);
    static QJsonObject definition();
    static QByteArray source(const QString &name);
    static QByteArray canonicalOwner(QByteArray, qsizetype start);
    static void validateOwner(const Rom &, qsizetype start, qsizetype size);
    bool recognized() const {
        return reason_.isEmpty();
    }
    QString reason() const {
        return reason_;
    }
    int version() const {
        return version_;
    } // 0: retail; 1/2/3: authenticated OBSO.
    const QVector<Row> &rows() const {
        return rows_;
    }
    QByteArray identity() const;
    void requireUnchanged(const Rom &) const;

  private:
    int version_ = 0;
    QString reason_;
    QVector<Row> rows_;
    QMap<int, QByteArray> regions_;
};
} // namespace lordly
