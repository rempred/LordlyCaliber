#include "engine/project.h"
#include "engine/save_document.h"
#include "game/classes/save_format.h"
#include "game/army/soldier_reserve.h"
#include <array>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
using namespace lordly;
class SaveDocumentTests : public QObject {
    Q_OBJECT
  private slots:
    void soldierReserveLifecycle() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess process;
                process.start("node",
                              {QFINDTESTDATA("soldier-reserve-mips.cjs"), work.path(), mode});
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing Soldier fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 0x2a, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal Soldier fixture");
                }
                return out;
            };
            QJsonArray counts, creation, free, removal, saves;
            for (int maximum : {0, 1, 2, 3, 4, 299, 300, 301, 65535}) {
                const int third = maximum / 3;
                for (int hp : QSet<int>{0, 1, third, third + 1, third * 2, third * 2 + 1, maximum,
                                        std::min(65535, maximum + 1), 65535})
                    counts << QJsonObject{
                        {"maximum", maximum},
                        {"hp", hp},
                        {"count", int(lordly_soldier_count(quint16(hp), quint16(maximum)))}};
                for (int pool : {0, 1, 2, 3, 9, 255})
                    creation << QJsonObject{
                        {"maximum", maximum},
                        {"pool", pool},
                        {"remaining", pool - std::min(3, pool)},
                        {"hp", int(lordly_soldier_created_hp(quint16(maximum),
                                                             quint8(std::min(3, pool))))}};
                for (int id : {100, 219})
                    for (int hp : {0, 1, 100, 101, 200, 201, 65535})
                        removal << QJsonObject{
                            {"id", id},
                            {"maximum", maximum},
                            {"hp", hp},
                            {"pool", 9},
                            {"remaining",
                             9 + int(lordly_soldier_count(quint16(hp), quint16(maximum)))}};
            }
            for (int first : {0, 1, 119, 120}) {
                std::array<uint16_t, 120> hp;
                hp.fill(1);
                if (first < 120)
                    hp[first] = 0;
                QJsonArray values;
                for (auto value : hp)
                    values << int(value);
                const int id = lordly_soldier_free_id(hp.data());
                QCOMPARE(id, 100 + first);
                free << QJsonObject{{"hp", values}, {"id", id}};
            }
            auto capture = [&](const SaveDocument &doc) {
                const auto bytes = doc.serialize();
                // Compare original decoding of native and expanded payloads. Swapped
                // containers are covered by the complete-memory reopen checks.
                const auto at = lc_save_record_offset(doc.selectedSlot(), doc.expanded());
                saves << QJsonObject{
                    {"payload", QString::fromLatin1(bytes.mid(int(at) + 0x2a, 6180).toHex())},
                    {"soldiers", QString::fromLatin1(doc.memory().mid(0x190f80, 364).toHex())},
                    {"units", QString::fromLatin1(doc.memory().mid(0x1971f0, 750).toHex())}};
            };
            const auto battery = batteryFor("unit-payload.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (const auto &input : {battery, swapped, expanded}) {
                auto doc = SaveDocument::open(rom, input);
                const auto before = doc.memory();
                QCOMPARE(doc.reserveSoldiers(), 9);
                QCOMPARE(doc.soldiersInGroup(100), 1);
                const auto disband = doc.planToReserve(2, 1, classes);
                doc.apply(disband, true);
                auto expected = before;
                expected[0x1971f0 + 2 * 25 + 3] = 0;
                expected[0x1971f0 + 2 * 25 + 8] = 0;
                put16(expected, 0x190f84, 0);
                expected[0x191074] = 0;
                expected[0x190f81] = 10;
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                QCOMPARE(doc.units()[1].unity, 23);
                if (input != swapped)
                    capture(doc);
                const auto recreate = doc.planSoldierGroup(2, 7, classes);
                doc.apply(recreate, true);
                expected = before;
                put16(expected, 0x190f84, 300);
                expected[0x191074] = 0;
                expected[0x190f81] = 7;
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(doc.units()[1].unity, 23);
                QCOMPARE(doc.soldiersInGroup(100), 3);
                if (input != swapped)
                    capture(doc);
                doc.apply(recreate, false);
                doc.apply(disband, false);
                QCOMPARE(doc.serialize(), input);
                const auto create = doc.planSoldierGroup(0, 1, classes);
                doc.apply(create, true);
                expected = before;
                expected[0x1971f0 + 5] = 101;
                expected[0x1971f0 + 10] = 1;
                expected[0x190f81] = 6;
                put16(expected, 0x190f86, 300);
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                doc.apply(doc.planToReserve(0, 3, classes), true);
                QCOMPARE(doc.serialize(), input);
                doc.selectSlot(1);
                const auto other = doc.planSoldierGroup(0, 1, classes);
                doc.apply(other, true);
                QCOMPARE(doc.memory(), expected);
                doc.apply(other, false);
                QCOMPARE(doc.serialize(), input);
                doc.selectSlot(15);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planSoldierGroup(0, 1, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(2, 1, classes));
            }
            for (int pool : {1, 2}) {
                auto doc =
                    SaveDocument::open(rom, batteryFor(QString("soldier-pool-%1.bin").arg(pool)));
                const auto before = doc.memory();
                const auto created = doc.planSoldierGroup(0, 1, classes);
                doc.apply(created, true);
                auto expected = before;
                expected[0x1971f0 + 5] = 101;
                expected[0x1971f0 + 10] = 1;
                expected[0x190f81] = 0;
                put16(expected, 0x190f86, quint16(pool * 100));
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(doc.soldiersInGroup(101), pool);
                capture(doc);
                doc.apply(created, false);
                QCOMPARE(doc.memory(), before);
            }
            for (const auto &name : {"soldier-pool-0.bin", "soldier-zero-maximum.bin",
                                     "soldier-no-free.bin", "soldier-free-assigned.bin"}) {
                const auto doc = SaveDocument::open(rom, batteryFor(name));
                const auto before = doc.serialize();
                QVERIFY_THROWS_EXCEPTION(Error, doc.planSoldierGroup(0, 1, classes));
                QCOMPARE(doc.serialize(), before);
            }
            for (const auto &name : {"soldier-pool-255.bin", "soldier-duplicate.bin"}) {
                const auto doc = SaveDocument::open(rom, batteryFor(name));
                const auto before = doc.serialize();
                QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(2, 1, classes));
                QCOMPARE(doc.serialize(), before);
            }
            auto doc = SaveDocument::open(rom, battery);
            const auto before = doc.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, doc.planSoldierGroup(0, 0, classes));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planSoldierGroup(30, 1, classes));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(2, 0, classes));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(4, 1, classes));
            auto capacity = classes;
            auto fighter = capacity.entries()[2];
            fighter.record[ClassData::itemCapacityOffset] = 0;
            capacity.set(2, fighter);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(2, 1, capacity));
            QCOMPARE(doc.serialize(), before);
            auto size = classes;
            auto soldier = size.entries()[1];
            soldier.record[ClassData::unitSizeOffset] = 5;
            size.set(1, soldier);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planSoldierGroup(0, 1, size));
            auto full = SaveDocument::open(rom, batteryFor("unit-full-payload.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, full.planSoldierGroup(5, 6, classes));
            auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planSoldierGroup(0, 1, classes));
            QVERIFY_THROWS_EXCEPTION(Error, raw.planToReserve(2, 1, classes));
            QFile reference(work.filePath("soldier-verify.json"));
            QVERIFY(reference.open(QIODevice::WriteOnly));
            reference.write(QJsonDocument(QJsonObject{{"counts", counts},
                                                      {"free", free},
                                                      {"creation", creation},
                                                      {"removal", removal},
                                                      {"saves", saves}})
                                .toJson());
            reference.close();
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void squadSupplies() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess process;
                process.start("node", {QFINDTESTDATA("save-unit-mips.cjs"), work.path(), mode});
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &file) {
                QFile source(work.filePath(file));
                if (!source.open(QIODevice::ReadOnly))
                    throw Error("Missing squad fixture");
                const auto payload = source.readAll();
                QByteArray input(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    input[at + 12] = 0;
                    input.replace(at + 0x2a, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal squad fixture");
                }
                return input;
            };
            const auto battery = batteryFor("unit-payload.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            QJsonArray choices, cases;
            auto recordChoices = [&] {
                QByteArray records;
                for (const auto &record : project.records(Table::Consumables))
                    records += record.bytes;
                QJsonArray allowed;
                for (int id : SaveDocument::carryableItems(project))
                    allowed << id;
                choices << QJsonObject{{"records", QString::fromLatin1(records.toHex())},
                                       {"allowed", allowed}};
            };
            recordChoices();
            QCOMPARE(SaveDocument::carryableItems(project).size(), 28);
            for (const auto &input : {battery, swapped, expanded}) {
                auto doc = SaveDocument::open(rom, input);
                for (int slot : {0, 1}) {
                    doc.selectSlot(slot);
                    const auto before = doc.memory();
                    QCOMPARE(doc.unitCarryingCapacity(0, project.classData()), 6);
                    QCOMPARE(doc.maximumUnitSupply(0, 0), 31);
                    QCOMPARE(doc.maximumUnitSupply(0, 9), 31);
                    QCOMPARE(doc.unitCarryingCapacity(2, project.classData()), 3);
                    const QVector<int> values{1, 2, 0, 24, 31, 0, 7, 0, 20, 0};
                    const auto change = doc.planUnitSupplies(0, values, project);
                    doc.apply(change, true);
                    auto expected = before;
                    for (int i = 0; i < 10; ++i)
                        expected[0x1971f0 + 13 + i] = char(values[i]);
                    QCOMPARE(doc.memory(),
                             expected); // Includes every inventory/roster/unknown byte.
                    QCOMPARE(doc.units()[0].supplies, values);
                    auto reopened = SaveDocument::open(rom, doc.serialize());
                    reopened.selectSlot(slot);
                    QCOMPARE(reopened.memory(), expected);
                    QJsonArray items;
                    for (int id : values)
                        items << id;
                    // Always normalize through a retail container for the independent decoder.
                    auto reference = SaveDocument::open(rom, battery);
                    reference.apply(reference.planUnitSupplies(0, values, project), true);
                    cases << QJsonObject{
                        {"payload",
                         QString::fromLatin1(reference.serialize().mid(0x3a, 6180).toHex())},
                        {"items", items}};
                    doc.validateUnitSupplyEdits(project);
                    auto reducedProject = project;
                    auto small = reducedProject.record(Table::Classes, 2).bytes;
                    small[ClassData::itemCapacityOffset] = 1;
                    reducedProject.replaceBytes(Table::Classes, 2, small);
                    QVERIFY_THROWS_EXCEPTION(Error, doc.validateUnitSupplyEdits(reducedProject));
                    QCOMPARE(doc.planUnitSupplies(0, values, project).spans.size(), 0);
                    for (int invalid : {-1, 21, 22, 23, 32, 44, 45, 255, 256}) {
                        auto bad = values;
                        bad[0] = invalid;
                        QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(0, bad, project));
                    }
                    auto full = values;
                    full[2] = 1;
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(0, full, project));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(1, values, project));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(30, values, project));
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             doc.planUnitSupplies(0, QVector<int>(9), project));
                    QCOMPARE(doc.memory(), expected);
                    doc.apply(change, false);
                    QCOMPARE(doc.serialize(), input);
                    // Removing supplies allows an otherwise unsafe membership edit.
                    auto capacity =
                        SaveDocument::open(rom, batteryFor("unit-capacity-payload.bin"));
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             capacity.planToReserve(0, 1, project.classData()));
                    capacity.apply(capacity.planUnitSupplies(0, QVector<int>(10, 0), project),
                                   true);
                    capacity.apply(capacity.planToReserve(0, 1, project.classData()), true);
                    QVERIFY(capacity.reserveMembers().contains(2));
                }
                doc.selectSlot(15);
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitSupplies(0, QVector<int>(10, 0), project));
            }
            auto unknown = SaveDocument::open(rom, batteryFor("unit-unknown-supply-payload.bin"));
            auto unchanged = unknown.units()[0].supplies;
            QCOMPARE(unchanged[0], 21);
            QCOMPARE(unknown.planUnitSupplies(0, unchanged, project).spans.size(), 0);
            unchanged[2] = 1;
            unknown.apply(unknown.planUnitSupplies(0, unchanged, project), true);
            QCOMPARE(unknown.units()[0].supplies[0], 21);
            auto doc = SaveDocument::open(rom, battery);
            auto current = doc.units()[0].supplies;
            current[1] = 21;
            QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(0, current, project));
            auto metadata = project.record(Table::Consumables, 21).bytes;
            metadata[5] = 0;
            project.replaceBytes(Table::Consumables, 21, metadata);
            recordChoices();
            QVERIFY(SaveDocument::carryableItems(project).contains(21));
            doc.apply(doc.planUnitSupplies(0, current, project), true);
            QCOMPARE(doc.units()[0].supplies[1], 21);
            doc.validateUnitSupplyEdits(project);
            metadata[5] = 1;
            project.replaceBytes(Table::Consumables, 21, metadata);
            doc.selectSlot(1); // Export must also validate edits in another slot.
            QVERIFY_THROWS_EXCEPTION(Error, doc.validateUnitSupplyEdits(project));
            metadata[5] = 0;
            project.replaceBytes(Table::Consumables, 21, metadata);
            doc.validateUnitSupplyEdits(project);
            doc.selectSlot(0);
            metadata = project.record(Table::Consumables, 32).bytes;
            metadata[5] = 0;
            project.replaceBytes(Table::Consumables, 32, metadata);
            QVERIFY(SaveDocument::carryableItems(project).contains(32));
            current[1] = 32;
            QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(0, current, project));
            auto ram = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, ram.planUnitSupplies(0, QVector<int>(10), project));
            QFile fixture(work.filePath("supply-verify.json"));
            QVERIFY(fixture.open(QIODevice::WriteOnly));
            fixture.write(
                QJsonDocument(QJsonObject{{"choices", choices}, {"cases", cases}}).toJson());
            fixture.close();
            run("verify-supplies");
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void squadFormation() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](bool verify) {
                QProcess process;
                QStringList args{QFINDTESTDATA("save-unit-mips.cjs"), work.path()};
                if (verify)
                    args << "verify";
                process.start("node", args);
                QVERIFY(process.waitForFinished(30000));
                QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
                qInfo().noquote() << process.readAllStandardOutput();
            };
            auto read = [&](const QString &file) {
                QFile f(work.filePath(file));
                if (!f.open(QIODevice::ReadOnly))
                    throw Error("Missing squad fixture");
                return f.readAll();
            };
            run(false);
            QByteArray battery(0x10000, char(0x6d));
            for (int slot : {0, 1, 15}) {
                const int at = int(lc_save_record_offset(slot, 0));
                battery[at + 12] = 0;
                battery.replace(at + 0x2a, 6180, read("unit-payload.bin"));
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(battery.data() + at),
                                            lc_save_record_size(slot, 0), slot, 0));
            }
            auto swapped = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            auto expanded = battery;
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (const auto &input : {battery, swapped, expanded}) {
                auto doc = SaveDocument::open(rom, input);
                QCOMPARE(doc.units().size(), 5);
                QCOMPARE(doc.units()[0].index, 0);
                QCOMPARE(doc.units()[1].index, 2);
                QCOMPARE(doc.units()[1].members[1], 100);
                QCOMPARE(doc.units()[0].cells, QVector<int>({0, 4, 8, 0, 0}));
                const auto before = doc.memory();
                const auto swap = doc.planUnitSwap(0, 1, 5, 1, classes);
                doc.apply(swap, true);
                auto swappedMemory = before;
                swappedMemory[0x1971f3] = 8;
                swappedMemory[0x1971f0 + 5 * 25 + 3] = 2;
                swappedMemory[0x1971fc] = 0;
                swappedMemory[0x1971f0 + 5 * 25 + 12] = 0;
                QCOMPARE(doc.memory(), swappedMemory);
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), swappedMemory);
                doc.apply(swap, false);
                QCOMPARE(doc.serialize(), input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 0, 5, 1, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 0, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 5, 4, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 2, 1, classes));
                auto noCapacity = classes;
                auto fighter = noCapacity.entries()[2];
                fighter.record[ClassData::itemCapacityOffset] = 0;
                noCapacity.set(2, fighter);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 5, 1, noCapacity));
                QCOMPARE(doc.memory(), before);
                const auto edit = doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 3, classes);
                doc.apply(edit, true);
                auto expected = before;
                expected[0x1971f7] = 1;
                expected[0x1971f8] = 2;
                expected[0x1971fc] = 99;
                expected[0x197207] = 3;
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 1, 8, 0, 0}, 99, 3, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 9, 8, 0, 0}, 99, 3, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 2, 8, 1, 0}, 99, 3, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 101, 3, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 4, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(4, {2, 7, 0, 0, 0}, 23, 2, classes));
                QCOMPARE(doc.memory(), expected);
                doc.selectSlot(1);
                QCOMPARE(doc.memory(), before);
                doc.selectSlot(15);
                QVERIFY(!doc.canEditUnits());
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 5, 1, classes));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 3, classes));
                doc.apply(edit, false);
                QCOMPARE(doc.serialize(), input);
            }
            auto raw = SaveDocument::open(rom, read("unit-ram.bin"));
            QVERIFY(!raw.canEditUnits());
            QCOMPARE(raw.units().size(), 5);
            QVERIFY_THROWS_EXCEPTION(Error, raw.planUnitSwap(0, 1, 5, 1, classes));
            QVERIFY_THROWS_EXCEPTION(Error, raw.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 3, classes));
            auto doc = SaveDocument::open(rom, battery);
            for (int index : {2, 3}) {
                const auto before = doc.memory();
                const auto edit = doc.planUnitEdit(index, {0, 8, 0, 0, 0}, 23, 2, classes);
                doc.apply(edit, true);
                auto expected = before;
                expected[0x1971f0 + index * 25 + 7] = 0;
                expected[0x1971f0 + index * 25 + 8] = 8;
                QCOMPARE(doc.memory(), expected); // Includes all Soldier HP and companion bytes.
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                doc.apply(edit, false);
                QCOMPARE(doc.memory(), before);
            }
            int large = -1;
            for (int id = 1; id < classes.entries().size(); ++id)
                if (classes.unitSize(id, id) == 2) {
                    large = id;
                    break;
                }
            QVERIFY(large > 0);
            auto larger = doc;
            larger.apply(larger.planEdit(SaveTable::Characters, 3, "class", large), true);
            larger.apply(larger.planEdit(SaveTable::Characters, 3, "secondaryClass", large), true);
            QVERIFY_THROWS_EXCEPTION(Error,
                                     larger.planUnitEdit(0, {1, 5, 8, 0, 0}, 23, 2, classes));
            // Corner8 is two rows from cells0/2, so its neighboring cells stay clear.
            larger.apply(larger.planUnitEdit(0, {0, 2, 8, 0, 0}, 23, 2, classes), true);
            for (int id : {1, 2}) {
                larger.apply(larger.planEdit(SaveTable::Characters, id, "class", large), true);
                larger.apply(larger.planEdit(SaveTable::Characters, id, "secondaryClass", large),
                             true);
            }
            QVERIFY_THROWS_EXCEPTION(Error,
                                     larger.planUnitEdit(0, {0, 6, 8, 0, 0}, 23, 2, classes));
            QJsonArray sizes;
            for (int primary = 0; primary < classes.entries().size(); ++primary)
                for (int secondary = 0; secondary < classes.entries().size(); ++secondary)
                    sizes << QJsonObject{{"primary", primary},
                                         {"secondary", secondary},
                                         {"expected", classes.unitSize(primary, secondary)},
                                         {"capacity", classes.carryingCapacity(primary, secondary)},
                                         {"movement", classes.movementType(primary, secondary)}};
            auto movementClasses = classes;
            for (int type = 0; type < 8; ++type) {
                auto asset = movementClasses.entries()[type + 1];
                asset.record[ClassData::movementTypeOffset] = char(type);
                movementClasses.set(type + 1, asset);
            }
            QJsonArray movementCases;
            for (int bits = 0; bits < 32768; ++bits) {
                QVector<QPair<int, int>> pairs;
                for (int i = 0; i < 5; ++i) {
                    const int id = ((bits >> (i * 3)) & 7) + 1;
                    pairs << qMakePair(id, int(u8(movementClasses.entries()[id].record, 69)));
                }
                movementCases << QJsonArray{
                    bits, movementClasses.unitMovementType(pairs, rom.bytes().mid(0x5cebc, 7))};
            }
            doc.apply(doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 3, classes), true);
            for (int index : {2, 3})
                doc.apply(doc.planUnitEdit(index, {0, 8, 0, 0, 0}, 23, 2, classes), true);
            QJsonArray cases;
            for (int secondId : {2, 100, 219})
                for (int leader = 0; leader < 9; ++leader)
                    for (int member = 0; member < 9; ++member) {
                        if (leader == member)
                            continue;
                        QByteArray record(25, 0);
                        put8(record, 2, 1);
                        put8(record, 3, quint8(secondId));
                        put8(record, 7, quint8(leader));
                        put8(record, 8, quint8(member));
                        QJsonArray grid;
                        for (int cell = 0; cell < 9; ++cell)
                            grid << (cell == leader ? 1 : cell == member ? secondId : 255);
                        cases << QJsonObject{{"record", QString::fromLatin1(record.toHex())},
                                             {"leaderCell", leader},
                                             {"grid", grid}};
                    }
            cases << QJsonObject{
                {"record", QString::fromLatin1(doc.memory().mid(0x1971f0, 25).toHex())},
                {"leaderCell", 1},
                {"grid", QJsonArray{255, 1, 2, 255, 255, 255, 255, 255, 3}}};
            doc.apply(doc.planUnitSwap(0, 1, 5, 1, classes), true);
            atomicWrite(
                work.filePath("unit-verify.json"),
                QJsonDocument(
                    QJsonObject{
                        {"payload", QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                        {"units", QString::fromLatin1(doc.memory().mid(0x1971f0, 150).toHex())},
                        {"cases", cases},
                        {"soldierSize", std::max(1, classes.unitSize(1, 1))},
                        {"sizes", sizes},
                        {"movementCases", movementCases},
                        {"unitMovements", QJsonArray{QJsonObject{{"index", 0}, {"expected", 2}},
                                                     QJsonObject{{"index", 5}, {"expected", 2}}}}})
                    .toJson());
            run(true);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void reserveMembership() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](bool verify) {
                QProcess process;
                QStringList args{QFINDTESTDATA("save-unit-mips.cjs"), work.path()};
                if (verify)
                    args << "verify";
                process.start("node", args);
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run(false);
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing squad fixture");
                const auto payload = file.readAll();
                QByteArray result(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    result[at + 12] = 0;
                    result.replace(at + 0x2a, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(result.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Could not seal reserve fixture");
                }
                return result;
            };
            const auto battery = batteryFor("unit-payload.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            QJsonArray reserveChecks;
            for (const auto &input : {battery, swapped, expanded}) {
                auto doc = SaveDocument::open(rom, input);
                const auto before = doc.memory();
                QVERIFY(doc.reserveMembers().isEmpty());
                const auto remove = doc.planToReserve(0, 1, classes);
                doc.apply(remove, true);
                auto expected = before;
                for (int offset : {3, 8, 12})
                    expected[0x1971f0 + offset] = 0;
                QCOMPARE(doc.memory(), expected);
                QCOMPARE(doc.reserveMembers(), QVector<int>{2});
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                if (input == battery)
                    reserveChecks << QJsonObject{
                        {"payload", QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                        {"members", QJsonArray{2}}};
                const auto reserveBytes = doc.serialize();
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(2, 5, 4, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(2, 1, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(2, 5, 9, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(1, 5, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(100, 5, 2, classes));
                QCOMPARE(doc.serialize(), reserveBytes);
                const auto add = doc.planFromReserve(2, 5, 2, classes);
                doc.apply(add, true);
                const int dest = 0x1971f0 + 5 * 25;
                expected[dest + 5] = 2;
                expected[dest + 10] = 2;
                expected[dest + 12] = 0;
                QCOMPARE(doc.memory(), expected);
                QVERIFY(doc.reserveMembers().isEmpty());
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                if (input == battery) {
                    reserveChecks << QJsonObject{
                        {"payload", QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                        {"members", QJsonArray{}}};
                    atomicWrite(
                        work.filePath("unit-verify.json"),
                        QJsonDocument(
                            QJsonObject{{"payload", QString::fromLatin1(
                                                        doc.serialize().mid(0x3a, 6180).toHex())},
                                        {"units", QString::fromLatin1(
                                                      doc.memory().mid(0x1971f0, 150).toHex())},
                                        {"sizes", QJsonArray{}},
                                        {"cases", QJsonArray{}},
                                        {"soldierSize", std::max(1, classes.unitSize(1, 1))},
                                        {"reserveChecks", reserveChecks}})
                            .toJson());
                    run(true);
                }
                doc.apply(add, false);
                QCOMPARE(doc.serialize(), reserveBytes);
                doc.apply(remove, false);
                QCOMPARE(doc.serialize(), input);
                for (const auto pair : {qMakePair(0, 0), qMakePair(0, 4),
                                        qMakePair(4, 1), qMakePair(30, 1)})
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             doc.planToReserve(pair.first, pair.second, classes));
                auto changedClasses = classes;
                auto fighter = classes.entries()[2];
                fighter.record[ClassData::movementTypeOffset] = 6;
                changedClasses.set(2, fighter);
                const auto changed = doc.planToReserve(0, 1, changedClasses);
                doc.apply(changed, true);
                QCOMPARE(u8(doc.memory(), 0x1971f0 + 24), quint8(6));
                doc.apply(changed, false);
                QCOMPARE(doc.serialize(), input);
                doc.selectSlot(1);
                const auto second = doc.planToReserve(0, 1, classes);
                doc.apply(second, true);
                QCOMPARE(doc.reserveMembers(), QVector<int>{2});
                doc.apply(second, false);
                doc.selectSlot(15);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(0, 1, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planFromReserve(2, 5, 2, classes));
                QVERIFY(doc.reserveMembers().isEmpty());
            }
            auto capacity = SaveDocument::open(rom, batteryFor("unit-capacity-payload.bin"));
            const auto beforeCapacity = capacity.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, capacity.planToReserve(0, 1, classes));
            QCOMPARE(capacity.serialize(), beforeCapacity);
            auto full = SaveDocument::open(rom, batteryFor("unit-full-payload.bin"));
            full.apply(full.planToReserve(2, 2, classes), true);
            QVERIFY(full.reserveMembers().contains(10));
            const auto beforeFull = full.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, full.planFromReserve(10, 5, 6, classes));
            QCOMPARE(full.serialize(), beforeFull);
            auto missing =
                SaveDocument::open(rom, batteryFor("unit-missing-character-payload.bin"));
            const auto beforeMissing = missing.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, missing.planToReserve(5, 3, classes));
            QCOMPARE(missing.serialize(), beforeMissing);
            auto raw = SaveDocument::open(rom, SaveDocument::open(rom, battery).memory());
            QVERIFY(raw.reserveMembers().isEmpty());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planToReserve(0, 1, classes));
            QVERIFY_THROWS_EXCEPTION(Error, raw.planFromReserve(2, 5, 2, classes));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void squadTransfers() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](bool verify) {
                QProcess process;
                QStringList args{QFINDTESTDATA("save-unit-mips.cjs"), work.path()};
                if (verify)
                    args << "verify";
                process.start("node", args);
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            auto read = [&](const QString &name) {
                QFile f(work.filePath(name));
                if (!f.open(QIODevice::ReadOnly))
                    throw Error("Missing squad fixture");
                return f.readAll();
            };
            run(false);
            auto batteryFor = [&](const QString &name) {
                QByteArray result(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    result[at + 12] = 0;
                    result.replace(at + 0x2a, 6180, read(name));
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(result.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Could not seal squad fixture");
                }
                return result;
            };
            const auto battery = batteryFor("unit-payload.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            auto verifyNative = [&](const SaveDocument &doc, const QJsonArray &unitMovements) {
                atomicWrite(
                    work.filePath("unit-verify.json"),
                    QJsonDocument(
                        QJsonObject{
                            {"payload",
                             QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                            {"units", QString::fromLatin1(doc.memory().mid(0x1971f0, 150).toHex())},
                            {"sizes", QJsonArray{}},
                            {"cases", QJsonArray{}},
                            {"soldierSize", std::max(1, classes.unitSize(1, 1))},
                            {"unitMovements", unitMovements}})
                        .toJson());
                run(true);
            };
            for (const auto &input : {battery, swapped, expanded}) {
                auto doc = SaveDocument::open(rom, input);
                const auto before = doc.memory();
                const auto move = doc.planUnitTransfer(0, 1, 5, 2, classes);
                doc.apply(move, true);
                auto expected = before;
                for (int offset : {3, 8, 12})
                    expected[0x1971f0 + offset] = 0;
                const int dest = 0x1971f0 + 5 * 25;
                expected[dest + 5] = 2;
                expected[dest + 10] = 2;
                expected[dest + 12] = 0;
                QCOMPARE(doc.memory(),
                         expected); // Every other decoded byte, including all Soldier data.
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), expected);
                if (input == battery)
                    verifyNative(doc, {QJsonObject{{"index", 0}, {"expected", 2}},
                                       QJsonObject{{"index", 5}, {"expected", 2}}});
                const auto exported = doc.serialize();
                doc.apply(move, false);
                QCOMPARE(doc.serialize(), input);
                doc.apply(move, true);
                QCOMPARE(doc.serialize(), exported);
                doc.apply(move, false);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 0, 5, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 0, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 5, 4, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 5, 9, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 1, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 4, 5, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(2, 1, 5, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(4, 1, 5, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 4, 2, classes));
                QCOMPARE(doc.serialize(), input);
                auto changedClasses = classes;
                auto fighter = changedClasses.entries()[2];
                fighter.record[ClassData::movementTypeOffset] = 6;
                changedClasses.set(2, fighter);
                const auto swap = doc.planUnitSwap(0, 1, 5, 1, changedClasses);
                doc.apply(swap, true);
                auto swapExpected = before;
                swapExpected[0x1971f3] = 8;
                swapExpected[dest + 3] = 2;
                swapExpected[0x1971fc] = 0;
                swapExpected[dest + 12] = 0;
                swapExpected[0x1971f0 + 24] = 6;
                swapExpected[dest + 24] = 6;
                QCOMPARE(doc.memory(), swapExpected);
                doc.apply(swap, false);
                doc.selectSlot(1);
                QCOMPARE(doc.memory(), before);
                doc.selectSlot(15);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 5, 2, classes));
                QCOMPARE(doc.serialize(), input);
            }
            auto raw = SaveDocument::open(rom, read("unit-ram.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, raw.planUnitTransfer(0, 1, 5, 2, classes));
            auto capacity = SaveDocument::open(rom, batteryFor("unit-capacity-payload.bin"));
            const auto capacityBefore = capacity.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, capacity.planUnitTransfer(0, 1, 5, 2, classes));
            QCOMPARE(capacity.serialize(), capacityBefore);
            auto full = SaveDocument::open(rom, batteryFor("unit-full-payload.bin"));
            const auto fullBefore = full.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, full.planUnitTransfer(2, 2, 5, 3, classes));
            QCOMPARE(full.serialize(), fullBefore);
            auto soldiers = SaveDocument::open(rom, batteryFor("unit-soldier-base-payload.bin"));
            const auto soldierBefore = soldiers.serialize();
            const auto into = soldiers.planUnitTransfer(0, 1, 2, 0, classes);
            soldiers.apply(into, true);
            const auto out = soldiers.planUnitTransfer(2, 2, 0, 4, classes);
            soldiers.apply(out, true);
            QCOMPARE(u8(soldiers.memory(), 0x1971f0 + 2 * 25 + 24), quint8(7));
            // Capacity uses Soldier pair1/1 even though its saved base record is pair44/44.
            QCOMPARE(soldiers.units()[1].members, QVector<int>({4, 100, 0, 0, 0}));
            QCOMPARE(soldiers.memory().mid(0x1971f0 + 2 * 25 + 13, 3), QByteArray(3, char(9)));
            verifyNative(
                soldiers,
                {QJsonObject{{"index", 2}, {"expected", 7}, {"carried", 3}, {"capacity", 3}}});
            QCOMPARE(SaveDocument::open(rom, soldiers.serialize()).memory(), soldiers.memory());
            soldiers.apply(out, false);
            soldiers.apply(into, false);
            QCOMPARE(soldiers.serialize(), soldierBefore);
            auto overloaded =
                SaveDocument::open(rom, batteryFor("unit-soldier-overload-payload.bin"));
            overloaded.apply(overloaded.planUnitTransfer(0, 1, 2, 0, classes), true);
            const auto overloadBefore = overloaded.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, overloaded.planUnitTransfer(2, 2, 0, 4, classes));
            QCOMPARE(overloaded.serialize(), overloadBefore);
            auto larger = SaveDocument::open(rom, battery);
            int large = -1;
            for (int id = 1; id < classes.entries().size(); ++id)
                if (classes.unitSize(id, id) == 2) {
                    large = id;
                    break;
                }
            QVERIFY(large > 0);
            larger.apply(larger.planEdit(SaveTable::Characters, 2, "class", large), true);
            larger.apply(larger.planEdit(SaveTable::Characters, 2, "secondaryClass", large), true);
            const auto largeBefore = larger.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, larger.planUnitTransfer(0, 1, 5, 1, classes));
            QCOMPARE(larger.serialize(), largeBefore);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void editsPreserveNativeSaves() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            auto run = [&](bool verify) {
                QProcess process;
                QStringList args{QFINDTESTDATA("save-document-mips.cjs"), work.path()};
                if (verify)
                    args << "verify";
                process.start("node", args);
                if (!process.waitForFinished(60000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            auto read = [&](const QString &file) {
                QFile f(work.filePath(file));
                if (!f.open(QIODevice::ReadOnly))
                    throw Error("Fixture unavailable");
                return f.readAll();
            };
            run(false);
            const auto payload = read("payload.bin");
            QCOMPARE(payload.size(), 6180);
            QByteArray battery(0x10000, char(0x6d));
            for (int slot : {0, 1, 15}) {
                const int at = int(lc_save_record_offset(slot, 0)),
                          size = int(lc_save_record_size(slot, 0));
                battery[at + 12] = 0;
                battery.replace(at + 0x2a, payload.size(), payload);
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(battery.data() + at),
                                            size, slot, 0));
            }
            auto swapped = [](QByteArray bytes) {
                for (int i = 0; i < bytes.size(); i += 4) {
                    std::swap(bytes[i], bytes[i + 3]);
                    std::swap(bytes[i + 1], bytes[i + 2]);
                }
                return bytes;
            };
            QByteArray expanded = battery;
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (const auto &input : {battery, swapped(battery.first(0x8000)), expanded}) {
                auto doc = SaveDocument::open(rom, input, 256);
                QCOMPARE(doc.serialize(), input);
                QCOMPARE(doc.saveSlots().size(), 3);
                QCOMPARE(doc.rows(SaveTable::Characters).size(), 99);
                QCOMPARE(doc.value(SaveTable::Characters, 1, "hpMax").toUInt(), 999U);
                QCOMPARE(doc.value(SaveTable::Characters, 1, "weapon").toUInt(), 277U);
                QCOMPARE(doc.value(SaveTable::Characters, 1, "secondaryClass").toUInt(), 163U);
                const auto a = doc.planEdit(SaveTable::Characters, 1, "class", 255);
                doc.apply(a, true);
                doc.validateClassEdits(256);
                QVERIFY_THROWS_EXCEPTION(Error, doc.validateClassEdits(165));
                QCOMPARE(doc.value(SaveTable::Characters, 1, "secondaryClass").toUInt(), 163U);
                const auto b = doc.planEdit(SaveTable::Characters, 1, "hpCur", 0);
                doc.apply(b, true);
                const auto c = doc.planEdit(SaveTable::Characters, 99, "name", "Final Knight");
                doc.apply(c, true);
                doc.selectSlot(1);
                const auto funds = doc.planEdit(SaveTable::Game, 0, "goth", 2147483647U);
                doc.apply(funds, true);
                doc.selectSlot(15);
                const auto suspend = doc.planEdit(SaveTable::Characters, 2, "weapon", 256);
                doc.apply(suspend, true);
                auto reopened = SaveDocument::open(rom, doc.serialize(), 256);
                QCOMPARE(reopened.value(SaveTable::Characters, 1, "hpCur").toUInt(), 0U);
                QCOMPARE(reopened.value(SaveTable::Characters, 1, "class").toUInt(), 255U);
                QCOMPARE(reopened.value(SaveTable::Characters, 99, "name").toString(),
                         QString("Final Knight"));
                reopened.selectSlot(1);
                QCOMPARE(reopened.value(SaveTable::Game, 0, "goth").toUInt(), 2147483647U);
                reopened.selectSlot(15);
                QCOMPARE(reopened.value(SaveTable::Characters, 2, "weapon").toUInt(), 256U);
                // Exact backward patches preserve tokens, unrelated slots, padding and unused bits.
                for (const auto &change : {suspend, funds, c, b, a})
                    doc.apply(change, false);
                QCOMPARE(doc.serialize(), input);
                doc.apply(a, true);
                QVERIFY_THROWS_EXCEPTION(Error, doc.apply(a, true));
                const auto snapshot = doc.serialize();
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planEdit(SaveTable::Characters, 1, "element", 5));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planEdit(SaveTable::Characters, 1, "hpCur", 1000));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planEdit(SaveTable::Characters, 1, "hpMax", 1));
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planEdit(SaveTable::Characters, 1, "weapon", 278));
                QVERIFY_THROWS_EXCEPTION(
                    Error, doc.planEdit(SaveTable::Characters, 1, "name", "ThirteenChars"));
                QCOMPARE(doc.serialize(), snapshot);
            }
            auto doc = SaveDocument::open(rom, battery, 256);
            // Inventory edits keep existing identities and unknown bytes intact.
            auto inventory = doc;
            const auto inventoryBytes = inventory.serialize();
            QVERIFY_THROWS_EXCEPTION(Error,
                                     inventory.planEdit(SaveTable::Equipment, 1, "owned", 2));
            QVERIFY_THROWS_EXCEPTION(Error, inventory.planEdit(SaveTable::Equipment, 1, "id", 277));
            const auto add = inventory.planEdit(SaveTable::Equipment, 1, "id", 256);
            inventory.apply(add, true);
            const auto quantity = inventory.planEdit(SaveTable::Equipment, 1, "owned", 100);
            inventory.apply(quantity, true);
            const auto zeroCount = inventory.planEdit(SaveTable::Consumables, 0, "count", 0);
            inventory.apply(zeroCount, true);
            auto inventoryOpen = SaveDocument::open(rom, inventory.serialize(), 256);
            QCOMPARE(inventoryOpen.value(SaveTable::Equipment, 1, "id").toUInt(), 256U);
            QCOMPARE(inventoryOpen.value(SaveTable::Equipment, 1, "owned").toUInt(), 100U);
            QCOMPARE(inventoryOpen.value(SaveTable::Consumables, 0, "count").toUInt(), 0U);
            for (const auto &change : {zeroCount, quantity, add})
                inventory.apply(change, false);
            QCOMPARE(inventory.serialize(), inventoryBytes);
            QJsonArray expected;
            for (const auto &[key, n, off, width] :
                 {std::tuple{"class", 255, 17, 1}, std::tuple{"hpCur", 0, 24, 2},
                  std::tuple{"hpMax", 999, 22, 2}, std::tuple{"weapon", 256, 42, 2},
                  std::tuple{"strength", 900, 28, 2}, std::tuple{"element", 3, 26, 1}}) {
                doc.apply(doc.planEdit(SaveTable::Characters, 1, key, n), true);
                expected.append(
                    QJsonObject{{"address", 0x193bf8 + off}, {"bytes", width}, {"value", n}});
            }
            expected.append(QJsonObject{{"address", 0x193bf8 + 18}, {"bytes", 1}, {"value", 163}});
            QJsonArray dates;
            auto date = doc;
            for (int year : {0, 51, 52, 152, 251, 252, 352, 998})
                for (int month = 1; month <= 15; ++month) {
                    date.apply(date.planEdit(SaveTable::Game, 0, "day", 1), true);
                    date.apply(date.planEdit(SaveTable::Game, 0, "year", year), true);
                    date.apply(date.planEdit(SaveTable::Game, 0, "month", month), true);
                    const int maximum = int(date.fields(SaveTable::Game).last().maximum);
                    dates.append(QJsonObject{{"year", year}, {"month", month}, {"days", maximum}});
                    date.apply(date.planEdit(SaveTable::Game, 0, "day", maximum), true);
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             date.planEdit(SaveTable::Game, 0, "day", maximum + 1));
                    auto reopenedDate = SaveDocument::open(rom, date.serialize(), 256);
                    QCOMPARE(reopenedDate.value(SaveTable::Game, 0, "day").toInt(), maximum);
                }
            QVERIFY_THROWS_EXCEPTION(Error, date.planEdit(SaveTable::Game, 0, "month", 3));
            for (const auto &[key, n, at, width] :
                 {std::tuple{"year", 252, 0x196a30, 2}, std::tuple{"month", 3, 0x196a32, 1},
                  std::tuple{"day", 24, 0x196a33, 1}}) {
                doc.apply(doc.planEdit(SaveTable::Game, 0, key, n), true);
                expected.append(QJsonObject{{"address", at}, {"bytes", width}, {"value", n}});
            }
            const auto edited = doc.serialize();
            QCOMPARE(edited.mid(0x1860), battery.mid(0x1860));
            QCOMPARE(quint8(edited[0x10 + 0x2a + 6179]) & 127, quint8(payload[6179]) & 127);
            atomicWrite(work.filePath("verify.json"),
                        QJsonDocument(QJsonObject{{"payload", QString::fromLatin1(
                                                                  edited.mid(0x3a, 6180).toHex())},
                                                  {"values", expected},
                                                  {"dates", dates}})
                            .toJson());
            for (const auto &filename : {"ram.bin", "raw.state", "original.state"}) {
                const auto input = read(filename);
                auto state = SaveDocument::open(rom, input, 256);
                QCOMPARE(state.serialize(), input);
                QCOMPARE(state.value(SaveTable::Characters, 1, "name").toString(),
                         QString("SixteenCharName!!"));
                const auto before = state.memory();
                const auto zero = state.planEdit(SaveTable::Characters, 1, "hpCur", 0);
                state.apply(zero, true);
                auto memory = state.memory();
                memory.replace(0x193bf8 + 24, 2, before.mid(0x193bf8 + 24, 2));
                QCOMPARE(memory, before);
                auto open = SaveDocument::open(rom, state.serialize(), 256);
                QCOMPARE(open.value(SaveTable::Characters, 1, "hpCur").toUInt(), 0U);
                if (QString(filename) == "original.state")
                    atomicWrite(work.filePath("edited.state"), state.serialize());
                state.apply(zero, false);
                QCOMPARE(state.serialize(), input);
            }
            run(true);
            auto broken = battery;
            broken[0x3a] ^= 1;
            auto partial = SaveDocument::open(rom, broken);
            QVERIFY(!partial.saveSlots()[0].valid);
            QCOMPARE(partial.selectedSlot(), 1);
            for (int slot : {1, 15})
                broken[int(lc_save_record_offset(slot, 0)) + 0x2a] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, SaveDocument::open(rom, broken));
            auto rzip = read("original.state");
            QVERIFY_THROWS_EXCEPTION(Error, SaveDocument::open(rom, rzip.chopped(1)));
            QVERIFY_THROWS_EXCEPTION(Error, SaveDocument::open(rom, rzip + QByteArray("x")));
            qToLittleEndian(quint32(1),
                            rzip.data() +
                                8); // Bounded output must reject expansion beyond declared chunk.
            QVERIFY_THROWS_EXCEPTION(Error, SaveDocument::open(rom, rzip));
            const auto goldenPath = QFileInfo(path).dir().filePath("tools/pj64_golden.sra");
            QFile golden(goldenPath);
            QVERIFY(golden.open(QIODevice::ReadOnly));
            const auto goldenBytes = golden.readAll();
            auto real = SaveDocument::open(rom, goldenBytes);
            QCOMPARE(real.serialize(), goldenBytes);
            QCOMPARE(real.value(SaveTable::Equipment, 0, "id").toUInt(), 2U);
            QCOMPARE(real.value(SaveTable::Equipment, 0, "owned").toUInt(), 2U);
            const auto funds = real.planEdit(SaveTable::Game, 0, "goth", 12345);
            real.apply(funds, true);
            QCOMPARE(SaveDocument::open(rom, real.serialize())
                         .value(SaveTable::Game, 0, "goth")
                         .toUInt(),
                     12345U);
            real.apply(funds, false);
            QCOMPARE(real.serialize(), goldenBytes);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
};
QTEST_GUILESS_MAIN(SaveDocumentTests)
#include "save_document_tests.moc"
