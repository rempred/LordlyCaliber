#pragma once
#include "engine/rom.h"
#include <QVector>
#include <optional>
namespace lordly {
class ActorDivider {
  public:
    const QByteArray &record() const {
        return record_;
    }
    void supply(QByteArray);
    void start(const QVector<quint32> &);
    void release() {
        record_.clear();
    }
    std::optional<quint8> channel(float x, float z) const;

  private:
    QByteArray record_;
};
} // namespace lordly
