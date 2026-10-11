#pragma once
#include "engine/archive.h"
#include <QMap>
#include <QVector>
#include <array>
#include <optional>

namespace lordly {
struct ScenarioPlacement {
    quint32 offset = 0, allocation = 0;
};
// Original resource envelopes and ordinal selection, without a PI/DMA redirect.
class ScenarioResources {
  public:
    static constexpr int keyCount = 65, selectorTable = 0x654a0, selectorStride = 9;
    static constexpr int descriptorOffset = 0x27c4400, descriptorSize = 64;
    static constexpr quint32 keyBase = 0x594280, enemyLink = 0x27429cc, esetLink = 0x2747832;
    static ScenarioResources load(const Rom &rom);
    const LhaMember &enemyMember() const {
        return enemy_;
    }
    const QVector<LhaMember> &members() const {
        return members_;
    }
    int selector(int key) const;
    QByteArray scenario(const Rom &rom, int key) const;
    const QByteArray &enemyData() const {
        return enemyData_;
    }
    std::optional<ScenarioPlacement> placement() const {
        return placement_;
    }
    QByteArray rebuild(const Rom &rom, const QByteArray &enemyData,
                       const QMap<int, QByteArray> &scenarioEdits) const;

  private:
    LhaMember enemy_;
    QVector<LhaMember> members_;
    QByteArray selectors_, baselineSelectors_, enemyData_;
    int baselineCount_ = 0;
    std::optional<ScenarioPlacement> placement_;
};
} // namespace lordly
