#pragma once
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <functional>

namespace lordly {
// All ranges are half-open, normalized z64 ROM offsets, never RAM addresses.
struct ExportRange {
    qsizetype begin = 0, end = 0;
};

// Coordinates existing builders; it does not allocate resources or interpret IDs.
// The image is private to rebuild(). A failed operation restores the entire plan's
// input, and no file is written until rebuild's independent readback succeeds.
class ExportPlan {
  public:
    using Action = std::function<void()>;
    using Ranges = QVector<ExportRange>;
    using RangeProvider = std::function<Ranges(const QByteArray &)>;
    explicit ExportPlan(QByteArray &image) : image_(image) {}
    void add(QString name, QStringList prerequisites, Action prepare, Ranges owners = {});
    // Permission is directional, bounded and separate from execution order.
    void compose(const QString &later, const QString &earlier, Ranges ranges);
    void compose(const QString &later, const QString &earlier, RangeProvider ranges);
    // Derived fields are recalculated by the finalizer, not feature-owned data.
    void derived(Ranges ranges);
    void validate() const;
    void run();
    int changedSteps() const { return changedSteps_; }
    static Ranges changedRanges(const QByteArray &before, const QByteArray &after);
  private:
    struct Step {
        QString name;
        QStringList prerequisites;
        Action prepare;
        Ranges owners;
    };
    struct Composition { QString later, earlier; RangeProvider ranges; };
    struct Claim { QString owner; Ranges ranges; };
    QByteArray &image_;
    QVector<Step> steps_;
    QVector<Composition> compositions_;
    Ranges derived_;
    int changedSteps_ = 0;
};
}
