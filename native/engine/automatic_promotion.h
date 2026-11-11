#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QVector>

namespace lordly {
// Requirements are owned by their actual consumer, not by the manual class menu.
class AutomaticPromotion {
  public:
    struct Route {
        int id, source, target;
        QString label;
    };
    static constexpr int evolutionOffset = 0x228e68, attackOffset = 0x239d48;
    static constexpr int angelRoute = 21;
    static AutomaticPromotion load(const Rom &rom);
    QVector<Route> routes(int target) const;
    bool supports(int route, int field) const;
    int value(int route, int field) const;
    void set(int route, int field, int value);
    QJsonObject json() const;
    void apply(const QJsonObject &json);
    bool modified() const;
    bool available() const { return !evolution_.isEmpty(); }
    // Only data tables and the five proven Angel Knight threshold operands change.
    QByteArray rebuild(const QByteArray &image) const;

  private:
    QByteArray evolution_, attacks_, angel_;
    QByteArray originalEvolution_, originalAttacks_, originalAngel_;
    QByteArray angelInstructions_;
};
} // namespace lordly
