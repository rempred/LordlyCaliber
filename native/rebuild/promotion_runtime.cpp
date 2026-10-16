#include "rebuild/promotion_runtime.h"
#include "engine/art_resource_links.h"
#include <QFile>
#include <QJsonDocument>
#include <QSet>

namespace lordly {
static QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing runtime source: " + path);
    return file.readAll();
}
PromotionRuntimeBuild buildPromotionRuntime(const Rom &source, const ClassDataLayout &classes,
                                            const Promotion &promotion, const Compiler &compiler,
                                            const QString &directory, bool familyTabs) {
    source.requireSupported();
    validateArtResourceLinks(source);
    if (source.revision() != 0)
        throw Error("Expanded class menus require US Rev 0");
    PromotionRuntimeBuild result;
    const auto data = promotion.buildExpandedData();
    result.layout = ClassMenuLayout::build(classes, data, familyTabs);
    result.inventory.classCount = classes.count;
    const auto placement = ClassPlacement::read(source);
    const auto previous = placement && !placement->runtimeInventory.isEmpty()
                              ? std::optional<ClassRuntimeInventory>(
                                    ClassRuntimeInventory::decode(placement->runtimeInventory))
                              : std::nullopt;
    const auto canonicalBases = ClassRuntimeInventory::canonicalBases();
    const auto nextBases = ClassRuntimeInventory::dataBases(classes);
    const auto classCatalog =
        QJsonDocument::fromJson(read(":/game/classes/relocations.json")).object();
    const QMap<quint32, QByteArray> familyChanges = {{0xc38c0, "bne $v1, $v0, .+32"},
                                                     {0xc3a88, "bne $v1, $v0, .+32"},
                                                     {0xcce00, "move $v0, $v1"}};
    for (const auto &entry : expandedPromotionOwners()) {
        const auto owner = entry.toObject();
        const quint32 start = owner["start"].toInt(), size = owner["size"].toInt();
        auto original = source.bytes().mid(start, size);
        if (placement)
            original = placement->canonicalReferences(original, start);
        const auto hash = sha256(canonicalArtOwner(original,start));
        bool known = false;
        for (const auto key : {"retailSha256", "familySha256", "cSourceSha256", "directGateSha256"})
            known |= owner.contains(key) && hash == owner[key].toString().toLatin1();
        if (previous)
            for (const auto &record : previous->owners)
                if (record.start == start)
                    known |= hash == record.canonicalSha256 || sha256(original)==record.canonicalSha256;
        if (!known || original.size() != size)
            throw Error("Unrecognized expanded menu source owner: " + owner["name"].toString());

        QMap<QString, quint32> symbols;
        const auto fixedSymbols = owner["symbols"].toObject();
        for (auto it = fixedSymbols.begin(); it != fixedSymbols.end(); ++it)
            symbols[it.key()] = quint32(it.value().toDouble());
        symbols.insert(result.layout.dataSymbols);
        if (familyTabs)
            symbols["lc_family_roster_class"] = 0xbc1f8U + 0x801135a0U;
        QVector<RuntimeDataReference> references;
        if (previous)
            for (const auto &record : previous->owners)
                if (record.start == start)
                    references = record.references;
        if (!previous)
            for (const auto &v : owner["expandedDataReferences"].toArray()) {
                const auto ref = v.toObject();
                references.append({ref["group"].toString(), quint32(ref["high"].toInt()),
                                   quint32(ref["low"].toInt()), ref["addend"].toInt()});
            }
        // Retained instructions use the authenticated original reference map.
        for (const auto &value : classCatalog["owners"].toArray()) {
            if (previous)
                break;
            const auto candidate = value.toObject();
            if (candidate["start"].toInt() != int(start))
                continue;
            const auto bases = classCatalog["originalBases"].toObject();
            for (const auto &v : classOwnerReferences(candidate, original, canonicalBases, bases)) {
                const auto ref = v.toObject();
                references.append({ref["group"].toString(), quint32(ref["high"].toInt()),
                                   quint32(ref["low"].toInt()), ref["addend"].toInt()});
            }
        }
        if (!references.isEmpty()) {
            ClassRuntimeInventory move;
            move.classCount = classes.count;
            move.owners.append(
                ClassRuntimeInventory::capture(start, original, references, canonicalBases));
            original = move.relocate(start, original, canonicalBases, nextBases);
        }
        auto addModuleReferences = [&](const CompiledModule &module, quint32 offset) {
            for (const auto &ref : module.addressReferences) {
                QString group;
                qint32 addend = ref.addend;
                if (ref.symbol == "lc_class_records") {
                    group = "classes";
                    addend += 72;
                } else if (ref.symbol == "lc_named_class_map")
                    group = "aliases";
                else if (ref.symbol == "lc_discovery_flags")
                    group = "discovery";
                else
                    continue;
                references.append({group, offset + ref.highOffset, offset + ref.lowOffset, addend});
            }
        };
        auto assemble = [&](const QByteArray &text, const QString &suffix, quint32 at,
                            const QMap<QString, quint32> &bindings = QMap<QString, quint32>{}) {
            return compiler.assembleOwner(result.layout.assemblyConstants + text, directory,
                                          owner["name"].toString() + suffix, at + 0x801135a0U,
                                          bindings);
        };
        auto compile = [&](const QString &filename, const QString &suffix, quint32 at) {
            return compiler.compileModule(
                read(":/game/promotion/" + filename), directory, owner["name"].toString() + suffix,
                at + 0x801135a0U, {{"class_menu_config.h", result.layout.cHeader}}, symbols);
        };
        QByteArray built;
        if (owner.contains("cSource")) {
            const auto extent = owner.value("cExtent").toInt(size);
            if (extent <= 0 || quint32(extent) > size || (extent & 3))
                throw Error("Invalid expanded C allocation");
            // No retained reference may straddle the replaced C region.
            for (const auto &ref : references)
                if ((ref.high < quint32(extent)) != (ref.low < quint32(extent)))
                    throw Error("C replacement splits a retained data reference");
            references.removeIf([&](const auto &ref) { return ref.high < quint32(extent); });
            const auto module = compile(owner["cSource"].toString(), "_expanded", start);
            if (familyTabs && start == 0xbc0bc) {
                // The list needs 284 bytes; its unused tail holds the shared
                // roster lookup so the adjacent count owner keeps its allocation.
                const auto helper = compiler.compileModule(
                    read(":/game/promotion/category-class-counts.c"), directory,
                    "family_roster_class", 0xbc1f8U + 0x801135a0U,
                    {{"class_menu_config.h",
                      result.layout.cHeader + "\n#define LC_FAMILY_ROSTER_ONLY 1\n"}},
                    {{"lc_named_class_map", classes.aliases},
                     {"lc_class_records", classes.records}});
                built =
                    module.fitOwner(extent - 96) + helper.fitOwner(96) + original.sliced(extent);
                addModuleReferences(helper, extent - 96);
            } else
                built = module.fitOwner(extent) + original.sliced(extent);
            addModuleReferences(module, 0);
        } else if (owner.contains("cFragment")) {
            const quint32 begin = owner["fragmentStart"].toInt(),
                          end = owner["fragmentEnd"].toInt();
            if (begin < start || end > start + size || end <= begin + 16)
                throw Error("Invalid expanded drawing fragment");
            const auto module = compile(owner["cFragment"].toString(), "_rows", begin + 16);
            auto fragment = assemble(read(":/game/promotion/" + owner["fragmentEntry"].toString()),
                                     "_entry", begin,
                                     {{"CLASS_RENDER_ROWS", begin + 16 + 0x801135a0U},
                                      {"CLASS_RENDER_CONTINUE", end + 0x801135a0U}});
            if (fragment.size() != 16)
                throw Error("Unexpected class renderer entry size");
            fragment += module.fitOwner(end - begin - 16);
            for (const auto &ref : references)
                if ((ref.high >= begin - start && ref.high < end - start) ||
                    (ref.low >= begin - start && ref.low < end - start))
                    throw Error("Drawing fragment replaces an undeclared data reference");
            built = original;
            built.replace(begin - start, fragment.size(), fragment);
            addModuleReferences(module, begin + 16 - start);
            const quint32 at = owner["styleStart"].toInt(), length = owner["styleSize"].toInt();
            const auto style =
                assemble(read(":/game/promotion/" + owner["styleSource"].toString()), "_style", at,
                         {{"CLASS_DRAW_STYLE", 0x8019b5c4U}, {"DRAW_CLASS_SPRITE", 0x801ba900U}});
            if (at < end || at + length > start + size || style.size() != length)
                throw Error("Expanded drawing style exceeds its allocation");
            built.replace(at - start, length, style);
        } else if (owner["acceptedC"].toBool()) {
            built = compiler.compileAcceptedOwner(start, directory, owner["name"].toString() + "_accepted");
            // The expanded family helper returns behavior separately in v1.
            // Retain that one comparison hook on the freshly compiled C body.
            for (auto it = familyChanges.begin(); it != familyChanges.end(); ++it) {
                if (it.key() < start || it.key() >= start + size) continue;
                const auto patch = assemble(it.value(), "_family_compare", it.key());
                if (patch.size() != 4) throw Error("Family comparison exceeds one instruction");
                built.replace(it.key() - start, 4, patch);
            }
        } else if (start == 0xbbd50) {
            built = assemble(read(":/game/promotion/family-lookup.s"), "_expanded", start);
        } else {
            QMap<quint32, QByteArray> changes;
            for (const auto &v : owner["expandedInstructions"].toArray()) {
                const auto field = v.toObject();
                const auto offset = field["offset"].toInt(-1);
                if (offset < 0 || quint32(offset) + 4 > size || (offset & 3) ||
                    changes.contains(offset))
                    throw Error("Invalid expanded menu instruction placement");
                changes[offset] = field["assembly"].toString().toLatin1();
            }
            QByteArray text;
            for (quint32 offset = 0; offset < size; offset += 4) {
                const auto at = start + offset;
                if (changes.contains(offset))
                    text += changes[offset];
                else if (familyChanges.contains(at))
                    text += familyChanges[at];
                else if (owner.contains("keySite") && at == quint32(owner["keySite"].toInt()))
                    text += "lui $a0, 0x0223";
                else if (owner.contains("keySite") && at == quint32(owner["keySite"].toInt()) + 4)
                    text += "ori $a0, $a0, 0xbd80";
                else
                    text += ".word 0x" + QByteArray::number(u32(original, offset), 16);
                text += '\n';
            }
            built = assemble(text, "_expanded", start);
        }
        quint32 fragmentEnd = 0;
        for (const auto &v : owner["assemblyFragments"].toArray()) {
            const auto fragment = v.toObject();
            const quint32 offset = fragment["offset"].toInt(), length = fragment["size"].toInt();
            if (offset < fragmentEnd || !length || quint64(offset) + length > size ||
                ((offset | length) & 3))
                throw Error("Invalid expanded assembly fragment");
            const auto code = assemble(read(":/game/promotion/" + fragment["source"].toString()),
                                       "_fragment_" + QString::number(offset), start + offset);
            if (code.size() != length)
                throw Error("Expanded assembly fragment exceeds its allocation");
            built.replace(offset, length, code);
            fragmentEnd = offset + length;
        }
        if (start == 0xcc828) {
            // All category-list reads in this controller retain byte IDs. Only
            // their owner-relative array moves to the expanded allocation.
            for (const quint32 site : {0xcc924U, 0xccce4U, 0xcd2bcU}) {
                const auto at = site - start, word = u32(built, at);
                if ((word >> 26) != 0x24)
                    throw Error("Unexpected category selection reader");
                put32(built, at, (word & 0xffff0000) | result.layout.categoryTabs);
            }
        }
        if (familyTabs && start == 0x9bc80) {
            constexpr quint32 begin = 0x9c02c, end = 0x9c358;
            auto bindings = result.layout.dataSymbols;
            bindings["lc_army_owner"] = 0x80196af8;
            bindings["lc_gfx"] = 0x800e9ba0;
            bindings["lc_load_tab"] = 0x8019b38c;
            const auto body =
                compiler.compileModule(read(":/game/promotion/category-tabs.c"), directory,
                                       "family_category_tabs", begin + 28 + 0x801135a0U,
                                       {{"class_menu_config.h", result.layout.cHeader}}, bindings);
            auto tabEntry = assemble(
                "move $a0,$s5\nmove $a1,$s8\nmove $a2,$s3\njal TAB_BODY\nnop\nj TAB_END\nnop\n",
                "_tab_entry", begin,
                {{"TAB_BODY", begin + 28 + 0x801135a0U}, {"TAB_END", end + 0x801135a0U}});
            if (tabEntry.size() != 28)
                throw Error("Unexpected family tab entry size");
            built.replace(begin - start, end - begin, tabEntry + body.fitOwner(end - begin - 28));
        }
        if (built.size() != size)
            throw Error("Expanded class code exceeds its owner");
        result.inventory.owners.append(
            ClassRuntimeInventory::capture(start, built, references, nextBases));
        result.owners.insert(start, built);
    }
    const auto tables = compiler.compileData(data.bytes, directory, "expanded_promotion_tables");
    result.container = ExpandedPromotionResource::encode(promotion, tables, result.inventory);
    return result;
}
} // namespace lordly
