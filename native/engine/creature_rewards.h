#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <array>
#include <optional>
namespace lordly {
struct CreatureReward {
    std::array<int, 3> outcomes{0, 0, 0}, weights{1, 1, 1};
    bool operator==(const CreatureReward &) const = default;
    QJsonObject json() const;
    static CreatureReward parse(const QJsonObject &object);
};
class CreatureRewards {
  public:
    static constexpr quint32 tableOffset = 0x142258, tableSize = 296;
    static constexpr quint32 ownerOffset = 0x101ddc, ownerSize = 4388, ownerAddress = 0x801ad69c;
    static constexpr quint32 descriptorOffset = 0x27c4100, descriptorSize = 64;
    static constexpr int count = 256, rowSize = 16, dataSize = count * rowSize;
    static CreatureRewards load(const Rom &rom);
    static QJsonObject catalog();
    static QMap<QString, quint32> services();
    static std::optional<quint32> placement(const Rom &rom);
    static QByteArray definitions(quint32 offset, bool native = true);
    const CreatureReward &row(int id) const;
    void setRow(int id, const CreatureReward &value);
    QJsonObject json() const;
    void apply(const QJsonObject &object);
    void importLegacy(const QJsonObject &object);
    QByteArray bytes() const;
    int changedRows() const;
    bool modified() const {
        return rows_ != original_;
    }

  private:
    static std::array<CreatureReward, count>
    decodeRetail(const QByteArray &table, const QMap<int, std::array<int, 3>> &weights = {});
    std::array<CreatureReward, count> rows_, original_;
    QByteArray retail_;
};
} // namespace lordly
