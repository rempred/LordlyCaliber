#include "engine/leader_affinity.h"
#include <QJsonArray>
#include <cmath>

namespace lordly {
namespace {
constexpr int head = 0x5c, tail = 0x204;
// Live overlay placement verified by original callsites, not the boot mapping.
constexpr quint32 treeAddress = LeaderAffinity::address + head;
struct Lookup {
    QByteArray code;
    int table = 0;
};
Lookup lookup() {
    QVector<quint32> words;
    QMap<QString, int> labels;
    QVector<QPair<int, QString>> branches;
    auto instruction = [&](quint32 word) { words << word; };
    auto label = [&](const QString &name) { labels[name] = int(words.size()); };
    auto branch = [&](quint32 word, const QString &name) {
        branches << qMakePair(int(words.size()), name); instruction(word);
    };
    auto finish = [&] { instruction(0x08000000u | ((LeaderAffinity::address + tail) >> 2 & 0x03ffffff)); };
    instruction(0x8e04004c); // lw a0,0x4c(s0): leader's current class
    instruction(0x8e22004c); // lw v0,0x4c(s1): member's current class
    for (auto group : {std::array<int, 3>{0x45, 8, 0}, {0x4e, 3, 1}, {0x38, 13, 2}}) {
        instruction(0x24480000u | quint16(-group[0])); // addiu t0,v0,-first
        instruction(0x2d080000u | quint16(group[1]));  // sltiu t0,t0,count
        branch(0x15000000, "group");           // bne t0,zero,group
        instruction(0x24090000u | quint16(group[2]));  // delay: group column
    }
    instruction(0x2448ffcc); // member - 0x34
    instruction(0x2d080002); // Gremlin/Goblin
    branch(0x15000000, "group"); instruction(0x24090003);
    instruction(0x24080031); // Gorgon
    branch(0x10480000, "group"); instruction(0);
    instruction(0x24080037); // Ogre
    branch(0x10480000, "group"); instruction(0);
    label("zero"); finish(); instruction(0x00001021); // return zero through retail epilogue
    label("group");
    const int pointer = int(words.size());
    instruction(0); instruction(0); // lui/ori t2, table
    label("loop");
    instruction(0x91480000); // lbu t0,0(t2)
    branch(0x11000000, "zero"); instruction(0);
    branch(0x10880000, "found"); instruction(0);
    branch(0x10000000, "loop"); instruction(0x254a0005);
    label("found");
    instruction(0x01495021); // addu t2,t2,t1
    instruction(0x81420001); // lb v0,1(t2): signed strength
    finish(); instruction(0);
    Lookup result;
    result.table = int(words.size()) * 4 + 8;
    const quint32 tableAddress = treeAddress + result.table;
    words[pointer] = 0x3c0a0000u | (tableAddress >> 16);
    words[pointer + 1] = 0x354a0000u | (tableAddress & 0xffff);
    for (const auto &fix : branches)
        words[fix.first] |= quint16(labels[fix.second] - fix.first - 1);
    result.code = QByteArray(result.table, 0);
    for (int i = 0; i < words.size(); ++i) put32(result.code, i * 4, words[i]);
    result.code.replace(result.table - 8, 8, "LCAF0001");
    return result;
}
QByteArray tree(const QMap<int, LeaderAffinity::Bonuses> &rows) {
    const auto routine = lookup();
    if (rows.size() > LeaderAffinity::capacity())
        throw Error(QString("Leader affinity supports %1 classes with nonzero bonuses in its original code allocation.")
                        .arg(LeaderAffinity::capacity()));
    QByteArray result(tail - head, 0);
    result.replace(0, routine.code.size(), routine.code);
    int at = routine.table;
    for (auto row = rows.cbegin(); row != rows.cend(); ++row) {
        put8(result, at++, quint8(row.key()));
        for (int strength : row.value()) put8(result, at++, quint8(strength));
    }
    return result; // zero leader terminates; unused bytes stay zero
}
}
int LeaderAffinity::capacity() { return (tail - head - lookup().table) / 5 - 1; }
LeaderAffinity::Bonuses LeaderAffinity::retail(int leader) {
    switch (leader) {
    case 0x09: return {10, 10, 10, 10};
    case 0x0a: return {0, 10, 10, 10};
    case 0x0e: return {0, 0, 10, 10};
    case 0x18: case 0x5a: return {15, 10, 10, 10};
    case 0x19: case 0x7b: return {0, 15, 10, 10};
    case 0x1d: return {0, 0, 15, 10};
    case 0x2b: case 0x36: return {0, 0, 0, 10};
    case 0x74: return {0, 0, 0, 15};
    default: return {};
    }
}
QVector<int> LeaderAffinity::members(int group) {
    QVector<int> out;
    if (group == 3) return {0x31, 0x34, 0x35, 0x37};
    const int first = group == 0 ? 0x45 : group == 1 ? 0x4e : 0x38;
    const int count = group == 0 ? 8 : group == 1 ? 3 : group == 2 ? 13 : 0;
    for (int i = 0; i < count; ++i) out << first + i;
    return out;
}
QString LeaderAffinity::groupName(int group) {
    const QStringList names{"Beasts", "Golems", "Dragons", "Other monsters"};
    return names.value(group);
}
LeaderAffinity LeaderAffinity::load(const Rom &rom) {
    LeaderAffinity out;
    try {
        rom.requireSupported();
        if (rom.revision()) throw Error("Leader affinity requires US revision 0.");
        out.owner_ = rom.bytes().mid(offset, size);
        if (sha256(out.owner_) == "e28eed794d8c20a6b78428bd53da9f8633c3cf8588d82ca4a8eafd8e8dd9c890") {
            for (int id = 1; id <= 255; ++id)
                if (retail(id) != Bonuses{}) out.rows_[id] = retail(id);
        } else {
            const auto routine = lookup();
            if (out.owner_.size() != size ||
                sha256(out.owner_.first(head)) != "efcd7bfcb41a6ac9e3d7c7a3292e4433ed4244cd173644ce657a02de9dfbd6ed" ||
                sha256(out.owner_.mid(tail)) != "629af1e865bef8779081c052ec164b969b4a89cd836de4b9364a637e40ce92a8")
                throw Error("Unrecognized leader-affinity code; existing ROM bytes will be preserved.");
            int at = head + routine.table, previous = 0;
            while (at + 5 <= tail && u8(out.owner_, at)) {
                const int id = u8(out.owner_, at++);
                if (id <= previous) throw Error("Invalid leader-affinity table order.");
                Bonuses values;
                for (int &v : values) {
                    v = qint8(u8(out.owner_, at++));
                    if (v < -100 || v > 100) throw Error("Unsupported leader-affinity strength.");
                }
                if (values == Bonuses{}) throw Error("Invalid empty leader-affinity row.");
                out.rows_[id] = values; previous = id;
            }
            if (out.owner_.mid(head, tail - head) != tree(out.rows_))
                throw Error("Unrecognized leader-affinity lookup; existing ROM bytes will be preserved.");
        }
    } catch (const Error &e) {
        out.rows_.clear(); out.unavailable_ = QString::fromUtf8(e.what());
    }
    out.original_ = out.rows_;
    return out;
}
void LeaderAffinity::set(int leader, int group, int strength) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (leader < 1 || leader > 255 || group < 0 || group > 3 || strength < -100 || strength > 100)
        throw Error("Choose a class, an affinity group and a strength from -100 to 100.");
    auto next = rows_; auto values = bonuses(leader); values[group] = strength;
    if (values == Bonuses{}) next.remove(leader); else next[leader] = values;
    tree(next); rows_ = next;
}
QJsonObject LeaderAffinity::json() const {
    QJsonObject rows;
    for (auto it = rows_.cbegin(); it != rows_.cend(); ++it) {
        QJsonArray values; for (int v : it.value()) values << v;
        rows[QString::number(it.key())] = values;
    }
    return {{"version", 1}, {"rows", rows}};
}
void LeaderAffinity::apply(const QJsonObject &value) {
    if (value == json()) return;
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (value.size() != 2 || value["version"] != 1 || !value["rows"].isObject())
        throw Error("Invalid leader-affinity settings.");
    QMap<int, Bonuses> next;
    const auto rows = value["rows"].toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        bool valid = false; const int id = it.key().toInt(&valid);
        if (!valid || QString::number(id) != it.key() || id < 1 || id > 255 ||
            !it->isArray() || it->toArray().size() != 4)
            throw Error("Invalid leader-affinity class or group row.");
        Bonuses values; const auto array = it->toArray();
        for (int group = 0; group < 4; ++group) {
            const double v = array[group].toDouble(-999);
            if (!array[group].isDouble() || !std::isfinite(v) || std::floor(v) != v || v < -100 || v > 100)
                throw Error("Leader-affinity strength must be an integer from -100 to 100.");
            values[group] = int(v);
        }
        if (values != Bonuses{}) next[id] = values;
    }
    tree(next); rows_ = next;
}
QByteArray LeaderAffinity::image() const {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (!modified()) return owner_;
    auto result = owner_; result.replace(head, tail - head, tree(rows_)); return result;
}
QByteArray LeaderAffinity::assembly() const {
    const auto bytes = image(); QByteArray out(".set noreorder\n");
    for (int at = 0; at < bytes.size(); at += 4)
        out += ".word 0x" + QByteArray::number(u32(bytes, at), 16) + '\n';
    return out;
}
}
