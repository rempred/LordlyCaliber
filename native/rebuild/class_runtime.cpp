#include "engine/scenario_member_runtime.h"
#include "rebuild/class_runtime.h"
#include "rebuild/attack_runtime.h"
#include "rebuild/capacity_runtime.h"
#include "rebuild/leader_affinity.h"
#include "rebuild/pose_runtime.h"
#include "engine/class_text.h"
#include "engine/art_resource_links.h"
#include "engine/promotion.h"
#include "engine/shop_runtime.h"
#include "engine/weapon_types.h"
#include "engine/mission_tables.h"
#include "engine/mission_runtime.h"
#include "rebuild/promotion_runtime.h"
#include "engine/automatic_promotion_owner.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace lordly {
ClassRuntimeBuild
buildClassRuntime(const ClassData &classes, const Compiler &compiler, const QString &directory,
                  quint32 dataRomOffset, bool expandedSaves,
                  const std::function<QByteArray(quint32, quint32)> &residentBuilder,
                  const ItemData *items, CapacityRuntimeBuild *capacity, const AttackData *attacks,
                  const LeaderAffinity *affinity, bool customStoryFlags, const ConsumableTreeInput *consumableTrees,
                  const QByteArray &missionTables, const NewCampaignInput *campaign,
                  bool wideIdleHeight, const MovementInput *movement) {
    ClassRuntimeBuild result;
    result.data = classes.build(0x80243db0);
    if (items) {
        result.data.layout.items = result.data.layout.end;
        result.data.layout.itemCount = int(items->entries().size());
        result.data.bytes += items->build(result.data.layout.items);
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
    }
    if (attacks) {
        AttackPlacement placed;
        placed.address = result.data.layout.end;
        placed.romOffset = dataRomOffset + quint32(result.data.bytes.size());
        placed.records = placed.address + AttackData::recordOffset;
        placed.settings = placed.address + AttackData::settingsOffset;
        placed.count = int(attacks->entries().size());
        const auto bytes = compileAttackRuntime(*attacks, placed, compiler, directory,
            [&](quint32 address) {
                return capacity ? capacity->relocated(address, result.data.layout) : address;
            });
        placed.size = quint32(bytes.size());
        result.data.bytes += bytes;
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        result.attacks = placed;
    }
    if(capacity) {
        if(capacity->base<result.data.layout.end || capacity->storage.isEmpty())
            throw Error("Capacity storage overlaps class or item data");
        result.data.bytes+=QByteArray(capacity->base-result.data.layout.end,0)+capacity->storage;
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
    }
    if (customStoryFlags) {
        result.data.layout.customFlags = result.data.layout.end;
        result.data.bytes += QByteArray(64, 0);
        result.data.layout.end += 64;
    }
    if (capacity || expandedSaves || customStoryFlags || result.data.layout.count > 165) {
        const quint32 codecAddress = (result.data.layout.end + 15U) & ~15U;
        result.data.bytes += QByteArray(codecAddress - result.data.layout.end, 0);
        result.saves = buildSaveRuntime(result.data.layout.discovery, compiler, directory, codecAddress,capacity!=nullptr, result.data.layout.customFlags);
        result.data.bytes += result.saves->compression.bytes;
        while (result.data.bytes.size() % 16) result.data.bytes.append(char(0));
        if(capacity) {
            if(result.saves->capacityMigration.address!=result.data.layout.indexBase+result.data.bytes.size())
                throw Error("Save migration disagrees with its permanent allocation");
            result.data.bytes+=result.saves->capacityMigration.bytes;
            while(result.data.bytes.size()%16)result.data.bytes.append(char(0));
        }
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        if (result.data.layout.end > 0x80400000U)
            throw Error("Save compression exceeds permanent class storage");
        result.placement.saves = result.saves->placement;
    }
    if (!missionTables.isEmpty()) {
        while (result.data.bytes.size() % 16) result.data.bytes.append(char(0));
        result.missionTableAddress = result.data.layout.indexBase + quint32(result.data.bytes.size());
        result.data.bytes += missionTables;
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        if (result.data.layout.end > 0x80400000U)
            throw Error("Extended mission tables exceed permanent class storage");
        if (capacity) capacity->missionWorldTable = MissionTables::worldAddress(result.missionTableAddress);
    }
    auto reclaimStart =
        result.saves ? result.saves->placement.endAddress() : ClassRuntimeBuild::reclaimedStart;
    if (residentBuilder) {
        if (reclaimStart > ClassRuntimeBuild::reclaimedEnd)
            throw Error("Expanded saves leave no resident extension allocation");
        result.resident =
            residentBuilder(reclaimStart, ClassRuntimeBuild::reclaimedEnd - reclaimStart);
        if (result.resident.isEmpty() || result.resident.size() % 16 ||
            quint64(reclaimStart) + result.resident.size() > ClassRuntimeBuild::reclaimedEnd)
            throw Error("Resident extension exceeds its reclaimed class allocation");
        result.placement.residentAddress = reclaimStart;
        result.placement.residentBytes = quint32(result.resident.size());
        result.placement.residentChecksum = crc32(result.resident);
        reclaimStart += result.placement.residentBytes;
    }
    if(capacity) {
        compileCapacityRuntime(*capacity,result.data.layout.end,result.data.layout,compiler,directory);
        result.data.bytes+=capacity->code;
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
    }
    if (affinity) {
        while (result.data.bytes.size()%16) result.data.bytes.append(char(0));
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        const auto payload = buildLeaderAffinity(*affinity, result.data.layout.end,
            dataRomOffset + quint32(result.data.bytes.size()), compiler, directory);
        result.data.bytes += payload.data;
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        if (result.data.layout.end > 0x80400000U)
            throw Error("Combat affinity exceeds permanent class storage");
        result.affinityOwner = payload.owner;
    }
    if(consumableTrees){
        while(result.data.bytes.size()%16)result.data.bytes.append(char(0));
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
        result.consumableTrees=buildConsumableTrees(*consumableTrees,result.data.layout.end,
            dataRomOffset+quint32(result.data.bytes.size()),compiler,directory,
            [&](quint32 at){return capacity?capacity->relocated(at,result.data.layout):at;});
        result.data.bytes+=result.consumableTrees->data;
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
    }
    if(campaign && !campaign->settings.isEmpty()) {
        while(result.data.bytes.size()%16)result.data.bytes.append(char(0));
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
        const auto module=buildNewCampaignRuntime(*campaign,compiler,directory,result.data.layout.end,
            [&](quint32 at){return capacity?capacity->relocated(at,result.data.layout):at;},result.data.layout.customFlags);
        result.data.bytes+=module.bytes;result.campaignBridge=module.entries.value("lc_campaign_bridge");
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
        if(result.data.layout.end>0x80400000U)throw Error("Starting armies exceed permanent game storage");
    }
    if (wideIdleHeight) {
        while (result.data.bytes.size() % 16) result.data.bytes.append(char(0));
        result.bodyHeight = result.data.layout.indexBase + quint32(result.data.bytes.size()) + poseHeightHeaderSize;
        result.data.bytes += buildPoseHeightRuntime(result.bodyHeight, compiler, directory);
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        if (result.data.layout.end > 0x80400000U)
            throw Error("Wide idle-frame height support exceeds permanent class storage");
    }
    if (movement) {
        while(result.data.bytes.size()%16)result.data.bytes.append(char(0));
        result.data.layout.end=result.data.layout.indexBase+quint32(result.data.bytes.size());
        auto input=*movement; input.classCount=result.data.layout.count; if(capacity){input.bindings=capacity->symbols;for(auto i=capacity->entries.begin();i!=capacity->entries.end();++i)input.bindings[i.key()]=i.value();}
        result.movement=buildMovementRuntime(input,result.data.layout.end,dataRomOffset+result.data.bytes.size(),compiler,directory,[&](quint32 a){
            if (result.missionTableAddress) {
                const auto world=MissionTables::worldAddress(result.missionTableAddress);
                if(a>=0x8018f5a0 && a<0x8018f80d)return result.missionTableAddress+a-0x8018f5a0;
                if(a>=0x801e7e10 && a<0x801e8134)return world+a-0x801e7e10;
                if(a>=0x801e8134 && a<0x801e8240)return world+MissionTables::keyCount*12+a-0x801e8134;
                if(a>=0x801e8240 && a<0x801e8564)return world+MissionTables::keyCount*16+a-0x801e8240;
            }
            if (capacity) return capacity->relocated(a,result.data.layout);
            if(a>=0x80187bcc && a<0x8018aa70)return result.data.layout.indexBase+a-0x80187bcc;
            return a;
        });
        result.data.bytes+=result.movement->data;result.data.layout.end=result.data.layout.indexBase+result.data.bytes.size();
    }
    if (customStoryFlags) {
        while (result.data.bytes.size() % 16) result.data.bytes.append(char(0));
        QByteArray metadata(32, 0);
        metadata.replace(0, 8, "LCFLAGS1");
        result.placement.customFlagsGetter = result.saves->code.entries.value("lc_event_story_flag_get");
        result.placement.customFlagsSetter = result.saves->code.entries.value("lc_event_story_flag_set");
        put32(metadata, 8, result.data.layout.customFlags);
        put32(metadata, 12, result.placement.customFlagsGetter);
        put32(metadata, 16, result.placement.customFlagsSetter);
        put32(metadata, 20, 256);
        put32(metadata, 24, 512);
        put32(metadata, 28, crc32(metadata.first(28)));
        result.data.bytes += metadata;
        result.data.layout.end = result.data.layout.indexBase + quint32(result.data.bytes.size());
        if (result.data.layout.end > 0x80400000U)
            throw Error("Custom story flags exceed permanent class storage");
    }
    result.dataRomOffset = dataRomOffset;
    if ((dataRomOffset & 15) || dataRomOffset < 0x2800000 ||
        quint64(dataRomOffset) + result.data.bytes.size() > 0x4000000)
        throw Error("Class data needs an aligned allocation within the cartridge address range");
    QFile namesSource(":/game/classes/personal-names.c");
    if (!namesSource.open(QIODevice::ReadOnly)) throw Error("Missing personal-name C source");
    const auto namesModule = compiler.compileModule(namesSource.readAll(), directory, "personal_names", 0x8016bd20,
        {{"personal_names_config.h", QByteArray("#define LC_ENEMY_COUNT ") + (capacity ? "256\n#define LC_CAPACITY 1\n" : "100\n")}},
        {{"g_func_0019554C_records_56", 0x80193bc0},
         {"g_func_0019554C_records_52", capacity ? capacity->base : 0x80195560U},
         {"lc_enemy_types", capacity ? capacity->symbols.value("lc_enemy_types") : 0},
         {"D_8018EA00", result.data.layout.names + PersonalNames::pointerOffset}, {"D_8018FEAC", 0x8018feac},
         {"func_0002E138", 0x8009dd38}, {"rand", 0x8009c7cc}, {"func_80093380", 0x80093380},
         {"func_0002C950", 0x8009c550}, {"func_0002C9E0", 0x8009c5e0}, {"resource_free", 0x800712c4}});
    result.personalNamesCode = namesModule.fitOwner(PersonalNames::ownerSize);
    result.data.bytes.replace(result.data.layout.names - result.data.layout.indexBase + 32, 32,
                              QByteArray::fromHex(sha256(result.personalNamesCode)));
    result.compiledData = compiler.compileData(result.data.bytes, directory, "class_data");
    QFile source(":/game/classes/class-runtime.c");
    if (!source.open(QIODevice::ReadOnly))
        throw Error("Missing maintained class initialization source");
    const auto header = QString("#define LC_RESERVED_BYTES %1\n"
                                "#define LC_RECLAIM_START %2U\n"
                                "#define LC_RECLAIM_SIZE %3\n"
                                "#define LC_ROM_DATA %4U\n")
                            .arg(result.data.bytes.size())
                            .arg(reclaimStart)
                            .arg(ClassRuntimeBuild::reclaimedEnd - reclaimStart)
                            .arg(dataRomOffset)
                            .toLatin1();
    result.code = compiler.compileModule(source.readAll(), directory, "class_runtime", 0x80187c14,
                                         {{"class_runtime_config.h", header}},
                                         {{"lc_original_heap_init", 0x80070c60},
                                          {"lc_class_data_end", result.data.layout.end},
                                          {"lc_original_pool_add", 0x80070d20},
                                          {"lc_original_secondary_reset", 0x80070f14},
                                          {"lc_dma_read", result.saves ? result.saves->code.entries.value("lc_save_load_data") : 0x8009da50}});
    if (result.code.bytes.size() > ClassPlacement::codeCapacity ||
        result.code.entries.size() != 3 || !result.code.entries.contains("lc_heap_reset") ||
        !result.code.entries.contains("lc_secondary_reset") ||
        !result.code.entries.contains("lc_boot_classes"))
        throw Error("Class initialization module exceeds or disagrees with its runtime layout");
    if (result.saves) {
        QFile adapter(":/game/classes/description-index.s");
        if (!adapter.open(QIODevice::ReadOnly))
            throw Error("Missing maintained class description routing source");
        result.placement.descriptionEntry = result.code.address + quint32(result.code.bytes.size());
        const auto bytes = compiler.assembleOwner(adapter.readAll(), directory, "description_index",
                                                  result.placement.descriptionEntry,
                                                  {{"lc_description_continue", 0x801805d8}});
        if (bytes.size() != 64 ||
            result.code.bytes.size() + bytes.size() > ClassPlacement::codeCapacity)
            throw Error("Class description routing exceeds its resident code allocation");
        result.code.bytes += bytes;
    }
    result.placement.layout = result.data.layout;
    result.placement.dataRomOffset = dataRomOffset;
    result.placement.codeBytes = quint32(result.code.bytes.size());
    result.placement.bootEntry = result.code.entries["lc_boot_classes"];
    result.placement.heapEntry = result.code.entries["lc_heap_reset"];
    result.placement.secondaryEntry = result.code.entries["lc_secondary_reset"];
    result.placement.directory = ClassRuntimeBuild::directoryAddress;
    result.placement.directoryCapacity = ClassRuntimeBuild::directoryCapacity;
    result.descriptor =
        compiler.compileData(result.placement.descriptor(result.compiledData, result.code.bytes),
                             directory, "class_placement");
    return result;
}
QByteArray linkClassRuntime(const Rom &rom, const ClassRuntimeBuild &build,
                            const PromotionRuntimeBuild *menus) {
    rom.requireSupported();
    if (rom.revision() != 0 || build.code.address != ClassPlacement::codeRamAddress ||
        build.compiledData != build.data.bytes ||
        build.resident.size() != build.placement.residentBytes ||
        (!build.resident.isEmpty() && crc32(build.resident) != build.placement.residentChecksum) ||
        build.descriptor != build.placement.descriptor(build.compiledData, build.code.bytes))
        throw Error("Class runtime build does not match its declared layout");
    const auto prior = ClassPlacement::read(rom);
    const auto shopPlacement = ShopPlacement::read(rom);
    if (prior && prior->layout.customFlags && !build.data.layout.customFlags)
        throw Error("Rebuilding classes must preserve their custom story flags");
    for (const auto slot : {ClassPlacement::flagGetterSlot, ClassPlacement::flagSetterSlot}) {
        const auto expected = prior && prior->layout.customFlags
            ? (slot == ClassPlacement::flagGetterSlot ? prior->customFlagsGetter : prior->customFlagsSetter)
            : (slot == ClassPlacement::flagGetterSlot ? 0x801702f8U : 0x8017049cU);
        if (build.data.layout.customFlags && u32(rom.bytes(), slot) != expected)
            throw Error("Unrecognized primary event story flag service");
    }
    if (prior && prior->saves.installed() && !build.saves)
        throw Error("Rebuilding this class allocation also requires its expanded save runtime");
    if (prior && prior->residentBytes && build.resident.isEmpty() && !build.removeResident)
        throw Error("Rebuilding classes also requires preserving their resident extension");
    if (prior && prior->descriptionEntry && !build.placement.descriptionEntry)
        throw Error("Rebuilding this class allocation requires its description routing");
    const auto inventory = prior && !prior->runtimeInventory.isEmpty()
                               ? std::optional<ClassRuntimeInventory>(
                                     ClassRuntimeInventory::decode(prior->runtimeInventory))
                               : std::nullopt;
    QSet<quint32> configuredOwners;
    if (inventory)
        for (const auto &owner : inventory->owners)
            configuredOwners.insert(owner.start);
    if (inventory && !menus && prior->layout.count != build.data.layout.count)
        throw Error("Growing expanded classes also requires rebuilding their menu code");
    const auto catalog = classRelocationCatalog();
    const auto original = catalog["originalBases"].toObject();
    const QMap<QString, quint32> nextBases = {
        {"classes", build.data.layout.indexBase},
        {"appearances", build.data.layout.appearances},
        {"directory", build.placement.directory},
        {"items", build.data.layout.items ? build.data.layout.items : 0x8018c400U}};
    const QMap<QString, quint32> previousBases = {
        {"classes", prior ? prior->layout.indexBase : quint32(original["classes"].toDouble())},
        {"appearances",
         prior ? prior->layout.appearances : quint32(original["appearances"].toDouble())},
        {"directory", prior ? prior->directory : quint32(original["directory"].toDouble())},
        {"items", prior && prior->layout.items ? prior->layout.items : 0x8018c400U}};
    const QMap<QString, quint32> nextCalls = {
        {"lc_boot_classes", build.placement.bootEntry},
        {"lc_heap_reset", build.placement.heapEntry},
        {"lc_secondary_reset", build.placement.secondaryEntry}};
    QMap<QString, quint32> previousCalls;
    if (prior)
        previousCalls = {{"lc_boot_classes", prior->bootEntry},
                         {"lc_heap_reset", prior->heapEntry},
                         {"lc_secondary_reset", prior->secondaryEntry}};
    QMap<qsizetype, quint32> patches;
    auto patch = [&](qsizetype site, quint32 value) {
        if (patches.contains(site) && patches[site] != value)
            throw Error("Class relocation conflicts at z64 " + QString::number(site, 16));
        patches[site] = value;
    };
    auto promotion = promotionOwners()["owners"].toArray();
    promotion.append(scenarioMemberOwner());
    for (const auto entry : catalog["owners"].toArray()) {
        const auto owner = entry.toObject();
        const qsizetype start = owner["start"].toInt(), size = owner["size"].toInt();
        auto canonical = MissionTables::canonicalCode(rom, start, size);
        if (canonical.size() != size)
            throw Error("Truncated class runtime owner");
        if (configuredOwners.contains(quint32(start))) {
            if (!owner["calls"].toArray().isEmpty() || !owner["constants"].toArray().isEmpty())
                throw Error("Configured menu overlaps class initialization instructions");
            continue; // Already verified; relocated with its actual reference map below.
        }
        for (const auto value : classOwnerReferences(owner, canonical, previousBases, original)) {
            const auto ref = value.toObject();
            const auto group = ref["group"].toString();
            const auto highSite = ref["high"].toInt(), lowSite = ref["low"].toInt();
            const auto high = u32(rom.bytes(), start + highSite),
                       low = u32(rom.bytes(), start + lowSite);
            const bool signedLow = low >> 26 != 13;
            const quint32 addend = ref["addend"].toInt();
            auto highPart = [&](quint32 address) {
                return (address + (signedLow ? 0x8000U : 0U)) >> 16;
            };
            auto highWord = [&](quint32 address) {
                return (high & 0xffff0000U) | highPart(address);
            };
            auto lowWord = [&](quint32 address) { return (low & 0xffff0000U) | (address & 65535); };
            const auto before = previousBases.value(group) + addend;
            const auto retail = quint32(original[group].toDouble()) + addend;
            const bool fixed = ref["fixedAddress"].toBool();
            const auto after = fixed ? retail : nextBases.value(group) + addend;
            const bool previousMatches = high == highWord(before) && low == lowWord(before);
            const bool fixedMatches = fixed && high == highWord(retail) && low == lowWord(retail);
            if (!nextBases.contains(group) || high >> 26 != 15 || (!previousMatches && !fixedMatches))
                throw Error("Class reference disagrees with its declared prior placement");
            put32(canonical, highSite, highWord(retail));
            put32(canonical, lowSite, lowWord(retail));
            patch(start + highSite, highWord(after));
            patch(start + lowSite, lowWord(after));
        }
        for (const auto value : owner["calls"].toArray()) {
            const auto call = value.toObject();
            const auto at = call["offset"].toInt();
            const auto symbol = call["symbol"].toString();
            const auto word = u32(rom.bytes(), start + at);
            const auto originalTarget = quint32(call["originalTarget"].toDouble());
            const auto previousTarget = prior ? previousCalls.value(symbol) : originalTarget;
            if (!nextCalls.contains(symbol) || (word >> 26 != 2 && word >> 26 != 3) ||
                ((word & 0x3ffffffU) << 2) != (previousTarget & 0xfffffffU))
                throw Error("Class initialization call was changed outside its rebuild");
            put32(canonical, at, (word & 0xfc000000U) | ((originalTarget >> 2) & 0x3ffffffU));
            patch(start + at, (word & 0xfc000000U) | ((nextCalls[symbol] >> 2) & 0x3ffffffU));
        }
        for (const auto value : owner["constants"].toArray()) {
            const auto field = value.toObject();
            const auto at = field["offset"].toInt();
            const auto word = u32(rom.bytes(), start + at);
            const bool capacity = field["field"] == "directoryCapacity";
            const bool high = field["field"] == "codeCacheEndHigh";
            const auto half = [high](quint32 address) {
                return high ? (address + 0x8000U) >> 16 : address & 65535U;
            };
            const auto retail = capacity ? 4U : half(0x80186310U);
            const auto previous = prior ? (capacity ? quint32(prior->directoryCapacity)
                                                    : half(prior->instructionEnd()))
                                        : retail;
            const auto next = capacity ? quint32(build.placement.directoryCapacity)
                                       : half(build.placement.instructionEnd());
            if ((word & 65535) != previous)
                throw Error("Class allocator/cache preparation disagrees with its prior layout");
            put32(canonical, at, (word & 0xffff0000U) | retail);
            patch(start + at, (word & 0xffff0000U) | next);
        }
        canonical = ClassTextPlacement::canonicalAll(rom, canonical, start);
        canonical = canonicalWeaponTypeText(rom, canonical, start);
        if (shopPlacement)
            canonical = shopPlacement->canonicalReferences(canonical, start);
        validateArtOwnerLinks(rom,start,size);
        const auto hash = QString::fromLatin1(sha256(canonicalAutomaticRequirementOwner(
            canonicalArtOwner(canonical,start), start)));
        bool recognized = hash == owner["retailSha256"].toString();
        for (const auto &v : shopConsumerOwners()) {
            const auto variant = v.toObject();
            if (variant["start"].toInt() == start && variant["size"].toInt() == size)
                recognized |= hash == variant["cSourceSha256"].toString();
        }
        for (const auto item : promotion) {
            const auto compatible = item.toObject();
            if (compatible["start"].toInt() == start && compatible["size"].toInt() == size)
                for (const auto kind : {"familySha256", "directGateSha256", "cSourceSha256"})
                    recognized |= hash == compatible[kind].toString();
        }
        if (!recognized)
            throw Error("Unrecognized class runtime owner at z64 " + QString::number(start, 16));
    }
    if (inventory) {
        for (const auto &owner : inventory->owners) {
            const auto code =
                inventory->relocate(owner.start, rom.bytes().mid(owner.start, owner.size),
                                    ClassRuntimeInventory::dataBases(prior->layout),
                                    ClassRuntimeInventory::dataBases(build.data.layout));
            for (quint32 offset = 0; offset < owner.size; offset += 4)
                patch(owner.start + offset, u32(code, offset));
        }
    }
    const auto at = build.placement.dataRomOffset;
    const auto bytes = build.compiledData.size();
    const auto end = quint64(at) + bytes;
    if (at < quint64(rom.bytes().size()) &&
        (!prior || at != prior->dataRomOffset ||
         (bytes > prior->layout.end - prior->layout.indexBase &&
          quint64(prior->dataRomOffset) + prior->layout.end - prior->layout.indexBase !=
              quint64(rom.bytes().size()))))
        throw Error("Expanded class allocation would overwrite unrelated ROM data");
    QByteArray priorNames;
    if (prior) {
        const auto at = prior->dataRomOffset + prior->layout.names - prior->layout.indexBase;
        if (rom.bytes().mid(at,8)=="LCNAMES1") priorNames=rom.bytes().mid(at,u32(rom.bytes(),at+8));
    }
    PersonalNames::verifyOwner(rom, priorNames);
    if (!build.affinityOwner.isEmpty() && !LeaderAffinity::load(rom).unavailable().isEmpty())
        throw Error("Cannot replace unrecognized combat-affinity code");
    auto image = rom.bytes();
    // Remove the authenticated field-exit call before reclaiming its code.
    image.replace(missionExitDispatch, 16, canonicalMissionExitCode(image));
    image.replace(PersonalNames::ownerOffset, PersonalNames::ownerSize, build.personalNamesCode);
    if (prior && prior->residentBytes)
        image.replace(prior->residentAddress - ClassPlacement::residentDelta, prior->residentBytes,
                      QByteArray(prior->residentBytes, 0));
    if (prior)
        image.replace(prior->dataRomOffset, prior->layout.end - prior->layout.indexBase,
                      QByteArray(prior->layout.end - prior->layout.indexBase, char(-1)));
    if (prior && at == prior->dataRomOffset && end < quint64(image.size()) &&
        quint64(prior->dataRomOffset) + prior->layout.end - prior->layout.indexBase ==
            quint64(image.size()))
        image.resize(qsizetype(end));
    if (end > quint64(image.size()))
        image.append(QByteArray(qsizetype(end) - image.size(), char(-1)));
    for (auto it = patches.begin(); it != patches.end(); ++it)
        put32(image, it.key(), it.value());
    auto paddedCode = build.code.bytes;
    paddedCode.append(QByteArray(ClassPlacement::codeCapacity - paddedCode.size(), 0));
    image.replace(ClassPlacement::codeRomOffset, paddedCode.size(), paddedCode);
    auto placement = build.placement;
    if (build.saves)
        placement.saves =
            installSaveRuntime(image, *build.saves, prior ? prior->saves : SavePlacement{});
    if (!build.resident.isEmpty())
        image.replace(placement.residentAddress - ClassPlacement::residentDelta,
                      build.resident.size(), build.resident);
    if (placement.descriptionEntry)
        installClassDescriptionRouting(image, prior ? prior->descriptionEntry : 0,
                                       placement.descriptionEntry);
    if (build.data.layout.customFlags) {
        put32(image, ClassPlacement::flagGetterSlot, build.placement.customFlagsGetter);
        put32(image, ClassPlacement::flagSetterSlot, build.placement.customFlagsSetter);
    }
    const auto descriptor = placement.descriptor(build.compiledData, build.code.bytes);
    image.replace(ClassPlacement::descriptorRomOffset, descriptor.size(), descriptor);
    image.replace(at, bytes, build.compiledData);
    if (!build.affinityOwner.isEmpty())
        image.replace(LeaderAffinity::offset, LeaderAffinity::size, build.affinityOwner);
    if(build.consumableTrees)image=installConsumableTrees(image,*build.consumableTrees);
    if (build.attacks) {
        const auto old = rom.bytes().mid(AttackPlacement::descriptorOffset, AttackPlacement::descriptorSize);
        if (!AttackPlacement::read(rom) && old != QByteArray(AttackPlacement::descriptorSize, char(-1)))
            throw Error("The attack descriptor area contains unrelated data");
        const auto &p = *build.attacks;
        const auto h = p.descriptor(build.compiledData.mid(p.address - build.data.layout.indexBase, p.size));
        image.replace(AttackPlacement::descriptorOffset, h.size(), h);
    }
    if (menus) {
        const auto resource = ExpandedPromotionResource::decode(menus->container);
        resource.inventory.requireComplete();
        if (resource.inventory.encode() != menus->inventory.encode() ||
            resource.data.classCount != build.data.layout.count ||
            menus->owners.size() != resource.inventory.owners.size())
            throw Error("Class menu build disagrees with its resource or class count");
        for (const auto &owner : resource.inventory.owners) {
            if (!menus->owners.contains(owner.start) ||
                menus->owners[owner.start].size() != owner.size)
                throw Error("Class menu build is missing a complete owner");
            image.replace(owner.start, owner.size, menus->owners[owner.start]);
        }
        const auto slot = rom.bytes().mid(ExpandedPromotionResource::romOffset,
                                          ExpandedPromotionResource::slotSize);
        const auto priorPromotion = Promotion::load(rom);
        if (!priorPromotion.expanded &&
            std::any_of(slot.begin(), slot.end(), [](char byte) { return quint8(byte) != 255; }))
            throw Error("Expanded promotion allocation is occupied");
        if (priorPromotion.directGates)
            image.replace(0x27b0800, 1024, QByteArray(1024, char(-1)));
        image.replace(ExpandedPromotionResource::romOffset, menus->container.size(),
                      menus->container);
        resource.inventory.verify(Rom::fromBytes(image),
                                  ClassRuntimeInventory::dataBases(build.data.layout));
    }
    Rom::updateChecksum(image);
    ClassData::load(Rom::fromBytes(image));
    if (menus)
        Promotion::load(Rom::fromBytes(image));
    return image;
}
} // namespace lordly
