#include "rebuild/class_runtime.h"
#include "engine/class_text.h"
#include "engine/promotion.h"
#include "engine/shop_runtime.h"
#include "rebuild/promotion_runtime.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace lordly {
ClassRuntimeBuild buildClassRuntime(const ClassData &classes, const Compiler &compiler,
                                    const QString &directory, quint32 dataRomOffset,
                                    bool expandedSaves) {
    ClassRuntimeBuild result;
    result.data = classes.build(0x80243db0);
    if (expandedSaves || result.data.layout.count > 165) {
        result.saves = buildSaveRuntime(result.data.layout.discovery, compiler, directory);
        result.placement.saves = result.saves->placement;
    }
    const auto reclaimStart =
        result.saves ? result.saves->placement.endAddress() : ClassRuntimeBuild::reclaimedStart;
    result.dataRomOffset = dataRomOffset;
    if ((dataRomOffset & 15) || dataRomOffset < 0x2800000 ||
        quint64(dataRomOffset) + result.data.bytes.size() > 0x4000000)
        throw Error("Class data needs an aligned allocation within the cartridge address range");
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
                                          {"lc_original_pool_add", 0x80070d20},
                                          {"lc_original_secondary_reset", 0x80070f14},
                                          {"lc_dma_read", 0x8009da50}});
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
        build.descriptor != build.placement.descriptor(build.compiledData, build.code.bytes))
        throw Error("Class runtime build does not match its declared layout");
    const auto prior = ClassPlacement::read(rom);
    const auto textPlacement = ClassTextPlacement::read(rom);
    const auto shopPlacement = ShopPlacement::read(rom);
    if (prior && prior->saves.installed() && !build.saves)
        throw Error("Rebuilding this class allocation also requires its expanded save runtime");
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
    QFile file(":/game/classes/relocations.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class relocation catalog");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    const auto original = catalog["originalBases"].toObject();
    const QMap<QString, quint32> nextBases = {{"classes", build.data.layout.indexBase},
                                              {"appearances", build.data.layout.appearances},
                                              {"directory", build.placement.directory}};
    const QMap<QString, quint32> previousBases = {
        {"classes", prior ? prior->layout.indexBase : quint32(original["classes"].toDouble())},
        {"appearances",
         prior ? prior->layout.appearances : quint32(original["appearances"].toDouble())},
        {"directory", prior ? prior->directory : quint32(original["directory"].toDouble())}};
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
    const auto promotion = promotionOwners()["owners"].toArray();
    for (const auto entry : catalog["owners"].toArray()) {
        const auto owner = entry.toObject();
        const qsizetype start = owner["start"].toInt(), size = owner["size"].toInt();
        auto canonical = rom.bytes().mid(start, size);
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
            const auto after = nextBases.value(group) + addend;
            const auto retail = quint32(original[group].toDouble()) + addend;
            if (!nextBases.contains(group) || high >> 26 != 15 || high != highWord(before) ||
                low != lowWord(before))
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
        if (textPlacement)
            canonical = textPlacement->canonicalReferences(canonical, start);
        if (shopPlacement)
            canonical = shopPlacement->canonicalReferences(canonical, start);
        const auto hash = QString::fromLatin1(sha256(canonical));
        bool recognized = hash == owner["retailSha256"].toString();
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
    auto image = rom.bytes();
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
    if (placement.descriptionEntry)
        installClassDescriptionRouting(image, prior ? prior->descriptionEntry : 0,
                                       placement.descriptionEntry);
    const auto descriptor = placement.descriptor(build.compiledData, build.code.bytes);
    image.replace(ClassPlacement::descriptorRomOffset, descriptor.size(), descriptor);
    image.replace(at, bytes, build.compiledData);
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
