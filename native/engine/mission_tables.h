#pragma once
#include "engine/rom.h"

namespace lordly {
class MissionTables {
  public:
    enum Table { Scenario, World, Boundary, Camera };
    static constexpr int firstKey = 69, keyCount = 133, retailKeyCount = 65;
    static constexpr int descriptorOffset = 0x27c5600;
    static bool newKey(int key) { return key >= firstKey && key < keyCount; }
    static bool authoringKey(int key) {
        return newKey(key) || key == 10 || key == 22 || key == 35 || key == 62;
    }
    static bool hasScenario(const Rom &, int key);
    static QByteArray row(const Rom &, Table, int key);
    static void ensure(QByteArray &, int key);
    static void replace(QByteArray &, Table, int key, const QByteArray &);
    static void updateSelectors(QByteArray &, Table, const QByteArray &);
    static QByteArray runtimeData(const Rom &);
    static quint32 worldAddress(quint32 runtime);
    static void linkRuntime(QByteArray &, quint32 address);
    static QByteArray canonicalCode(const Rom &, int offset, int length);
};
} // namespace lordly
