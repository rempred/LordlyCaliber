#include "engine/class_data.h"
#include "engine/project.h"
#include "engine/resource.h"
#include "rebuild/class_runtime.h"
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
    void expandableClassData() {
        auto bytes = syntheticRom();
        bytes.replace(0x65dbc, 7, QByteArray("Knight\0", 7));
        for (int i = 0; i < 72; ++i)
            bytes[0x5dacc + i] = char(i ^ 0xa6);
        for (int id = 0; id < 165; ++id) {
            const auto record = 0x5db14 + id * 72;
            for (int field = 0; field < 72; ++field)
                bytes[record + field] = char(id * 13 + field);
            put32(bytes, record, 0x8018febc);
            for (int field = 0; field < 6; ++field)
                bytes[0x64be0 + id * 6 + field] = char(id + field);
        }
        bytes[0xdbe18 + 90] = 24;
        auto classes = ClassData::loadRetail(Rom::fromBytes(bytes));
        QCOMPARE(classes.entries().size(), 165);
        const auto retail = classes.build(0x80243db0);
        QCOMPARE(retail.layout.records, quint32(0x80243df8));
        QCOMPARE(retail.layout.appearances, quint32(0x80246c60));
        QCOMPARE(retail.layout.aliases, quint32(0x80247040));
        QCOMPARE(retail.layout.names, quint32(0x802470f0));
        QCOMPARE(retail.layout.discovery, quint32(0x80247100));
        QCOMPARE(retail.layout.end, quint32(0x80247120));
        QCOMPARE(classes.append(90, "Sentinel"), 165);
        QCOMPARE(classes.entries()[165].namedAlias, quint8(0));
        QCOMPARE(classes.entries()[90].namedAlias, quint8(24));
        for (int id = 166; id < 256; ++id)
            QCOMPARE(classes.append(5, "Knight"), id);
        const auto built = classes.build(0x80243db0);
        const auto &layout = built.layout;
        QCOMPARE(layout.count, 256);
        QCOMPARE(built.bytes.first(72), bytes.mid(0x5dacc, 72));
        QCOMPARE(built.bytes.mid(layout.discovery - layout.indexBase, 32), QByteArray(32, 0));
        QVERIFY(layout.end <= 0x80400000);
        for (int id = 0; id < 256; ++id) {
            auto source = id < 165 ? id : id == 165 ? 90 : 5;
            const auto record = 72 + id * 72;
            QCOMPARE(built.bytes.mid(record + 4, 68), bytes.mid(0x5db14 + source * 72 + 4, 68));
            QCOMPARE(built.bytes.mid(layout.appearances - layout.indexBase + id * 6, 6),
                     bytes.mid(0x64be0 + source * 6, 6));
            const auto pointer = u32(built.bytes, record);
            QVERIFY(pointer >= layout.names && pointer < layout.discovery);
            QVERIFY((pointer & 3) == 0);
            const auto offset = pointer - layout.indexBase;
            const auto end = built.bytes.indexOf(char(0), offset);
            QCOMPARE(built.bytes.mid(offset, end - offset),
                     QByteArray(id == 165 ? "Sentinel" : "Knight"));
        }
        // Equal names share storage, including a freshly appended class.
        QCOMPARE(u32(built.bytes, 72), u32(built.bytes, 72 + 255 * 72));
        QVERIFY_THROWS_EXCEPTION(Error, classes.append(5, "Overflow"));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x80243db1));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x80243db0, 0x802441b0));
        QVERIFY_THROWS_EXCEPTION(Error, classes.build(0x803ff000, 0xffffffff));
        auto bad = classes.entries()[5];
        bad.name = QByteArray("Bad\0Name", 8);
        QVERIFY_THROWS_EXCEPTION(Error, classes.set(5, bad));
        QCOMPARE(classes.entries()[5].name, QByteArray("Knight"));
        bad = classes.entries()[5];
        bad.record.chop(1);
        QVERIFY_THROWS_EXCEPTION(Error, classes.set(5, bad));
        put32(bytes, 0x5db14, 0x80180000);
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::loadRetail(Rom::fromBytes(bytes)));
    }
    void retailClassDataCompilation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Set LORDLY_TEST_ROM and LORDLY_N64_TOOLCHAIN for the class-data build");
        const auto rom = Rom::open(path);
        auto classes = ClassData::loadRetail(rom);
        const auto original = classes.entries();
        QCOMPARE(original[5].name, QByteArray("Knight"));
        QCOMPARE(original[164].name, QByteArray::fromHex("0e44656174681063426168616d75740f"));
        QCOMPARE(classes.append(5, "Silver Knight"), 165);
        const auto result = classes.build(0x80243db0);
        QTemporaryDir work;
        QVERIFY(work.isValid());
        Compiler compiler(tools);
        compiler.authenticate();
        const auto runtime = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
        QCOMPARE(runtime.compiledData, result.bytes);
        QCOMPARE(runtime.data.layout.end, result.layout.end);
        QCOMPARE(runtime.dataRomOffset, quint32(0x2800000));
        QCOMPARE(runtime.code.entries.size(), 3);
        QVERIFY(runtime.code.entries.contains("lc_boot_classes"));
        QVERIFY(runtime.code.entries.contains("lc_heap_reset"));
        QVERIFY(runtime.code.entries.contains("lc_secondary_reset"));
        QVERIFY(runtime.code.address + runtime.code.bytes.size() <= runtime.directoryAddress);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 buildClassRuntime(classes, compiler, work.path(), 0x2800001));
        QVERIFY_THROWS_EXCEPTION(Error,
                                 buildClassRuntime(classes, compiler, work.path(), 0x3fffff0));
        for (int id = 0; id < 165; ++id) {
            const auto record = result.layout.records - result.layout.indexBase + id * 72;
            QCOMPARE(result.bytes.mid(record + 4, 68), original[id].record.sliced(4));
            const auto offset = u32(result.bytes, record) - result.layout.indexBase;
            QCOMPARE(result.bytes.mid(offset, original[id].name.size()), original[id].name);
            QCOMPARE(u8(result.bytes, offset + original[id].name.size()), quint8(0));
        }
        QCOMPARE(Rom::open(path).bytes(), rom.bytes());
    }
    void sourcePreprocessing() {
        auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty())
            QSKIP("Set LORDLY_N64_TOOLCHAIN to exercise the bundled compiler");
        Compiler compiler(tools);
        compiler.authenticate();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        atomicWrite(dir.filePath("contract.h"), "#error Stale build header was included\n");
        const QByteArray source = R"C(
/* A maintained source may contain comments, includes and macros. */
#include "contract.h"
#if defined(_WIN32) || defined(i386) || defined(__i386__)
#error Host target macros leaked into the N64 compilation
#endif
#if __mips != 3 || !defined(_MIPSEB)
#error Missing N64 target definitions
#endif
unsigned char result[] = { LC_TWICE(21), sizeof(void *), sizeof(long), sizeof("/* kept */") };
)C";
        auto built = compiler.compile(source, dir.path(), "preprocessing", 0, ".data",
                                      {{"contract.h", "#define LC_TWICE(x) ((x) * 2)\n"}});
        QCOMPARE(built.first(4), QByteArray::fromHex("2a04040b"));
        for (auto byte : built.sliced(4))
            QCOMPARE(byte, char(0));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compile("#include <missing.h>\n", dir.path(), "missing", 0, ".data"));
    }
    void linkedModuleEntries() {
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty())
            QSKIP("Set LORDLY_N64_TOOLCHAIN to exercise linked C modules");
        QTemporaryDir work;
        QVERIFY(work.isValid());
        Compiler compiler(tools);
        compiler.authenticate();
        const auto module =
            compiler.compileModule("unsigned int first(unsigned int x) { return x + 7; }\n"
                                   "unsigned int second(unsigned int x) { return x ^ 90; }\n",
                                   work.path(), "entries", 0x80187c14);
        QCOMPARE(module.entries.size(), 2);
        QVERIFY(module.entries.contains("first") && module.entries.contains("second"));
        QVERIFY(module.entries["first"] != module.entries["second"]);
        // Independent MIPS expectations: jr ra; addiu v0,a0,7 / xori v0,a0,90.
        QCOMPARE(module.bytes.mid(module.entries["first"] - module.address, 8),
                 QByteArray::fromHex("03e0000824820007"));
        QCOMPARE(module.bytes.mid(module.entries["second"] - module.address, 8),
                 QByteArray::fromHex("03e000083882005a"));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compileModule("volatile unsigned int state = 4;\nunsigned int "
                                          "use_state(void) { return state; }\n",
                                          work.path(), "requires_data", 0x80187c14));
        QVERIFY_THROWS_EXCEPTION(
            Error, compiler.compileModule(
                       "unsigned int state;\nunsigned int use_state(void) { return state; }\n",
                       work.path(), "requires_bss", 0x80187c14));
    }
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
            QVERIFY_THROWS_EXCEPTION(Error, exportRom(reopened, result, output));
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
    void promotionFamiliesExport() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            p.loadPromotion();
            auto model = *p.promotion();
            // Documented retail Knight requirements provide an independent framing check.
            QCOMPARE(u8(model.gates, 5 * 8), quint8(72));
            QCOMPARE(u8(model.gates, 5 * 8 + 1), quint8(67));
            QCOMPARE(u8(model.gates, 5 * 8 + 6), quint8(40));
            auto family = model.addFamily("Order of Dawn", 1);
            QCOMPARE(family, 16);
            model.assign(2, family);
            model.assign(5, family);
            model.setGate(5, 0, 90);
            model.setMask(5, 0xa5);
            QVERIFY_THROWS_EXCEPTION(Error, model.addFamily("order of dawn", 1));
            QVERIFY_THROWS_EXCEPTION(Error, model.assign(0, 16));
            QVERIFY_THROWS_EXCEPTION(Error, model.assign(5, 65534));
            p.setPromotion(model.json());
            QTemporaryDir projectDir;
            p.saveFolder(projectDir.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(projectDir.path()));
            QCOMPARE(restored.promotion()->json(), model.json());
            QTemporaryDir build;
            auto result = rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-promotion-XXXXXX");
            QVERIFY(candidate.isValid());
            auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Project::fromRom(Rom::open(output));
            reopened.loadPromotion();
            QCOMPARE(reopened.promotion()->json(), model.json());
            QVERIFY(reopened.promotion()->expanded);
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QTemporaryDir repeatBuild;
            auto repeat =
                rebuild(reopened, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), repeatBuild.path());
            QCOMPARE(repeat.image, result.image);
            auto occupied = p.rom().bytes();
            occupied[0x27d0000] = 0;
            auto conflict = Project::fromRom(Rom::fromBytes(occupied));
            conflict.setPromotion(model.json());
            QVERIFY_THROWS_EXCEPTION(Error,
                                     rebuild(conflict, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),
                                             repeatBuild.path()));
            QVERIFY(QFileInfo::exists(build.filePath("class_families.compiler.s")));
            QVERIFY(QFileInfo::exists(build.filePath("func_000bf248.compiler.s")));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void relocatedPromotionMigration() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM not supplied");
            const auto retail = Rom::open(path);
            auto image = retail.bytes();
            auto gates = Promotion::load(retail).gates;
            quint32 seed = 0x2468ace1;
            for (int i = 8; i < gates.size(); ++i) {
                seed = seed * 1664525u + 1013904223u;
                gates[i] = char(seed >> 24);
            }
            gates[40] = char(123); // Independently chosen Knight STR requirement.
            const auto stream = encodeLzss(gates);
            QVERIFY(stream.size() > 376);
            QVERIFY(stream.size() <= 659);
            const auto stored = (stream.size() + 1) & ~qsizetype(1);
            QByteArray slot(1024, char(-1)), descriptor(64, 0);
            put32(slot, 0, stored + 4);
            put32(slot, 4, 648);
            slot.replace(8, stored, QByteArray(stored, 3));
            slot.replace(8, stream.size(), stream);
            put32(descriptor, 0, 0x4f425347);
            put32(descriptor, 4, 0x02000100);
            put32(descriptor, 8, stream.size());
            put32(descriptor, 12, stored);
            put32(descriptor, 16, 648);
            put32(descriptor, 20, crc32(gates));
            put32(descriptor, 28, crc32(descriptor.first(28)));
            slot.replace(960, 64, descriptor);
            image.replace(0x27b0800, 1024, slot);
            int firstKeySite = -1;
            for (auto v : promotionOwners()["owners"].toArray()) {
                const auto owner = v.toObject();
                if (!owner.contains("keySite"))
                    continue;
                const auto site = owner["keySite"].toInt();
                if (firstKeySite < 0)
                    firstKeySite = site;
                // Existing OBSG-v2 loader contract: ROM entry minus 0x594280.
                const quint32 key = 0x27b0800 - 0x594280;
                put32(image, site, 0x3c040000 | (key >> 16));
                put32(image, site + 4, 0x34840000 | (key & 65535));
            }
            auto p = Project::fromRom(Rom::fromBytes(image));
            p.loadPromotion();
            QVERIFY(p.promotion()->directGates);
            QCOMPARE(p.promotion()->gates, gates);
            QCOMPARE(u8(p.promotion()->gates, 40), quint8(123));
            QTemporaryDir work;
            // Merely opening/rebuilding preserves the user's existing export.
            QCOMPARE(rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), work.path()).image,
                     image);
            auto model = *p.promotion();
            model.setGate(5, 0, 124);
            p.setPromotion(model.json());
            const auto result =
                rebuild(p, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), work.path());
            QCOMPARE(result.image.mid(0x27b0800, 1024), QByteArray(1024, char(-1)));
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir candidate(candidates + "/native-promotion-migration-XXXXXX");
            QVERIFY(candidate.isValid());
            const auto output = candidate.filePath(QFileInfo(path).fileName());
            exportRom(p, result, output);
            auto reopened = Promotion::load(Rom::open(output));
            QVERIFY(reopened.expanded);
            QVERIFY(!reopened.directGates);
            QCOMPARE(reopened.json(), model.json());
            QCOMPARE(u8(reopened.gates, 40), quint8(124));
            auto corrupt = image;
            corrupt[0x27b0800 + 960 + 20] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt.replace(firstKeySite, 8, retail.bytes().mid(firstKeySite, 8));
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt.replace(0x27b0800, 1024, QByteArray(1024, char(-1)));
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            corrupt = image;
            corrupt[0x27b0800 + 960 + 4] = 1;
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(corrupt)));
            QCOMPARE(Rom::open(path).bytes(), retail.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "engine_tests.moc"
