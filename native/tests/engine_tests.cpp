#include "engine/project.h"
#include "engine/resource.h"
#include "rebuild/compiler.h"
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

using namespace lordly;
static QByteArray syntheticRom() {
    QByteArray data(0x2800000, char(0));
    put32(data, 0, 0x80371240);
    data.replace(0x20, 12, "OgreBattle64");
    data.replace(0x3b, 2, "NO");
    data[0x3e] = 'E';
    return data;
}
class EngineTests : public QObject {
    Q_OBJECT
  private slots:
    void byteOrders() {
        const auto expected = QByteArray::fromHex("8037124011223344") + QByteArray(56, 0);
        const auto swapped = QByteArray::fromHex("3780401222114433") + QByteArray(56, 0);
        const auto little = QByteArray::fromHex("4012378044332211") + QByteArray(56, 0);
        for (const auto &input : {expected, swapped, little}) {
            auto rom = Rom::fromBytes(input);
            QCOMPARE(rom.bytes(), expected);
            QCOMPARE(rom.serialize(rom.bytes()), input);
        }
        QVERIFY_THROWS_EXCEPTION(Error, Rom::fromBytes(expected + char(0)));
        QVERIFY_THROWS_EXCEPTION(Error, Rom::fromBytes(QByteArray(64, 0)));
    }
    void compressionCommands() {
        QCOMPARE(decodeLzss(QByteArray::fromHex("42414243"), 3), QByteArray("ABC"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("20"), 2), QByteArray(2, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0102"), 5), QByteArray(5, char(-1)));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0201"), 4), QByteArray(4, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("424142438802"), 7), QByteArray("ABCABCA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("4041100000"), 5), QByteArray("AAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("404100000000"), 6), QByteArray("AAAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("8000"), 3), QByteArray(3, 0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("030f4042"), 1), QByteArray("B"));
        QVERIFY_THROWS_EXCEPTION(Error, decodeLzss(QByteArray::fromHex("424142"), 3));
        QVERIFY_THROWS_EXCEPTION(Error, decodeLzss(QByteArray::fromHex("80"), 3));
        QByteArray mixed;
        for (int i = 0; i < 1024; ++i)
            mixed.append(char(i));
        mixed += QByteArray(1000, 0) + QByteArray(700, char(-1));
        QCOMPARE(decodeLzss(encodeLzss(mixed), mixed.size()), mixed);
    }
    void recordFramingAndPreservation() {
        auto data = syntheticRom();
        const int knight = 5;
        auto header = 0x5dad8 + (knight + 1) * 72 - 12;
        data[header + 7] = char(-61);
        put16(data, header + 8, 123);
        put16(data, header + 12, 456);
        auto p = Project::fromRom(Rom::fromBytes(data));
        const auto &row = p.record(Table::Classes, knight);
        QCOMPARE(u16(row.bytes, 8), quint16(123));
        QCOMPARE(u16(row.bytes, 12), quint16(456));
        int str = -1;
        for (int n = 0; n < fields(Table::Classes).size(); ++n)
            if (fields(Table::Classes)[n].key == "strBase")
                str = n;
        QVERIFY(str >= 0);
        p.setValue(Table::Classes, knight, str, 900);
        auto rebuilt = p.rebuildRecords();
        auto expected = data;
        put16(expected, header + 12, 900);
        QCOMPARE(rebuilt, expected);
        QVERIFY_THROWS_EXCEPTION(Error, p.setValue(Table::Classes, knight, str, 65536));
        QCOMPARE(p.rom().bytes(), data);
    }
    void projectPersistence() {
        auto rom = Rom::fromBytes(syntheticRom());
        auto p = Project::fromRom(rom);
        p.setValue(Table::Classes, 5, 0, 2);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        p.saveFolder(dir.path());
        auto saved = Project::readProject(dir.path());
        saved["extensionExample"] = QJsonObject{{"keep", 42}};
        auto restored = Project::fromRom(rom);
        restored.applyManifest(saved);
        QCOMPARE(restored.value(Table::Classes, 5, 0), 2);
        QCOMPARE(restored.manifest()["extensionExample"], saved["extensionExample"]);
        auto archive = dir.filePath("share.lcproject");
        restored.saveArchive(archive);
        auto packed = Project::readProject(archive);
        QVERIFY(!packed.contains("sourcePath"));
        QCOMPARE(packed["records"], saved["records"]);
        auto wrong = saved;
        wrong["sourceSha256"] = "wrong";
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyManifest(wrong));
        QCOMPARE(restored.value(Table::Classes, 5, 0), 2);
    }
    void retailChecksumAndCompiledExport() {
        try {
            auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Set LORDLY_TEST_ROM to run the private-ROM integration check");
            auto source = Rom::open(path);
            source.requireSupported();
            QCOMPARE(QString::fromLatin1(sha256(source.bytes())),
                     QString("571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a"));
            auto checked = source.bytes();
            Rom::updateChecksum(checked);
            QCOMPARE(u32(checked, 0x10), quint32(0xe6419bc5));
            QCOMPARE(u32(checked, 0x14), quint32(0x69011de3));
            QCOMPARE(checked, source.bytes());
            auto project = Project::fromRom(source);
            auto prior = project.value(Table::Classes, 5, 4);
            project.setValue(Table::Classes, 5, 4, (prior + 1) & 255);
            QTemporaryDir build;
            QVERIFY(build.isValid());
            auto result =
                rebuild(project, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto expected = project.rebuildRecords();
            Rom::updateChecksum(expected);
            QCOMPARE(result.image, expected);
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(candidates));
            QTemporaryDir candidate(candidates + "/native-export-XXXXXX");
            QVERIFY(candidate.isValid());
            auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(project, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.value(Table::Classes, 5, 4), (prior + 1) & 255);
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
            QVERIFY_THROWS_EXCEPTION(Error, exportRom(project, result, path));
            QVERIFY(QFileInfo::exists(build.filePath("classes_records.c")));
            QVERIFY(QFileInfo::exists(build.filePath("func_000bbd50.compiler.s")));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void browserProjectImport() {
        QJsonObject legacy{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches",
             QJsonObject{
                 {"classDefs",
                  QJsonObject{
                      {"5", QJsonObject{{"record_index", 6},
                                        {"bytes", QJsonObject{{"0", 1}, {"1", 44}, {"64", 2}}}}}}},
                 {"items", QJsonObject{{"7", QJsonObject{{"str", -4},
                                                         {"growthHpStr", 2},
                                                         {"growthUnknown", 1},
                                                         {"growthInt", 3},
                                                         {"growthAgi", 1}}}}},
                 {"item_prices", QJsonObject{{"7", 1234}}},
                 {"art", QJsonObject{{"schemaVersion", 5}, {"avatars", QJsonObject{}}}}}}};
        auto p = Project::fromRom(Rom::fromBytes(syntheticRom()));
        p.importLegacy(legacy);
        QCOMPARE(u16(p.record(Table::Classes, 5).bytes, 12), quint16(300));
        QCOMPARE(u8(p.record(Table::Classes, 5).bytes, 4), quint8(2));
        QCOMPARE(u8(p.record(Table::Items, 7).bytes, 10), quint8(252));
        QCOMPARE(u8(p.record(Table::Items, 7).bytes, 24), quint8(0x9d));
        QCOMPARE(u16(p.record(Table::Items, 7).bytes, 8), quint16(1234));
        QVERIFY(p.pendingLegacySections().isEmpty());
        QCOMPARE(p.manifest()["legacyImport"].toObject(), legacy);
        auto before = p.manifest();
        auto malformed = legacy;
        auto patches = malformed["patches"].toObject();
        patches["items"] = QJsonObject{{"7", QJsonObject{{"str", 200}}}};
        malformed["patches"] = patches;
        QVERIFY_THROWS_EXCEPTION(Error, p.importLegacy(malformed));
        QCOMPARE(p.manifest(), before);
        patches = legacy["patches"].toObject();
        patches["tools"] = QJsonObject{{"futureTool", false}};
        legacy["patches"] = patches;
        p.importLegacy(legacy);
        QCOMPARE(p.pendingLegacySections(), QStringList{"tools"});
    }
    void retailResourceCompression() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            const auto rom = Rom::open(path);
            const std::array<std::pair<int, int>, 5> resources = {{{0x34fa4e, 9710},
                                                                   {0x352b88, 2064},
                                                                   {0x353f92, 6408},
                                                                   {0x356180, 3154},
                                                                   {0x3a960c, 384}}};
            for (auto [relative, allocation] : resources) {
                auto resource = Resource::load(rom, 0x20248c2 + relative, allocation);
                auto rebuilt = resource.container(resource.decoded);
                QCOMPARE(rebuilt.size(), allocation);
                QCOMPARE(decodeLzss(rebuilt.mid(8, u32(rebuilt, 0) - 4), u32(rebuilt, 4)),
                         resource.original);
            }
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void textEncodingAndReferences() {
        QCOMPARE(encodeText("First\nSecond", false),
                 QByteArray::fromHex("466972737481975365636f6e64"));
        QCOMPARE(encodeText("A\nB\nC", true), QByteArray::fromHex("0e411030420f81970e431030200f"));
        QCOMPARE(decodeText(QByteArray::fromHex("41819632383030819742876a")),
                 QString("A{REF:2800}\nB{CTRL:876A}"));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("{BYTE:0D}{BYTE:0A}", false));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("{REF:9999}", false));
        QVERIFY_THROWS_EXCEPTION(Error, encodeText("1\n2\n3\n4\n5", false));
        TextBank bank;
        bank.spec.id = "classes";
        auto original = QByteArray("Knight") + QByteArray::fromHex("8197819632383030");
        bank.entries = {{QByteArray(), QByteArray()}, {original, original}};
        QCOMPARE(bank.text(1), QString("Knight"));
        bank.setText(1, "Armored fighter");
        QCOMPARE(bank.entries[1].bytes,
                 QByteArray("Armored fighter") + QByteArray::fromHex("8197819632383030"));
        QCOMPARE(bank.entries[1].original, original);
    }
    void retailDescriptionExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            p.loadTexts();
            auto originalBanks = p.textBanks();
            int editedId = -1;
            for (int id = 1; id <= 164; ++id)
                if (originalBanks["classes"].entries[id].bytes.contains(
                        QByteArray::fromHex("8196"))) {
                    editedId = id;
                    break;
                }
            QVERIFY(editedId > 0);
            auto original = originalBanks["classes"].entries[editedId].bytes;
            auto ref = original.indexOf(QByteArray::fromHex("8196"));
            QVERIFY(ref >= 0);
            auto suffix = original.mid(
                ref >= 2 && original.mid(ref - 2, 2) == QByteArray::fromHex("8197") ? ref - 2
                                                                                    : ref);
            p.setText("classes", editedId, "Armored fighter");
            QCOMPARE(p.textBanks()["classes"].entries[editedId].bytes,
                     QByteArray("Armored fighter") + suffix);
            QTemporaryDir projectDir;
            p.saveFolder(projectDir.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(projectDir.path()));
            QCOMPARE(restored.textBanks()["classes"].serialized(),
                     p.textBanks()["classes"].serialized());
            QTemporaryDir build;
            auto result = rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir outputDir(candidates + "/native-text-XXXXXX");
            QVERIFY(outputDir.isValid());
            auto output = outputDir.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            reopened.loadTexts();
            for (auto bank = originalBanks.begin(); bank != originalBanks.end(); ++bank) {
                for (int i = 0; i < bank->entries.size(); ++i) {
                    auto expected = bank.key() == "classes" && i == editedId
                                        ? QByteArray("Armored fighter") + suffix
                                        : bank->entries[i].bytes;
                    QCOMPARE(reopened.textBanks()[bank.key()].entries[i].bytes, expected);
                }
            }
            QCOMPARE(u32(result.image, p.textBanks()["classes"].resource.romOffset), quint32(6403));
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QVERIFY(QFileInfo::exists(build.filePath("descriptions_classes.compiler.s")));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "engine_tests.moc"
