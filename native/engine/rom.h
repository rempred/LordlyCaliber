#pragma once
#include <QByteArray>
#include <QString>
#include <QtTypes>
#include <stdexcept>

namespace lordly {
class Error : public std::runtime_error {
  public:
    explicit Error(const QString &text) : std::runtime_error(text.toStdString()) {}
};
enum class ByteOrder { BigEndian, ByteSwapped, LittleEndian };
quint8 u8(const QByteArray &data, qsizetype offset);
quint16 u16(const QByteArray &data, qsizetype offset);
quint32 u32(const QByteArray &data, qsizetype offset);
void put8(QByteArray &data, qsizetype offset, quint8 value);
void put16(QByteArray &data, qsizetype offset, quint16 value);
void put32(QByteArray &data, qsizetype offset, quint32 value);
QByteArray decodeLzss(const QByteArray &data, qsizetype outputSize, qsizetype *consumed = nullptr);
QByteArray encodeLzss(const QByteArray &data);
QByteArray sha256(const QByteArray &data);

class Rom {
  public:
    static Rom fromBytes(QByteArray bytes, const QString &sourcePath = {});
    static Rom open(const QString &path);
    const QByteArray &bytes() const {
        return bytes_;
    }
    ByteOrder byteOrder() const {
        return order_;
    }
    QString extension() const;
    QString sourcePath() const {
        return path_;
    }
    QString title() const;
    int revision() const;
    void requireSupported() const;
    QByteArray serialize(const QByteArray &normalized) const;
    static void updateChecksum(QByteArray &normalized);

  private:
    QByteArray bytes_;
    QString path_;
    ByteOrder order_ = ByteOrder::BigEndian;
};
} // namespace lordly
