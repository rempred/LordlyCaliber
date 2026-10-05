#include "engine/project.h"
#include "engine/scene_import.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QtZlib/zlib.h>
#include <cmath>
#include <limits>
#include <tuple>

namespace lordly {
QString tableKey(Table t) {
    switch (t) {
    case Table::Classes:
        return "classes";
    case Table::Items:
        return "items";
    case Table::Consumables:
        return "consumables";
    }
    throw Error("Unknown record table");
}
const QVector<Field> &fields(Table table) {
    static const auto classes = [] {
        QVector<Field> f;
        auto add = [&](QString key, QString label, QString group, int offset, int width = 1,
                       QString choices = {}) {
            f.push_back({key, label, group, offset, width, false, choices});
        };
        add("unitSize", "Size", "Identity", ClassData::unitSizeOffset, 1, "UNIT_SIZES");
        add("sexOrVoice", "Body / voice", "Identity", 5, 1, "CLASS_SEX_VOICE");
        add("leadership", "Leadership", "Identity", 6, 1, "CLASS_LEADERSHIP");
        add("baseHp", "Base HP", "Stats", 8, 2);
        add("hpGrowth", "HP growth", "Growth", 10);
        const QStringList stats = {"STR", "VIT", "INT", "MEN", "AGI", "DEX"};
        for (int i = 0; i < stats.size(); ++i) {
            add(stats[i].toLower() + "Base", stats[i] + " base", "Stats", 12 + i * 4, 2);
            add(stats[i].toLower() + "Growth", stats[i] + " growth", "Growth", 14 + i * 4);
        }
        add("luck", "Base LCK", "Stats", 35);
        add("alignment", "Alignment", "Stats", 36);
        const QStringList res = {"Physical", "Wind", "Fire", "Earth", "Water", "Virtue", "Bane"};
        for (int i = 0; i < res.size(); ++i)
            add("res" + res[i], res[i], "Resistance", 37 + i);
        add("moveType", "Movement", "Identity", 44, 1, "MOVEMENT_TYPES");
        const QStringList equip = {"Weapon", "Body", "Off-hand", "Head"};
        for (int i = 0; i < equip.size(); ++i)
            add("equipment" + QString::number(i), equip[i], "Equipment", 46 + i * 2, 2,
                "ITEM_NAMES");
        add("fixedEquipment", "Fixed equipment mask", "Equipment", 54);
        const QStringList rows = {"Front", "Middle", "Rear"};
        for (int i = 0; i < rows.size(); ++i) {
            add("attack" + QString::number(i), rows[i] + " action", "Attacks", 55 + i * 2, 1,
                "ACTION_NAMES");
            add("attackCount" + QString::number(i), rows[i] + " count", "Attacks", 56 + i * 2);
        }
        add("physAtk", "Physical attack", "Combat", 61);
        add("magAtk", "Magic attack", "Combat", 62);
        add("physDef", "Physical defense", "Combat", 63);
        add("magDef", "Magic defense", "Combat", 64);
        add("baseClass", "Base growth class", "Growth", 65, 1, "CLASS_NAMES");
        add("baseTransitionLevel", "First transition level", "Growth", 66);
        add("intermediateClass", "Intermediate growth class", "Growth", 67, 1, "CLASS_NAMES");
        add("finalTransitionLevel", "Second transition level", "Growth", 68);
        add("classCopyMatch", "Class-copy match", "Growth", 69, 1, "CLASS_NAMES");
        add("element", "Default element", "Combat", 70, 1, "DEFAULT_ELEMENTS");
        add("itemCapacity", "Carried-item contribution", "Equipment",
            ClassData::itemCapacityOffset);
        for (int byte = 0; byte < 72; ++byte) {
            bool covered = false;
            for (const auto &field : f)
                if (byte >= field.offset && byte < field.offset + field.width)
                    covered = true;
            if (!covered)
                add("raw" + QString::number(byte), QString("Raw byte %1").arg(byte), "Advanced",
                    byte);
        }
        return f;
    }();
    static const auto items = [] {
        QVector<Field> f = {{"equipType", "Equipment type", "Identity", 4, 1, false, "EQUIP_TYPES"},
                            {"element", "Element", "Identity", 5, 1, false, "ELEMENT_NAMES"},
                            {"grade", "Grade", "Identity", 6},
                            {"price", "Price", "Identity", 8, 2}};
        const QStringList stats = {"STR", "INT", "AGI", "DEX", "VIT", "MEN"};
        for (int i = 0; i < stats.size(); ++i)
            f.push_back({stats[i].toLower(), stats[i], "Stats", 10 + i, 1, true});
        const QStringList res = {"Physical", "Wind", "Fire", "Earth", "Water", "Virtue", "Bane"};
        const QStringList keys = {"Phys", "Wind", "Fire", "Earth", "Water", "Virtue", "Bane"};
        for (int i = 0; i < res.size(); ++i)
            f.push_back({"res" + keys[i], res[i], "Resistance", 17 + i, 1, true});
        const QStringList growth = {"HpStr", "Unknown", "Int", "Agi", "Dex", "Vit", "Men", "Lck"};
        const QStringList labels = {"HP and STR", "Unused lane", "INT", "AGI",
                                    "DEX",        "VIT",         "MEN", "LCK"};
        for (int i = 0; i < 8; ++i)
            f.push_back({"growth" + growth[i],
                         labels[i],
                         "Growth",
                         24 + i / 4,
                         1,
                         false,
                         {},
                         2,
                         6 - 2 * (i % 4)});
        for (int logical = 0; logical < 32; ++logical) {
            int physical = logical < 28 ? logical + 4 : logical - 28;
            bool covered = false;
            for (const auto &field : f)
                if (physical >= field.offset && physical < field.offset + field.width)
                    covered = true;
            if (!covered)
                f.push_back({QString("b%1Raw").arg(logical), QString("Raw byte B%1").arg(logical),
                             "Advanced", physical});
        }
        return f;
    }();
    static const QVector<Field> consumables = {{"price", "Price (Goth)", "Values", 6, 2},
                                               {"flagHi", "Category metadata", "Advanced", 4, 2},
                                               {"tier", "Tier", "Values", 8},
                                               {"metadata", "Raw metadata byte", "Advanced", 9},
                                               {"raw10", "Raw byte 10", "Advanced", 10},
                                               {"raw11", "Raw byte 11", "Advanced", 11}};
    return table == Table::Classes ? classes : table == Table::Items ? items : consumables;
}
Project Project::fromRom(Rom rom, const QJsonObject &catalog) {
    rom.requireSupported();
    Project p;
    p.rom_ = std::move(rom);
    p.catalog_ = catalog;
    const auto classPlacement = ClassPlacement::read(p.rom_);
    if (classPlacement)
        p.originalClasses_ = ClassData::load(p.rom_);
    else if (p.rom_.revision() == 0) {
        // Numeric records remain inspectable in ROM variants whose relocated
        // strings need a separate owner. Such names are not offered for editing.
        try {
            p.originalClasses_ = ClassData::loadRetail(p.rom_);
        } catch (const Error &) {
        }
    }
    for (auto table : {Table::Classes, Table::Items, Table::Consumables}) {
        auto &records = p.tables_[table];
        bool cls = table == Table::Classes;
        int count = cls ? 165 : 278, stride = cls ? 72 : 32;
        qsizetype base = cls ? 0x5dad8 + 72 - 12 : 0x62310 - 4;
        if (table == Table::Consumables) {
            count = 45;
            stride = 12;
            base = 0x645cc;
        }
        if (p.rom_.revision() == 1)
            base += 0x20;
        if (cls && classPlacement) {
            count = classPlacement->layout.count;
            base = classPlacement->dataRomOffset + classPlacement->layout.records -
                   classPlacement->layout.indexBase;
        }
        auto names = catalog
                         .value(cls                     ? "CLASS_NAMES"
                                : table == Table::Items ? "ITEM_NAMES"
                                                        : "CONSUMABLE_NAMES")
                         .toObject();
        if (cls)
            for (auto it = names.begin(); it != names.end();) {
                bool valid = false;
                const auto id = it.key().toInt(&valid);
                if (valid && id >= count)
                    it = names.erase(it);
                else
                    ++it;
            }
        for (int id = 0; id < count; ++id) {
            Record r;
            r.id = id;
            r.romOffset = base + id * stride;
            r.name = names.value(QString::number(id))
                         .toString(QString("%1 %2").arg(cls ? "Class" : "Item").arg(id));
            if (cls && p.originalClasses_)
                r.name = decodeText(p.originalClasses_->entries()[id].name);
            if (cls)
                names[QString::number(id)] = r.name;
            r.original = p.rom_.bytes().mid(r.romOffset, stride);
            r.bytes = r.original;
            if (r.bytes.size() != stride)
                throw Error("Truncated record table");
            if (table == Table::Consumables) {
                const auto pointer = u32(r.bytes, 0);
                const auto stringOffset = qint64(pointer) - 0x8012a100;
                const auto shift = p.rom_.revision() == 1 ? 0x20 : 0;
                if (stringOffset >= 0x66280 + shift && stringOffset < 0x665c4 + shift) {
                    const auto end = p.rom_.bytes().indexOf(char(0), stringOffset);
                    if (end >= stringOffset && end < 0x665c4 + shift)
                        r.name = decodeText(p.rom_.bytes().mid(stringOffset, end - stringOffset));
                }
            }
            records.push_back(std::move(r));
        }
        if (cls)
            p.catalog_["CLASS_NAMES"] = names;
    }
    return p;
}
const Record &Project::record(Table t, int row) const {
    const auto &records = tables_.at(t);
    if (row < 0 || row >= records.size())
        throw Error("Record index outside table");
    return records[row];
}
int Project::value(Table t, int row, int column) const {
    if (column < 0 || column >= fields(t).size())
        throw Error("Field index outside table");
    const auto &f = fields(t)[column];
    const auto &bytes = record(t, row).bytes;
    auto value = f.width == 2 ? int(u16(bytes, f.offset)) : int(u8(bytes, f.offset));
    if (f.bits < 8)
        value = (value >> f.shift) & f.maximum();
    return f.signedValue && value >= 128 ? value - 256 : value;
}
void Project::setValue(Table t, int row, int column, int value) {
    if (column < 0 || column >= fields(t).size())
        throw Error("Field index outside table");
    const auto &f = fields(t)[column];
    if (value < f.minimum() || value > f.maximum())
        throw Error("Field value outside its stored range");
    auto bytes = record(t, row).bytes;
    if (f.width == 2)
        put16(bytes, f.offset, quint16(value));
    else if (f.bits < 8)
        put8(bytes, f.offset,
             quint8((u8(bytes, f.offset) & ~(f.maximum() << f.shift)) | (value << f.shift)));
    else
        put8(bytes, f.offset, quint8(value));
    replaceBytes(t, row, bytes);
}
void Project::replaceBytes(Table t, int row, const QByteArray &bytes) {
    if (bytes.size() != record(t, row).bytes.size())
        throw Error("Record size changed unexpectedly");
    if (t == Table::Classes && originalClasses_ &&
        bytes.first(4) != record(t, row).original.first(4))
        throw Error("Class name pointers are allocated during rebuild; edit the name instead");
    if (t == Table::Consumables && bytes.first(4) != record(t, row).original.first(4))
        throw Error("Consumable name pointers must retain their loaded allocation");
    tables_.at(t)[row].bytes = bytes;
}
int Project::changedRecords() const {
    int count = int(classNames_.size() + addedClasses_.size());
    for (const auto &[table, records] : tables_)
        for (const auto &r : records)
            if (r.original != r.bytes)
                ++count;
    for (const auto &bank : texts_)
        for (const auto &entry : bank.entries)
            if (entry.bytes != entry.original)
                ++count;
    if (promotion_ && promotion_->modified())
        ++count;
    if (shops_)
        count += shops_->changedShops();
    if (consumableEffects_ && consumableEffects_->modified())
        ++count;
    if (tools_ && tools_->modified())
        ++count;
    if (icons_ && icons_->modified())
        ++count;
    if (avatars_ && avatars_->modified())
        ++count;
    if (army_ && army_->modified())
        ++count;
    if (encounters_)
        count += encounters_->changedRows();
    if (scenarios_)
        count += scenarios_->changedScenarios();
    if (world_)
        count += world_->changedEntries();
    if (dialogues_)
        count += dialogues_->changedEntries();
    if (scenePrograms_)
        count += scenePrograms_->changedPrograms();
    if (sceneEvents_)
        count += sceneEvents_->changedSites();
    return count;
}
QByteArray encodeClassName(const QString &name) {
    if (name.trimmed().isEmpty())
        throw Error("Enter a class name");
    const auto lines = name.split('\n');
    if (lines.size() > 2)
        throw Error("Class names support at most two lines");
    QVector<QByteArray> encoded;
    for (const auto &line : lines) {
        if (line.trimmed().isEmpty())
            throw Error("Class name lines cannot be empty");
        QByteArray bytes;
        for (const auto character : line) {
            if (character.unicode() < 32 || character.unicode() > 126)
                throw Error("Use printable game characters in class names");
            bytes.append(char(character.unicode()));
        }
        encoded.push_back(bytes);
    }
    // All seven two-line retail names use this native name framing.
    return encoded.size() == 1 ? encoded[0]
                               : QByteArray(1, 0x0e) + encoded[0] + QByteArray::fromHex("1063") +
                                     encoded[1] + QByteArray(1, 0x0f);
}
const ClassAsset &Project::classAsset(int row) const {
    record(Table::Classes, row);
    if (!originalClasses_)
        throw Error("This ROM's class names need a supported source layout before editing");
    const auto originalCount = originalClasses_->entries().size();
    return row < originalCount ? originalClasses_->entries()[row]
                               : addedClasses_[row - originalCount];
}
QByteArray Project::classNameBytes(int row) const {
    return classNames_.value(row, classAsset(row).name);
}
void Project::setClassNameBytes(int row, const QByteArray &name) {
    classNameBytes(row);
    auto candidate = classData();
    auto entry = candidate.entries()[row];
    entry.name = name;
    candidate.set(row, entry);
    if (name == classAsset(row).name)
        classNames_.remove(row);
    else
        classNames_[row] = name;
    tables_.at(Table::Classes)[row].name = decodeText(name);
    auto names = catalog_["CLASS_NAMES"].toObject();
    names[QString::number(row)] = tables_.at(Table::Classes)[row].name;
    catalog_["CLASS_NAMES"] = names;
}
ClassData Project::classData() const {
    if (!originalClasses_)
        throw Error("Class data requires a supported source layout");
    auto result = *originalClasses_;
    for (const auto &asset : addedClasses_) {
        const auto id = result.append(0, asset.name);
        result.set(id, asset);
    }
    const auto &records = tables_.at(Table::Classes);
    if (records.size() != result.entries().size())
        throw Error("Class record and asset counts disagree");
    for (int id = 0; id < records.size(); ++id) {
        auto entry = result.entries()[id];
        entry.record = records[id].bytes;
        entry.name = classNameBytes(id);
        result.set(id, entry);
    }
    if (army_)
        army_->applyRoutes(result);
    return result;
}
int Project::classDescriptionIndex(int classId) {
    if (classId < 0 || classId > 255)
        throw Error("Class ID is outside its byte-sized representation");
    return classId < 165 ? classId : classId + 60;
}
int Project::appendClassAsset(const ClassAsset &asset, const QByteArray &description,
                              int promotionTemplate) {
    auto classes = classData();
    const auto id = classes.append(0, asset.name);
    classes.set(id, asset);
    if (asset.namedAlias || description.contains("\r\n"))
        throw Error(
            "New classes cannot inherit a named-character alias or a description delimiter");
    loadTexts();
    loadPromotion();
    auto &bank = texts_["classes"];
    if (bank.entries.size() != classDescriptionIndex(id) ||
        (promotion_->classCount() != 81 && promotion_->classCount() != id))
        throw Error("Class, description and promotion counts disagree");
    if (promotion_->appendClass(promotionTemplate) != id)
        throw Error("Promotion append produced a different class ID");
    addedClasses_.push_back(asset);
    bank.entries.push_back({description, description});
    auto &rows = tables_.at(Table::Classes);
    rows.push_back({id, decodeText(asset.name), asset.record, asset.record, -1});
    auto names = catalog_["CLASS_NAMES"].toObject();
    names[QString::number(id)] = rows.last().name;
    catalog_["CLASS_NAMES"] = names;
    return id;
}
int Project::appendClass(int templateId, const QString &name) {
    if (templateId < 1 || templateId >= records(Table::Classes).size())
        throw Error("Choose an existing nonempty class as the template");
    auto candidate = *this;
    candidate.loadTexts();
    auto classes = candidate.classData();
    const auto id = classes.append(templateId, encodeClassName(name));
    const auto sourceText = classDescriptionIndex(templateId);
    const auto &bank = candidate.texts_["classes"];
    if (sourceText >= bank.entries.size())
        throw Error("The template class description is missing");
    candidate.appendClassAsset(classes.entries()[id], bank.entries[sourceText].bytes, templateId);
    if (candidate.army_ && candidate.army_->routes().contains(templateId)) {
        const auto route = candidate.army_->routes()[templateId];
        candidate.army_->setRoute(candidate.classData(), id, route.lane, route.model);
    }
    if (candidate.avatars_) {
        const auto current = candidate.classData();
        for (int selector = 0; selector < 2; ++selector) {
            const auto words = candidate.avatars_->words(current, templateId, selector);
            if (!words.isEmpty())
                candidate.avatars_->setWords(current, id, selector, words);
        }
    }
    *this = std::move(candidate);
    return id;
}
void Project::loadTexts() {
    if (texts_.isEmpty())
        texts_ = TextBank::load(rom_);
}
void Project::loadPromotion() {
    if (!promotion_)
        promotion_ = Promotion::load(rom_);
}
void Project::loadShops() {
    if (!shops_)
        shops_ = Shops::load(rom_);
}
void Project::setShops(const QJsonObject &value) {
    loadShops();
    shops_->apply(value);
}
void Project::loadConsumableEffects() {
    if (!consumableEffects_)
        consumableEffects_ = ConsumableEffects::load(rom_);
}
void Project::loadTools() {
    if (!tools_)
        tools_ = ToolSettings::load(rom_);
}
void Project::setTools(const QJsonObject &value) {
    loadTools();
    tools_->apply(value);
}
void Project::importLegacyTools(const QJsonObject &object) {
    const auto patches = object["patches"].toObject();
    if (patches.contains("tools") && !patches["tools"].isObject())
        throw Error("Imported tool settings must be an object");
    const auto tools = patches["tools"].toObject();
    const auto owner = object["rebuild_owners"].toObject();
    QJsonValue percent(QJsonValue::Undefined);
    if (tools.contains("experience-size-scale")) {
        const auto value = tools["experience-size-scale"].toObject();
        if (value["schema"] != 1 || !value.contains("percent"))
            throw Error("Invalid imported experience setting");
        percent = value["percent"];
    }
    if (owner.contains("experience_scale_percent")) {
        if (!percent.isUndefined() && percent != owner["experience_scale_percent"])
            throw Error("Imported experience settings disagree");
        percent = owner["experience_scale_percent"];
    }
    QJsonValue chaos(QJsonValue::Undefined);
    if (tools.contains("cf-army-counter"))
        chaos = tools["cf-army-counter"];
    if (owner.contains("chaos_frame_counter")) {
        if (!chaos.isUndefined() && chaos != owner["chaos_frame_counter"])
            throw Error("Imported Chaos Frame settings disagree");
        chaos = owner["chaos_frame_counter"];
    }
    QJsonObject displays;
    for (const auto &[id, ownerKey, field] :
         {std::tuple{"character-card-luck", "character_card_luck", "characterCardLuck"},
          std::tuple{"squad-menu-alignment", "squad_menu_alignment", "squadMenuAlignment"},
          std::tuple{"high-attack-streamsplit", "high_attack_streamsplit", "highAttackStreamSplit"}}) {
        QJsonValue value(QJsonValue::Undefined);
        if (tools.contains(id))
            value = tools[id];
        if (owner.contains(ownerKey)) {
            if (!value.isUndefined() && value != owner[ownerKey])
                throw Error("Imported Tools settings disagree: " + QString(id));
            value = owner[ownerKey];
        }
        if (!value.isUndefined())
            displays[field] = value;
    }
    if (!percent.isUndefined() || !chaos.isUndefined() || !displays.isEmpty()) {
        loadTools();
        auto settings = tools_->json();
        if (!percent.isUndefined())
            settings["experienceScalePercent"] = percent;
        if (!chaos.isUndefined())
            settings["chaosFrameCounter"] = chaos;
        for (auto it = displays.begin(); it != displays.end(); ++it)
            settings[it.key()] = it.value();
        setTools(settings);
    }
}
void Project::loadEncounters() {
    if (!encounters_)
        encounters_ = Encounters::load(rom_, int(records(Table::Classes).size()));
}
void Project::setEncounters(const QJsonObject &value) {
    loadEncounters();
    encounters_->apply(value, int(records(Table::Classes).size()));
}
void Project::loadScenarios() {
    if (!scenarios_)
        scenarios_ = Scenarios::load(rom_);
}
void Project::loadWorld() {
    if (!world_)
        world_ = ScenarioWorld::load(rom_);
}
void Project::setWorld(const QJsonObject &value) {
    loadWorld();
    world_->apply(value);
}
void Project::setScenarios(const QJsonObject &value) {
    loadScenarios();
    scenarios_->apply(value, classData());
}
void Project::setConsumableEffects(const QJsonObject &value) {
    loadConsumableEffects();
    consumableEffects_->apply(value);
}
void Project::editConsumableEffect(const QString &key, const QJsonObject &value,
                                   bool updateHealingDescription) {
    auto next = *this;
    next.loadConsumableEffects();
    next.consumableEffects_->setModel(key, value);
    const int id = key == "healLeaf" ? 1 : key == "healSeed" ? 2 : key == "healPack" ? 3 : 0;
    if (id && updateHealingDescription) {
        next.loadTexts();
        auto &bank = next.texts_["consumables"];
        auto &bytes = bank.entries[id].bytes;
        const auto text = QString::fromLatin1(bytes);
        const QRegularExpression amount("(?<![0-9])[0-9]{1,3}(?=HP\\b)");
        auto matches = amount.globalMatch(text);
        if (!matches.hasNext())
            throw Error("Description has no HP amount to update. Edit the description or disable "
                        "automatic description updates.");
        const auto match = matches.next();
        if (matches.hasNext())
            throw Error("Description has multiple HP amounts. Edit it directly or disable "
                        "automatic description updates.");
        bytes.replace(match.capturedStart(), match.capturedLength(),
                      QByteArray::number(value["magnitude"].toInt()).rightJustified(3, '0'));
        bank.validateRuntimeSize();
    }
    *this = std::move(next);
}
void Project::setPromotion(const QJsonObject &value) {
    loadPromotion();
    promotion_->apply(value);
}
void Project::setText(const QString &kind, int id, const QString &text) {
    loadTexts();
    if (!texts_.contains(kind))
        throw Error("Unknown description bank");
    texts_[kind].setText(id, text);
}
void Project::replaceTextBytes(const QString &kind, int id, const QByteArray &bytes) {
    loadTexts();
    if (!texts_.contains(kind) || id < 0 || id >= texts_[kind].entries.size() ||
        bytes.contains("\r\n"))
        throw Error("Invalid description record");
    texts_[kind].entries[id].bytes = bytes;
}
void Project::loadCombatArtwork() {
    if (!combat_) combat_ = CombatArtwork::load(rom_);
}
void Project::setCombatArtwork(const QJsonObject &value) {
    loadCombatArtwork(); combat_->apply(value);
}
void Project::importLegacyCombatArtwork(const QJsonObject &object) {
    const auto art = object["patches"].toObject().value("art");
    if (art.isNull() || art.isUndefined()) return;
    if (!art.isObject()) throw Error("Invalid imported art");
    if (art.toObject().contains("animations")) {loadCombatArtwork(); combat_->importLegacy(art.toObject());}
}
void Project::loadArmyArtwork() {
    if(!army_)army_=ArmyArtwork::load(rom_);
}
void Project::setArmyArtwork(const QJsonObject &value) {
    loadArmyArtwork();
    auto next = *army_;
    next.apply(value);
    auto classes = classData();
    next.applyRoutes(classes);
    army_ = std::move(next);
}
void Project::importLegacyArmyArtwork(const QJsonObject &object) {
    const auto art = object["patches"].toObject().value("art");
    if (art.isNull() || art.isUndefined())
        return;
    if (!art.isObject())
        throw Error("Invalid imported art");
    if(art.toObject().contains("armySprites")) {loadArmyArtwork();army_->importLegacy(art.toObject());}
}
void Project::loadAvatarArtwork() {
    if(!avatars_)avatars_=AvatarArtwork::load(rom_);
}
void Project::setAvatarArtwork(const QJsonObject &value) {
    loadAvatarArtwork();avatars_->apply(classData(),value);
}
void Project::importLegacyAvatarArtwork(const QJsonObject &object) {
    const auto art=object["patches"].toObject().value("art");
    if(art.isNull() || art.isUndefined())return;
    if(!art.isObject())throw Error("Invalid imported art");
    if(art.toObject().contains("avatars")) {
        loadAvatarArtwork();avatars_->importLegacy(classData(),art.toObject());
    }
}
void Project::loadIconArtwork() {
    if(!icons_)icons_=IconArtwork::load(rom_);
}
void Project::setIconArtwork(const QJsonObject &value) {
    loadIconArtwork();icons_->apply(value);
}
void Project::importLegacyIconArtwork(const QJsonObject &object) {
    const auto art=object["patches"].toObject().value("art");
    if(art.isNull() || art.isUndefined())return;
    if(!art.isObject())throw Error("Invalid imported art");
    if(art.toObject().contains("icons")) {
        loadIconArtwork();icons_->importLegacy(art.toObject());
    }
}
QJsonObject Project::manifest() const {
    auto result = retained_;
    result["format"] = "LordlyCaliberProject";
    result["version"] = 1;
    result["sourceSha256"] = QString::fromLatin1(sha256(rom_.bytes()));
    result["sourcePath"] = rom_.sourcePath();
    QJsonObject edits;
    for (const auto &[table, records] : tables_) {
        QJsonObject rows;
        for (const auto &r : records)
            if (r.original != r.bytes)
                rows[QString::number(r.id)] = QString::fromLatin1(r.bytes.toHex());
        edits[tableKey(table)] = rows;
    }
    result["records"] = edits;
    result["spriteLibrary"] = sprites_.json();
    if(icons_ && (icons_->modified() || legacy_["patches"].toObject()["art"].toObject().contains("icons")))
        result["iconArtwork"]=icons_->json();
    else result.remove("iconArtwork");
    if(avatars_ && (avatars_->modified() || legacy_["patches"].toObject()["art"].toObject().contains("avatars")))
        result["avatarArtwork"]=avatars_->json();
    else result.remove("avatarArtwork");
    if(army_ && (army_->modified() || legacy_["patches"].toObject()["art"].toObject().contains("armySprites")))
        result["armyArtwork"]=army_->json();
    else result.remove("armyArtwork");
    if (combat_ && (combat_->modified() || legacy_["patches"].toObject()["art"].toObject().contains("animations")))
        result["combatArtwork"] = combat_->json();
    else result.remove("combatArtwork");
    QJsonArray additions;
    for (int i = 0; i < addedClasses_.size(); ++i) {
        const auto &asset = addedClasses_[i];
        const auto id = int(originalClasses_->entries().size()) + i;
        additions << QJsonObject{
            {"record", QString::fromLatin1(asset.record.toHex())},
            {"appearance", QString::fromLatin1(asset.appearance.toHex())},
            {"name", QString::fromLatin1(asset.name.toHex())},
            {"description",
             QString::fromLatin1(
                 texts_["classes"].entries[classDescriptionIndex(id)].original.toHex())}};
    }
    if (additions.isEmpty())
        result.remove("addedClasses");
    else
        result["addedClasses"] = additions;
    QJsonObject names;
    for (auto it = classNames_.begin(); it != classNames_.end(); ++it)
        names[QString::number(it.key())] = QString::fromLatin1(it.value().toHex());
    if (names.isEmpty())
        result.remove("classNames");
    else
        result["classNames"] = names;
    QJsonObject descriptions;
    for (auto bank = texts_.begin(); bank != texts_.end(); ++bank) {
        QJsonObject rows;
        for (int i = 0; i < bank->entries.size(); ++i)
            if (bank->entries[i].bytes != bank->entries[i].original)
                rows[QString::number(i)] = QString::fromLatin1(bank->entries[i].bytes.toHex());
        if (!rows.isEmpty())
            descriptions[bank.key()] = rows;
    }
    result["descriptions"] = descriptions;
    if (promotion_ && promotion_->modified())
        result["promotion"] = promotion_->json();
    else
        result.remove("promotion");
    if (!legacy_.isEmpty()) {
        auto legacy = legacy_;
        auto patches = legacy["patches"].toObject();
        // The canonical library carries these pixels, including deletion intent.
        // Retaining a second raw-RGBA copy can exceed the project size budget.
        patches.remove("spriteLibrary");
        legacy["patches"] = patches;
        result["legacyImport"] = legacy;
    }
    if (shops_ &&
        (shops_->modified() || !legacy_["patches"].toObject()["shops"].toObject().isEmpty() ||
         legacy_["rebuild_owners"].toObject()["shop_memberships"].toBool()))
        result["shops"] = shops_->json();
    else
        result.remove("shops");
    if (tools_ &&
        (tools_->modified() ||
         legacy_["patches"].toObject()["tools"].toObject().contains("experience-size-scale") ||
         legacy_["rebuild_owners"].toObject().contains("experience_scale_percent") ||
         legacy_["patches"].toObject()["tools"].toObject().contains("cf-army-counter") ||
         legacy_["rebuild_owners"].toObject().contains("chaos_frame_counter") ||
         legacy_["patches"].toObject()["tools"].toObject().contains("character-card-luck") ||
         legacy_["patches"].toObject()["tools"].toObject().contains("squad-menu-alignment") ||
         legacy_["rebuild_owners"].toObject().contains("character_card_luck") ||
         legacy_["rebuild_owners"].toObject().contains("squad_menu_alignment") ||
         legacy_["patches"].toObject()["tools"].toObject().contains("high-attack-streamsplit") ||
         legacy_["rebuild_owners"].toObject().contains("high_attack_streamsplit")))
        result["tools"] = tools_->json();
    else
        result.remove("tools");
    if (consumableEffects_ &&
        (consumableEffects_->modified() ||
         !legacy_["patches"].toObject()["consumableEffects"].toObject().isEmpty()))
        result["consumableEffects"] = consumableEffects_->json();
    else
        result.remove("consumableEffects");
    if (encounters_ && (encounters_->modified() ||
                        !legacy_["patches"].toObject()["creatureDrops"].toObject().isEmpty() ||
                        !legacy_["patches"].toObject()["neutral_encounters"].toObject().isEmpty() ||
                        !legacy_["patches"].toObject()["neutral_global_rate"].toObject().isEmpty()))
        result["encounters"] = encounters_->json();
    else
        result.remove("encounters");
    const auto oldPatches = legacy_["patches"].toObject();
    if (scenarios_ &&
        (scenarios_->modified() || oldPatches["scenario"].isObject() ||
         oldPatches["squadOverrides"].isObject() || oldPatches["squad_overrides"].isObject()))
        result["scenarios"] = scenarios_->json();
    else
        result.remove("scenarios");
    if (world_ && (world_->modified() || oldPatches["scenario"].isObject()))
        result["world"] = world_->json();
    else
        result.remove("world");
    if (dialogues_ && (dialogues_->modified() || oldPatches["cutscenes"].isObject()))
        result["dialogues"] = dialogues_->json();
    else
        result.remove("dialogues");
    if (scenePrograms_ && (scenePrograms_->modified() || legacyScenesImported_))
        result["scenePrograms"] = scenePrograms_->json();
    else
        result.remove("scenePrograms");
    if (sceneEvents_ && (sceneEvents_->modified() || legacyScenesImported_))
        result["sceneEvents"] = sceneEvents_->json();
    else
        result.remove("sceneEvents");
    if (legacyScenesImported_)
        result["legacySceneImportVersion"] = 1;
    else
        result.remove("legacySceneImportVersion");
    return result;
}
void Project::loadScenePrograms() {
    if (!scenePrograms_)
        scenePrograms_ = ScenePrograms::load(rom_);
}
void Project::setScenePrograms(const QJsonObject &value) {
    loadScenePrograms();
    auto next = *scenePrograms_;
    next.apply(value);
    bool removed = false;
    for (const auto &scene : scenePrograms_->authoredScenes())
        removed |= !next.authoredScenes().contains(scene.id);
    if (removed && !sceneEvents_)
        sceneEvents_ = SceneEvents::load(rom_);
    if (sceneEvents_)
        sceneEvents_->validate(next);
    scenePrograms_ = std::move(next);
}
void Project::loadSceneEvents() {
    loadScenePrograms();
    if (!sceneEvents_)
        sceneEvents_ = SceneEvents::load(rom_);
}
void Project::setSceneEvents(const QJsonObject &value) {
    loadSceneEvents();
    sceneEvents_->apply(value, *scenePrograms_);
}
void Project::loadDialogues() {
    if (!dialogues_)
        dialogues_ = Dialogues::load(rom_);
}
void Project::setDialogues(const QJsonObject &value) {
    loadDialogues();
    dialogues_->apply(value);
}
void Project::applyManifest(const QJsonObject &object) {
    if (object.value("format") != "LordlyCaliberProject" || object.value("version").toInt() != 1)
        throw Error("Unsupported native project format");
    if (object.value("sourceSha256").toString().toLatin1() != sha256(rom_.bytes()))
        throw Error("Project requires a different source ROM");
    if (!object.value("records").isObject())
        throw Error("Project records must be an object");
    auto candidate = Project::fromRom(rom_, catalog_);
    if (!texts_.isEmpty())
        candidate.loadTexts();
    if (promotion_)
        candidate.loadPromotion();
    if (shops_)
        candidate.loadShops();
    if (consumableEffects_)
        candidate.loadConsumableEffects();
    if (tools_)
        candidate.loadTools();
    if(icons_)candidate.loadIconArtwork();
    if(avatars_)candidate.loadAvatarArtwork();
    if(army_)candidate.loadArmyArtwork();
    if (encounters_)
        candidate.loadEncounters();
    if (scenarios_)
        candidate.loadScenarios();
    if (world_)
        candidate.loadWorld();
    if (dialogues_)
        candidate.loadDialogues();
    if (scenePrograms_)
        candidate.loadScenePrograms();
    if (object.contains("addedClasses") && !object["addedClasses"].isArray())
        throw Error("Added classes must be an array");
    for (const auto &value : object["addedClasses"].toArray()) {
        if (!value.isObject())
            throw Error("Invalid added class");
        const auto row = value.toObject();
        auto raw = [&](const QString &field) {
            const auto hex = row[field].toString().toLatin1();
            const auto bytes = QByteArray::fromHex(hex);
            if (!row[field].isString() || bytes.toHex() != hex.toLower())
                throw Error("Invalid added class " + field);
            return bytes;
        };
        candidate.appendClassAsset({raw("record"), raw("appearance"), raw("name"), 0},
                                   raw("description"), 0);
    }
    auto edits = object.value("records").toObject();
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        Table table;
        if (it.key() == "classes")
            table = Table::Classes;
        else if (it.key() == "items")
            table = Table::Items;
        else if (it.key() == "consumables")
            table = Table::Consumables;
        else
            throw Error("Unsupported project record type: " + it.key());
        if (!it.value().isObject())
            throw Error("Project record table must be an object");
        const auto rows = it.value().toObject();
        for (auto row = rows.begin(); row != rows.end(); ++row) {
            bool ok = false;
            int id = row.key().toInt(&ok);
            if (!ok)
                throw Error("Invalid project record ID");
            auto text = row.value().toString().toLatin1();
            auto bytes = QByteArray::fromHex(text);
            if (text.toLower() != bytes.toHex())
                throw Error("Invalid project record bytes");
            candidate.replaceBytes(table, id, bytes);
        }
    }
    if (object.contains("classNames") && !object["classNames"].isObject())
        throw Error("Class names must be an object");
    const auto names = object["classNames"].toObject();
    for (auto it = names.begin(); it != names.end(); ++it) {
        bool ok = false;
        const auto id = it.key().toInt(&ok);
        const auto text = it.value().toString().toLatin1();
        const auto bytes = QByteArray::fromHex(text);
        if (!ok || !it.value().isString() || text.toLower() != bytes.toHex())
            throw Error("Invalid saved class name");
        candidate.setClassNameBytes(id, bytes);
    }
    candidate.retained_ = object;
    for (auto &bank : candidate.texts_)
        for (auto &entry : bank.entries)
            entry.bytes = entry.original;
    if (object.contains("descriptions") && !object["descriptions"].isObject())
        throw Error("Project descriptions must be an object");
    const auto descriptions = object.value("descriptions").toObject();
    for (auto bank = descriptions.begin(); bank != descriptions.end(); ++bank) {
        if (!bank.value().isObject())
            throw Error("Description bank must be an object");
        const auto rows = bank.value().toObject();
        for (auto row = rows.begin(); row != rows.end(); ++row) {
            bool ok = false;
            int id = row.key().toInt(&ok);
            auto hex = row.value().toString().toLatin1();
            auto bytes = QByteArray::fromHex(hex);
            if (!ok || !row.value().isString() || bytes.toHex() != hex.toLower())
                throw Error("Invalid saved description bytes");
            candidate.replaceTextBytes(bank.key(), id, bytes);
        }
    }
    candidate.legacy_ = object.value("legacyImport").toObject();
    if(object.contains("iconArtwork")) {
        if(!object["iconArtwork"].isObject())throw Error("Invalid saved icon artwork");
        candidate.setIconArtwork(object["iconArtwork"].toObject());
    } else candidate.importLegacyIconArtwork(candidate.legacy_);
    if(object.contains("avatarArtwork")) {
        if(!object["avatarArtwork"].isObject())throw Error("Invalid saved portrait artwork");
        candidate.setAvatarArtwork(object["avatarArtwork"].toObject());
    } else candidate.importLegacyAvatarArtwork(candidate.legacy_);
    if(object.contains("armyArtwork")) {
        if(!object["armyArtwork"].isObject())throw Error("Invalid saved Army artwork");
        candidate.setArmyArtwork(object["armyArtwork"].toObject());
    } else candidate.importLegacyArmyArtwork(candidate.legacy_);
    if (object.contains("combatArtwork")) {
        if (!object["combatArtwork"].isObject()) throw Error("Invalid saved combat artwork");
        candidate.setCombatArtwork(object["combatArtwork"].toObject());
    } else candidate.importLegacyCombatArtwork(candidate.legacy_);
    const auto spriteLibrary = object.contains("spriteLibrary") ? object["spriteLibrary"] :
        candidate.legacy_["patches"].toObject().value("spriteLibrary");
    if (!spriteLibrary.isUndefined() && !spriteLibrary.isNull()) {
        if (!spriteLibrary.isObject()) throw Error("Invalid saved sprite library");
        candidate.sprites_ = SpriteLibrary::fromJson(spriteLibrary.toObject());
    }
    if (!object["records"].toObject().contains("consumables"))
        candidate.importLegacyConsumables(candidate.legacy_);
    if (object.contains("shops")) {
        if (!object["shops"].isObject())
            throw Error("Invalid saved shops");
        candidate.setShops(object["shops"].toObject());
    } else {
        candidate.importLegacyShops(candidate.legacy_);
    }
    if (object.contains("tools")) {
        if (!object["tools"].isObject())
            throw Error("Invalid saved tools");
        candidate.setTools(object["tools"].toObject());
    } else
        candidate.importLegacyTools(candidate.legacy_);
    if (object.contains("consumableEffects")) {
        if (!object["consumableEffects"].isObject())
            throw Error("Invalid saved consumable effects");
        candidate.setConsumableEffects(object["consumableEffects"].toObject());
    } else {
        candidate.importLegacyConsumableEffects(candidate.legacy_);
    }
    if (object.contains("encounters")) {
        if (!object["encounters"].isObject())
            throw Error("Invalid saved encounters");
        candidate.setEncounters(object["encounters"].toObject());
    } else {
        candidate.importLegacyEncounters(candidate.legacy_);
    }
    if (!object["encounters"].toObject().contains("globalRate"))
        candidate.importLegacyGlobalRate(candidate.legacy_);
    if (!object["encounters"].toObject().contains("rewards"))
        candidate.importLegacyRewards(candidate.legacy_);
    if (!object["encounters"].toObject().contains("scenarioRates"))
        candidate.importLegacyScenarioRates(candidate.legacy_);
    if (!object["encounters"].toObject().contains("customSquads"))
        candidate.importLegacyCustomSquads(candidate.legacy_);
    if (object.contains("scenarios")) {
        if (!object["scenarios"].isObject())
            throw Error("Invalid saved scenarios");
        candidate.setScenarios(object["scenarios"].toObject());
    } else
        candidate.importLegacyScenarios(candidate.legacy_);
    if (object.contains("world")) {
        if (!object["world"].isObject())
            throw Error("Invalid saved scenario world data");
        candidate.setWorld(object["world"].toObject());
    } else
        candidate.importLegacyWorld(candidate.legacy_);
    if (object.contains("scenePrograms")) {
        if (!object["scenePrograms"].isObject())
            throw Error("Invalid saved scene programs");
        candidate.loadScenePrograms();
        candidate.scenePrograms_->apply(object["scenePrograms"].toObject());
    }
    if (object.contains("sceneEvents")) {
        if (!object["sceneEvents"].isObject())
            throw Error("Invalid saved scene event bindings");
        candidate.setSceneEvents(object["sceneEvents"].toObject());
    } else if (sceneEvents_ || object.contains("scenePrograms"))
        candidate.loadSceneEvents();
    if (candidate.sceneEvents_)
        candidate.sceneEvents_->validate(*candidate.scenePrograms_);
    if (object.contains("dialogues")) {
        if (!object["dialogues"].isObject())
            throw Error("Invalid saved dialogue edits");
        candidate.setDialogues(object["dialogues"].toObject());
    } else
        candidate.importLegacyDialogues(candidate.legacy_);
    if (object.contains("legacySceneImportVersion")) {
        if (object["legacySceneImportVersion"] != 1 || !object["scenePrograms"].isObject() ||
            !object["sceneEvents"].isObject() || !object["dialogues"].isObject())
            throw Error("Incomplete saved native conversion of browser scenes");
        candidate.legacyScenesImported_ = true;
    } else
        candidate.importLegacyScenes(candidate.legacy_);
    if (candidate.promotion_ && candidate.addedClasses_.isEmpty())
        candidate.promotion_ = Promotion::load(rom_);
    if (object.contains("promotion")) {
        if (!object["promotion"].isObject())
            throw Error("Invalid saved promotion data");
        candidate.setPromotion(object["promotion"].toObject());
    }
    if (!candidate.addedClasses_.isEmpty() &&
        (!object.contains("promotion") ||
         candidate.promotion_->classCount() != candidate.records(Table::Classes).size()))
        throw Error("Added classes require matching promotion data");
    for (const auto &bank : candidate.texts_)
        bank.validateRuntimeSize();
    *this = std::move(candidate);
}
void Project::importLegacyGlobalRate(const QJsonObject &object) {
    // Copy the value; a reference into the temporary patch object would dangle.
    const QJsonValue value = object["patches"].toObject()["neutral_global_rate"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported global encounter rate");
    if (value.toObject().isEmpty())
        return;
    loadEncounters();
    encounters_->importLegacyGlobalRate(value.toObject());
}
void Project::importLegacyRewards(const QJsonObject &object) {
    const QJsonValue value = object["patches"].toObject()["creatureDrops"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported creature rewards");
    if (value.toObject().isEmpty())
        return;
    loadEncounters();
    encounters_->importLegacyRewards(value.toObject());
}
void Project::importLegacyScenarioRates(const QJsonObject &object) {
    const QJsonValue value =
        object["patches"].toObject()["neutral_encounters"].toObject()["scenario_rates"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported scenario rates");
    if (value.toObject().isEmpty())
        return;
    loadEncounters();
    encounters_->importLegacyScenarioRates(value.toObject());
}
void Project::importLegacyCustomSquads(const QJsonObject &object) {
    const QJsonValue value =
        object["patches"].toObject()["neutral_encounters"].toObject()["custom_squads"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported custom squads");
    if (value.toObject().isEmpty())
        return;
    loadEncounters();
    encounters_->importLegacyCustomSquads(value.toObject(), int(records(Table::Classes).size()));
}
void Project::importLegacyScenarios(const QJsonObject &object) {
    const auto patches = object["patches"].toObject();
    auto part = [&](const QString &key) {
        const auto value = patches[key];
        if (value.isUndefined() || value.isNull())
            return QJsonObject{};
        if (!value.isObject())
            throw Error("Invalid imported " + key);
        return value.toObject();
    };
    const auto scenario = part("scenario"), squads = part("squadOverrides"),
               oldSquads = part("squad_overrides");
    if (!squads.isEmpty() && !oldSquads.isEmpty() && squads != oldSquads)
        throw Error("Conflicting imported squad override sections");
    if (scenario.isEmpty() && squads.isEmpty() && oldSquads.isEmpty())
        return;
    loadScenarios();
    scenarios_->importBrowser(rom_, scenario, squads.isEmpty() ? oldSquads : squads, classData());
}
void Project::importLegacyScenes(const QJsonObject &object) {
    const auto value = object["patches"].toObject()["cutscenes"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported scene project");
    loadSceneEvents();
    auto imported = importBrowserScenes(rom_, *scenePrograms_, *sceneEvents_, value.toObject());
    scenePrograms_ = std::move(imported.programs);
    sceneEvents_ = std::move(imported.events);
    legacyScenesImported_ = true;
}
void Project::importLegacyDialogues(const QJsonObject &object) {
    const auto value = object["patches"].toObject()["cutscenes"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported cutscene project");
    loadDialogues();
    dialogues_->importBrowser(value.toObject());
}
void Project::importLegacyWorld(const QJsonObject &object) {
    const auto value = object["patches"].toObject()["scenario"];
    if (value.isUndefined() || value.isNull())
        return;
    if (!value.isObject())
        throw Error("Invalid imported scenario world data");
    loadWorld();
    world_->importBrowser(rom_, value.toObject());
}
void Project::importLegacyEncounters(const QJsonObject &object) {
    const auto patches = object["patches"].toObject();
    if (!patches.contains("neutral_encounters"))
        return;
    if (!patches["neutral_encounters"].isObject())
        throw Error("Invalid imported encounters");
    const auto values = patches["neutral_encounters"].toObject();
    if (values.isEmpty())
        return;
    loadEncounters();
    encounters_->importLegacy(values, int(records(Table::Classes).size()));
}
void Project::importLegacyConsumableEffects(const QJsonObject &object) {
    const auto patch = object["patches"].toObject();
    if (patch.contains("consumableEffects") && !patch["consumableEffects"].isObject())
        throw Error("Imported consumable effects must be an object");
    const auto values = patch["consumableEffects"].toObject();
    if (values.isEmpty())
        return;
    loadConsumableEffects();
    auto effects = *consumableEffects_;
    effects.importLegacy(values, object["version"].toInt());
    for (const auto &entry : ConsumableEffects::definitions()) {
        const auto def = entry.toObject();
        if (values.contains(def["projectKey"].toString()))
            editConsumableEffect(def["key"].toString(),
                                 effects.models()[def["key"].toString()].toObject());
    }
}
void Project::importLegacyConsumables(const QJsonObject &object) {
    const auto patch = object["patches"].toObject();
    if (patch.contains("consumables") && !patch["consumables"].isObject())
        throw Error("Imported consumables must be an object");
    const auto edits = patch["consumables"].toObject();
    auto number = [](const QJsonValue &v, int maximum) {
        const auto value = v.toDouble(-1);
        if (!v.isDouble() || !std::isfinite(value) || value != std::floor(value) || value < 0 ||
            value > maximum)
            throw Error("Invalid imported consumable value");
        return int(value);
    };
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        bool ok = false;
        const auto id = it.key().toInt(&ok);
        if (!ok || QString::number(id) != it.key() || !it.value().isObject())
            throw Error("Invalid imported consumable ID");
        auto bytes = record(Table::Consumables, id).bytes;
        const auto fields = it.value().toObject();
        for (auto field = fields.begin(); field != fields.end(); ++field) {
            if (field.key() == "price" || field.key() == "flagHi")
                put16(bytes, field.key() == "price" ? 6 : 4, quint16(number(field.value(), 65535)));
            else if (field.key() == "flagLo") {
                if (!field.value().isArray() || field.value().toArray().size() > 4)
                    throw Error("Consumable metadata must contain at most four bytes");
                const auto values = field.value().toArray();
                for (int index = 0; index < values.size(); ++index)
                    put8(bytes, 8 + index, quint8(number(values[index], 255)));
            } else
                throw Error("Unrecognized imported consumable field: " + field.key());
        }
        replaceBytes(Table::Consumables, id, bytes);
    }
}
void Project::importLegacyShops(const QJsonObject &object) {
    const auto patch = object["patches"].toObject();
    if (patch.contains("shops") && !patch["shops"].isObject())
        throw Error("Imported shops must be an object");
    const auto importedShops = patch["shops"].toObject();
    if (!importedShops.isEmpty() ||
        object["rebuild_owners"].toObject()["shop_memberships"].toBool()) {
        loadShops();
        auto next = shops_->json();
        auto records = next["records"].toArray();
        for (auto it = importedShops.begin(); it != importedShops.end(); ++it) {
            bool ok = false;
            const auto id = it.key().toInt(&ok);
            if (!ok || QString::number(id) != it.key() || id < 0 || id >= Shops::count ||
                !it.value().isObject())
                throw Error("Invalid imported shop ID");
            const auto entry = it.value().toObject();
            if (entry.isEmpty())
                throw Error("Imported shop needs equipment or consumable membership");
            for (auto value = entry.begin(); value != entry.end(); ++value)
                if (value.key() != "items" && value.key() != "consumables")
                    throw Error("Unrecognized imported shop field: " + value.key());
            auto row = records[id].toObject();
            if (entry.contains("items"))
                row["equipment"] = entry["items"];
            if (entry.contains("consumables"))
                row["consumables"] = entry["consumables"];
            records[id] = row;
        }
        next["records"] = records;
        setShops(next);
    }
}
void Project::importLegacy(const QJsonObject &object) {
    if (object["format"] == "ob64-cutscene-project") {
        importLegacy(QJsonObject{{"format", "ob64-patch"},
                                 {"version", 36},
                                 {"patches", QJsonObject{{"cutscenes", object}}}});
        return;
    }
    if (object["format"] == "ob64-scenario-project") {
        importLegacy(QJsonObject{{"format", "ob64-patch"},
                                 {"version", 36},
                                 {"patches", QJsonObject{{"scenario", object}}}});
        return;
    }
    if (object.value("format").toString() != "ob64-patch")
        throw Error("Unsupported browser project format");
    int version = object.value("version").toInt();
    if (version < 9 || version > 36)
        throw Error("Browser project version requires an explicit migration before import");
    if (!object.value("patches").isObject())
        throw Error("Browser project patches must be an object");
    auto candidate = *this;
    auto patch = object.value("patches").toObject();
    for (auto table : {Table::Classes, Table::Items}) {
        auto key = table == Table::Classes ? QString("classDefs") : QString("items");
        if (patch.contains(key) && !patch[key].isObject())
            throw Error("Imported record table must be an object");
        auto rows = patch.value(key).toObject();
        for (auto it = rows.begin(); it != rows.end(); ++it) {
            bool ok = false;
            auto id = it.key().toInt(&ok);
            if (!ok || !it.value().isObject())
                throw Error("Invalid imported record ID");
            auto bytes = candidate.record(table, id).bytes;
            auto entry = it.value().toObject();
            if (table == Table::Items) {
                for (auto value = entry.begin(); value != entry.end(); ++value) {
                    int field = -1;
                    for (int n = 0; n < fields(table).size(); ++n)
                        if (fields(table)[n].key == value.key())
                            field = n;
                    if (field < 0)
                        throw Error("Unrecognized imported item field: " + value.key());
                    double number =
                        value.value().toDouble(std::numeric_limits<double>::quiet_NaN());
                    if (!std::isfinite(number) || std::floor(number) != number ||
                        number < fields(table)[field].minimum() ||
                        number > fields(table)[field].maximum())
                        throw Error("Invalid imported item field value");
                    candidate.setValue(table, id, field, int(number));
                }
                continue;
            }
            if (table == Table::Classes && entry.contains("record_index") &&
                entry["record_index"].toInt() != id + 1)
                throw Error("Imported class record index disagrees with class ID");
            if (entry.contains("bytes") && !entry["bytes"].isObject())
                throw Error("Imported class bytes must be an object");
            auto changes = entry.value("bytes").toObject();
            for (auto v = changes.begin(); v != changes.end(); ++v) {
                int offset = v.key().toInt(&ok);
                double value = v.value().toDouble(-1);
                if (!ok || offset < 0 || offset >= bytes.size() || value < 0 || value > 255 ||
                    std::floor(value) != value)
                    throw Error("Invalid imported record byte");
                int header = table == Table::Classes ? 12 : 4, body = int(bytes.size()) - header;
                put8(bytes, offset < body ? offset + header : offset - body, quint8(value));
            }
            candidate.replaceBytes(table, id, bytes);
        }
    }
    if (patch.contains("item_prices") && !patch["item_prices"].isObject())
        throw Error("Imported item prices must be an object");
    const auto prices = patch.value("item_prices").toObject();
    for (auto it = prices.begin(); it != prices.end(); ++it) {
        bool ok = false;
        int id = it.key().toInt(&ok);
        double value = it.value().toDouble(-1);
        if (!ok || value < 0 || value > 65535 || std::floor(value) != value)
            throw Error("Invalid imported item price");
        auto bytes = candidate.record(Table::Items, id).bytes;
        put16(bytes, 8, quint16(value));
        candidate.replaceBytes(Table::Items, id, bytes);
    }
    candidate.importLegacyShops(object);
    candidate.importLegacyConsumables(object);
    candidate.importLegacyConsumableEffects(object);
    candidate.importLegacyTools(object);
    candidate.importLegacyIconArtwork(object);
    candidate.importLegacyAvatarArtwork(object);
    candidate.importLegacyArmyArtwork(object);
    candidate.importLegacyCombatArtwork(object);
    if (patch.contains("spriteLibrary") && !patch["spriteLibrary"].isNull()) {
        if (!patch["spriteLibrary"].isObject()) throw Error("Invalid imported sprite library");
        candidate.sprites_.merge(SpriteLibrary::fromJson(patch["spriteLibrary"].toObject()));
    }
    candidate.importLegacyEncounters(object);
    candidate.importLegacyGlobalRate(object);
    candidate.importLegacyRewards(object);
    candidate.importLegacyScenarioRates(object);
    candidate.importLegacyCustomSquads(object);
    candidate.importLegacyScenarios(object);
    candidate.importLegacyWorld(object);
    candidate.importLegacyDialogues(object);
    candidate.importLegacyScenes(object);
    candidate.legacy_ = object;
    if (patch.contains("descriptions") && !patch["descriptions"].isObject())
        throw Error("Imported descriptions must be an object");
    const auto descriptions = patch.value("descriptions").toObject();
    for (auto bank = descriptions.begin(); bank != descriptions.end(); ++bank) {
        if (!bank.value().isObject())
            throw Error("Imported description bank must be an object");
        const auto rows = bank.value().toObject();
        for (auto row = rows.begin(); row != rows.end(); ++row) {
            bool ok = false;
            int id = row.key().toInt(&ok);
            if (!ok || !row.value().isString())
                throw Error("Invalid imported description");
            candidate.setText(bank.key(), id, row.value().toString());
        }
    }
    candidate.importWarnings_.clear();
    if (patch.contains("statGates")) {
        if (!patch["statGates"].isObject())
            throw Error("Invalid imported stat gates");
        const auto gates = patch["statGates"].toObject();
        if (!gates.isEmpty()) {
            candidate.loadPromotion();
            const QStringList keys = {"str", "vit", "int", "men", "agi", "dex", "alnMin", "alnMax"};
            for (auto row = gates.begin(); row != gates.end(); ++row) {
                bool ok = false;
                auto id = row.key().toInt(&ok);
                if (!ok || !row.value().isObject())
                    throw Error("Invalid imported stat gate row");
                const auto values = row.value().toObject();
                for (auto field = values.begin(); field != values.end(); ++field) {
                    auto v = field.value().toDouble(-1);
                    if (!std::isfinite(v) || v != std::floor(v) || v < 0 || v > 255)
                        throw Error("Invalid imported stat gate value");
                    candidate.promotion_->setGate(id, keys.indexOf(field.key()), int(v));
                }
            }
        }
    }
    if (patch.contains("classFamilies") && !patch["classFamilies"].isNull()) {
        if (!patch["classFamilies"].isObject())
            throw Error("Invalid imported class families");
        candidate.loadPromotion();
        candidate.promotion_->families =
            Promotion::validateFamilies(patch["classFamilies"].toObject());
    }
    auto pending = candidate.pendingLegacySections();
    if (!pending.isEmpty())
        candidate.importWarnings_ = {
            "Retained sections awaiting native support: " + pending.join(", ") +
            ". Export will preserve your work by stopping until these sections can be built."};
    *this = std::move(candidate);
}
QStringList Project::pendingLegacySections() const {
    std::function<bool(QJsonValue)> changed = [&](QJsonValue value) {
        if (value.isNull() || value.isUndefined())
            return false;
        if (value.isArray())
            return !value.toArray().isEmpty();
        if (value.isObject()) {
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (it.key() == "schemaVersion" || it.key() == "schema")
                    continue;
                if (changed(it.value()))
                    return true;
            }
            return false;
        }
        return true;
    };
    QStringList pending;
    const auto patches = legacy_.value("patches").toObject();
    for (auto it = patches.begin(); it != patches.end(); ++it) {
        if (it.key() == "spriteLibrary")
            continue;
        if(it.key()=="art" && (icons_ || avatars_ || army_ || combat_)) {
            const auto sections=it.value().toObject();
            for(auto section=sections.begin();section!=sections.end();++section)
                if(section.key()!="schemaVersion" && !(section.key()=="icons" && icons_) && !(section.key()=="avatars" && avatars_) && !(section.key()=="armySprites" && army_) && !(section.key()=="animations" && combat_) && changed(section.value()))
                    pending << "art."+section.key();
            continue;
        }
        if (it.key() == "tools" && tools_) {
            const auto values = it.value().toObject();
            for (auto setting = values.begin(); setting != values.end(); ++setting)
                if (setting.key() != "experience-size-scale" &&
                    setting.key() != "cf-army-counter" && setting.key() != "character-card-luck" &&
                    setting.key() != "squad-menu-alignment" &&
                    setting.key() != "high-attack-streamsplit" && changed(setting.value()))
                    pending << "tools." + setting.key();
            continue;
        }
        if (it.key() == "cutscenes" && legacyScenesImported_)
            continue;
        if (scenarios_ && (it.key() == "squadOverrides" || it.key() == "squad_overrides"))
            continue;
        if (scenarios_ && it.key() == "scenario") {
            const QSet<QString> consumed{
                "format",        "version",        "created_at",  "source",           "settings",
                "modifiedEsets", "levelBaseEdits", "addedSquads", "squadLevelCopies", "layers"};
            const auto parts = it.value().toObject();
            for (auto part = parts.begin(); part != parts.end(); ++part)
                if (!consumed.contains(part.key()) &&
                    !(world_ &&
                      (part.key() == "modifiedTreasures" || part.key() == "siteAllegiances" ||
                       part.key() == "strongholdFields")) &&
                    changed(part.value()))
                    pending << "scenario." + part.key();
            continue;
        }
        if (it.key() == "neutral_global_rate" && encounters_)
            continue;
        if (it.key() == "creatureDrops" && encounters_)
            continue;
        if (it.key() == "neutral_encounters" && encounters_) {
            const auto parts = it.value().toObject();
            for (auto part = parts.begin(); part != parts.end(); ++part)
                if (part.key() != "scenario_slices" && part.key() != "terrain_rates" &&
                    part.key() != "scenario_rates" && part.key() != "custom_squads" &&
                    changed(part.value()))
                    pending << "neutral_encounters." + part.key();
            continue;
        }
        if (it.key() != "classDefs" && it.key() != "items" && it.key() != "item_prices" &&
            it.key() != "descriptions" && it.key() != "statGates" && it.key() != "classFamilies" &&
            it.key() != "shops" && it.key() != "consumables" && it.key() != "consumableEffects" &&
            changed(it.value()))
            pending << it.key();
    }
    const auto owners = legacy_.value("rebuild_owners").toObject();
    for (auto it = owners.begin(); it != owners.end(); ++it) {
        if (it.key() == "schema")
            continue;
        if (it.key() == "shop_memberships" && it.value().isBool() && shops_)
            continue;
        if (it.key() == "experience_scale_percent" && (tools_ || it.value().toInt() == 100))
            continue;
        if (it.key() == "chaos_frame_counter" && tools_)
            continue;
        if ((it.key() == "character_card_luck" || it.key() == "squad_menu_alignment" ||
             it.key() == "high_attack_streamsplit") && tools_)
            continue;
        if (it.value().isBool() && !it.value().toBool())
            continue;
        pending << "rebuild owner " + it.key();
    }
    return pending;
}
void atomicWrite(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw Error("Cannot save " + path + ": " + file.errorString());
}
void Project::saveFolder(const QString &folder) const {
    if (!QDir().mkpath(folder))
        throw Error("Cannot create project folder");
    const auto bytes = QJsonDocument(manifest()).toJson();
    if (bytes.size() > 512 * 1024 * 1024)
        throw Error("Project exceeds the 512 MiB document limit");
    atomicWrite(QDir(folder).filePath("project.json"), bytes);
}
QJsonObject Project::readProject(const QString &path) {
    constexpr qsizetype limit = 512 * 1024 * 1024;
    QFile file(QFileInfo(path).isDir() ? QDir(path).filePath("project.json") : path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit)
        throw Error("Cannot read project or project is too large");
    auto bytes = file.readAll();
    if (bytes.startsWith("LC05PACK\n")) {
        bytes = bytes.mid(9);
        if (bytes.size() < 5 || u32(bytes, 0) == 0 || u32(bytes, 0) > limit)
            throw Error("Project archive exceeds decoded size limit");
        uLongf produced = u32(bytes, 0);
        uLong consumed = uLong(bytes.size() - 4);
        QByteArray decoded(qsizetype(produced), Qt::Uninitialized);
        const int status = uncompress2(reinterpret_cast<Bytef *>(decoded.data()), &produced,
            reinterpret_cast<const Bytef *>(bytes.constData() + 4), &consumed);
        if (status != Z_OK || produced != uLongf(decoded.size()) || consumed != uLong(bytes.size() - 4))
            throw Error("Damaged project archive");
        bytes = std::move(decoded);
    }
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        throw Error("Invalid project JSON: " + error.errorString());
    return doc.object();
}
void Project::saveArchive(const QString &path) const {
    if (!rom_.sourcePath().isEmpty() &&
        QFileInfo(path).canonicalFilePath() == QFileInfo(rom_.sourcePath()).canonicalFilePath())
        throw Error("A project archive cannot overwrite the source ROM");
    auto object = manifest();
    object.remove("sourcePath");
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (bytes.size() > 512 * 1024 * 1024)
        throw Error("Project exceeds the 512 MiB document limit");
    atomicWrite(path, "LC05PACK\n" + qCompress(bytes, 9));
}
QByteArray Project::rebuildRecords() const {
    auto out = rom_.bytes();
    for (const auto &[table, records] : tables_)
        for (const auto &r : records) {
            if (r.romOffset < 0 || r.romOffset + r.bytes.size() > out.size())
                throw Error("Record allocation exceeds ROM");
            out.replace(r.romOffset, r.bytes.size(), r.bytes);
        }
    return out;
}
} // namespace lordly
