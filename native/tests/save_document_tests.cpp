#include "engine/project.h"
#include "engine/save_document.h"
#include "game/classes/save_format.h"
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
                QCOMPARE(doc.units().size(), 4);
                QCOMPARE(doc.units()[0].index, 0);
                QCOMPARE(doc.units()[1].index, 2);
                QCOMPARE(doc.units()[1].members[1], 100);
                QCOMPARE(doc.units()[0].cells, QVector<int>({0, 4, 8, 0, 0}));
                const auto before = doc.memory();
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
                QVERIFY_THROWS_EXCEPTION(Error,
                                         doc.planUnitEdit(0, {1, 2, 8, 0, 0}, 99, 3, classes));
                doc.apply(edit, false);
                QCOMPARE(doc.serialize(), input);
            }
            auto raw = SaveDocument::open(rom, read("unit-ram.bin"));
            QVERIFY(!raw.canEditUnits());
            QCOMPARE(raw.units().size(), 4);
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
                                         {"expected", classes.unitSize(primary, secondary)}};
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
            atomicWrite(
                work.filePath("unit-verify.json"),
                QJsonDocument(
                    QJsonObject{
                        {"payload", QString::fromLatin1(doc.serialize().mid(0x3a, 6180).toHex())},
                        {"units", QString::fromLatin1(doc.memory().mid(0x1971f0, 125).toHex())},
                        {"cases", cases},
                        {"soldierSize", std::max(1, classes.unitSize(1, 1))},
                        {"sizes", sizes}})
                    .toJson());
            run(true);
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
