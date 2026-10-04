#pragma once
#include "engine/dialogue_archive.h"
#include <QJsonObject>
namespace lordly {
class Dialogues {
  public:
    using Table = DialogueArchive::Table;
    static Dialogues load(Rom);
    const DialogueArchive &original(Table, int archive) const;
    int entryCount(Table, int archive) const;
    QByteArray entry(Table, int archive, int index) const;
    void setEntry(Table, int archive, int index, const QByteArray &);
    int appendEntry(Table, int archive, const QByteArray &);
    void restoreEntry(Table, int archive, int index);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    int changedEntries() const;
    bool modified() const {
        return !changes_.isEmpty();
    }
    // Keys encode table in bit16 and archive selector in the lower sixteen bits.
    QMap<quint32, QByteArray> plan() const;
    static quint32 identity(Table, int archive);

  private:
    struct Changes {
        QMap<int, QByteArray> edits;
        QVector<QByteArray> added;
    };
    explicit Dialogues(Rom rom) : rom_(std::move(rom)) {}
    Rom rom_;
    mutable QMap<quint32, DialogueArchive> originals_;
    QMap<quint32, Changes> changes_;
};
} // namespace lordly
