#include "engine/project.h"
#include "engine/save_document.h"
#include "engine/scenario_seeder.h"
#include "game/classes/save_format.h"
#include "game/army/soldier_reserve.h"
#include "game/army/leader.h"
#include "game/army/character.h"
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
    void scenarioSeederParity() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto refs = qEnvironmentVariable("LORDLY_SEED_REFERENCE");
        if (path.isEmpty() || refs.isEmpty())
            QSKIP("Private ROM and standalone reference saves required");
        try {
            const auto rom = Rom::open(path);
            ScenarioSeeder seeder(rom);
            QFile json(QDir(refs).filePath("reference.json"));
            QVERIFY(json.open(QIODevice::ReadOnly));
            const auto expected = QJsonDocument::fromJson(json.readAll()).array();
            QCOMPARE(seeder.choices().size(), expected.size());
            for (int i = 0; i < expected.size(); ++i) {
                const auto ref = expected[i].toObject();
                const auto &choice = seeder.choices()[i];
                QCOMPARE(choice.route.node, ref["node"].toInt());
                QCOMPARE(choice.route.event, ref["event"].toInt());
                QCOMPARE(choice.chaos, ref["chaos"].toInt());
                QFile file(QDir(refs).filePath(QString("seed-%1.sra").arg(i)));
                QVERIFY(file.open(QIODevice::ReadOnly));
                const auto raw = seeder.seed(i);
                const auto expectedBytes = file.readAll();
                // Python iterates a set of equipment IDs; native sorts IDs when
                // filling free slots. Compare all decoded data after normalizing
                // only inventory order, and compare the save-menu summary too.
                auto normalizeInventory = [](QByteArray memory) {
                    for (auto [at, count] : {qMakePair(0x196b00, 278), qMakePair(0x193ac0, 40)}) {
                        QVector<QByteArray> rows;
                        for (int j = 0; j < count; ++j)
                            rows << memory.mid(at + j * 4, 4);
                        std::sort(rows.begin(), rows.end());
                        for (int j = 0; j < count; ++j)
                            memory.replace(at + j * 4, 4, rows[j]);
                    }
                    return memory;
                };
                QCOMPARE(normalizeInventory(SaveDocument::open(rom, raw).memory()),
                         normalizeInventory(SaveDocument::open(rom, expectedBytes).memory()));
                QCOMPARE(raw.mid(0x14, 0x24), expectedBytes.mid(0x14, 0x24));
                auto doc = SaveDocument::open(rom, raw);
                doc.selectSlot(0);
                QVERIFY(!doc.saveSlots()[2].valid);
                QCOMPARE(int(u8(doc.memory(), 0x1936aa)), i == 0                     ? 7
                                                          : choice.route.event == 57 ? 28
                                                                                     : 6);
                QCOMPARE(int(u8(doc.memory(), 0x193bf8 + 40)), 50);
            }
            auto donor = SaveDocument::open(rom, seeder.defaultArmyBytes());
            donor.selectSlot(0);
            donor.apply(donor.planEdit(SaveTable::Characters, 1, "luck", 37), true);
            const auto personalized = seeder.seed(5, &donor);
            auto doc = SaveDocument::open(rom, personalized);
            QCOMPARE(doc.value(SaveTable::Characters, 1, "luck").toInt(), 37);
            QCOMPARE(doc.memory().mid(0x193686, 32),
                     SaveDocument::open(rom, seeder.seed(5)).memory().mid(0x193686, 32));
            QCOMPARE(seeder.seed(5, &donor), personalized);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void scenarioSeederInstallation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            if (ScenarioSeeder::project64Running())
                QSKIP("Leave the user's running emulator alone");
            const auto rom = Rom::open(path);
            ScenarioSeeder seeder(rom);
            QTemporaryDir dir;
            QVERIFY(dir.isValid());
            const auto exe = dir.filePath("Project64.exe");
            QFile fake(exe);
            QVERIFY(fake.open(QIODevice::WriteOnly));
            fake.close();
            const auto dest = ScenarioSeeder::batteryPath(rom, exe);
            QVERIFY(dest.startsWith(dir.path() + "/Save/"));
            auto first = seeder.seed(0);
            QVERIFY(ScenarioSeeder::install(rom, first, dest, true).isEmpty());
            const auto second = seeder.seed(5);
            auto backup = ScenarioSeeder::install(rom, second, dest, true);
            QVERIFY(!backup.isEmpty());
            QFile savedBackup(backup);
            QVERIFY(savedBackup.open(QIODevice::ReadOnly));
            QCOMPARE(savedBackup.readAll(), first);
            QFile saved(dest);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            auto merged = saved.readAll();
            saved.close();
            QCOMPARE(merged.mid(0x1860, 0x1850), first.mid(0x1860, 0x1850));
            auto loaded = SaveDocument::open(rom, merged);
            loaded.selectSlot(0);
            QCOMPARE(loaded.memory(), SaveDocument::open(rom, second).memory());
            ScenarioSeeder::install(rom, first, dest, false);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), first);
            saved.close();
            QVERIFY_THROWS_EXCEPTION(
                Error, ScenarioSeeder::install(rom, QByteArray(0x8000, 0), dest, false));
            auto nativeOrder = first;
            for (int i = 0; i < nativeOrder.size(); i += 4) {
                std::swap(nativeOrder[i], nativeOrder[i+3]);
                std::swap(nativeOrder[i+1], nativeOrder[i+2]);
            }
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioSeeder::install(rom, nativeOrder, dest, false));
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), first);
            saved.close();
            QDir().mkpath(dir.filePath("Config"));
            QFile cfg(dir.filePath("Config/Project64.cfg"));
            QVERIFY(cfg.open(QIODevice::WriteOnly));
            cfg.write("[Native Save Directory]\nUse Selected=1\nDirectory=My Saves\n");
            cfg.close();
            QVERIFY(ScenarioSeeder::batteryPath(rom, exe).startsWith(dir.path() + "/My Saves/"));
            auto swapped = rom.bytes();
            for (int i = 0; i < swapped.size(); i += 2)
                std::swap(swapped[i], swapped[i + 1]);
            QCOMPARE(ScenarioSeeder::batteryPath(Rom::fromBytes(swapped), exe),
                     ScenarioSeeder::batteryPath(rom, exe));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void scenarioSeederExpandedInstallation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        if (ScenarioSeeder::project64Running()) QSKIP("Leave the user's emulator alone");
        try {
            const auto rom = Rom::open(path);
            ScenarioSeeder seeder(rom);
            auto swap = [](QByteArray bytes) {
                for (int i=0; i<bytes.size(); i+=4) {
                    std::swap(bytes[i],bytes[i+3]); std::swap(bytes[i+1],bytes[i+2]);
                }
                return bytes;
            };
            auto native = swap(seeder.seed(0));
            const int first = lc_save_record_offset(0,0), second = lc_save_record_offset(1,0);
            native.replace(second,LC_SAVE_NORMAL_BYTES,native.mid(first,LC_SAVE_NORMAL_BYTES));
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(native.data()+second),
                                        LC_SAVE_NORMAL_BYTES,1,0));
            auto originalTarget = SaveDocument::open(rom,swap(native));
            originalTarget.selectSlot(1);
            originalTarget.apply(originalTarget.planEdit(SaveTable::Characters,1,"luck",37),true);
            native = swap(originalTarget.serialize());
            const auto legacy = native;
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(native.data()),unsigned(native.size())));
            auto expanded = SaveDocument::open(rom,swap(native));
            expanded.selectSlot(1);
            QCOMPARE(expanded.value(SaveTable::Characters,1,"luck").toInt(),37);
            const auto donorSeed = seeder.seed(5,&expanded);
            QVERIFY(SaveDocument::open(rom,donorSeed).expanded());
            QCOMPARE(SaveDocument::open(rom,donorSeed).value(SaveTable::Characters,1,"luck").toInt(),37);
            QTemporaryDir dir; QVERIFY(dir.isValid());
            const auto destination = dir.filePath("test.sra");
            for (bool expandedTarget : {true,false}) {
                const auto initial = swap(expandedTarget ? native : legacy);
                QFile target(destination); QVERIFY(target.open(QIODevice::WriteOnly));
                QCOMPARE(target.write(initial),initial.size()); target.close();
                for (const auto &seed : {expandedTarget ? seeder.seed(5) : donorSeed,seeder.seed(0)}) {
                    QVERIFY(target.open(QIODevice::ReadOnly)); const auto before=target.readAll(); target.close();
                    const auto backup = ScenarioSeeder::install(rom,seed,destination,true);
                    QFile savedBackup(backup); QVERIFY(savedBackup.open(QIODevice::ReadOnly));
                    QCOMPARE(savedBackup.readAll(),before);
                    QVERIFY(target.open(QIODevice::ReadOnly)); const auto actual=target.readAll(); target.close();
                    auto reopened=SaveDocument::open(rom,actual); QVERIFY(reopened.expanded());
                    reopened.selectSlot(0); QCOMPARE(reopened.memory(),SaveDocument::open(rom,seed).memory());
                    reopened.selectSlot(1); QCOMPARE(reopened.memory(),expanded.memory());
                    const auto normalized=swap(actual);
                    QCOMPARE(normalized.mid(lc_save_record_offset(1,1),LC_SAVE_EXPANDED_NORMAL_BYTES),
                             native.mid(lc_save_record_offset(1,1),LC_SAVE_EXPANDED_NORMAL_BYTES));
                    QCOMPARE(normalized.first(16),native.first(16));
                    QVERIFY(!reopened.saveSlots()[2].valid);
                }
            }
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void characterInitialization() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QByteArray table;
            QByteArray items;
            const auto itemData = project.itemData();
            for (const auto &r : itemData.entries())
                items += r.record;
            // These retail items have no growth bonuses. Distinct authored
            // fields exercise each recovered getter, including HP/STR sharing.
            put8(items, 210 * 32 + 24, 0x93); put8(items, 210 * 32 + 25, 0x6a);
            put8(items, 277 * 32 + 24, 0x6c); put8(items, 277 * 32 + 25, 0xb9);
            for (const auto &r : project.records(Table::Classes))
                table += r.bytes;
            struct Random {
                unsigned used = 0;
                uint32_t value = 0;
            };
            const auto random = [](void *context) -> uint32_t {
                auto &r = *static_cast<Random *>(context);
                ++r.used;
                return r.value;
            };
            QJsonArray cases;
            const auto compare = [&](const QByteArray &classes, int primary, int secondary,
                                     uint32_t draw, int level = 1, bool explicitEquipment = false) {
                QByteArray before(56, char(0x6d));
                before[17] = char(primary);
                before[18] = char(secondary);
                if (level > 1) {
                    before.replace(42, 8, QByteArray(8, 0));
                    if (explicitEquipment) {
                        put16(before, 42, 277);
                        put16(before, 46, 210);
                    }
                }
                auto after = before;
                Random state{0, draw};
                if (!lordly_character_initialize(
                        reinterpret_cast<uint8_t *>(after.data()), unsigned(level),
                        reinterpret_cast<const uint8_t *>(classes.constData()),
                        unsigned(classes.size() / 72),
                        reinterpret_cast<const uint8_t *>(items.constData()),
                        unsigned(items.size() / 32), random, &state))
                    throw Error("Unexpected initializer rejection");
                QJsonArray draws;
                for (unsigned i = 0; i < state.used; ++i) draws << int(draw);
                cases << QJsonObject{{"classes", QString::fromLatin1(classes.toHex())},
                                     {"before", QString::fromLatin1(before.toHex())},
                                     {"after", QString::fromLatin1(after.toHex())},
                                     {"draws", draws}, {"level", level},
                                     {"used", int(state.used)}};
            };
            for (int id = 1; id < 165; ++id)
                for (uint32_t draw : {0U, 1U, 2U, 3U}) {
                    const int paired = u8(table, id * 72 + 69);
                    compare(table, id, paired ? paired : id, draw);
                    compare(table, id, 2, draw);
                }
            for (int stage : {0, 1, 2})
                for (int body : {0, 1, 2, 3, 4, 5})
                    for (int element : {0, 4, 255}) {
                        auto custom = table;
                        custom[5 * 72 + 66] = char(stage == 1 ? 2 : 255);
                        custom[5 * 72 + 68] = char(stage == 2 ? 2 : 255);
                        custom[5 * 72 + 65] = 10;
                        custom[5 * 72 + 67] = 33;
                        const int base = stage == 1 ? 10 : stage == 2 ? 33 : 5;
                        custom[base * 72 + 5] = char(body);
                        custom[5 * 72 + 70] = char(element);
                        compare(custom, 5, 5, 3);
                    }
            auto invalid = table;
            invalid[5 * 72 + 66] = 2;
            invalid[5 * 72 + 65] = char(255);
            QByteArray record(56, 0);
            record[17] = 5;
            record[18] = 5;
            Random state;
            QVERIFY(!lordly_character_level_one(
                reinterpret_cast<uint8_t *>(record.data()),
                reinterpret_cast<const uint8_t *>(invalid.constData()), 165, random, &state));
            for (int id : {1, 5, 22, 71, 98, 160})
                for (int level : {2, 10, 30, 99})
                    for (uint32_t draw : {0U, 1U})
                        compare(table, id, id, draw, level, draw != 0);
            auto transitions = table;
            transitions[5 * 72 + 65] = 10; transitions[5 * 72 + 66] = 3;
            transitions[5 * 72 + 67] = 33; transitions[5 * 72 + 68] = 7;
            for (int level : {3, 4, 7, 8}) compare(transitions, 5, 5, 1, level);
            for (int at : {8, 12, 16, 20, 24, 28, 32}) put16(transitions, 10 * 72 + at, 998);
            compare(transitions, 5, 5, 1, 10, true);
            QTemporaryDir work;
            const auto file = work.filePath("character-reference.json");
            atomicWrite(
                file, QJsonDocument(QJsonObject{{"cases", cases}, {"items", QString::fromLatin1(items.toHex())}}).toJson(QJsonDocument::Compact));
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("army-character-mips.cjs"), file});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(!reference.exitCode(), reference.readAllStandardError().constData());
            qInfo().noquote() << reference.readAllStandardOutput();
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void characterRosterLifecycle() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            const auto items = project.itemData();
            QTemporaryDir work;
            QProcess generator;
            generator.start("node",
                            {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile file(work.filePath("legion-export.bin"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto payload = file.readAll();
            QByteArray battery(0x10000, char(0x6d));
            for (int slot : {0, 1, 15}) {
                const int at = int(lc_save_record_offset(slot, 0));
                battery[at + 12] = 0;
                battery.replace(at + 42, 6180, payload);
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(battery.data() + at),
                                            lc_save_record_size(slot, 0), slot, 0));
            }
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (const auto &input : {battery, swapped, expanded})
                for (int slot : {0, 1}) {
                    auto doc = SaveDocument::open(rom, input);
                    doc.selectSlot(slot);
                    const auto before = doc.memory();
                    QCOMPARE(doc.freeCharacterSlot(), 21);
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planRemoveCharacter(1));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planRemoveCharacter(18));
                    QVERIFY_THROWS_EXCEPTION(
                        Error, doc.planCreateCharacter(0, "Recruit", project.classData(), 7));
                    QVERIFY_THROWS_EXCEPTION(
                        Error, doc.planCreateCharacter(5, "Too long name", project.classData(), 7));
                    const auto create =
                        doc.planCreateCharacter(5, "Recruit", project.classData(), 7, 20, &items);
                    doc.apply(create, true);
                    QCOMPARE(doc.freeCharacterSlot(), 22);
                    QVERIFY(doc.reserveMembers().contains(21));
                    QCOMPARE(doc.value(SaveTable::Characters, 21, "name").toString(),
                             QString("Recruit"));
                    QCOMPARE(doc.value(SaveTable::Characters, 21, "level").toInt(), 20);
                    QCOMPARE(doc.value(SaveTable::Characters, 21, "hpMax"),
                             doc.value(SaveTable::Characters, 21, "hpCur"));
                    QCOMPARE(u8(doc.memory(), 0x193bc0 + 21 * 56 + 52), quint8(21));
                    auto expected = before;
                    expected.replace(0x193bc0 + 21 * 56, 56,
                                     doc.memory().mid(0x193bc0 + 21 * 56, 56));
                    QCOMPARE(doc.memory(), expected);
                    const auto created = doc.serialize();
                    QVERIFY(doc.planArmyRefresh(project).spans.isEmpty());
                    auto reopened = SaveDocument::open(rom, created);
                    reopened.selectSlot(slot);
                    QCOMPARE(reopened.memory(), expected);
                    const auto remove = doc.planRemoveCharacter(21);
                    doc.apply(remove, true);
                    QCOMPARE(doc.serialize(), input);
                    doc.apply(remove, false);
                    QCOMPARE(doc.serialize(), created);
                    doc.apply(create, false);
                    QCOMPARE(doc.serialize(), input);
                    const auto removeOriginal = doc.planRemoveCharacter(19);
                    doc.apply(removeOriginal, true);
                    QCOMPARE(doc.freeCharacterSlot(), 19);
                    auto removedExpected = before;
                    removedExpected.replace(0x193bc0 + 19 * 56, 56, QByteArray(56, 0));
                    QCOMPARE(doc.memory(), removedExpected);
                    doc.apply(removeOriginal, false);
                    QCOMPARE(doc.serialize(), input);
                }
            auto full = SaveDocument::open(rom, battery);
            for (int id = 21; id < 100; ++id)
                full.apply(full.planCreateCharacter(5, QString("Recruit%1").arg(id),
                                                    project.classData(), quint32(id)),
                           true);
            QCOMPARE(full.freeCharacterSlot(), -1);
            const auto saved = full.serialize();
            QVERIFY_THROWS_EXCEPTION(
                Error, full.planCreateCharacter(5, "Overflow", project.classData(), 0));
            QCOMPARE(full.serialize(), saved);
            full.selectSlot(15);
            QCOMPARE(full.freeCharacterSlot(), -1);
            QVERIFY_THROWS_EXCEPTION(
                Error, full.planCreateCharacter(5, "Suspend", project.classData(), 0));
            QVERIFY_THROWS_EXCEPTION(Error, full.planRemoveCharacter(19));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void armyExportConsistency() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto baseline = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing export fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal export fixture");
                }
                return out;
            };
            const auto battery = batteryFor("legion-export.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (bool projectEdit : {false, true}) {
                auto project = baseline;
                if (projectEdit) {
                    auto bytes = project.record(Table::Classes, 5).bytes;
                    bytes[44] = 7;
                    project.replaceBytes(Table::Classes, 5, bytes);
                }
                QJsonArray cases, saves;
                for (const auto &input : {battery, swapped, expanded}) {
                    auto doc = SaveDocument::open(rom, input);
                    QVector<QByteArray> expected;
                    for (int slot : {0, 1}) {
                        doc.selectSlot(slot);
                        if (!projectEdit)
                            doc.apply(doc.planEdit(SaveTable::Characters, 18, "secondaryClass", 44),
                                      true);
                        auto memory = doc.memory();
                        if (projectEdit) {
                            for (int i = 0; i < 17; ++i)
                                if (i != 0 && i != 5 && i != 10)
                                    memory[0x1971f0 + i * 25 + 24] = 7;
                        } else
                            memory[0x1971f0 + 2 * 25 + 24] = 7;
                        memory[0x1969c1] = 7;
                        expected << memory;
                    }
                    doc.selectSlot(15);
                    const auto suspended = doc.memory(), before = doc.serialize();
                    const auto refresh = doc.planArmyRefresh(project);
                    QVERIFY(!refresh.spans.isEmpty());
                    QCOMPARE(doc.serialize(), before);
                    doc.apply(refresh, true);
                    QCOMPARE(doc.selectedSlot(), 15);
                    QCOMPARE(doc.memory(), suspended);
                    QVERIFY(doc.planArmyRefresh(project).spans.isEmpty());
                    const auto saved = doc.serialize();
                    auto reopened = SaveDocument::open(rom, saved);
                    for (int slot : {0, 1}) {
                        doc.selectSlot(slot);
                        reopened.selectSlot(slot);
                        QCOMPARE(doc.memory(), expected[slot]);
                        QCOMPARE(reopened.memory(), expected[slot]);
                    }
                    if (input == battery) {
                        auto original = SaveDocument::open(rom, input);
                        const auto originalMemory = original.memory(), memory = expected[0];
                        QJsonArray changed;
                        if (projectEdit) {
                            for (int i = 0; i < 17; ++i)
                                if (i != 0 && i != 5 && i != 10)
                                    changed << i;
                        } else
                            changed << 2;
                        cases << QJsonObject{
                            {"index", 0},
                            {"changed", changed},
                            {"units", QString::fromLatin1(memory.mid(0x1971f0, 750).toHex())},
                            {"legions",
                             QString::fromLatin1(originalMemory.mid(0x1969b8, 110).toHex())},
                            {"order", QString::fromLatin1(memory.mid(0x193ab0, 6).toHex())},
                            {"unitOrder", QString::fromLatin1(memory.mid(0x1977d8, 30).toHex())},
                            {"afterUnits", QString::fromLatin1(memory.mid(0x1971f0, 750).toHex())},
                            {"afterLegions",
                             QString::fromLatin1(memory.mid(0x1969b8, 110).toHex())},
                            {"afterOrder", QString::fromLatin1(memory.mid(0x193ab0, 6).toHex())},
                            {"characters", QString::fromLatin1(memory.mid(0x193bc0, 5600).toHex())},
                            {"soldiers", QString::fromLatin1(memory.mid(0x190f80, 364).toHex())},
                            {"inventory", QString::fromLatin1(memory.mid(0x193ac0, 160).toHex())}};
                        for (int slot : {0, 1})
                            saves << QJsonObject{
                                {"payload",
                                 QString::fromLatin1(
                                     saved.mid(int(lc_save_record_offset(slot, 0)) + 42, 6180)
                                         .toHex())},
                                {"units",
                                 QString::fromLatin1(expected[slot].mid(0x1971f0, 750).toHex())},
                                {"legions",
                                 QString::fromLatin1(expected[slot].mid(0x1969b8, 110).toHex())},
                                {"order",
                                 QString::fromLatin1(expected[slot].mid(0x193ab0, 6).toHex())}};
                    }
                    doc.apply(refresh, false);
                    QCOMPARE(doc.selectedSlot(), 15);
                    QCOMPARE(doc.serialize(), before);
                    doc.apply(refresh, true);
                    QCOMPARE(doc.serialize(), saved);
                }
                QByteArray classes;
                for (const auto &record : project.records(Table::Classes))
                    classes += record.bytes;
                atomicWrite(
                    work.filePath("legion-coordination-verify.json"),
                    QJsonDocument(QJsonObject{{"classes", QString::fromLatin1(classes.toHex())},
                                              {"cases", cases},
                                              {"saves", saves}})
                        .toJson());
                run("verify-coordination");
            }
            for (const auto &change :
                 {QVector<int>{5, 4, 2}, QVector<int>{5, 71, 0}, QVector<int>{33, 6, 1}}) {
                auto project = baseline;
                auto bytes = project.record(Table::Classes, change[0]).bytes;
                bytes[change[1]] = char(change[2]);
                project.replaceBytes(Table::Classes, change[0], bytes);
                auto doc = SaveDocument::open(rom, battery);
                doc.selectSlot(15);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planArmyRefresh(project));
                QCOMPARE(doc.serialize(), battery);
                QCOMPARE(doc.selectedSlot(), 15);
            }
            auto odd = SaveDocument::open(rom, batteryFor("legion-export-odd.bin"));
            const auto unusual = odd.serialize();
            QVERIFY(odd.planArmyRefresh(baseline).spans.isEmpty());
            auto irrelevant = baseline;
            auto stats = irrelevant.record(Table::Classes, 5).bytes;
            stats[10] = char(u8(stats, 10) + 1);
            irrelevant.replaceBytes(Table::Classes, 5, stats);
            irrelevant.setClassNameBytes(5, QByteArray::fromHex("516263ff"));
            QVERIFY(odd.planArmyRefresh(irrelevant).spans.isEmpty());
            QCOMPARE(odd.serialize(), unusual);
            auto hidden = SaveDocument::open(rom, battery);
            hidden.selectSlot(1);
            hidden.apply(hidden.planEdit(SaveTable::Characters, 18, "secondaryClass", 55), true);
            hidden.selectSlot(0);
            const auto invalid = hidden.serialize();
            QVERIFY_THROWS_EXCEPTION(Error, hidden.planArmyRefresh(baseline));
            QCOMPARE(hidden.serialize(), invalid);
            QCOMPARE(hidden.selectedSlot(), 0);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void characterClassCoordination() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            const auto battery = batteryFor("legion-class.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            for (const auto &input : {battery, swapped, expanded})
                for (int slot : {0, 1})
                    for (bool primary : {false, true}) {
                        auto doc = SaveDocument::open(rom, input);
                        doc.selectSlot(slot);
                        const auto before = doc.memory();
                        QVERIFY(doc.planCharacterClassEdit(18, "class", 5, project.classData())
                                    .spans.isEmpty());
                        SaveChange first;
                        if (primary) {
                            first =
                                doc.planCharacterClassEdit(18, "class", 44, project.classData());
                            doc.apply(first, true);
                        }
                        const auto middle = doc.serialize();
                        const auto second = doc.planCharacterClassEdit(18, "secondaryClass", 44,
                                                                       project.classData());
                        doc.apply(second, true);
                        auto expected = before;
                        if (primary)
                            expected[0x193bc0 + 18 * 56 + 17] = 44;
                        expected[0x193bc0 + 18 * 56 + 18] = 44;
                        expected[0x1971f0 + 2 * 25 + 24] = 7;
                        expected[0x1969c1] = 7;
                        QCOMPARE(doc.memory(), expected);
                        auto reopened = SaveDocument::open(rom, doc.serialize());
                        reopened.selectSlot(slot);
                        QCOMPARE(reopened.memory(), expected);
                        if (input == battery && slot == 0) {
                            cases << QJsonObject{
                                {"index", 0},
                                {"changed", QJsonArray{2}},
                                {"units", QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                {"legions", QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                {"unitOrder",
                                 QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                {"afterUnits",
                                 QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                {"afterLegions",
                                 QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                {"afterOrder",
                                 QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())},
                                {"characters",
                                 QString::fromLatin1(expected.mid(0x193bc0, 5600).toHex())},
                                {"soldiers",
                                 QString::fromLatin1(expected.mid(0x190f80, 364).toHex())},
                                {"inventory",
                                 QString::fromLatin1(expected.mid(0x193ac0, 160).toHex())}};
                            saves << QJsonObject{
                                {"payload",
                                 QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                {"units", QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                {"legions",
                                 QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                {"order", QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())}};
                        }
                        const auto saved = doc.serialize();
                        doc.apply(second, false);
                        QCOMPARE(doc.serialize(), middle);
                        doc.apply(second, true);
                        QCOMPARE(doc.serialize(), saved);
                        doc.apply(second, false);
                        if (primary)
                            doc.apply(first, false);
                        QCOMPARE(doc.serialize(), input);
                        const auto reserve = doc.planCharacterClassEdit(19, "secondaryClass", 55,
                                                                        project.classData());
                        doc.apply(reserve, true);
                        auto reserveExpected = before;
                        reserveExpected[0x193bc0 + 19 * 56 + 18] = 55;
                        QCOMPARE(doc.memory(), reserveExpected);
                        doc.apply(reserve, false);
                        QCOMPARE(doc.serialize(), input);
                        QVERIFY_THROWS_EXCEPTION(
                            Error, doc.planCharacterClassEdit(18, "secondaryClass", 55,
                                                              project.classData()));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 doc.planCharacterClassEdit(1, "secondaryClass", 5,
                                                                            project.classData()));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 doc.planCharacterClassEdit(16, "secondaryClass", 2,
                                                                            project.classData()));
                        QVERIFY_THROWS_EXCEPTION(
                            Error, doc.planCharacterClassEdit(18, "level", 5, project.classData()));
                        QCOMPARE(doc.serialize(), input);
                    }
            auto carry = SaveDocument::open(rom, batteryFor("legion-class-carry.bin"));
            const auto unchanged = carry.serialize();
            QVERIFY_THROWS_EXCEPTION(
                Error, carry.planCharacterClassEdit(18, "secondaryClass", 10, project.classData()));
            QCOMPARE(carry.serialize(), unchanged);
            atomicWrite(work.filePath("legion-coordination-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify-coordination");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void legionCreation() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            for (const auto &kind : {"empty", "contiguous", "holes"}) {
                const QString name = QString::fromLatin1(kind);
                const int index = name == "empty" ? 0 : 1, label = index + 1;
                const auto battery = batteryFor(QString("legion-create-%1.bin").arg(name));
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int slot : {0, 1})
                        for (int unit : {15, 16}) {
                            auto doc = SaveDocument::open(rom, input);
                            doc.selectSlot(slot);
                            const auto before = doc.memory();
                            const auto count = doc.legions().size();
                            QVERIFY(doc.newLegionCommanders(project.classData()).contains(unit));
                            const auto change = doc.planCreateLegion(unit, project.classData());
                            doc.apply(change, true);
                            auto expected = before;
                            const QByteArray record = QByteArray::fromHex("000100ffffffff00010202");
                            auto row = record;
                            row[0] = char(label);
                            row[2] = char(unit);
                            expected.replace(0x1969b8 + index * 11, 11, row);
                            expected[0x1971f0 + unit * 25 + 1] =
                                char(u8(before, 0x1971f0 + unit * 25 + 1) | 2);
                            QByteArray order(6, 7);
                            int n = 0;
                            for (int i = 0; i < 6; ++i)
                                if (u8(before, 0x193ab0 + i) != 7)
                                    order[n++] = before[0x193ab0 + i];
                            order[n] = char(index);
                            expected.replace(0x193ab0, 6, order);
                            QCOMPARE(doc.memory(), expected);
                            QCOMPARE(doc.legions().size(), count + 1);
                            QCOMPARE(doc.legionCapacity(index), unit == 15 ? 2 : 1);
                            QVERIFY_THROWS_EXCEPTION(
                                Error, doc.planCreateLegion(unit, project.classData()));
                            auto reopened = SaveDocument::open(rom, doc.serialize());
                            reopened.selectSlot(slot);
                            QCOMPARE(reopened.memory(), expected);
                            if (input == battery && slot == 0) {
                                cases << QJsonObject{
                                    {"index", index},
                                    {"unit", unit},
                                    {"units",
                                     QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                    {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                    {"unitOrder",
                                     QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                    {"afterUnits",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"afterLegions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"afterOrder",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())},
                                    {"characters",
                                     QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(before.mid(0x190f80, 364).toHex())},
                                    {"inventory",
                                     QString::fromLatin1(before.mid(0x193ac0, 160).toHex())}};
                                saves << QJsonObject{
                                    {"payload",
                                     QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                    {"units",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"order",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())}};
                            }
                            if (unit == 15) {
                                const auto join =
                                    doc.planJoinLegion(index, 16, project.classData());
                                doc.apply(join, true);
                                QCOMPARE(doc.legions()[index].units[1], 16);
                                doc.apply(join, false);
                                QCOMPARE(doc.memory(), expected);
                            }
                            const auto created = doc.serialize();
                            const auto disband = doc.planDisbandLegion(index);
                            doc.apply(disband, true);
                            doc.apply(doc.planCreateLegion(unit, project.classData()), true);
                            QCOMPARE(doc.serialize(), created);
                            doc.apply(change, false);
                            QCOMPARE(doc.serialize(), input);
                            doc.apply(change, true);
                            QCOMPARE(doc.serialize(), created);
                        }
            }
            for (const auto &name : {"full", "order", "number", "deployed", "dead", "group"}) {
                const auto input = batteryFor(QString("legion-create-%1.bin").arg(name));
                const auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateLegion(15, project.classData()));
                QCOMPARE(doc.serialize(), input);
            }
            auto doc = SaveDocument::open(rom, batteryFor("legion-create-holes.bin"));
            for (int unit : {-1, 0, 5, 14, 29, 255})
                QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateLegion(unit, project.classData()));
            const auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY(raw.newLegionCommanders(project.classData()).isEmpty());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planCreateLegion(15, project.classData()));
            doc.selectSlot(15);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateLegion(15, project.classData()));
            atomicWrite(work.filePath("legion-create-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify-create");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void legionSquadCoordination() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            for (const auto &operation : {"remove", "transfer", "swap", "create", "leader",
                                          "add-reserve", "remove-reserve"}) {
                const QString op = QString::fromLatin1(operation);
                const auto battery = batteryFor(op == "swap"     ? "legion-coordinate-swap.bin"
                                                : op == "leader" ? "legion-coordinate-leader.bin"
                                                                 : "legion-coordinate.bin");
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int slot : {0, 1}) {
                        auto doc = SaveDocument::open(rom, input);
                        doc.selectSlot(slot);
                        if (op == "create")
                            doc.apply(doc.planToReserve(0, 1, project.classData()), true);
                        if (op == "remove-reserve")
                            doc.apply(doc.planFromReserve(18, 2, 8, project.classData()), true);
                        const auto before = doc.memory(), original = doc.serialize();
                        SaveChange change;
                        QVector<int> affected{0};
                        if (op == "remove")
                            change = doc.planToReserve(0, 1, project.classData());
                        if (op == "create")
                            change = doc.planSoldierGroup(0, 0, project.classData());
                        if (op == "transfer") {
                            change = doc.planUnitTransfer(0, 1, 15, 0, project.classData());
                            affected << 15;
                        }
                        if (op == "swap") {
                            change = doc.planUnitSwap(0, 1, 15, 1, project.classData());
                            affected << 15;
                        }
                        if (op == "leader")
                            change = doc.planUnitLeader(0, 4, project.classData());
                        if (op == "add-reserve") {
                            change = doc.planFromReserve(18, 2, 8, project.classData());
                            affected = {2};
                        }
                        if (op == "remove-reserve") {
                            change = doc.planToReserve(2, 1, project.classData());
                            affected = {2};
                        }
                        doc.apply(change, true);
                        const auto after = doc.memory();
                        auto expected = before;
                        for (int id : affected)
                            expected.replace(0x1971f0 + id * 25, 25,
                                             after.mid(0x1971f0 + id * 25, 25));
                        expected.replace(0x1969b8, 11, after.mid(0x1969b8, 11));
                        if (op == "remove" || op == "create") {
                            expected[0x190f81] = after[0x190f81];
                            expected.replace(0x190f84, 2, after.mid(0x190f84, 2));
                            expected[0x191074] = after[0x191074];
                            QCOMPARE(doc.legionCapacity(0), op == "remove" ? 4 : 5);
                        }
                        if (op == "leader") {
                            expected[0x193bc0 + 56 + 51] = after[0x193bc0 + 56 + 51];
                            expected[0x193bc0 + 18 * 56 + 51] = after[0x193bc0 + 18 * 56 + 51];
                            QCOMPARE(doc.units()[0].members[0], 18);
                        }
                        QCOMPARE(after, expected);
                        QCOMPARE(after.mid(0x1969ba, 5), before.mid(0x1969ba, 5));
                        QCOMPARE(u8(after, 0x1969c2), quint8(7));
                        if (affected.contains(0))
                            QCOMPARE(u8(after, 0x1969c1), quint8(7));
                        auto reopened = SaveDocument::open(rom, doc.serialize());
                        reopened.selectSlot(slot);
                        QCOMPARE(reopened.memory(), after);
                        if (input == battery && slot == 0) {
                            QJsonArray changed;
                            for (int id : affected)
                                changed << id;
                            cases << QJsonObject{
                                {"index", 0},
                                {"changed", changed},
                                {"units", QString::fromLatin1(after.mid(0x1971f0, 750).toHex())},
                                {"legions", QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                {"unitOrder",
                                 QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                {"afterUnits",
                                 QString::fromLatin1(after.mid(0x1971f0, 750).toHex())},
                                {"afterLegions",
                                 QString::fromLatin1(after.mid(0x1969b8, 110).toHex())},
                                {"afterOrder", QString::fromLatin1(after.mid(0x193ab0, 6).toHex())},
                                {"characters",
                                 QString::fromLatin1(after.mid(0x193bc0, 5600).toHex())},
                                {"soldiers", QString::fromLatin1(after.mid(0x190f80, 364).toHex())},
                                {"inventory",
                                 QString::fromLatin1(after.mid(0x193ac0, 160).toHex())}};
                            saves << QJsonObject{
                                {"payload",
                                 QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                {"units", QString::fromLatin1(after.mid(0x1971f0, 750).toHex())},
                                {"legions", QString::fromLatin1(after.mid(0x1969b8, 110).toHex())},
                                {"order", QString::fromLatin1(after.mid(0x193ab0, 6).toHex())}};
                        }
                        const auto saved = doc.serialize();
                        doc.apply(change, false);
                        QCOMPARE(doc.serialize(), original);
                        doc.apply(change, true);
                        QCOMPARE(doc.serialize(), saved);
                    }
            }
            const auto input = batteryFor("legion-coordinate-full.bin");
            auto doc = SaveDocument::open(rom, input);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planToReserve(0, 1, project.classData()));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(0, 1, 15, 0, project.classData()));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 15, 1, project.classData()));
            QCOMPARE(doc.serialize(), input);
            const auto detached = doc.planDetachLegionUnit(0, 4);
            doc.apply(detached, true);
            doc.apply(doc.planToReserve(0, 1, project.classData()), true);
            QCOMPARE(doc.legionCapacity(0), 4);
            QCOMPARE(doc.legions()[0].units, QVector<int>({0, 1, 2, 3, 255}));
            atomicWrite(work.filePath("legion-coordination-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify-coordination");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void legionJoining() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            const auto priority = rom.bytes().mid(0x5cebc, 7);
            for (int position : {1, 2, 3, 4}) {
                const auto battery = batteryFor(QString("legion-join-%1.bin").arg(position));
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int saveSlot : {0, 1})
                        for (int unit : {15, 16}) {
                            auto doc = SaveDocument::open(rom, input);
                            doc.selectSlot(saveSlot);
                            const auto before = doc.memory();
                            QCOMPARE(doc.legionCapacity(0), 5);
                            const auto change = doc.planJoinLegion(0, unit, project.classData());
                            doc.apply(change, true);
                            auto expected = before;
                            expected[0x1969ba + position] = char(unit);
                            QVector<int> types;
                            for (int i = 0; i < 5; ++i)
                                types
                                    << u8(before, 0x1971f0 + u8(expected, 0x1969ba + i) * 25 + 24);
                            int movement = 7;
                            for (auto type : priority)
                                if (types.contains(quint8(type))) {
                                    movement = quint8(type);
                                    break;
                                }
                            expected[0x1969c1] = char(movement);
                            expected[0x1969c2] = 6;
                            QCOMPARE(doc.memory(), expected);
                            QCOMPARE(doc.units().size(), 17);
                            QCOMPARE(doc.legions()[0].units[position], unit);
                            QVERIFY_THROWS_EXCEPTION(
                                Error,
                                doc.planJoinLegion(0, unit == 15 ? 16 : 15, project.classData()));
                            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(unit));
                            auto reopened = SaveDocument::open(rom, doc.serialize());
                            reopened.selectSlot(saveSlot);
                            QCOMPARE(reopened.memory(), expected);
                            if (input == battery && saveSlot == 0) {
                                cases << QJsonObject{
                                    {"index", 0},
                                    {"slot", position},
                                    {"unit", unit},
                                    {"units",
                                     QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                    {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                    {"unitOrder",
                                     QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                    {"afterUnits",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"afterLegions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"afterOrder",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())},
                                    {"characters",
                                     QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(before.mid(0x190f80, 364).toHex())},
                                    {"inventory",
                                     QString::fromLatin1(before.mid(0x193ac0, 160).toHex())}};
                                saves << QJsonObject{
                                    {"payload",
                                     QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                    {"units",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"order",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())}};
                            }
                            const auto saved = doc.serialize();
                            doc.apply(change, false);
                            QCOMPARE(doc.serialize(), input);
                            doc.apply(change, true);
                            QCOMPARE(doc.serialize(), saved);
                        }
            }
            for (const auto &name : {"legion-join-deployed.bin", "legion-join-incapacitated.bin",
                                     "legion-join-zero-hp.bin", "legion-join-full.bin",
                                     "legion-join-capacity.bin", "legion-join-dead-group.bin",
                                     "legion-join-command.bin", "legion-join-shared.bin"}) {
                const auto input = batteryFor(name);
                const auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planJoinLegion(0, 15, project.classData()));
                QCOMPARE(doc.serialize(), input);
            }
            auto doc = SaveDocument::open(rom, batteryFor("legion-join-1.bin"));
            for (int unit : {-1, 0, 2, 29, 255})
                QVERIFY_THROWS_EXCEPTION(Error, doc.planJoinLegion(0, unit, project.classData()));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planJoinLegion(1, 15, project.classData()));
            const auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planJoinLegion(0, 15, project.classData()));
            doc.selectSlot(15);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planJoinLegion(0, 15, project.classData()));
            atomicWrite(work.filePath("legion-join-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify-join");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void legionFollowerRemoval() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            const auto priority = rom.bytes().mid(0x5cebc, 7);
            for (const auto &name : {"legions.bin", "legion-movement.bin"}) {
                const auto battery = batteryFor(name);
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int saveSlot : {0, 1}) {
                        auto doc = SaveDocument::open(rom, input);
                        doc.selectSlot(saveSlot);
                        auto check = [&](int index, int position) {
                            const auto before = doc.memory();
                            const int at = 0x1969b8 + index * 11;
                            const int unit = u8(before, at + 2 + position);
                            const auto change = doc.planDetachLegionUnit(index, unit);
                            doc.apply(change, true);
                            auto expected = before;
                            expected[at + 2 + position] =
                                position < 3 ? before[at + 4 + position] : char(255);
                            if (position < 3)
                                expected[at + 4 + position] = char(255);
                            QVector<int> movements;
                            int count = 0;
                            for (int i = 0; i < 5; ++i) {
                                const int id = u8(expected, at + 2 + i);
                                movements << (id == 255 ? 0 : u8(before, 0x1971f0 + id * 25 + 24));
                                count += id != 255;
                            }
                            int movement = 7;
                            for (auto type : priority)
                                if (movements.contains(quint8(type))) {
                                    movement = quint8(type);
                                    break;
                                }
                            expected[at + 9] = char(movement);
                            expected[at + 10] = char((11 - count) & 7);
                            QCOMPARE(doc.memory(), expected);
                            QCOMPARE(doc.units().size(), 17);
                            QCOMPARE(doc.legions().size(), 3);
                            QCOMPARE(doc.reserveSoldiers(), 9);
                            auto reopened = SaveDocument::open(rom, doc.serialize());
                            reopened.selectSlot(saveSlot);
                            QCOMPARE(reopened.memory(), expected);
                            if (input == battery && saveSlot == 0) {
                                cases << QJsonObject{
                                    {"index", index},
                                    {"slot", position},
                                    {"units",
                                     QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                    {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                    {"unitOrder",
                                     QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                    {"afterUnits",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"afterLegions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"afterOrder",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())},
                                    {"characters",
                                     QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(before.mid(0x190f80, 364).toHex())},
                                    {"inventory",
                                     QString::fromLatin1(before.mid(0x193ac0, 160).toHex())}};
                                saves << QJsonObject{
                                    {"payload",
                                     QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                    {"units",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"order",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())}};
                            }
                            const auto disband = doc.planDisbandUnit(unit);
                            doc.apply(disband, true);
                            QCOMPARE(doc.units().size(), 16);
                            doc.apply(disband, false);
                            QCOMPARE(doc.memory(), expected);
                            const auto saved = doc.serialize();
                            doc.apply(change, false);
                            QCOMPARE(doc.memory(), before);
                            doc.apply(change, true);
                            QCOMPARE(doc.serialize(), saved);
                            QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(index, unit));
                        };
                        for (int index : {0, 2})
                            for (int position : {1, 2, 3, 4}) {
                                check(index, position);
                                doc = SaveDocument::open(rom, input);
                                doc.selectSlot(saveSlot);
                            }
                        for (int position : {1, 2, 1, 2})
                            check(0, position);
                        QCOMPARE(doc.legions()[0].units, QVector<int>({0, 255, 255, 255, 255}));
                        QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(0, 0));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 doc.planUnitLeader(0, 1, project.classData()));
                    }
            }
            for (const auto &name : {"legion-member-duplicate.bin", "legion-member-missing.bin",
                                     "legion-member-shared.bin", "legion-member-hidden.bin",
                                     "legion-no-commander.bin"}) {
                const auto input = batteryFor(name);
                const auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(0, 2));
                QCOMPARE(doc.serialize(), input);
            }
            auto doc = SaveDocument::open(rom, batteryFor("legions.bin"));
            for (int unit : {-1, 0, 29, 255})
                QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(0, unit));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(1, 2));
            const auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planDetachLegionUnit(0, 2));
            doc.selectSlot(15);
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDetachLegionUnit(0, 2));
            atomicWrite(work.filePath("legion-detach-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify-detach");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void legionDisbanding() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-legion-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing legion fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal legion fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            for (const auto &name : {"legions.bin", "legion-last.bin"}) {
                const bool last = QString::fromLatin1(name).contains("last");
                const auto battery = batteryFor(name);
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int slot : {0, 1})
                        for (int index : last ? QVector<int>{0} : QVector<int>{0, 2, 4}) {
                            auto doc = SaveDocument::open(rom, input);
                            doc.selectSlot(slot);
                            const auto before = doc.memory();
                            const auto count = doc.legions().size();
                            QCOMPARE(count, last ? 1 : 3);
                            const auto change = doc.planDisbandLegion(index);
                            doc.apply(change, true);
                            auto expected = before;
                            expected[0x1969b8 + index * 11 + 1] =
                                char(u8(before, 0x1969b8 + index * 11 + 1) & ~1);
                            const int first = index / 2 * 5;
                            QByteArray order(6, 7);
                            int n = 0;
                            for (int id : last ? QVector<int>{0} : QVector<int>{4, 0, 2})
                                if (id != index)
                                    order[n++] = char(id);
                            expected.replace(0x193ab0, 6, order);
                            QCOMPARE(doc.memory(), expected);
                            QCOMPARE(doc.legions().size(), count - 1);
                            QCOMPARE(doc.units().size(), 17);
                            QCOMPARE(doc.reserveSoldiers(), 9);
                            doc.validateUnitSupplyEdits(project);
                            auto reopened = SaveDocument::open(rom, doc.serialize());
                            reopened.selectSlot(slot);
                            QCOMPARE(reopened.memory(), expected);
                            if (input == battery && slot == 0) {
                                cases << QJsonObject{
                                    {"index", index},
                                    {"units",
                                     QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(before.mid(0x1969b8, 110).toHex())},
                                    {"order", QString::fromLatin1(before.mid(0x193ab0, 6).toHex())},
                                    {"unitOrder",
                                     QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                    {"afterUnits",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"afterLegions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"afterOrder",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())},
                                    {"characters",
                                     QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(before.mid(0x190f80, 364).toHex())},
                                    {"inventory",
                                     QString::fromLatin1(before.mid(0x193ac0, 160).toHex())}};
                                saves << QJsonObject{
                                    {"payload",
                                     QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                    {"units",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"legions",
                                     QString::fromLatin1(expected.mid(0x1969b8, 110).toHex())},
                                    {"order",
                                     QString::fromLatin1(expected.mid(0x193ab0, 6).toHex())}};
                            }
                            if (index == 0) {
                                const auto leader = doc.planUnitLeader(0, 1, project.classData());
                                doc.apply(leader, true);
                                QCOMPARE(doc.units()[0].members[0], 18);
                                doc.apply(leader, false);
                                QCOMPARE(doc.memory(), expected);
                            }
                            const auto disband = doc.planDisbandUnit(first);
                            doc.apply(disband, true);
                            QCOMPARE(doc.units().size(), 16);
                            doc.apply(disband, false);
                            QCOMPARE(doc.memory(), expected);
                            const auto saved = doc.serialize();
                            doc.apply(change, false);
                            QCOMPARE(doc.serialize(), input);
                            doc.apply(change, true);
                            QCOMPARE(doc.serialize(), saved);
                            doc.apply(change, false);
                            doc.selectSlot(15);
                            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandLegion(index));
                            QCOMPARE(doc.serialize(), input);
                        }
            }
            for (const auto &name : {"legion-order-duplicate.bin", "legion-order-missing.bin",
                                     "legion-member-duplicate.bin", "legion-member-missing.bin",
                                     "legion-member-shared.bin", "legion-member-hidden.bin", "legion-no-commander.bin",
                                     "legion-unrepresented.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandLegion(0));
                QCOMPARE(doc.serialize(), input);
            }
            auto doc = SaveDocument::open(rom, batteryFor("legions.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandLegion(1));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(0));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitLeader(0, 1, project.classData()));
            auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planDisbandLegion(0));
            atomicWrite(work.filePath("legion-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void squadDisbanding() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("army-disband-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing disband fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal disband fixture");
                }
                return out;
            };
            QJsonArray cases, saves;
            for (const auto &name : {"disband.bin", "disband-multi.bin", "disband-last.bin"}) {
                const auto battery = batteryFor(name);
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                const QVector<int> indexes = QString::fromLatin1(name) == "disband.bin"
                                                 ? QVector<int>{0, 2, 3, 5}
                                                 : QVector<int>{2};
                for (const auto &input : {battery, swapped, expanded})
                    for (int slot : {0, 1})
                        for (int index : indexes) {
                            auto doc = SaveDocument::open(rom, input);
                            doc.selectSlot(slot);
                            const auto before = doc.memory();
                            const auto list = doc.units();
                            const int at = 0x1971f0 + index * 25, leader = u8(before, at + 2),
                                      prior = u8(before, 0x1977d8 + index),
                                      available = doc.consumableAvailable(9);
                            const auto change = doc.planDisbandUnit(index);
                            doc.apply(change, true);
                            auto expected = before;
                            expected[at + 1] = char(u8(before, at + 1) & ~1);
                            expected[0x193bc0 + leader * 56 + 51] =
                                char(u8(before, 0x193bc0 + leader * 56 + 51) & ~2);
                            int returned = 0;
                            for (int i = 0; i < 5; ++i) {
                                const int id = u8(before, at + 2 + i);
                                if (id >= 100) {
                                    returned += id == 101 ? 2 : 1;
                                    put16(expected, 0x190f84 + (id - 100) * 2, 0);
                                    expected[0x191074 + id - 100] = 0;
                                }
                            }
                            expected[0x190f81] = char(u8(before, 0x190f81) + returned);
                            expected[0x1977d8 + index] = 31;
                            for (const auto &unit : list)
                                if (unit.index != index &&
                                    u8(before, 0x1977d8 + unit.index) > prior)
                                    expected[0x1977d8 + unit.index] =
                                        char(u8(before, 0x1977d8 + unit.index) - 1);
                            QCOMPARE(doc.memory(), expected);
                            QCOMPARE(doc.units().size(), list.size() - 1);
                            QVERIFY(doc.reserveMembers().contains(leader));
                            QCOMPARE(doc.consumableAvailable(9), available + 1);
                            doc.validateUnitSupplyEdits(project);
                            auto reopened = SaveDocument::open(rom, doc.serialize());
                            reopened.selectSlot(slot);
                            QCOMPARE(reopened.memory(), expected);
                            if (input == battery && slot == 0) {
                                cases << QJsonObject{
                                    {"index", index},
                                    {"before",
                                     QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                    {"record", QString::fromLatin1(expected.mid(at, 25).toHex())},
                                    {"order",
                                     QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                    {"afterOrder",
                                     QString::fromLatin1(expected.mid(0x1977d8, 30).toHex())},
                                    {"characters",
                                     QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                    {"afterCharacters",
                                     QString::fromLatin1(expected.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(before.mid(0x190f80, 364).toHex())},
                                    {"afterSoldiers",
                                     QString::fromLatin1(expected.mid(0x190f80, 364).toHex())},
                                    {"inventory",
                                     QString::fromLatin1(before.mid(0x193ac0, 160).toHex())}};
                                saves << QJsonObject{
                                    {"payload",
                                     QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                                    {"units",
                                     QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                    {"order",
                                     QString::fromLatin1(expected.mid(0x1977d8, 30).toHex())},
                                    {"characters",
                                     QString::fromLatin1(expected.mid(0x193bc0, 5600).toHex())},
                                    {"soldiers",
                                     QString::fromLatin1(expected.mid(0x190f80, 364).toHex())}};
                            }
                            // The released slot and number can be reused without restoring stale
                            // members/items.
                            const auto recreate = doc.planCreateUnit(10, 4, project.classData());
                            doc.apply(recreate, true);
                            QCOMPARE(doc.units().size(), list.size());
                            bool found = false;
                            for (const auto &unit : doc.units())
                                if (unit.members[0] == 10) {
                                    found = true;
                                    QCOMPARE(unit.index,
                                             QString::fromLatin1(name) == "disband-last.bin"
                                                 ? 0
                                                 : index);
                                    QCOMPARE(unit.supplies, QVector<int>(10, 0));
                                }
                            QVERIFY(found);
                            doc.apply(recreate, false);
                            QCOMPARE(doc.memory(), expected);
                            const auto saved = doc.serialize();
                            doc.apply(change, false);
                            QCOMPARE(doc.serialize(), input);
                            doc.apply(change, true);
                            QCOMPARE(doc.serialize(), saved);
                            doc.apply(change, false);
                            doc.selectSlot(15);
                            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(index));
                            QCOMPARE(doc.serialize(), input);
                        }
            }
            for (const auto &name :
                 {"disband-overflow.bin", "disband-duplicate.bin", "disband-fixed.bin",
                  "disband-missing.bin", "disband-order.bin", "disband-legion.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(2));
                if (QString::fromLatin1(name) == "disband-legion.bin")
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(5));
                QCOMPARE(doc.serialize(), input);
            }
            auto flagged = SaveDocument::open(rom, batteryFor("disband-flags.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, flagged.planDisbandUnit(0));
            auto doc = SaveDocument::open(rom, batteryFor("disband.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(4));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planDisbandUnit(1));
            auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planDisbandUnit(0));
            atomicWrite(work.filePath("disband-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void squadInventoryStock() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto project = Project::fromRom(rom);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess p;
                p.start("node", {QFINDTESTDATA("squad-stock-mips.cjs"), work.path(), mode});
                if (!p.waitForFinished(30000) || p.exitCode())
                    throw Error(QString::fromUtf8(p.readAllStandardError()));
                qInfo().noquote() << p.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing stock fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal stock fixture");
                }
                return out;
            };
            const auto battery = batteryFor("stock-two.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            QJsonArray cases;
            auto reference = [&](const SaveDocument &doc, int slot) {
                QJsonArray assigned, available;
                const auto &ram = doc.memory();
                for (int row = 0; row < 40; ++row) {
                    int count = 0, id = u16(ram, 0x193ac0 + row * 4);
                    for (int unit = 0; unit < 30; ++unit)
                        if (id && (u8(ram, 0x1971f0 + unit * 25 + 1) & 1))
                            for (int item = 0; item < 10; ++item)
                                count += u8(ram, 0x1971f0 + unit * 25 + 13 + item) == id;
                    assigned << count;
                    available << (id ? doc.consumableAvailable(id) : 0);
                }
                cases << QJsonObject{
                    {"assigned", assigned},
                    {"available", available},
                    {"units", QString::fromLatin1(ram.mid(0x1971f0, 750).toHex())},
                    {"inventory", QString::fromLatin1(ram.mid(0x193ac0, 160).toHex())},
                    {"payload",
                     QString::fromLatin1(doc.serialize()
                                             .mid(int(lc_save_record_offset(slot, 0)) + 42, 6180)
                                             .toHex())}};
            };
            for (const auto &input : {battery, swapped, expanded})
                for (int slot : {0, 1}) {
                    auto doc = SaveDocument::open(rom, input);
                    doc.selectSlot(slot);
                    const auto before = doc.memory();
                    QCOMPARE(doc.consumableAvailable(24), 2);
                    auto values = doc.units()[0].supplies;
                    values[1] = 24;
                    const auto first = doc.planUnitSupplies(0, values, project);
                    doc.apply(first, true);
                    auto expected = before;
                    expected[0x1971f0 + 14] = 24;
                    QCOMPARE(doc.memory(), expected);
                    QCOMPARE(doc.consumableAvailable(24), 1);
                    auto secondValues = doc.units()[1].supplies;
                    secondValues[1] = 24;
                    const auto second = doc.planUnitSupplies(2, secondValues, project);
                    doc.apply(second, true);
                    expected[0x1971f0 + 2 * 25 + 14] = 24;
                    QCOMPARE(doc.memory(), expected);
                    QCOMPARE(doc.consumableAvailable(24), 0);
                    auto thirdValues = doc.units().last().supplies;
                    thirdValues[1] = 24;
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(5, thirdValues, project));
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             doc.planEdit(SaveTable::Consumables, 23, "count", 1));
                    QCOMPARE(doc.value(SaveTable::Consumables, 23, "count").toInt(), 2);
                    doc.validateUnitSupplyEdits(project);
                    if (input == battery)
                        reference(doc, slot);
                    auto reopened = SaveDocument::open(rom, doc.serialize());
                    reopened.selectSlot(slot);
                    QCOMPARE(reopened.memory(), expected);
                    values[1] = 0;
                    const auto remove = doc.planUnitSupplies(0, values, project);
                    doc.apply(remove, true);
                    QCOMPARE(doc.consumableAvailable(24), 1);
                    const auto reduce = doc.planEdit(SaveTable::Consumables, 23, "count", 1);
                    doc.apply(reduce, true);
                    QCOMPARE(doc.consumableAvailable(24), 0);
                    doc.validateUnitSupplyEdits(project);
                    if (input == battery)
                        reference(doc, slot);
                    doc.apply(reduce, false);
                    doc.apply(remove, false);
                    doc.apply(second, false);
                    doc.apply(first, false);
                    QCOMPARE(doc.serialize(), input);
                    // The excluded flag81 row still reserves its carried item, as native
                    // bookkeeping does.
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             doc.planEdit(SaveTable::Consumables, 8, "count", 5));
                    const auto six = doc.planEdit(SaveTable::Consumables, 8, "count", 6);
                    doc.apply(six, true);
                    QCOMPARE(doc.consumableAvailable(9), 0);
                    doc.apply(six, false);
                }
            for (const auto &name :
                 {"stock-empty.bin", "stock-missing.bin", "stock-duplicate.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                auto values = doc.units()[0].supplies;
                values[1] = 24;
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSupplies(0, values, project));
                QCOMPARE(doc.serialize(), input);
                doc.validateUnitSupplyEdits(project);
            }
            auto odd = SaveDocument::open(rom, batteryFor("stock-source-overassigned.bin"));
            odd.validateUnitSupplyEdits(project);
            QCOMPARE(odd.consumableAvailable(9), 0);
            auto values = odd.units()[0].supplies;
            values[0] = 0;
            odd.apply(odd.planUnitSupplies(0, values, project), true);
            odd.validateUnitSupplyEdits(project);
            reference(odd, 0);
            auto memory = SaveDocument::open(rom, battery).memory();
            const int at = 0x193ac0 + 23 * 4;
            memory[at + 2] = 3;
            memory[at + 3] = 7;
            auto runtime = SaveDocument::open(rom, memory);
            QCOMPARE(runtime.value(SaveTable::Consumables, 23, "count").toInt(), 7);
            const auto change = runtime.planEdit(SaveTable::Consumables, 23, "count", 9);
            runtime.apply(change, true);
            auto expected = memory;
            expected[at + 3] = 9;
            QCOMPARE(runtime.memory(), expected);
            QVERIFY_THROWS_EXCEPTION(Error,
                                     runtime.planEdit(SaveTable::Consumables, 23, "count", 2));
            runtime.apply(change, false);
            QCOMPARE(runtime.memory(), memory);
            atomicWrite(work.filePath("stock-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}}).toJson());
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void squadCreation() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess process;
                process.start("node", {QFINDTESTDATA("army-create-mips.cjs"), work.path(), mode});
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing creation fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 42, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal creation fixture");
                }
                return out;
            };
            QJsonArray eligibility, cases, saves;
            for (const auto &name : {"create.bin", "create-dead.bin", "create-unavailable.bin"})
                for (int primary : {1, 2, 33, 44, 83, 163})
                    for (int secondary : {primary, 2}) {
                        auto doc = SaveDocument::open(rom, batteryFor(name));
                        doc.apply(doc.planEdit(SaveTable::Characters, 10, "class", primary), true);
                        doc.apply(
                            doc.planEdit(SaveTable::Characters, 10, "secondaryClass", secondary),
                            true);
                        eligibility
                            << QJsonObject{{"primary", primary},
                                           {"secondary", secondary},
                                           {"flags", u8(doc.memory(), 0x193bc0 + 10 * 56 + 51)},
                                           {"hp", u16(doc.memory(), 0x193bc0 + 10 * 56 + 24)},
                                           {"allowed", doc.newUnitLeaders(classes).contains(10)}};
                    }
            for (const auto &name : {"create.bin", "create-empty.bin"}) {
                const bool empty = QString::fromLatin1(name).contains("empty");
                const auto battery = batteryFor(name);
                auto swapped = battery, expanded = battery;
                for (int i = 0; i < swapped.size(); i += 4) {
                    std::swap(swapped[i], swapped[i + 3]);
                    std::swap(swapped[i + 1], swapped[i + 2]);
                }
                QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                             unsigned(expanded.size())));
                for (const auto &input : {battery, swapped, expanded})
                    for (int slot : {0, 1}) {
                        auto doc = SaveDocument::open(rom, input);
                        doc.selectSlot(slot);
                        const auto before = doc.memory();
                        const int index = empty ? 0 : 6, label = empty ? 1 : 2,
                                  count = empty ? 0 : 5;
                        for (int leader : {10, 11})
                            for (int cell : {0, 4, 8}) {
                                const auto change = doc.planCreateUnit(leader, cell, classes);
                                doc.apply(change, true);
                                auto expected = before;
                                QByteArray record(25, 0);
                                record[0] = char(label);
                                record[1] = 1;
                                record[2] = char(leader);
                                record[7] = char(cell);
                                const int primary = u8(before, 0x193bc0 + leader * 56 + 17);
                                record[24] = char(classes.movementType(primary, primary));
                                expected.replace(0x1971f0 + index * 25, 25, record);
                                expected[0x1977d8 + index] = char(count);
                                expected[0x193bc0 + leader * 56 + 51] =
                                    char(u8(before, 0x193bc0 + leader * 56 + 51) | 2);
                                QCOMPARE(doc.memory(), expected);
                                QCOMPARE(doc.units().size(), count + 1);
                                QVERIFY(!doc.reserveMembers().contains(leader));
                                auto reopened = SaveDocument::open(rom, doc.serialize());
                                reopened.selectSlot(slot);
                                QCOMPARE(reopened.memory(), expected);
                                if (input == battery && slot == 0) {
                                    cases << QJsonObject{
                                        {"leader", leader},
                                        {"cell", cell},
                                        {"index", index},
                                        {"record", QString::fromLatin1(record.toHex())},
                                        {"before",
                                         QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                        {"characters",
                                         QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                        {"order",
                                         QString::fromLatin1(before.mid(0x1977d8, 30).toHex())},
                                        {"afterOrder",
                                         QString::fromLatin1(expected.mid(0x1977d8, 30).toHex())}};
                                    saves << QJsonObject{
                                        {"payload", QString::fromLatin1(
                                                        doc.serialize().mid(0x3a, 6180).toHex())},
                                        {"units",
                                         QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                        {"characters",
                                         QString::fromLatin1(expected.mid(0x193bc0, 5600).toHex())},
                                        {"order",
                                         QString::fromLatin1(expected.mid(0x1977d8, 30).toHex())}};
                                }
                                const auto saved = doc.serialize();
                                doc.apply(change, false);
                                QCOMPARE(doc.serialize(), input);
                                doc.apply(change, true);
                                QCOMPARE(doc.serialize(), saved);
                                doc.apply(change, false);
                            }
                        QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateUnit(10, 9, classes));
                        QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateUnit(100, 4, classes));
                        doc.selectSlot(15);
                        QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateUnit(10, 4, classes));
                        QCOMPARE(doc.serialize(), input);
                    }
            }
            for (const auto &name :
                 {"create-full.bin", "create-no-record.bin", "create-order-conflict.bin",
                  "create-number-conflict.bin", "create-dead.bin", "create-unavailable.bin",
                  "create-no-command.bin", "create-flag-conflict.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                const int leader = QString::fromLatin1(name) == "create-full.bin" ? 40 : 10;
                QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateUnit(leader, 4, classes));
                QCOMPARE(doc.serialize(), input);
            }
            auto doc = SaveDocument::open(rom, batteryFor("create.bin"));
            QVERIFY_THROWS_EXCEPTION(Error, doc.planCreateUnit(1, 4, classes));
            auto raw = SaveDocument::open(rom, doc.memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planCreateUnit(10, 4, classes));
            atomicWrite(
                work.filePath("create-verify.json"),
                QJsonDocument(
                    QJsonObject{{"eligibility", eligibility}, {"cases", cases}, {"saves", saves}})
                    .toJson());
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void directMemberExchanges() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto classes = Project::fromRom(rom).classData();
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess process;
                process.start("node", {QFINDTESTDATA("army-exchange-mips.cjs"), work.path(), mode});
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing exchange fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 0x2a, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal exchange fixture");
                }
                return out;
            };
            const auto battery = batteryFor("exchange.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            QJsonArray cases, saves;
            const QVector<QVector<int>> operations{{2, 1, 0, -3}, {3, 1, 0, -2}, {2, 1, 3, 1},
                                                   {0, 1, 2, 1},  {3, 1, 0, 1},  {0, 1, 5, 1},
                                                   {0, 1, 5, -3}, {2, 1, 5, -7}};
            for (const auto &input : {battery, swapped, expanded})
                for (int slot : {0, 1}) {
                    auto doc = SaveDocument::open(rom, input);
                    doc.selectSlot(slot);
                    const auto before = doc.memory();
                    for (const auto &op : operations) {
                        const int from = op[0], member = op[1], to = op[2], target = op[3];
                        const int a = 0x1971f0 + from * 25, b = 0x1971f0 + to * 25;
                        const auto change =
                            target < 0
                                ? doc.planUnitTransfer(from, member, to, -target - 1, classes)
                                : doc.planUnitSwap(from, member, to, target, classes);
                        auto expected = before;
                        int toCell;
                        if (target < 0) {
                            int free = 1;
                            while (u8(before, b + 2 + free))
                                ++free;
                            toCell = -target - 1;
                            expected[b + 2 + free] = before[a + 2 + member];
                            expected[b + 7 + free] = char(toCell);
                            expected[a + 2 + member] = 0;
                            expected[a + 7 + member] = 0;
                        } else {
                            toCell = u8(before, b + 7 + target);
                            expected[a + 2 + member] = before[b + 2 + target];
                            expected[b + 2 + target] = before[a + 2 + member];
                        }
                        for (int base : {a, b}) {
                            int movement = 2;
                            for (int i = 0; i < 5; ++i)
                                if (u8(expected, base + 2 + i) >= 100)
                                    movement = 7;
                            expected[base + 24] = char(movement);
                        }
                        doc.apply(change, true);
                        QCOMPARE(doc.memory(), expected); // Includes every Soldier, roster,
                                                          // inventory and unknown byte.
                        auto reopened = SaveDocument::open(rom, doc.serialize());
                        reopened.selectSlot(slot);
                        QCOMPARE(reopened.memory(), expected);
                        if (input == battery) {
                            cases << QJsonObject{
                                {"from", from},
                                {"to", to},
                                {"fromCell", u8(before, a + 7 + member)},
                                {"toCell", toCell},
                                {"before", QString::fromLatin1(before.mid(0x1971f0, 750).toHex())},
                                {"after", QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                {"characters",
                                 QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                                {"soldiers",
                                 QString::fromLatin1(before.mid(0x190f80, 364).toHex())}};
                            saves << QJsonObject{
                                {"payload",
                                 QString::fromLatin1(
                                     doc.serialize()
                                         .mid(int(lc_save_record_offset(slot, 0)) + 42, 6180)
                                         .toHex())},
                                {"units", QString::fromLatin1(expected.mid(0x1971f0, 750).toHex())},
                                {"characters",
                                 QString::fromLatin1(expected.mid(0x193bc0, 5600).toHex())},
                                {"soldiers",
                                 QString::fromLatin1(expected.mid(0x190f80, 364).toHex())}};
                        }
                        const auto output = doc.serialize();
                        doc.apply(change, false);
                        QCOMPARE(doc.serialize(), input);
                        doc.apply(change, true);
                        QCOMPARE(doc.serialize(), output);
                        doc.apply(change, false);
                    }
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(2, 0, 0, 1, classes));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(2, 1, 4, 1, classes));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(2, 1, 0, 0, classes));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(2, 1, 4, 0, classes));
                    doc.selectSlot(15);
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(2, 1, 0, 1, classes));
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(2, 1, 0, 2, classes));
                    QCOMPARE(doc.serialize(), input);
                }
            for (const auto &name :
                 {"exchange-duplicate.bin", "exchange-zero-hp.bin", "exchange-overload.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitTransfer(2, 1, 0, 2, classes));
                if (QString::fromLatin1(name) != "exchange-overload.bin")
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(2, 1, 0, 1, classes));
                QCOMPARE(doc.serialize(), input);
            }
            for (const auto &name : {"exchange-missing.bin", "exchange-leader-flag.bin"}) {
                const auto input = batteryFor(name);
                auto doc = SaveDocument::open(rom, input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(2, 1, 0, 1, classes));
                QCOMPARE(doc.serialize(), input);
            }
            auto raw = SaveDocument::open(rom, SaveDocument::open(rom, battery).memory());
            QVERIFY_THROWS_EXCEPTION(Error, raw.planUnitTransfer(2, 1, 0, 2, classes));
            atomicWrite(work.filePath("exchange-verify.json"),
                        QJsonDocument(QJsonObject{{"cases", cases}, {"saves", saves}}).toJson());
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void squadLeaderChanges() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto classes = Project::fromRom(rom).classData();
            QCOMPARE(classes.append(33, classes.entries()[33].name), 165);
            QTemporaryDir work;
            auto run = [&](const QString &mode) {
                QProcess process;
                process.start("node", {QFINDTESTDATA("army-leader-mips.cjs"), work.path(), mode});
                if (!process.waitForFinished(30000) || process.exitCode())
                    throw Error(QString::fromUtf8(process.readAllStandardError()));
                qInfo().noquote() << process.readAllStandardOutput();
            };
            run("generate");
            auto batteryFor = [&](const QString &name) {
                QFile file(work.filePath(name));
                if (!file.open(QIODevice::ReadOnly))
                    throw Error("Missing leader fixture");
                const auto payload = file.readAll();
                QByteArray out(0x10000, char(0x6d));
                for (int slot : {0, 1, 15}) {
                    const int at = int(lc_save_record_offset(slot, 0));
                    out[at + 12] = 0;
                    out.replace(at + 0x2a, 6180, payload);
                    if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(out.data() + at),
                                             lc_save_record_size(slot, 0), slot, 0))
                        throw Error("Cannot seal leader fixture");
                }
                return out;
            };
            QJsonArray fixed, types, cases, saves;
            QByteArray records;
            for (const auto &entry : classes.entries())
                records += entry.record;
            for (int id = 0; id < 256; ++id)
                for (int primary : {0, 2, 80, 81, 82, 83, 84, 163, 164, 255})
                    fixed << QJsonObject{
                        {"member", id},
                        {"primary", primary},
                        {"fixed", lordly_army_fixed_leader(quint8(id), quint8(primary))}};
            for (int primary = 0; primary < classes.entries().size(); ++primary)
                for (int secondary = 0; secondary < classes.entries().size(); ++secondary)
                    types << QJsonArray{primary, secondary,
                                        classes.commandType(primary, secondary)};
            QVERIFY_THROWS_EXCEPTION(Error, classes.commandType(166, 1));
            const auto battery = batteryFor("unit-payload.bin");
            auto swapped = battery, expanded = battery;
            for (int i = 0; i < swapped.size(); i += 4) {
                std::swap(swapped[i], swapped[i + 3]);
                std::swap(swapped[i + 1], swapped[i + 2]);
            }
            QVERIFY(lc_save_expand_image(reinterpret_cast<unsigned char *>(expanded.data()),
                                         unsigned(expanded.size())));
            auto record = [&](const SaveDocument &doc, const QByteArray &before, int cell,
                              bool capture) {
                cases << QJsonObject{
                    {"before", QString::fromLatin1(before.mid(0x1971f0, 25).toHex())},
                    {"after", QString::fromLatin1(doc.memory().mid(0x1971f0, 25).toHex())},
                    {"characters", QString::fromLatin1(before.mid(0x193bc0, 5600).toHex())},
                    {"cell", cell}};
                if (capture) {
                    const auto bytes = doc.serialize();
                    const auto at = lc_save_record_offset(doc.selectedSlot(), doc.expanded());
                    saves << QJsonObject{
                        {"payload", QString::fromLatin1(bytes.mid(int(at) + 0x2a, 6180).toHex())},
                        {"characters",
                         QString::fromLatin1(doc.memory().mid(0x193bc0, 5600).toHex())},
                        {"units", QString::fromLatin1(doc.memory().mid(0x1971f0, 750).toHex())}};
                }
            };
            for (const auto &input : {battery, swapped, expanded})
                for (int member : {1, 2}) {
                    auto doc = SaveDocument::open(rom, input, 166);
                    const auto before = doc.memory();
                    const auto change = doc.planUnitLeader(0, member, classes);
                    doc.apply(change, true);
                    auto expected = before;
                    const QVector<int> ids =
                        member == 1 ? QVector<int>{2, 1, 3} : QVector<int>{3, 1, 2};
                    const QVector<int> cells =
                        member == 1 ? QVector<int>{4, 0, 8} : QVector<int>{8, 0, 4};
                    for (int i = 0; i < 3; ++i) {
                        expected[0x1971f0 + 2 + i] = char(ids[i]);
                        expected[0x1971f0 + 7 + i] = char(cells[i]);
                    }
                    expected[0x193bc0 + 56 + 51] = char(u8(before, 0x193bc0 + 56 + 51) & ~2);
                    expected[0x193bc0 + ids[0] * 56 + 51] =
                        char(u8(before, 0x193bc0 + ids[0] * 56 + 51) | 2);
                    QCOMPARE(doc.memory(), expected);
                    QCOMPARE(doc.units()[0].unity, 23);
                    QCOMPARE(SaveDocument::open(rom, doc.serialize(), 166).memory(), expected);
                    record(doc, before, cells[0], input != swapped);
                    doc.apply(change, false);
                    QCOMPARE(doc.serialize(), input);
                    doc.selectSlot(1);
                    const auto second = doc.planUnitLeader(0, member, classes);
                    doc.apply(second, true);
                    QCOMPARE(doc.memory(), expected);
                    doc.apply(second, false);
                    QCOMPARE(doc.serialize(), input);
                    doc.selectSlot(15);
                    QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitLeader(0, member, classes));
                }
            for (const auto &name :
                 {"fixed-leader-81.bin", "fixed-leader-82.bin", "fixed-leader-83.bin",
                  "fixed-leader-163.bin", "commander-bad.bin", "commander-pair.bin",
                  "leader-conflict.bin", "leader-duplicate.bin", "leader-missing.bin",
                  "leader-no-command.bin", "leader-dead.bin", "leader-incapacitated.bin"}) {
                const auto doc = SaveDocument::open(rom, batteryFor(name), 166);
                const auto before = doc.serialize();
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitLeader(0, 1, classes));
                QCOMPARE(doc.serialize(), before);
            }
            for (const auto &name : {"commander-good.bin", "commander-new.bin"}) {
                auto doc = SaveDocument::open(rom, batteryFor(name), 166);
                const auto before = doc.memory();
                doc.apply(doc.planUnitLeader(0, 1, classes), true);
                QCOMPARE(doc.units()[0].members[0], 2);
                QCOMPARE(u8(doc.memory(), 0x1971f1), quint8(3));
                QCOMPARE(doc.units()[0].unity, 23);
                QCOMPARE(doc.memory().mid(0x1969b8, 110), before.mid(0x1969b8, 110));
                record(doc, before, 4, true);
                QCOMPARE(SaveDocument::open(rom, doc.serialize(), 166).memory(), doc.memory());
            }
            const auto doc = SaveDocument::open(rom, battery, 166);
            for (const auto pair : {qMakePair(0, 0), qMakePair(0, 4), qMakePair(2, 1),
                                    qMakePair(4, 1), qMakePair(30, 1)})
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitLeader(pair.first, pair.second, classes));
            const auto raw = SaveDocument::open(rom, doc.memory(), 166);
            QVERIFY_THROWS_EXCEPTION(Error, raw.planUnitLeader(0, 1, classes));
            QFile reference(work.filePath("leader-verify.json"));
            QVERIFY(reference.open(QIODevice::WriteOnly));
            reference.write(
                QJsonDocument(QJsonObject{{"fixed", fixed},
                                          {"types", types},
                                          {"classes", QString::fromLatin1(records.toHex())},
                                          {"cases", cases},
                                          {"saves", saves}})
                    .toJson());
            reference.close();
            run("verify");
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
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
                QCOMPARE(doc.memory(), swappedMemory);
                QCOMPARE(SaveDocument::open(rom, doc.serialize()).memory(), swappedMemory);
                doc.apply(swap, false);
                QCOMPARE(doc.serialize(), input);
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 0, 5, 1, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 0, 2, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 5, 4, classes));
                QVERIFY_THROWS_EXCEPTION(Error, doc.planUnitSwap(0, 1, 4, 1, classes));
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
                for (int offset : {3, 8})
                    expected[0x1971f0 + offset] = 0;
                const int dest = 0x1971f0 + 5 * 25;
                expected[dest + 5] = 2;
                expected[dest + 10] = 2;
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
                                         doc.planEdit(SaveTable::Characters, 1, "weapon", 511));
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
