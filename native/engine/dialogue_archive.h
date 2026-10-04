#pragma once
#include "engine/archive.h"
#include <QVector>
namespace lordly {
// Current-ROM Serifu archives. Entries retain their native control bytes; this
// object does not interpret body, measurement or window-lifecycle grammars.
class DialogueArchive {
  public:
    static QVector<quint32> directory(const Rom &rom);
    static DialogueArchive load(const Rom &rom, int selector);
    static DialogueArchive fromDecoded(QByteArray bytes);
    int entryCount() const {
        return int(offsets_.size());
    }
    QByteArray entry(int index) const;
    const QByteArray &bytes() const {
        return bytes_;
    }
    QString name() const {
        return name_;
    }

  private:
    QByteArray bytes_;
    QVector<quint32> offsets_;
    QString name_;
};
} // namespace lordly
