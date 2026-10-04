#pragma once
#include "engine/class_data.h"
#include "engine/consumable_effects.h"
#include "engine/encounters.h"
#include "engine/promotion.h"
#include "engine/rom.h"
#include "engine/scenario_world.h"
#include "engine/scenarios.h"
#include "engine/shops.h"
#include "engine/text.h"
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <map>
#include <optional>

namespace lordly {
enum class Table { Classes, Items, Consumables };
struct Field {
    QString key, label, group;
    int offset = 0, width = 1;
    bool signedValue = false;
    QString choices;
    int bits = 8, shift = 0;
    int minimum() const {
        return signedValue ? -128 : 0;
    }
    int maximum() const {
        return signedValue ? 127 : width == 2 ? 65535 : (1 << bits) - 1;
    }
};
struct Record {
    int id = 0;
    QString name;
    QByteArray original, bytes;
    qsizetype romOffset = 0;
};
QString tableKey(Table table);
const QVector<Field> &fields(Table table);
class Project {
  public:
    static Project fromRom(Rom rom, const QJsonObject &catalog = {});
    const Rom &rom() const {
        return rom_;
    }
    const QVector<Record> &records(Table table) const {
        return tables_.at(table);
    }
    const Record &record(Table table, int row) const;
    int value(Table table, int row, int field) const;
    void setValue(Table table, int row, int field, int value);
    void replaceBytes(Table table, int row, const QByteArray &bytes);
    int changedRecords() const;
    QJsonObject manifest() const;
    void applyManifest(const QJsonObject &object);
    void importLegacy(const QJsonObject &object);
    void saveFolder(const QString &folder) const;
    static QJsonObject readProject(const QString &path);
    void saveArchive(const QString &path) const;
    const QJsonObject &catalog() const {
        return catalog_;
    }
    const QStringList &importWarnings() const {
        return importWarnings_;
    }
    QStringList pendingLegacySections() const;
    void loadTexts();
    const QMap<QString, TextBank> &textBanks() const {
        return texts_;
    }
    void setText(const QString &kind, int id, const QString &text);
    void replaceTextBytes(const QString &kind, int id, const QByteArray &bytes);
    QByteArray rebuildRecords() const;
    void loadPromotion();
    const std::optional<Promotion> &promotion() const {
        return promotion_;
    }
    void setPromotion(const QJsonObject &value);
    void loadShops();
    const std::optional<Shops> &shops() const {
        return shops_;
    }
    void setShops(const QJsonObject &value);
    void loadEncounters();
    const std::optional<Encounters> &encounters() const {
        return encounters_;
    }
    void setEncounters(const QJsonObject &value);
    void loadScenarios();
    const std::optional<Scenarios> &scenarios() const {
        return scenarios_;
    }
    void setScenarios(const QJsonObject &value);
    void loadWorld();
    const std::optional<ScenarioWorld> &world() const {
        return world_;
    }
    void setWorld(const QJsonObject &value);
    void loadConsumableEffects();
    const std::optional<ConsumableEffects> &consumableEffects() const {
        return consumableEffects_;
    }
    void setConsumableEffects(const QJsonObject &value);
    void editConsumableEffect(const QString &key, const QJsonObject &value,
                              bool updateHealingDescription = true);
    bool hasClassData() const {
        return originalClasses_.has_value();
    }
    bool classNamesModified() const {
        return !classNames_.isEmpty() || !addedClasses_.isEmpty();
    }
    bool classNameModified(int row) const {
        return classNames_.contains(row) ||
               (originalClasses_ && row >= originalClasses_->entries().size());
    }
    QByteArray classNameBytes(int row) const;
    void setClassNameBytes(int row, const QByteArray &name);
    ClassData classData() const;
    int appendClass(int templateId, const QString &name);
    static int classDescriptionIndex(int classId);

  private:
    void importLegacyShops(const QJsonObject &object);
    void importLegacyConsumables(const QJsonObject &object);
    void importLegacyConsumableEffects(const QJsonObject &object);
    void importLegacyEncounters(const QJsonObject &object);
    void importLegacyGlobalRate(const QJsonObject &object);
    void importLegacyRewards(const QJsonObject &object);
    void importLegacyScenarioRates(const QJsonObject &object);
    void importLegacyCustomSquads(const QJsonObject &object);
    void importLegacyScenarios(const QJsonObject &object);
    Rom rom_;
    QJsonObject catalog_, retained_, legacy_;
    std::map<Table, QVector<Record>> tables_;
    QStringList importWarnings_;
    QMap<QString, TextBank> texts_;
    std::optional<Promotion> promotion_;
    std::optional<Shops> shops_;
    std::optional<ConsumableEffects> consumableEffects_;
    std::optional<Encounters> encounters_;
    std::optional<Scenarios> scenarios_;
    std::optional<ScenarioWorld> world_;
    std::optional<ClassData> originalClasses_;
    QMap<int, QByteArray> classNames_;
    QVector<ClassAsset> addedClasses_;
    const ClassAsset &classAsset(int row) const;
    int appendClassAsset(const ClassAsset &asset, const QByteArray &description,
                         int promotionTemplate);
};
QByteArray encodeClassName(const QString &name);
void atomicWrite(const QString &path, const QByteArray &data);
} // namespace lordly
