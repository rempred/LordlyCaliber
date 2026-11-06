#pragma once
#include "engine/class_data.h"
#include "engine/item_data.h"
#include "rebuild/compiler.h"
#include "rebuild/save_runtime.h"
#include "rebuild/consumable_tree_runtime.h"
#include "rebuild/new_campaign_runtime.h"

namespace lordly {
struct PromotionRuntimeBuild;
struct CapacityRuntimeBuild;
class LeaderAffinity;
struct ClassRuntimeBuild {
    ClassDataImage data;
    QByteArray compiledData;
    CompiledModule code;
    ClassPlacement placement;
    QByteArray descriptor;
    std::optional<SaveRuntimeBuild> saves;
    std::optional<AttackPlacement> attacks;
    std::optional<ConsumableTreePlacement> consumableTrees;
    QByteArray resident, personalNamesCode, affinityOwner;
    bool removeResident = false;
    quint32 dataRomOffset = 0, missionTableAddress = 0, campaignBridge = 0;
    static constexpr quint32 codeRomOffset = 0x5db14;
    static constexpr quint32 directoryAddress = 0x80187e00;
    static constexpr int directoryCapacity = 10;
    static constexpr quint32 reclaimedStart = 0x80187f00, reclaimedEnd = 0x8018aa70;
};
// Compile the data and its initialization/reset functions. This does not install
// runtime references or produce a playable ROM on its own.
ClassRuntimeBuild
buildClassRuntime(const ClassData &classes, const Compiler &compiler, const QString &directory,
                  quint32 dataRomOffset, bool expandedSaves = false,
                  const std::function<QByteArray(quint32, quint32)> &residentBuilder = {},
                  const ItemData *items = nullptr, CapacityRuntimeBuild *capacity = nullptr,
                  const AttackData *attacks = nullptr, const LeaderAffinity *affinity = nullptr,
                  bool customStoryFlags = false, const ConsumableTreeInput *consumableTrees = nullptr,
                  const QByteArray &missionTables = {}, const NewCampaignInput *campaign = nullptr);
QByteArray linkClassRuntime(const Rom &rom, const ClassRuntimeBuild &build,
                            const PromotionRuntimeBuild *menus = nullptr);
} // namespace lordly
