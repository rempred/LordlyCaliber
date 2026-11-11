#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
namespace lordly {
struct MovementType {
    QString name;
    int baseSpeed = 35;
    int behavior = 2; // Existing ground, flying and immobile behavior, independent of costs.
    QVector<int> costs;
};
class MovementTypes {
  public:
    static constexpr int maximumId = 254, terrainCount = 32;
    static MovementTypes load(const Rom &);
    static MovementTypes fromJson(const QJsonObject &);
    QJsonObject json() const;
    const QMap<int, MovementType> &entries() const {
        return entries_;
    }
    const QVector<int> &priority() const {
        return priority_;
    }
    QJsonObject labels() const;
    QByteArray priorityBytes() const;
    int create(const QString &name, int copyFrom);
    void remove(int id);
    void set(int id, const MovementType &);
    void setPriority(const QVector<int> &);
    bool requiresRuntime() const {
        return installed_ || json() != original_;
    }
    bool modified() const {
        return json() != original_;
    }
    double speed(int id, int terrain) const;
    int behavior(int id) const;
    const QVector<float> &curve() const {
        return curve_;
    }
    void apply(const QJsonObject &);

  private:
    void validate() const;
    QMap<int, MovementType> entries_;
    QVector<int> priority_;
    QVector<float> curve_;
    QJsonObject original_;
    bool installed_ = false;
};
} // namespace lordly
