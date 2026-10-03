#include "engine/project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cmath>
#include <limits>

namespace lordly {
QString tableKey(Table t) {
    return t == Table::Classes ? "classes" : "items";
}
const QVector<Field> &fields(Table table) {
    static const auto classes = [] {
        QVector<Field> f;
        auto add = [&](QString key, QString label, QString group, int offset, int width = 1,
                       QString choices = {}) {
            f.push_back({key, label, group, offset, width, false, choices});
        };
        add("unitSize", "Size", "Identity", 4, 1, "UNIT_SIZES");
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
        add("itemCapacity", "Carried-item contribution", "Equipment", 71);
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
    return table == Table::Classes ? classes : items;
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
    for (auto table : {Table::Classes, Table::Items}) {
        auto &records = p.tables_[table];
        bool cls = table == Table::Classes;
        int count = cls ? 165 : 278, stride = cls ? 72 : 32;
        qsizetype base = cls ? 0x5dad8 + 72 - 12 : 0x62310 - 4;
        if (p.rom_.revision() == 1)
            base += 0x20;
        if (cls && classPlacement) {
            count = classPlacement->layout.count;
            base = classPlacement->dataRomOffset + classPlacement->layout.records -
                   classPlacement->layout.indexBase;
        }
        auto names = catalog.value(cls ? "CLASS_NAMES" : "ITEM_NAMES").toObject();
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
    tables_.at(t)[row].bytes = bytes;
}
int Project::changedRecords() const {
    int count = int(classNames_.size());
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
QByteArray Project::classNameBytes(int row) const {
    record(Table::Classes, row);
    if (!originalClasses_)
        throw Error("This ROM's class names need a supported source layout before editing");
    return classNames_.value(row, originalClasses_->entries()[row].name);
}
void Project::setClassNameBytes(int row, const QByteArray &name) {
    classNameBytes(row);
    auto candidate = *originalClasses_;
    auto entry = candidate.entries()[row];
    entry.name = name;
    candidate.set(row, entry);
    if (name == originalClasses_->entries()[row].name)
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
    const auto &records = tables_.at(Table::Classes);
    if (records.size() != result.entries().size())
        throw Error("Class record and asset counts disagree");
    for (int id = 0; id < records.size(); ++id) {
        auto entry = result.entries()[id];
        entry.record = records[id].bytes;
        entry.name = classNameBytes(id);
        result.set(id, entry);
    }
    return result;
}
void Project::loadTexts() {
    if (texts_.isEmpty())
        texts_ = TextBank::load(rom_);
}
void Project::loadPromotion() {
    if (!promotion_)
        promotion_ = Promotion::load(rom_);
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
    if (!legacy_.isEmpty())
        result["legacyImport"] = legacy_;
    return result;
}
void Project::applyManifest(const QJsonObject &object) {
    if (object.value("format") != "LordlyCaliberProject" || object.value("version").toInt() != 1)
        throw Error("Unsupported native project format");
    if (object.value("sourceSha256").toString().toLatin1() != sha256(rom_.bytes()))
        throw Error("Project requires a different source ROM");
    if (!object.value("records").isObject())
        throw Error("Project records must be an object");
    auto candidate = *this;
    const auto renamed = candidate.classNames_.keys();
    for (const auto id : renamed)
        candidate.setClassNameBytes(id, candidate.originalClasses_->entries()[id].name);
    for (auto &[table, records] : candidate.tables_)
        for (auto &record : records)
            record.bytes = record.original;
    auto edits = object.value("records").toObject();
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        Table table;
        if (it.key() == "classes")
            table = Table::Classes;
        else if (it.key() == "items")
            table = Table::Items;
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
    if (candidate.promotion_)
        candidate.promotion_ = Promotion::load(rom_);
    if (object.contains("promotion")) {
        if (!object["promotion"].isObject())
            throw Error("Invalid saved promotion data");
        candidate.setPromotion(object["promotion"].toObject());
    }
    *this = std::move(candidate);
}
void Project::importLegacy(const QJsonObject &object) {
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
    for (auto it = patches.begin(); it != patches.end(); ++it)
        if (it.key() != "classDefs" && it.key() != "items" && it.key() != "item_prices" &&
            it.key() != "descriptions" && it.key() != "statGates" && it.key() != "classFamilies" &&
            changed(it.value()))
            pending << it.key();
    const auto owners = legacy_.value("rebuild_owners").toObject();
    for (auto it = owners.begin(); it != owners.end(); ++it) {
        if (it.key() == "schema")
            continue;
        if (it.key() == "experience_scale_percent" && it.value().toInt() == 100)
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
    atomicWrite(QDir(folder).filePath("project.json"), QJsonDocument(manifest()).toJson());
}
QJsonObject Project::readProject(const QString &path) {
    QFile file(QFileInfo(path).isDir() ? QDir(path).filePath("project.json") : path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024)
        throw Error("Cannot read project or project is too large");
    auto bytes = file.readAll();
    if (bytes.startsWith("LC05PACK\n")) {
        bytes = bytes.mid(9);
        if (bytes.size() < 4 || u32(bytes, 0) > 64 * 1024 * 1024)
            throw Error("Project archive exceeds decoded size limit");
        bytes = qUncompress(bytes);
        if (bytes.isEmpty())
            throw Error("Damaged project archive");
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
    atomicWrite(path,
                "LC05PACK\n" + qCompress(QJsonDocument(object).toJson(QJsonDocument::Compact), 9));
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
