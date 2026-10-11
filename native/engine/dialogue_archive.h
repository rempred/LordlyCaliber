#pragma once
#include "engine/archive.h"
#include <QMap>
#include <QVector>
namespace lordly {
// Current-ROM Serifu archives. Entries retain their native control bytes; this
// object does not interpret body, measurement or window-lifecycle grammars.
class DialogueArchive {
  public:
    enum class Table { Primary, Alternate };
    static QVector<quint32> directory(const Rom &rom, Table table = Table::Primary);
    static DialogueArchive load(const Rom &rom, int selector, Table table = Table::Primary);
    static QByteArray windowSource(const Rom &rom, const QByteArray &window);
    static DialogueArchive fromDecoded(QByteArray bytes);
    int entryCount() const {
        return int(offsets_.size());
    }
    QByteArray entry(int index) const;
    // Rebuild the pointer table while preserving every original payload byte.
    // Inputs exclude their terminator. A changed alias receives its own string.
    QByteArray rebuild(const QMap<int, QByteArray> &edits,
                       const QVector<QByteArray> &added = {}) const;
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
