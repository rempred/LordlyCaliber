#pragma once
#include "engine/class_data.h"
#include "engine/consumable_effects.h"
#include "engine/dialogues.h"
#include "engine/encounters.h"
#include "engine/promotion.h"
#include "engine/rom.h"
#include "engine/scenario_world.h"
#include "engine/scenarios.h"
#include "engine/scene_events.h"
#include "engine/scene_programs.h"
#include "engine/shops.h"
#include "engine/text.h"
#include "engine/tool_settings.h"
#include "engine/sprite_library.h"
#include "engine/icon_artwork.h"
#include "engine/avatar_artwork.h"
#include "engine/army_artwork.h"
#include "engine/combat_artwork.h"
#include "engine/animation_routes.h"
#include "engine/animation_overrides.h"
#include "engine/body_animation_data.h"
#include "engine/animation_sequences.h"
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
    const SpriteLibrary &sprites() const { return sprites_; }
    void setSprites(const SpriteLibrary &library) { sprites_ = library; }
    BodyAnimationData::Pointer bodyAnimationData();
    void loadAnimationSequences();
    const std::optional<AnimationSequences> &animationSequences()const{return animationSequences_;}
    void setAnimationSequences(const QJsonObject &);
    void setPrivateAnimationState(const QJsonObject &sequences, const QJsonObject &overrides);
    void validateAnimationAssignments();
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
    void importLegacyTools(const QJsonObject &);
    void importLegacyIconArtwork(const QJsonObject &);
    void importLegacyAvatarArtwork(const QJsonObject &);
    void importLegacyCombatArtwork(const QJsonObject &);
    void importLegacyArmyArtwork(const QJsonObject &);
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
    QJsonObject catalog_, retained_, legacy_;
    bool legacyScenesImported_ = false;
    std::map<Table, QVector<Record>> tables_;
    QStringList importWarnings_;
    QMap<QString, TextBank> texts_;
    std::optional<Promotion> promotion_;
    std::optional<Shops> shops_;
    std::optional<ConsumableEffects> consumableEffects_;
    std::optional<ToolSettings> tools_;
    SpriteLibrary sprites_;
    std::optional<IconArtwork> icons_;
    std::optional<AvatarArtwork> avatars_;
    std::optional<ArmyArtwork> army_;
    std::optional<CombatArtwork> combat_;
    std::optional<AnimationRoutes> animationRoutes_;
    std::optional<AnimationOverrides> animationOverrides_;
    std::optional<AnimationSequences> animationSequences_;
    std::optional<Encounters> encounters_;
    std::optional<Scenarios> scenarios_;
    std::optional<ScenarioWorld> world_;
    std::optional<Dialogues> dialogues_;
    std::optional<ScenePrograms> scenePrograms_;
    std::optional<SceneEvents> sceneEvents_;
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
