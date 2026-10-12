#pragma once
#include "engine/scenarios.h"
namespace lordly {
struct TrainingOpponent {
    int node = 0, slot = 0, price = 0;
    QString name;
    QByteArray squad;
    QJsonObject json() const;
};
class Training {
  public:
    static constexpr int tableOffset = 0x86a70, tableSize = 48 * 24;
    static Training load(const Rom &);
    const QVector<TrainingOpponent> &entries() const { return rows_; }
    const TrainingOpponent &original(int row) const { return originals_.at(row); }
    void apply(const QJsonObject &, const ClassData &, int itemCount);
    void validate(const ClassData &, int itemCount) const;
    QJsonObject json() const;
    bool modified() const;
    QByteArray plan(QByteArray &enemies) const;
    static int fee(int level, int price);
  private:
    QVector<TrainingOpponent> rows_, originals_;
    QByteArray table_, classSizes_;
};
}
