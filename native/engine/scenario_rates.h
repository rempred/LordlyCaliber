#pragma once
#include "engine/encounter_rate.h"
#include <array>
#include <optional>
namespace lordly {
struct ScenarioRoll {
    quint32 divisor = 1, passCount = 0;
    bool operator==(const ScenarioRoll &) const = default;
};
struct ScenarioRatePlacement {
    quint32 offset = 0;
    QJsonObject global;
};
class ScenarioRates {
  public:
    static constexpr int count = 256, rowSize = 16, dataSize = count * (rowSize + 1);
    static constexpr quint32 descriptorOffset = 0x27c4200, descriptorSize = 64;
    static ScenarioRates load(const Rom &rom);
    static std::optional<ScenarioRatePlacement> placement(const Rom &rom);
    const std::optional<ScenarioRoll> &branch(int key, bool normal) const;
    void setBranch(int key, bool normal, const std::optional<ScenarioRoll> &value);
    void importLegacy(const QJsonObject &object);
    QJsonObject json() const;
    void apply(const QJsonObject &object);
    QByteArray bytes(const EncounterRate &global) const;
    static QByteArray definitions(quint32 offset);
    bool modified() const {
        return rows_ != original_;
    }
    bool hasOverrides() const;
    int changedRows() const;

  private:
    // Alternate (state flag17 clear), then normal (flag set), matching ROM order.
    using Row = std::array<std::optional<ScenarioRoll>, 2>;
    std::array<Row, count> rows_, original_;
};
} // namespace lordly
