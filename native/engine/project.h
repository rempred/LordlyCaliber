#pragma once
#include "engine/animation_overrides.h"
#include "engine/animation_routes.h"
#include "engine/animation_sequences.h"
#include "engine/army_artwork.h"
#include "engine/avatar_artwork.h"
#include "engine/body_animation_data.h"
#include "engine/category_icons.h"
#include "engine/change_report.h"
#include "engine/class_data.h"
#include "engine/combat_artwork.h"
#include "engine/consumable_effects.h"
#include "engine/dialogues.h"
#include "engine/encounters.h"
#include "engine/icon_artwork.h"
#include "engine/item_data.h"
#include "engine/attack_data.h"
#include "engine/map_edits.h"
#include "engine/promotion.h"
#include "engine/rom.h"
#include "engine/scenario_world.h"
#include "engine/campaign_map.h"
#include "engine/scenarios.h"
#include "engine/scene_events.h"
#include "engine/scene_programs.h"
#include "engine/shops.h"
#include "engine/sprite_library.h"
#include "engine/text.h"
#include "engine/name_tables.h"
#include "engine/tool_settings.h"
#include "engine/training.h"
#include <QJsonObject>
#include <QJsonArray>
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
    enum class SourcePolicy { RequireMatch, ImportIntoCurrentRom };
    static Project fromRom(Rom rom, const QJsonObject &catalog = {});
    const Rom &rom() const {
        return battlefieldRom_ ? *battlefieldRom_ : rom_;
    }
    const Rom &runtimeRom() const { return runtimeRom_ ? *runtimeRom_ : rom_; }
    bool expandedArmies() const;
    const QVector<Record> &records(Table table) const {
        return tables_.at(table);
    }
    const Record &record(Table table, int row) const;
    int value(Table table, int row, int field) const;
    void setValue(Table table, int row, int field, int value);
    void loadAttacks();
    const std::optional<AttackData> &attacks() const { return attacks_; }
    void setAttacks(const QJsonObject &);
    void replaceBytes(Table table, int row, const QByteArray &bytes);
    int changedRecords() const;
    void loadCampaignMap();
    void setCampaignMap(const QJsonObject &);
    const std::optional<CampaignMap> &campaignMap() const { return campaignMap_; }
    ChangeReport changeReport(const Project *reference = nullptr) const;
    ChangeReport compareWithRom(Rom reference) const;
    QJsonObject manifest() const;
    void applyManifest(const QJsonObject &object, SourcePolicy sourcePolicy = SourcePolicy::RequireMatch);
    void importLegacy(const QJsonObject &object);
    const SpriteLibrary &sprites() const { return sprites_; }
    void setSprites(const SpriteLibrary &library) { sprites_ = library; }
    BodyAnimationData::Pointer bodyAnimationData();
    void loadAnimationSequences();
    const std::optional<AnimationSequences> &animationSequences()const{return animationSequences_;}
    void setAnimationSequences(const QJsonObject &);
    void setPrivateAnimationState(const QJsonObject &sequences, const QJsonObject &overrides);
    void validateAnimationAssignments();
    QByteArray privatePoseMappings();
    int assignPrivateAttack(int classId, int flags, int action, bool blocked, int pose,
                            bool duplicate = true);
    void restorePrivateAttack(int classId, int flags, int action);
    void removePrivateAnimation(int classId, int flags);
    void loadAnimationRoutes();
    const std::optional<AnimationRoutes> &animationRoutes() const {return animationRoutes_;}
    void setAnimationRoutes(const QJsonObject &);
    void loadAnimationOverrides();
    const std::optional<AnimationOverrides> &animationOverrides()const{return animationOverrides_;}
    void setAnimationOverrides(const QJsonObject &);
    void importLegacyAnimationOverrides(const QJsonObject &);
    void loadCombatArtwork();
    const std::optional<CombatArtwork> &combatArtwork() const {return combat_;}
    void setCombatArtwork(const QJsonObject &);
    void loadArmyArtwork();
    const std::optional<ArmyArtwork> &armyArtwork() const {return army_;}
    void setArmyArtwork(const QJsonObject &);
    void loadAvatarArtwork();
    const std::optional<AvatarArtwork> &avatarArtwork() const { return avatars_; }
    void setAvatarArtwork(const QJsonObject &);
    void loadCategoryIcons();
    const std::optional<CategoryIcons> &categoryIcons() const {
        return categoryIcons_;
    }
    void setCategoryIcons(const QJsonObject &);
    void loadIconArtwork();
    const std::optional<IconArtwork> &iconArtwork() const { return icons_; }
    void setIconArtwork(const QJsonObject &);
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
    void loadTraining();
    const std::optional<Training> &training() const { return training_; }
    void setTraining(const QJsonObject &);
    void loadScenarios();
    const std::optional<Scenarios> &scenarios() const {
        return scenarios_;
    }
    void setScenarios(const QJsonObject &value);
    QVector<int> availableScenarioBattlefields() const;
    void createScenarioBattlefield(int targetKey, int sourceKey, const QString &mode,
                                   const QString &name);
    const QJsonArray &scenarioBattlefields() const { return scenarioBattlefields_; }
    QString scenarioName(int key) const;
    void loadWorld();
    const std::optional<ScenarioWorld> &world() const {
        return world_;
    }
    void setWorld(const QJsonObject &value);
    void loadMapEdits();
    const std::optional<MapEdits> &mapEdits() const {return mapEdits_;}
    void setMapEdits(const QJsonObject &);
    void applyMapDelta(const FieldEditDelta &delta, bool forward);
    void loadScenePrograms();
    const std::optional<ScenePrograms> &scenePrograms() const {
        return scenePrograms_;
    }
    void setScenePrograms(const QJsonObject &);
    void loadSceneEvents();
    const std::optional<SceneEvents> &sceneEvents() const {
        return sceneEvents_;
    }
    void setSceneEvents(const QJsonObject &);
    void loadDialogues();
    const std::optional<Dialogues> &dialogues() const {
        return dialogues_;
    }
    void setDialogues(const QJsonObject &value);
    void loadConsumableEffects();
    void loadTools();
    const std::optional<ToolSettings> &tools() const { return tools_; }
    void setTools(const QJsonObject &);
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
        return !classNames_.isEmpty() || !addedClasses_.isEmpty() || !replacedClassAssets_.isEmpty();
    }
    bool classNameModified(int row) const {
        return classNames_.contains(row) ||
               (originalClasses_ && row >= originalClasses_->entries().size());
    }
    QByteArray classNameBytes(int row) const;
    void setClassNameBytes(int row, const QByteArray &name);
    ClassData classData() const;
    PersonalNames personalNames() const;
    void setPersonalNames(const QJsonObject &);
    void loadNameTables();
    const std::optional<NameTables> &nameTables() const {return nameTables_;}
    void setNameTables(const QJsonObject &);
    bool personalNamesModified() const { return personalNames_.has_value(); }
    int appendClass(int templateId, const QString &name);
    int createClass(const QString &name);
    int appendItem(int templateId, const QString &name);
    int createItem(const QString &name);
    const WeaponTypes &weaponTypes() const { return weaponTypes_; }
    void setWeaponTypes(const QJsonObject &names);
    int createWeaponType(const QString &name);
    ItemData itemData() const;
    void validateItemReferences() const;
    QByteArray itemNameBytes(int row) const;
    void setItemNameBytes(int row, const QByteArray &);
    bool hasItemData() const {
        return originalItems_.has_value();
    }
    bool itemNamesModified() const {
        return !itemNames_.isEmpty() || !addedItems_.isEmpty() ||
               (originalItems_ && weaponTypes_.names() != originalItems_->weaponTypes().names());
    }
    bool itemNameModified(int row) const {
        return itemNames_.contains(row) ||
               (originalItems_ && row >= originalItems_->entries().size());
    }
    static int classDescriptionIndex(int classId);
    bool contentDeleted(const QString &kind, int id) const;
    const QJsonObject &deletedContent() const { return deletedContent_; }
    void setDeletedContent(const QJsonObject &);
    void markContentDeleted(const QString &, int id, bool deleted);
    int nextContentId(const QString &, int count) const;
    QMap<int, QString> customContent(const QString &);
    QStringList deletionUses(const QString &, int id);
    void deleteCustomContent(const QString &, int id);
    int createAttack(const QString &name, int source = 5);
    int createShop();
    int createEncounterPool();

  private:
    void setScenarioBattlefields(const QJsonArray &);
    void importLegacyShops(const QJsonObject &object);
    void importLegacyConsumables(const QJsonObject &object);
    void importLegacyConsumableEffects(const QJsonObject &object);
    void importLegacyTools(const QJsonObject &);
    void importLegacyIconArtwork(const QJsonObject &);
    void importLegacyAvatarArtwork(const QJsonObject &);
    void importLegacyCombatArtwork(const QJsonObject &);
    void importLegacyArmyArtwork(const QJsonObject &);
    void importLegacyAnimationSequences(const QJsonObject &);
    void importLegacyEncounters(const QJsonObject &object);
    void importLegacyGlobalRate(const QJsonObject &object);
    void importLegacyRewards(const QJsonObject &object);
    void importLegacyScenarioRates(const QJsonObject &object);
    void importLegacyCustomSquads(const QJsonObject &object);
    void importLegacyScenarios(const QJsonObject &object);
    void importLegacyDialogues(const QJsonObject &);
    void importLegacyScenes(const QJsonObject &);
    void importLegacyWorld(const QJsonObject &object);
    Rom rom_;
    std::optional<Rom> runtimeRom_;
    std::optional<Rom> battlefieldRom_;
    QJsonArray scenarioBattlefields_, originalScenarioBattlefields_;
    QJsonObject catalog_, retained_, legacy_;
    bool legacyScenesImported_ = false;
    bool legacyAnimationSequencesImported_ = false;
    std::map<Table, QVector<Record>> tables_;
    QStringList importWarnings_;
    QMap<QString, TextBank> texts_;
    std::optional<Promotion> promotion_;
    std::optional<Shops> shops_;
    std::optional<ConsumableEffects> consumableEffects_;
    std::optional<ToolSettings> tools_;
    SpriteLibrary sprites_;
    std::optional<IconArtwork> icons_;
    std::optional<CategoryIcons> categoryIcons_;
    std::optional<AvatarArtwork> avatars_;
    std::optional<ArmyArtwork> army_;
    std::optional<CombatArtwork> combat_;
    std::optional<AnimationRoutes> animationRoutes_;
    std::optional<AnimationOverrides> animationOverrides_;
    std::optional<AnimationSequences> animationSequences_;
    std::optional<Encounters> encounters_;
    std::optional<Scenarios> scenarios_;
    std::optional<Training> training_;
    std::optional<ScenarioWorld> world_;
    std::optional<CampaignMap> campaignMap_;
    std::optional<MapEdits> mapEdits_;
    std::optional<Dialogues> dialogues_;
    std::optional<ScenePrograms> scenePrograms_;
    std::optional<SceneEvents> sceneEvents_;
    std::optional<ItemData> originalItems_;
    std::optional<AttackData> attacks_;
    WeaponTypes weaponTypes_;
    QVector<ItemAsset> addedItems_;
    QMap<int, QByteArray> itemNames_;
    int appendItemAsset(const ItemAsset &, const QByteArray &description);
    std::optional<PersonalNames> personalNames_;
    std::optional<NameTables> nameTables_;
    std::optional<ClassData> originalClasses_;
    QMap<int, QByteArray> classNames_;
    QMap<int, ClassAsset> replacedClassAssets_;
    QJsonObject deletedContent_;
    QVector<ClassAsset> addedClasses_;
    const ClassAsset &classAsset(int row) const;
    void copyClassVisuals(int sourceId, int targetId);
    int appendClassAsset(const ClassAsset &asset, const QByteArray &description,
                         int promotionTemplate);
};
enum class ItemNameFont { Normal, Small };
ItemNameFont itemNameFont(const QByteArray &name);
QByteArray encodeItemName(const QString &name, ItemNameFont font = ItemNameFont::Normal);
QByteArray encodeClassName(const QString &name);
void atomicWrite(const QString &path, const QByteArray &data);
QJsonObject decodeProject(QByteArray bytes);
} // namespace lordly
