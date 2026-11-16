#pragma once
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>
#include <functional>

namespace lordly {
// Half-open normalized z64 offsets, never RAM addresses.
struct ExportRange { qsizetype begin = 0, end = 0; };

// The committed image is read-only. Every replacement is prepared separately,
// checked against named owners, then committed atomically.
class RomWriter {
  public:
    using Ranges = QVector<ExportRange>;
    struct Change { QString owner; Ranges ranges; };
    explicit RomWriter(QByteArray image) : image_(std::move(image)) {}
    const QByteArray &bytes() const { return image_; }
    const QVector<Change> &changes() const { return changes_; }
    void allow(const QString &later, const QString &earlier, Ranges ranges);
    void derived(Ranges ranges);
    void apply(const QString &owner, QByteArray candidate, Ranges completeOwners = {});
    void write(const QString &owner, qsizetype offset, const QByteArray &bytes);
    void resize(const QString &owner, qsizetype size, char fill = char(-1));
    void edit(const QString &owner, const std::function<void(QByteArray &)> &prepare,
              Ranges completeOwners = {});
    // Called once by ExportPlan after its checked restoration phase. Restored
    // old installations no longer own the space used by the new build.
    void finishRestoration();
    static Ranges changedRanges(const QByteArray &before, const QByteArray &after);
    static void validateRange(ExportRange range);
  private:
    void commit(const QString &owner, QByteArray candidate, Ranges writes, Ranges completeOwners);
    QByteArray image_;
    QVector<Change> claims_, changes_;
    QMap<QString, QMap<QString, Ranges>> permissions_;
    Ranges derived_;
    bool restored_ = false;
};
}
