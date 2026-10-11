#include "engine/art_resource_links.h"
#include "engine/custom_runtime.h"
#include "engine/class_data.h"
#include "engine/creature_rewards.h"
#include "engine/encounter_rate.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
namespace lordly {
QJsonObject customCallerCatalog() {
    QFile file(":/game/encounters/custom-owners/catalog.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing custom encounter caller catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QStringList customRuntimeEntries() {
    return {"lc_custom_reward",     "lc_selection_site", "lc_materializer_site",
            "lc_card_message_site", "lc_card_text_site", "lc_persuasion_site",
            "lc_talk_target_site",  "lc_retreat_site",   "lc_fight_budget_site",
            "lc_talk_budget_site",  "lc_round_site",     "lc_menu_iterator_site"};
}
static void validate(const CustomPlacement &p) {
    if ((!p.profileCount && p.slotCount == 10) || p.slotCount < 10 || p.slotCount > 32 || p.profileCount > 256U * p.slotCount ||
        p.dataSize !=
            p.indexSize() + p.profileCount * CustomPlacement::profileBytes + p.pairsSize() ||
        p.dataOffset < 0x2800000 || p.dataOffset % 16 ||
        quint64(p.dataOffset) + p.dataSize > 0x4000000 || p.codeBytes % 16 || !p.codeBytes ||
        p.residentBytes != p.codeBytes + CustomPlacement::stateBytes ||
        p.codeAddress < ClassPlacement::residentBegin || p.codeAddress % 16 ||
        quint64(p.codeAddress) + p.residentBytes > ClassPlacement::residentEnd ||
        p.stateAddress != p.codeAddress + p.codeBytes)
        throw Error("Unsupported custom encounter data or resident allocation");
    QSet<quint32> addresses;
    if (p.entries.size() != customRuntimeEntries().size())
        throw Error("Incomplete custom encounter entry directory");
    for (const auto &name : customRuntimeEntries()) {
        const auto at = p.entries.value(name);
        if (!at || at % 4 || at < p.codeAddress || at >= p.stateAddress || addresses.contains(at))
            throw Error("Custom encounter entry is outside its code or overlaps another entry");
        addresses.insert(at);
    }
}
QByteArray CustomPlacement::descriptor(const QByteArray &data, const QByteArray &resident) const {
    validate(*this);
    if (data.size() != dataSize || resident.size() != residentBytes ||
        resident.sliced(codeBytes) != QByteArray(stateBytes, 0))
        throw Error("Custom encounter payload disagrees with its allocation");
    QByteArray h(descriptorSize, 0);
    h.replace(0, 8, "LC05NEUT");
    put32(h, 8, slotCount > 10 ? 2 : 1);
    if (slotCount > 10) put32(h, 44, slotCount);
    put32(h, 12, dataOffset);
    put32(h, 16, dataSize);
    put32(h, 20, profileCount);
    put32(h, 24, stateAddress);
    put32(h, 28, codeAddress);
    put32(h, 32, codeBytes);
    put32(h, 36, residentBytes);
    put32(h, 40, crc32(data));
    put32(h, 48, crc32(resident));
    const auto names = customRuntimeEntries();
    for (int i = 0; i < names.size(); ++i)
        put32(h, 64 + i * 4, entries[names[i]]);
    put32(h, 124, crc32(h.first(124)));
    return h;
}
std::optional<CustomPlacement> CustomPlacement::read(const Rom &rom) {
    const auto h = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!h.startsWith("LC05NEUT"))
        return std::nullopt;
    rom.requireSupported();
    if (rom.revision() != 0 || h.size() != descriptorSize || (u32(h, 8) != 1 && u32(h, 8) != 2) ||
        (u32(h, 8) == 1 ? u32(h, 44) != 0 : u32(h, 44) < 11 || u32(h, 44) > 32) ||
        h.mid(52, 12) != QByteArray(12, 0) || h.mid(112, 12) != QByteArray(12, 0) ||
        u32(h, 124) != crc32(h.first(124)))
        throw Error("Damaged custom encounter descriptor");
    CustomPlacement p;
    p.slotCount = u32(h, 8) == 2 ? int(u32(h, 44)) : 10;
    p.dataOffset = u32(h, 12);
    p.dataSize = u32(h, 16);
    p.profileCount = u32(h, 20);
    p.stateAddress = u32(h, 24);
    p.codeAddress = u32(h, 28);
    p.codeBytes = u32(h, 32);
    p.residentBytes = u32(h, 36);
    const auto names = customRuntimeEntries();
    for (int i = 0; i < names.size(); ++i)
        p.entries[names[i]] = u32(h, 64 + i * 4);
    validate(p);
    const auto classes = ClassPlacement::read(rom);
    if (!classes || classes->residentAddress != p.codeAddress ||
        classes->residentBytes != p.residentBytes || classes->residentChecksum != u32(h, 48))
        throw Error("Custom encounter code is not reserved by the class allocator");
    const auto data = rom.bytes().mid(p.dataOffset, p.dataSize);
    if (data.size() != p.dataSize || crc32(data) != u32(h, 40) ||
        rom.bytes().mid(p.stateAddress - ClassPlacement::residentDelta, stateBytes) !=
            QByteArray(stateBytes, 0))
        throw Error("Custom encounter data differs from its declared allocation");
    int profiles = 0;
    for (int key = 0; key < 256; ++key)
        for (int slot = 0; slot < p.slotCount; ++slot) {
            const auto pointer = u32(data, (key * p.slotCount + slot) * 4);
            if (!pointer)
                continue;
            const auto expected = p.dataOffset + p.indexSize() + profiles * profileBytes;
            if (pointer != expected || ++profiles > int(p.profileCount) ||
                u8(data, pointer - p.dataOffset + 5) != key ||
                u8(data, pointer - p.dataOffset + 6) != slot)
                throw Error("Custom encounter index points outside its profile or to another key");
        }
    if (profiles != int(p.profileCount))
        throw Error("Custom encounter profile count disagrees with its index");
    return p;
}
static quint32 callInstruction(quint32 address) {
    return 0x0c000000U | ((address >> 2) & 0x3ffffff);
}
QByteArray CustomPlacement::canonicalReferences(const QByteArray &owner, qsizetype start) const {
    auto result = owner;
    for (const auto &value : customCallerCatalog()["owners"].toArray()) {
        const auto spec = value.toObject();
        for (const auto &item : spec["sites"].toArray()) {
            const auto site = item.toObject();
            const auto at = spec["start"].toInt() + site["offset"].toInt();
            if (at < start || at + 8 > start + owner.size())
                continue;
            const auto original = site["original"].toArray();
            const quint32 delay = site["keepDelay"].toBool() ? quint32(original[1].toDouble()) : 0;
            if (u32(result, at - start) !=
                    callInstruction(entries.value(site["entry"].toString())) ||
                u32(result, at - start + 4) != delay)
                throw Error("Custom encounter caller changed outside its rebuild");
            put32(result, at - start, quint32(original[0].toDouble()));
            put32(result, at - start + 4, quint32(original[1].toDouble()));
        }
    }
    const auto selection = EncounterRate::catalog()["selection"].toObject();
    const auto selectionAt = EncounterRate::romOffset + selection["offset"].toInt();
    if (selectionAt >= start && selectionAt + 8 <= start + owner.size()) {
        const auto at = selectionAt - start;
        if (u32(result, at) != callInstruction(entries.value("lc_selection_site")) ||
            u32(result, at + 4))
            throw Error("Custom squad selection changed outside its rebuild");
        const auto original = selection["original"].toArray();
        put32(result, at, quint32(original[0].toDouble()));
        put32(result, at + 4, quint32(original[1].toDouble()));
    }
    const auto prefix = CreatureRewards::catalog()["customPrefix"].toObject();
    const auto rewardAt = CreatureRewards::ownerOffset + prefix["offset"].toInt();
    if (rewardAt >= start && rewardAt + 120 <= start + owner.size()) {
        const auto at = rewardAt - start;
        const auto words = prefix["words"].toArray();
        if (prefix["size"] != 120 || words.size() != 4 ||
            u32(result, at) != callInstruction(entries.value("lc_custom_reward")))
            throw Error("Custom squad reward entry changed outside its rebuild");
        for (int i = 1; i < 4; ++i)
            if (u32(result, at + i * 4) != quint32(words[i].toDouble()))
                throw Error("Custom squad reward continuation changed outside its rebuild");
        result.replace(at, 120, result.mid(at + 16, 104) + QByteArray(16, 0));
    }
    return result;
}
void verifyCustomCallers(const Rom &rom, const std::optional<CustomPlacement> &placement) {
    const auto classes = ClassPlacement::read(rom);
    const auto catalog = customCallerCatalog();
    for (const auto &v : catalog["dependencies"].toArray()) {
        const auto d = v.toObject();
        if (sha256(rom.bytes().mid(d["start"].toInt(), d["size"].toInt())) !=
            d["sha256"].toString().toLatin1())
            throw Error("Custom encounter overlay dependency changed");
    }
    for (const auto &v : catalog["owners"].toArray()) {
        const auto owner = v.toObject();
        const auto at = owner["start"].toInt(), size = owner["size"].toInt();
        auto code = rom.bytes().mid(at, size);
        if (classes)
            code = classes->canonicalReferences(code, at);
        if (placement)
            code = placement->canonicalReferences(code, at);
        validateArtOwnerLinks(rom, at, size);
        if (code.size() != size || sha256(canonicalArtOwner(code, at)) != owner["retailSha256"].toString().toLatin1())
            throw Error("Unrecognized custom encounter caller at z64 " + QString::number(at, 16));
    }
}
} // namespace lordly
