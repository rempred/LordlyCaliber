#include "engine/scenario_seeder.h"
#include "engine/class_data.h"
#include "game/classes/save_format.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_WIN
#define NOMINMAX
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#endif
static void initSeederResources() {
    Q_INIT_RESOURCE(scenario_seeder);
}
namespace lordly {
namespace {
QByteArray read(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw Error("Cannot read " + path);
    return f.readAll();
}
QByteArray wordSwap(QByteArray b) {
    if (b.size() % 4)
        throw Error("Invalid battery word alignment");
    for (qsizetype i = 0; i < b.size(); i += 4) {
        std::swap(b[i], b[i + 3]);
        std::swap(b[i + 1], b[i + 2]);
    }
    return b;
}
} // namespace
SaveDocument ScenarioSeeder::expanded(SaveDocument source) {
    if (!source.battery())
        throw Error("Choose a battery save");
    if (!source.expanded_) {
        if (!lc_save_expand_image(reinterpret_cast<unsigned char *>(source.data_.data()),
                                  unsigned(source.data_.size())))
            throw Error("Cannot convert the battery save to the expanded layout");
        source.expanded_ = true;
        source.selectSlot(source.slot_);
    }
    return source;
}
SaveDocument ScenarioSeeder::patch(SaveDocument source, const QByteArray &memory) {
    if (!source.battery() || source.memory().size() != memory.size())
        throw Error("Choose a compatible battery save");
    // Match the standalone seeder's selective patch: keep all other packed
    // fields and unused payload bits exactly as they were loaded.
    QSet<int> mapped;
    const int base = int(lc_save_record_offset(source.slot_, source.expanded_));
    for (const auto &cell : source.codec_) {
        if (memory.mid(cell.address, cell.bytes) == source.ram_.mid(cell.address, cell.bytes))
            continue;
        quint32 value = 0;
        for (int n = 0; n < cell.bytes; ++n) {
            value = (value << 8) | u8(memory, cell.address + n);
            mapped.insert(cell.address + n);
        }
        const quint32 mask = cell.bits == 32 ? 0xffffffffU : (1U << cell.bits) - 1;
        const quint32 widthMask = cell.bytes == 4 ? 0xffffffffU : (1U << (cell.bytes * 8)) - 1;
        const auto extra = value & ~mask;
        if (extra && (!cell.sign || extra != (widthMask & ~mask)))
            throw Error(QString("Seed value %2 at RAM %1 exceeds its %3-bit saved width")
                            .arg(cell.address + 0x80000000U, 8, 16)
                            .arg(value)
                            .arg(cell.bits));
        for (int n = 0; n < cell.bits; ++n) {
            const int bit = cell.bit + n, at = base + 0x2a + bit / 8, shift = 7 - bit % 8;
            put8(source.data_, at,
                 quint8((u8(source.data_, at) & ~(1 << shift)) |
                        (((value >> (cell.bits - n - 1)) & 1) << shift)));
        }
    }
    for (int i = 0; i < memory.size(); ++i)
        if (memory[i] != source.ram_[i] && !mapped.contains(i))
            throw Error("Requested seed field is not persisted by this ROM");
    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(source.data_.data() + base),
                             lc_save_record_size(source.slot_, source.expanded_), source.slot_,
                             source.expanded_))
        throw Error("Cannot sign seed data");
    source.selectSlot(source.slot_);
    return source;
}
SaveDocument ScenarioSeeder::withArmy(SaveDocument base, const SaveDocument &donor) const {
    if (!donor.battery())
        throw Error("Choose a battery save for the tester army");
    if (donor.expanded())
        base = expanded(std::move(base));
    auto memory = base.memory();
    const auto &army = donor.memory();
    const QVector<QPair<int, int>> ranges{{0x193ab0, 0x193ab6}, // Saved legion display order.
                                          {0x193bc0, 0x1951a0}, {0x1971f0, 0x1974de},
                                          {0x1969b8, 0x196a26}, {0x197710, 0x197774},
                                          {0x1977d8, 0x1977f6}, {0x190f80, 0x1910ec}};
    for (const auto &cell : base.codec_)
        for (int i = 0; i < cell.bytes; ++i)
            if (std::any_of(ranges.begin(), ranges.end(), [&](auto r) {
                    return cell.address + i >= r.first && cell.address + i < r.second;
                }))
                memory[cell.address + i] = army[cell.address + i];
    auto classes = ClassData::load(rom_);
    auto assigned = [&](const QByteArray &ram) {
        std::array<QMap<int, int>, 2> result;
        for (int id = 1; id < 100; ++id) {
            int at = 0x193bc0 + id * 56, primary = u8(ram, at + 17), secondary = u8(ram, at + 18);
            if (!primary)
                continue;
            if (std::max(primary, secondary) >= classes.entries().size())
                throw Error("Army references a class absent from the selected ROM");
            // ClassData includes the 12-byte record prefix; the standalone
            // seeder's 0x5DB20 table address starts after that prefix.
            int cls = u8(classes.entries()[primary].record, 69) == secondary ? primary : secondary;
            for (int offset = 0; offset < 4; ++offset)
                for (int item : {int(u16(ram, at + 42 + offset * 2)),
                                 int(u16(classes.entries()[cls].record, 46 + offset * 2))})
                    if (item)
                        ++result[0][item];
        }
        for (int id = 0; id < 30; ++id) {
            int at = 0x1971f0 + id * 25;
            if (u8(ram, at + 1) & 1)
                for (int i = 13; i < 23; ++i)
                    if (int item = u8(ram, at + i))
                        ++result[1][item];
        }
        return result;
    };
    const auto old = assigned(base.memory()), next = assigned(army);
    for (int lane = 0; lane < 2; ++lane) {
        int table = lane ? 0x193ac0 : 0x196b00, count = lane ? 40 : 278;
        QMap<int, int> indexes;
        QVector<int> available;
        for (int i = 0; i < count; ++i) {
            int at = table + i * 4, item = u16(memory, at);
            if (item)
                indexes[item] = at;
            else
                available << at;
        }
        QSet<int> items;
        for (int i : old[lane].keys())
            items << i;
        for (int i : next[lane].keys())
            items << i;
        auto ordered = items.values();
        std::sort(ordered.begin(), ordered.end());
        for (int item : ordered) {
            int at = indexes.value(item, -1), owned = at < 0 ? 0 : u8(memory, at + 3);
            int needed = std::max(0, owned - old[lane].value(item)) + next[lane].value(item);
            if (needed > 127)
                throw Error("The army requires more item copies than the save can hold");
            if (at < 0) {
                if (!needed)
                    continue;
                if (available.isEmpty())
                    throw Error("No inventory space for army equipment");
                at = available.takeFirst();
                put16(memory, at, quint16(item));
            }
            put8(memory, at + 3, quint8(needed));
        }
    }
    return patch(std::move(base), memory);
}
ScenarioSeeder::ScenarioSeeder(Rom rom) : rom_(std::move(rom)), routing_(rom_) {
    initSeederResources();
    source_ = SaveDocument::open(rom_, read(":/game/seeder/story-start.sra"));
    source_.selectSlot(0);
    auto army = SaveDocument::open(rom_, read(":/game/seeder/default-army.sra"));
    army.selectSlot(0);
    source_ = withArmy(std::move(source_), army);
    auto memory = source_.memory();
    for (int id = 1; id < 100; ++id) {
        int at = 0x193bc0 + id * 56;
        if (!u8(memory, at + 17))
            continue;
        for (int offset : {22, 24, 28, 30, 32, 34, 36, 38})
            put16(memory, at + offset, quint16(std::min(999, int(u16(memory, at + offset)) + 100)));
        put8(memory, at + 40, 50);
    }
    for (int lane = 0; lane < 2; ++lane) {
        int table = lane ? 0x193ac0 : 0x196b00, count = lane ? 40 : 278;
        QMap<int, int> indexes;
        QVector<int> free;
        for (int i = 0; i < count; ++i) {
            int at = table + i * 4, item = u16(memory, at);
            if (item)
                indexes[item] = at;
            else
                free << at;
        }
        for (int item = 1; item < (lane ? 45 : 278); ++item) {
            if (lane && QVector<int>{32, 36, 37, 38}.contains(item))
                continue;
            int at = indexes.value(item, -1), owned = at < 0 ? 0 : u8(memory, at + 3);
            if (at < 0) {
                if (free.isEmpty())
                    throw Error("No space for default inventory");
                at = free.takeFirst();
                put16(memory, at, quint16(item));
            }
            put8(memory, at + 3, quint8(std::min(99, owned + 10)));
        }
    }
    source_ = patch(std::move(source_), memory);
    QMap<int, QString> labels;
    QMap<int, int> order;
    for (auto v : QJsonDocument::fromJson(read(":/game/seeder/mission-labels.json"))
                      .object()["keys"]
                      .toArray()) {
        const auto o = v.toObject();
        int key = o["key"].toInt();
        labels[key] = o["label"].toString();
        order[key] = o["wikiId"].toInt(999);
    }
    struct Best {
        QPair<int, int> score;
        StoryRoute route;
        int cf;
    };
    QMap<QPair<int, int>, Best> best;
    QMap<int, QMap<int, int>> history;
    QVector<int> cfs;
    for (int i = 0; i <= 100; ++i)
        cfs << i;
    const int sourceCf = u8(memory, 0x1936a9);
    std::stable_sort(cfs.begin(), cfs.end(),
                     [&](int a, int b) { return std::abs(a - sourceCf) < std::abs(b - sourceCf); });
    auto nodes = routing_.travelNodes.values();
    std::sort(nodes.begin(), nodes.end());
    for (int cf : cfs)
        for (int node : nodes)
            for (const auto &route : routing_.routes(node, cf)) {
                if (routing_.keys[route.event].isEmpty())
                    continue;
                if (!history.contains(route.event)) {
                    WorldProgress progress(routing_, memory);
                    progress.advanceTo(route.event);
                    history[route.event] = progress.flags;
                }
                int changes = 0;
                for (auto i = route.requirements.begin(); i != route.requirements.end(); ++i)
                    changes += history[route.event].value(i.key()) != i.value();
                auto id = qMakePair(node, route.event),
                     score = qMakePair(std::abs(cf - sourceCf), changes);
                if (!best.contains(id) || score < best[id].score)
                    best[id] = {score, route, cf};
            }
    for (const auto &b : best) {
        QStringList titles;
        for (int key : routing_.keys[b.route.event]) {
            auto title = labels.value(key, QString("Mission %1").arg(key));
            title.remove(QRegularExpression(" - (?:flagged key|runtime key|branch key).*$",
                                            QRegularExpression::CaseInsensitiveOption));
            if (!titles.contains(title))
                titles << title;
        }
        if (routing_.keys[b.route.event] == QVector<int>{20, 21})
            titles = {"The Grim Path — opening peace decision"};
        choices_ << SeedChoice{b.route, b.cf, routing_.nodes[b.route.node].name,
                               titles.join(" / ")};
    }
    auto rank = [&](const SeedChoice &c) {
        int n = 999;
        for (int k : routing_.keys[c.route.event])
            n = std::min(n, order.value(k, 999));
        return std::tuple(n, c.route.event, c.route.node);
    };
    std::sort(choices_.begin(), choices_.end(),
              [&](const auto &a, const auto &b) { return rank(a) < rank(b); });
}
QByteArray ScenarioSeeder::seed(int index, const SaveDocument *army) const {
    if (index < 0 || index >= choices_.size())
        throw Error("Choose a mission first");
    auto source = army ? withArmy(source_, *army) : source_;
    const auto &choice = choices_[index];
    const auto &route = choice.route;
    WorldProgress progress(routing_, source.memory());
    auto memory = progress.forEntry(route);
    auto flags = progress.flags;
    const bool first = route.node == 6 && route.event == 4;
    if (!first)
        flags[102] = 1;
    for (auto i = route.requirements.begin(); i != route.requirements.end(); ++i)
        flags[i.key()] = i.value();
    for (int i = 0; i < 32; ++i) {
        int v = 0;
        for (int b = 0; b < 8; ++b)
            v |= flags.value(i * 8 + b) << b;
        put8(memory, 0x193686 + i, quint8(v));
    }
    bool matches = false;
    for (const auto &r : routing_.routes(route.node, choice.chaos, flags))
        matches |= r.event == route.event;
    if (!matches)
        throw Error("The seeded story flags select a different mission");
    const int start = route.event == 57 ? 28 : first ? 7 : 6;
    put8(memory, 0x1936a8, 0);
    put8(memory, 0x1936a9, quint8(choice.chaos));
    put8(memory, 0x1936aa, quint8(start));
    put8(memory, 0x196a99, 0);
    put8(memory, 0x196a9a, quint8(start));
    const auto pos = routing_.nodes[start].position;
    const int x = std::clamp(pos.x() - 128, 0, 256), y = std::clamp(pos.y() - 96, 0, 124);
    put16(memory, 0x196a9c, quint16(x));
    put16(memory, 0x196a9e, quint16(y));
    put16(memory, 0x196aa0, quint16(pos.x() - x + 32));
    put16(memory, 0x196aa2, quint16(pos.y() - y + 24));
    source = patch(std::move(source), memory);
    int chapter = 0;
    for (int f = 1; f <= 4 && flags.value(f); ++f)
        ++chapter;
    int at = int(lc_save_record_offset(0, source.expanded_)),
        size = int(lc_save_record_size(0, source.expanded_));
    put8(source.data_, at + 0x10, quint8(chapter));
    put8(source.data_, at + 0x12, quint8(start));
    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(source.data_.data() + at), size, 0,
                             source.expanded_))
        throw Error("Cannot sign seeded save");
    put32(source.data_, lc_save_record_offset(15, source.expanded_), 0);
    return wordSwap(source.data_);
}
QString ScenarioSeeder::batteryPath(const Rom &rom, const QString &exe) {
    QFileInfo file(exe);
    if (!file.isFile() || file.fileName().compare("Project64.exe", Qt::CaseInsensitive))
        throw Error("Choose the folder containing Project64.exe");
    QSettings cfg(file.dir().filePath("Config/Project64.cfg"), QSettings::IniFormat);
    QString dir = "Save";
    if (cfg.value("Native Save Directory/Use Selected", 0).toInt() == 1)
        dir = cfg.value("Native Save Directory/Directory", "Save").toString();
    if (cfg.status() != QSettings::NoError)
        throw Error("Cannot read Project64 save directory settings");
    if (QDir::isRelativePath(dir))
        dir = file.dir().filePath(dir);
    auto name = QString::fromLatin1(rom.bytes().mid(0x20, 20));
    while (name.endsWith(' ') || name.endsWith(QChar(0)))
        name.chop(1);
    if (name.isEmpty() || name == "." || name == "..")
        throw Error("Invalid ROM save name");
    for (QChar c : name)
        if (c.unicode() < 32 || QString("<>:\"/\\|?*").contains(c))
            throw Error("Invalid ROM save name");
    const auto hash = QString::fromLatin1(
        QCryptographicHash::hash(wordSwap(rom.bytes()), QCryptographicHash::Md5).toHex().toUpper());
    return QDir(dir).absoluteFilePath(name + "-" + hash + "/" + name + ".sra");
}
bool ScenarioSeeder::project64Running() {
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        throw Error("Cannot check whether Project64 is running");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        throw Error("Cannot inspect running applications");
    }
    do {
        if (QString::fromWCharArray(entry.szExeFile)
                .compare("Project64.exe", Qt::CaseInsensitive) == 0) {
            running = true;
            break;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return running;
#else
    throw Error("Project64 save installation is currently supported on Windows");
#endif
}
QString ScenarioSeeder::install(const Rom &rom, const QByteArray &raw, const QString &destination,
                                bool mergeSeed) {
    if (QFileInfo(destination).suffix().compare("sra", Qt::CaseInsensitive))
        throw Error("Choose a Project64 .sra destination");
    auto source = SaveDocument::open(rom, raw);
    if (!source.battery())
        throw Error("Choose a battery save");
    if (!mergeSeed && !source.swapped_)
        throw Error("Choose a Project64 .sra save; native SaveRAM uses a different byte order");
    if (project64Running())
        throw Error("Close Project64 before installing a battery save");
    const bool exists = QFileInfo::exists(destination);
    const auto old = exists ? read(destination) : QByteArray();
    auto output = raw;
    if (mergeSeed) {
        source.selectSlot(0);
        put32(source.data_, lc_save_record_offset(15, source.expanded_), 0);
        const bool blank = old.size() == 0x8000 &&
                           (old == QByteArray(0x8000, 0) || old == QByteArray(0x8000, char(-1)));
        if (exists && !blank) {
            auto target = SaveDocument::open(rom, old);
            if (!target.battery())
                throw Error("The destination is not a battery save");
            if (source.expanded() || target.expanded()) {
                source = expanded(std::move(source));
                target = expanded(std::move(target));
            }
            const int at = int(lc_save_record_offset(0, source.expanded_)),
                      size = int(lc_save_record_size(0, source.expanded_));
            target.data_.replace(at, size, source.data_.mid(at, size));
            put32(target.data_, lc_save_record_offset(15, target.expanded_), 0);
            output = wordSwap(target.data_);
        } else
            output = wordSwap(source.data_);
    }
    auto verify = SaveDocument::open(rom, output);
    if (mergeSeed)
        verify.selectSlot(0);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
        throw Error("Cannot create the save directory");
    QString backup;
    if (exists) {
        backup = destination + ".before-seed-" +
                 QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzzZ") + "-" +
                 QUuid::createUuid().toString(QUuid::Id128) + ".bak";
        QFile f(backup);
        if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly) || f.write(old) != old.size() ||
            !f.flush())
            throw Error("Cannot back up the existing save");
        f.close();
        if (read(backup) != old)
            throw Error("Backup verification failed");
    }
    QSaveFile file(destination);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size())
        throw Error("Cannot prepare the battery save");
    if (project64Running() || QFileInfo::exists(destination) != exists ||
        (exists && read(destination) != old))
        throw Error("Destination changed while preparing the seed; close Project64 and try again");
    if (!file.commit() || read(destination) != output)
        throw Error("Battery save write verification failed");
    return backup;
}
} // namespace lordly
