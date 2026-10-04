#include "engine/class_data.h"
#include "engine/class_runtime_inventory.h"
#include "engine/class_text.h"
#include "engine/expanded_promotion_resource.h"
#include "engine/project.h"
#include "engine/resource.h"
#include "rebuild/class_menu_layout.h"
#include "rebuild/class_runtime.h"
#include "rebuild/compiler.h"
#include "rebuild/elf_references.h"
#include "rebuild/promotion_runtime.h"
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
static Promotion syntheticPromotion() {
    Promotion base;
    QJsonArray families, assignments;
    for (int id = 0; id <= 10; ++id)
        families << QJsonObject{
            {"id", id}, {"name", id ? QString("Family %1").arg(id) : "None"}, {"behavior", id}};
    for (int id = 0; id < 81; ++id) {
        assignments << id % 11;
        base.masks += char(id * 7);
        base.flags += char((id * 16) & 0xf0);
        for (int field = 0; field < 8; ++field)
            base.gates += char(id * 3 + field);
    }
    base.families = {{"schemaVersion", 1}, {"families", families}, {"assignments", assignments}};
    base.originalFamilies = base.families;
    base.originalGates = base.gates;
    base.originalMasks = base.masks;
    base.originalFlags = base.flags;
    return base;
}
class EngineTests : public QObject {
    Q_OBJECT
  private slots:
    void expandedMenuBuild() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler are needed for expanded menu installation checks");
        try {
            const auto rom = Rom::open(path);
            auto priorRom = rom;
            auto classes = ClassData::loadRetail(rom);
            auto promotion = Promotion::load(rom);
            Compiler compiler(tools);
            compiler.authenticate();
            QTemporaryDir work;
            QVERIFY(work.isValid());
            for (int count : {166, 256}) {
                while (classes.entries().size() < count) {
                    classes.append(5, "Silver Knight");
                    promotion.appendClass(5);
                }
                const auto runtime = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
                if (count == 256)
                    QVERIFY_THROWS_EXCEPTION(Error, linkClassRuntime(priorRom, runtime));
                const auto menus = buildPromotionRuntime(priorRom, runtime.data.layout, promotion,
                                                         compiler, work.path());
                QCOMPARE(menus.owners.size(), expandedPromotionOwners().size());
                const auto reopened = ExpandedPromotionResource::decode(menus.container);
                QCOMPARE(reopened.data.bytes, promotion.buildExpandedData().bytes);
                QCOMPARE(reopened.inventory.encode(), menus.inventory.encode());
                auto image = linkClassRuntime(priorRom, runtime, &menus);
                const auto candidateRom = Rom::fromBytes(image);
                const auto placed = ClassPlacement::read(candidateRom);
                QVERIFY(placed->saves.installed());
                placed->saves.verify(image);
                QCOMPARE(image.mid(SavePlacement::romAddress, placed->saves.codeBytes),
                         runtime.saves->code.bytes);
                for (const auto site : {SavePlacement::romAddress, 0x4c5cU, 0x38670U, 0x74cb4U,
                                        0x75028U, 0x564d0U, 0x15e578U}) {
                    auto changedSave = image;
                    changedSave[site] ^= 1;
                    QVERIFY_THROWS_EXCEPTION(Error,
                                             ClassPlacement::read(Rom::fromBytes(changedSave)));
                }
                menus.inventory.verify(candidateRom,
                                       ClassRuntimeInventory::dataBases(runtime.data.layout));
                QCOMPARE(ClassData::load(candidateRom).entries().size(), count);
                QCOMPARE(Promotion::load(candidateRom).json(), promotion.json());
                const auto resource =
                    Resource::load(candidateRom, ExpandedPromotionResource::romOffset,
                                   ExpandedPromotionResource::slotSize);
                QCOMPARE(resource.decoded, promotion.buildExpandedData().bytes);
                // The trailing entry of the next routine is outside class-screen C.
                QCOMPARE(image.mid(0xd0380, 8), rom.bytes().mid(0xd0380, 8));
                auto damaged = image;
                damaged[0xae3f4] ^= 1;
                QVERIFY_THROWS_EXCEPTION(
                    Error,
                    menus.inventory.verify(Rom::fromBytes(damaged),
                                           ClassRuntimeInventory::dataBases(runtime.data.layout)));
                QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(damaged)));
                damaged = image;
                auto incomplete = menus.inventory;
                incomplete.owners.removeLast();
                damaged.replace(ExpandedPromotionResource::romOffset, menus.container.size(),
                                ExpandedPromotionResource::encode(
                                    promotion, promotion.buildExpandedData().bytes, incomplete));
                QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(damaged)));
                damaged = image;
                auto emptyClassChanged = promotion;
                emptyClassChanged.masks[0] ^= 1;
                damaged.replace(ExpandedPromotionResource::romOffset, menus.container.size(),
                                ExpandedPromotionResource::encode(
                                    emptyClassChanged, emptyClassChanged.buildExpandedData().bytes,
                                    menus.inventory));
                QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(damaged)));
                const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
                QVERIFY(QDir().mkpath(candidates));
                QTemporaryDir candidate(candidates + "/native-expanded-menu-XXXXXX");
                QVERIFY(candidate.isValid());
                const auto output = candidate.filePath(QFileInfo(path).fileName());
                atomicWrite(output, rom.serialize(image));
                atomicWrite(
                    candidate.filePath("menu-layout.json"),
                    QJsonDocument(QJsonObject{{"rom", output},
                                              {"count", count},
                                              {"ownerBytes", int(menus.layout.ownerBytes)},
                                              {"list", int(menus.layout.list)},
                                              {"ids", int(menus.layout.categoryClasses)},
                                              {"current", int(menus.layout.currentCounts)},
                                              {"eligible", int(menus.layout.eligibleCounts)},
                                              {"records", double(runtime.data.layout.records)},
                                              {"aliases", double(runtime.data.layout.aliases)},
                                              {"discovery", double(runtime.data.layout.discovery)}})
                        .toJson());
                candidate.setAutoRemove(false);
                qInfo().noquote() << "Expanded-class candidate (remaining consumers and full-game "
                                     "checks pending):"
                                  << candidate.filePath("menu-layout.json");
                priorRom = candidateRom;
                if (count == 166) {
                    // Previous save installation metadata omitted the Pak owners.
                    // Reopening and upgrading it must authenticate both sets.
                    auto older = image;
                    auto previousOwners = saveRuntimeCatalog()["pakOwners"].toArray();
                    for (const auto value : saveRuntimeCatalog()["lifecycleOwners"].toArray())
                        previousOwners.append(value);
                    for (const auto value : previousOwners) {
                        const auto row = value.toObject();
                        older.replace(row["start"].toInt(), row["size"].toInt(),
                                      rom.bytes().mid(row["start"].toInt(), row["size"].toInt()));
                    }
                    auto legacyPlacement = *placed;
                    legacyPlacement.saves.version = 1;
                    legacyPlacement.saves.ownerChecksum = saveRuntimeOwnerChecksum(older, 1);
                    older.replace(
                        ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize,
                        legacyPlacement.descriptor(runtime.compiledData, runtime.code.bytes));
                    priorRom = Rom::fromBytes(older);
                    QCOMPARE(ClassPlacement::read(priorRom)->saves.version, 1U);
                }
            }
            // Longer names move the discovery bitmap without changing the count.
            auto named = classes.entries().last();
            named.name = QByteArray(200, 'A');
            classes.set(255, named);
            const auto next = buildClassRuntime(classes, compiler, work.path(), 0x2800000);
            const auto before = ClassPlacement::read(priorRom);
            QVERIFY(next.data.layout.discovery != before->layout.discovery);
            const auto renamed = Rom::fromBytes(linkClassRuntime(priorRom, next));
            QCOMPARE(ClassData::load(renamed).entries()[255].name, named.name);
            QCOMPARE(Promotion::load(renamed).json(), promotion.json());
            // Fresh compilation and metadata relocation must agree byte-for-byte.
            const auto recompiled =
                buildPromotionRuntime(priorRom, next.data.layout, promotion, compiler, work.path());
            for (auto it = recompiled.owners.begin(); it != recompiled.owners.end(); ++it)
                QCOMPARE(renamed.bytes().mid(it.key(), it.value().size()), it.value());
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void classNameProjectExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto p = Project::fromRom(Rom::open(path));
            QVERIFY(p.hasClassData());
            QCOMPARE(encodeClassName("Death\nBahamut"), p.classNameBytes(164));
            const auto knight = encodeClassName("Silver\nKnight");
            p.setClassNameBytes(5, knight);
            p.setClassNameBytes(164, encodeClassName("Ancient\nBahamut"));
            int str = -1;
            for (int n = 0; n < fields(Table::Classes).size(); ++n)
                if (fields(Table::Classes)[n].key == "strBase")
                    str = n;
            QVERIFY(str >= 0);
            p.setValue(Table::Classes, 5, str, 432);
            p.loadPromotion();
            auto families = *p.promotion();
            const auto family = families.addFamily("Order of Silver", 1);
            families.assign(5, family);
            p.setPromotion(families.json());
            QTemporaryDir folder, build;
            QVERIFY(folder.isValid() && build.isValid());
            p.saveFolder(folder.path());
            auto restored = Project::fromRom(p.rom());
            restored.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(restored.classNameBytes(5), knight);
            QCOMPARE(restored.record(Table::Classes, 5).name, QString("Silver\nKnight"));
            QCOMPARE(restored.catalog()["CLASS_NAMES"].toObject()["5"],
                     QJsonValue("Silver\nKnight"));
            auto invalid = restored.manifest();
            invalid["classNames"] = QJsonObject{{"5", "410042"}};
            QVERIFY_THROWS_EXCEPTION(Error, restored.applyManifest(invalid));
            QCOMPARE(restored.classNameBytes(5), knight);
            auto result =
                rebuild(restored, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(candidates));
            QTemporaryDir output(candidates + "/native-class-name-XXXXXX");
            QVERIFY(output.isValid());
            const auto file = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, file);
            auto reopened = Project::fromRom(Rom::open(file));
            QCOMPARE(reopened.classNameBytes(5), knight);
            QCOMPARE(reopened.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
            QCOMPARE(reopened.value(Table::Classes, 5, str), 432);
            reopened.loadPromotion();
            QCOMPARE(reopened.promotion()->json(), families.json());
            const auto placement = ClassPlacement::read(reopened.rom());
            QVERIFY(placement.has_value());
            // The screen owner's last eight bytes begin the following routine.
            QCOMPARE(result.image.mid(0xd0380, 8), p.rom().bytes().mid(0xd0380, 8));
            const auto screenCode = result.image.mid(0xd0150, 568);
            const auto canonicalScreen = placement->canonicalReferences(screenCode, 0xd0150);
            QCOMPARE(placement->placedReferences(canonicalScreen, 0xd0150), screenCode);
            const auto countsCode = result.image.mid(0xbbdac, 784);
            const auto canonicalCounts = placement->canonicalReferences(countsCode, 0xbbdac);
            QCOMPARE(placement->placedReferences(canonicalCounts, 0xbbdac), countsCode);
            QCOMPARE(u32(countsCode, 676) & 65535, (placement->layout.records + 69) & 65535);
            auto alteredCode = countsCode;
            alteredCode[0] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, placement->canonicalReferences(alteredCode, 0xbbdac));
            alteredCode = countsCode;
            alteredCode[679] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, placement->canonicalReferences(alteredCode, 0xbbdac));
            reopened.setClassNameBytes(5, "K");
            const auto smaller =
                rebuild(reopened, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            QVERIFY(smaller.image.size() < result.image.size());
            // A later allocation must preserve unrelated bytes after the old payload.
            const QByteArray marker(16, char(-90));
            auto growing = Project::fromRom(Rom::fromBytes(smaller.image + marker));
            growing.setClassNameBytes(5, encodeClassName("Knight of the Silver Dawn"));
            growing.loadPromotion();
            auto changedFamilies = *growing.promotion();
            changedFamilies.setGate(5, 0, 91);
            growing.setPromotion(changedFamilies.json());
            const auto larger =
                rebuild(growing, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path());
            QCOMPARE(larger.image.mid(smaller.image.size(), marker.size()), marker);
            auto finalProject = Project::fromRom(Rom::fromBytes(larger.image));
            finalProject.loadPromotion();
            QCOMPARE(finalProject.record(Table::Classes, 5).name,
                     QString("Knight of the Silver Dawn"));
            QCOMPARE(finalProject.promotion()->json(), changedFamilies.json());
            QCOMPARE(
                rebuild(finalProject, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), build.path())
                    .image,
                larger.image);
            QCOMPARE(Rom::open(path).bytes(), p.rom().bytes());
            QVERIFY_THROWS_EXCEPTION(Error, encodeClassName("First\nSecond\nThird"));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
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
        const auto linked = linkClassRuntime(rom, runtime);
        const auto relocated = Rom::fromBytes(linked);
        const auto placement = ClassPlacement::read(relocated);
        QVERIFY(placement.has_value());
        QCOMPARE(placement->layout.count, 166);
        QCOMPARE(placement->dataRomOffset, runtime.dataRomOffset);
        QCOMPARE((u32(linked, 0x25a8) & 0x3ffffffU) << 2, runtime.placement.bootEntry & 0xfffffffU);
        QCOMPARE(u32(linked, 0x1148), quint32(0x2c42000a));
        QVERIFY(runtime.saves.has_value());
        const auto cacheEnd = quint32(((u32(linked, 0x2438) & 65535) << 16) +
                                     qint16(u32(linked, 0x243c) & 65535));
        QCOMPARE(cacheEnd, quint32(0x80187f00U + runtime.saves->code.bytes.size()));
        auto reopened = ClassData::load(relocated);
        QCOMPARE(reopened.entries().size(), 166);
        for (int id = 0; id < 166; ++id) {
            QCOMPARE(reopened.entries()[id].record.sliced(4),
                     classes.entries()[id].record.sliced(4));
            QCOMPARE(reopened.entries()[id].appearance, classes.entries()[id].appearance);
            QCOMPARE(reopened.entries()[id].name, classes.entries()[id].name);
            QCOMPARE(reopened.entries()[id].namedAlias, classes.entries()[id].namedAlias);
        }
        auto corrupt = linked;
        corrupt[ClassPlacement::descriptorRomOffset + 124] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        corrupt = linked;
        corrupt[runtime.dataRomOffset + 100] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        corrupt = linked;
        corrupt[ClassPlacement::codeRomOffset] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        // A valid CRC is not sufficient: pointers must belong to the names allocation.
        auto badData = runtime.compiledData;
        put32(badData, 72, runtime.data.layout.discovery);
        corrupt = linked;
        corrupt.replace(runtime.dataRomOffset, badData.size(), badData);
        corrupt.replace(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize,
                        runtime.placement.descriptor(badData, runtime.code.bytes));
        QVERIFY_THROWS_EXCEPTION(Error, ClassData::load(Rom::fromBytes(corrupt)));
        reopened.append(5, "Second new class");
        const auto next = buildClassRuntime(reopened, compiler, work.path(), runtime.dataRomOffset);
        QVERIFY(next.compiledData.size() > runtime.compiledData.size());
        const auto relinked = linkClassRuntime(relocated, next);
        QCOMPARE(ClassData::load(Rom::fromBytes(relinked)).entries().size(), 167);
        QCOMPARE(ClassData::load(Rom::fromBytes(relinked)).entries()[166].name,
                 QByteArray("Second new class"));
        QVERIFY_THROWS_EXCEPTION(
            Error, linkClassRuntime(Rom::fromBytes(linked + QByteArray(16, char(-90))), next));
        corrupt = linked;
        put32(corrupt, 0x43100, u32(corrupt, 0x43100) ^ 1);
        QVERIFY_THROWS_EXCEPTION(Error, linkClassRuntime(Rom::fromBytes(corrupt), next));
        // Keep this owned candidate for instruction-level boot/reader checks.
        const auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(candidates));
        QTemporaryDir candidate(candidates + "/native-class-link-XXXXXX");
        QVERIFY(candidate.isValid());
        const auto output = candidate.filePath(QFileInfo(path).fileName());
        atomicWrite(output, rom.serialize(linked));
        candidate.setAutoRemove(false);
        qInfo().noquote() << "Relocated-data candidate (remaining class consumers not expanded):"
                          << output;
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
        const auto leaf = compiler.compileModule("void nothing(void) {}\n", work.path(),
                                                 "alignment_leaf", 0x80100000);
        QCOMPARE(leaf.functionExtent, quint32(8));
        QCOMPARE(leaf.fitOwner(8), QByteArray::fromHex("03e0000800000000"));
        QCOMPARE(leaf.fitOwner(20), QByteArray::fromHex("03e0000800000000") + QByteArray(12, 0));
        // The zero delay instruction belongs to the function, unlike zero link padding.
        QVERIFY_THROWS_EXCEPTION(Error, leaf.fitOwner(4));
        // Deliberately unaligned output-section start forces linker padding;
        // +69 crosses a signed low-half boundary, and -57 checks negative addends.
        const QMap<QString, quint32> bindings = {{"records", 0x80307ff0}, {"other", 0x80301230}};
        const auto references = compiler.compileModule(
            "extern unsigned char records[], other[];\n"
            "unsigned int first(unsigned int id) { return records[id * 72 + 69]; }\n"
            "unsigned int second(unsigned int id) { return records[id * 72 - 57] + other[id]; }\n",
            work.path(), "references", 0x80100004, {}, bindings);
        QVERIFY(references.entries["first"] > references.address);
        QSet<QString> observed;
        for (const auto &ref : references.addressReferences) {
            QVERIFY(ref.highOffset < references.functionExtent);
            QVERIFY(ref.lowOffset < references.functionExtent);
            const auto high = u32(references.bytes, ref.highOffset);
            const auto low = u32(references.bytes, ref.lowOffset);
            const quint32 target = ((high & 65535) << 16) + quint32(qint16(low & 65535));
            QCOMPARE(target, bindings[ref.symbol] + quint32(ref.addend));
            observed.insert(ref.symbol + ":" + QString::number(ref.addend));
        }
        QCOMPARE(observed, QSet<QString>({"records:69", "records:-57", "other:0"}));
        QFile objectFile(work.filePath("references.o"));
        QVERIFY(objectFile.open(QIODevice::ReadOnly));
        const auto object = objectFile.readAll();
        QVERIFY_THROWS_EXCEPTION(Error,
                                 readMipsAddressReferences(object.first(24), references, bindings));
        auto altered = references;
        altered.bytes[altered.addressReferences.first().lowOffset + 3] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, readMipsAddressReferences(object, altered, bindings));
        auto dirtyPadding = leaf;
        dirtyPadding.bytes.append(char(1));
        QVERIFY_THROWS_EXCEPTION(Error, dirtyPadding.fitOwner(8));
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
        bank.entries.resize(225);
        QCOMPARE(bank.text(1), QString("Knight"));
        bank.setText(1, "Armored fighter");
        QCOMPARE(bank.entries[1].bytes,
                 QByteArray("Armored fighter") + QByteArray::fromHex("8197819632383030"));
        QCOMPARE(bank.entries[1].original, original);
    }
    void descriptionRuntimeBounds() {
        TextBank bank;
        bank.spec.id = "classes";
        bank.spec.count = 225;
        bank.entries.resize(225);
        bank.setText(1, QString(1278, 'A'));
        QCOMPARE(bank.entries[1].bytes.size() + 2, 0x500);
        const auto maximum = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(1, QString(1279, 'A')));
        QCOMPARE(bank.serialized(), maximum);
        bank.setText(1, "{HELP:2800}{HELP:2801}");
        bank.setText(200, QString(639, 'B'));
        bank.setText(201, QString(639, 'C'));
        const auto valid = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(201, QString(640, 'C')));
        QCOMPARE(bank.serialized(), valid);
        bank.setText(200, "{HELP:2801}");
        const auto chained = bank.serialized();
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(201, "{HELP:2800}"));
        QCOMPARE(bank.serialized(), chained);
        QVERIFY_THROWS_EXCEPTION(Error, bank.setText(2, "{REF:10}"));
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
    void addedClassPublicExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler are required");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadTexts();
            const auto originalDescriptions = project.textBanks()["classes"].entries;
            QTemporaryDir build;
            for (const int count : {166, 256, 256}) {
                while (project.records(Table::Classes).size() < count)
                    project.appendClass(
                        5, QString("New class %1").arg(project.records(Table::Classes).size()));
                const auto description = QString("Class %1 export description").arg(count);
                project.setText("classes", Project::classDescriptionIndex(count - 1), description);
                auto promotion = *project.promotion();
                promotion.setGate(count - 1, 0, 75);
                promotion.assign(
                    count - 1,
                    promotion.addFamily(QString("Family %1 %2")
                                            .arg(count)
                                            .arg(promotion.families["families"].toArray().size()),
                                        1));
                project.setPromotion(promotion.json());
                const auto expectedData = project.classData().build(0x80243db0).bytes;
                const auto result = rebuild(project, tools, build.path());
                const auto candidateRoot =
                    QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
                QTemporaryDir output(candidateRoot + "/native-added-class-XXXXXX");
                QVERIFY(output.isValid());
                output.setAutoRemove(false);
                const auto outputPath = output.filePath(QFileInfo(path).fileName());
                exportRom(project, result, outputPath);
                auto reopened = Project::fromRom(Rom::open(outputPath));
                reopened.loadTexts();
                reopened.loadPromotion();
                QCOMPARE(reopened.records(Table::Classes).size(), count);
                QCOMPARE(reopened.classData().build(0x80243db0).bytes, expectedData);
                QCOMPARE(reopened.promotion()->json(), project.promotion()->json());
                QCOMPARE(reopened.textBanks()["classes"].serialized(),
                         project.textBanks()["classes"].serialized());
                QCOMPARE(
                    reopened.textBanks()["classes"].text(Project::classDescriptionIndex(count - 1)),
                    description);
                for (int id = 165; id < 225; ++id)
                    QCOMPARE(reopened.textBanks()["classes"].entries[id].bytes,
                             originalDescriptions[id].bytes);
                const auto placed = ClassPlacement::read(reopened.rom());
                QVERIFY(placed->descriptionEntry);
                QCOMPARE(placed->saves.version, 3U);
                auto checksum = result.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, result.image);
                const auto layout = ClassMenuLayout::build(
                    placed->layout, reopened.promotion()->buildExpandedData());
                atomicWrite(
                    output.filePath("menu-layout.json"),
                    QJsonDocument(QJsonObject{{"rom", outputPath},
                                              {"count", count},
                                              {"ownerBytes", int(layout.ownerBytes)},
                                              {"list", int(layout.list)},
                                              {"ids", int(layout.categoryClasses)},
                                              {"current", int(layout.currentCounts)},
                                              {"eligible", int(layout.eligibleCounts)},
                                              {"records", double(placed->layout.records)},
                                              {"aliases", double(placed->layout.aliases)},
                                              {"discovery", double(placed->layout.discovery)}})
                        .toJson());
                qInfo().noquote() << "Public added-class export:"
                                  << output.filePath("menu-layout.json");
                project = std::move(reopened);
            }
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void addedClassProjects() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadTexts();
            project.loadPromotion();
            const auto baseline = project.manifest();
            auto templateBytes = project.record(Table::Classes, 5).bytes;
            templateBytes[71] = char(u8(templateBytes, 71) ^ 0x40);
            project.replaceBytes(Table::Classes, 5, templateBytes);
            project.setText("classes", 5, "New template description");
            auto promotion = *project.promotion();
            promotion.setGate(5, 0, 123);
            const auto family = promotion.addFamily("New family", 1);
            promotion.assign(5, family);
            promotion.setMask(5, 0x5a);
            project.setPromotion(promotion.json());
            const auto templateAsset = project.classData().entries()[5];
            QCOMPARE(project.appendClass(5, "Authored class"), 165);
            QCOMPARE(project.record(Table::Classes, 165).romOffset, qsizetype(-1));
            QCOMPARE(project.record(Table::Classes, 165).bytes, templateBytes);
            const auto addedAsset = project.classData().entries()[165];
            QCOMPARE(addedAsset.appearance, templateAsset.appearance);
            QCOMPARE(addedAsset.namedAlias, quint8(0));
            QCOMPARE(project.textBanks()["classes"].entries[225].bytes,
                     project.textBanks()["classes"].entries[5].bytes);
            QCOMPARE(project.promotion()->gates.mid(165 * 8, 8), promotion.gates.mid(40, 8));
            QCOMPARE(u8(project.promotion()->masks, 165), quint8(0x5a));
            QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), family);
            project.setText("classes", 225, "Edited added description");
            project.setClassNameBytes(165, encodeClassName("Edited added class"));
            for (int id = 166; id <= 255; ++id)
                QCOMPARE(project.appendClass(165, QString("Added class %1").arg(id)), id);
            const auto authored = project.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, project.appendClass(5, "Too many"));
            QCOMPARE(project.manifest(), authored);
            QTemporaryDir folder, archive;
            project.saveFolder(folder.path());
            project.saveArchive(archive.filePath("added.lcproject"));
            for (const auto &location : {folder.path(), archive.filePath("added.lcproject")}) {
                auto reopened = Project::fromRom(project.rom());
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.manifest(), authored);
                QCOMPARE(reopened.classData().build(0x80243db0).bytes,
                         project.classData().build(0x80243db0).bytes);
                QCOMPARE(reopened.textBanks()["classes"].serialized(),
                         project.textBanks()["classes"].serialized());
                QCOMPARE(reopened.promotion()->json(), project.promotion()->json());
                reopened.applyManifest(baseline);
                QCOMPARE(reopened.records(Table::Classes).size(), 165);
                QCOMPARE(reopened.textBanks()["classes"].entries.size(), 225);
                QCOMPARE(reopened.promotion()->classCount(), 81);
                QCOMPARE(reopened.changedRecords(), 0);
                QVERIFY(!reopened.catalog()["CLASS_NAMES"].toObject().contains("165"));
            }
            auto damaged = authored;
            damaged.remove("promotion");
            QVERIFY_THROWS_EXCEPTION(Error, project.applyManifest(damaged));
            QCOMPARE(project.manifest(), authored);
            auto rows = damaged["addedClasses"].toArray();
            auto row = rows[0].toObject();
            row["appearance"] = "00";
            rows[0] = row;
            damaged["addedClasses"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, project.applyManifest(damaged));
            QCOMPARE(project.manifest(), authored);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void growingClassDescriptions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        try {
            const auto source = Rom::open(path);
            auto project = Project::fromRom(source);
            project.loadTexts();
            const auto original = project.textBanks();
            quint32 random = 0x18462476;
            const QByteArray alphabet(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
            auto edit = [&](Project &p, int width) {
                for (int id = 1; id < 165; ++id) {
                    QString text;
                    for (int line = 0; line < 3; ++line) {
                        if (line)
                            text += '\n';
                        for (int col = 0; col < width; ++col) {
                            random = random * 1664525U + 1013904223U;
                            text += QChar(alphabet[(random >> 16) % alphabet.size()]);
                        }
                    }
                    p.setText("classes", id, text);
                }
            };
            edit(project, 32);
            QVERIFY(encodeLzss(project.textBanks()["classes"].serialized()).size() > 6399);
            project.setClassNameBytes(1, encodeClassName("Description owner"));
            QTemporaryDir build;
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            auto result = rebuild(project, tools, build.path());
            const auto first = ClassTextPlacement::read(Rom::fromBytes(result.image));
            QVERIFY(first.has_value());
            QCOMPARE(u32(result.image, 0x66d60), first->offset - ClassTextPlacement::keyBase);
            QCOMPARE(((u32(result.image, 0x56404) & 65535) << 16) |
                         (u32(result.image, 0x56408) & 65535),
                     first->offset - ClassTextPlacement::keyBase);
            auto reopened = Project::fromRom(Rom::fromBytes(result.image));
            reopened.loadTexts();
            for (auto it = original.begin(); it != original.end(); ++it)
                for (int id = 0; id < it->entries.size(); ++id)
                    if (it.key() != "classes" || id == 0 || id >= 165)
                        QCOMPARE(reopened.textBanks()[it.key()].entries[id].bytes,
                                 it->entries[id].bytes);
            QCOMPARE(reopened.textBanks()["classes"].serialized(),
                     project.textBanks()["classes"].serialized());
            // Class data follows the first text allocation. Growing text must
            // move past it, retain it, and allow the next class relink.
            const auto classes = ClassPlacement::read(reopened.rom());
            QVERIFY(classes.has_value());
            edit(reopened, 64);
            reopened.setClassNameBytes(1, encodeClassName("Description owner with a longer name"));
            auto grown = rebuild(reopened, tools, build.path());
            const auto second = ClassTextPlacement::read(Rom::fromBytes(grown.image));
            QVERIFY(second.has_value());
            QVERIFY(second->offset >= result.image.size());
            auto again = Project::fromRom(Rom::fromBytes(grown.image));
            again.loadTexts();
            QCOMPARE(again.textBanks()["classes"].serialized(),
                     reopened.textBanks()["classes"].serialized());
            QCOMPARE(again.classNameBytes(1), reopened.classNameBytes(1));
            for (const auto at : {quint32(0x66d60), quint32(0x56404), second->offset + 12,
                                  ClassTextPlacement::descriptorOffset + 12}) {
                auto damaged = grown.image;
                put8(damaged, at, u8(damaged, at) ^ 1);
                QVERIFY_THROWS_EXCEPTION(Error, TextBank::load(Rom::fromBytes(damaged)));
            }
            auto candidates = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir output(candidates + "/native-growing-text-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            exportRom(reopened, grown, output.filePath(QFileInfo(path).fileName()));
            qInfo().noquote() << "Growing description candidate:" << output.path();
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void classMenuSourceLayout() {
        const auto base = syntheticPromotion();
        // Every supported class count gets independent, terminated buffers;
        // a mismatch with either runtime table must fail before compilation.
        for (int count = 165; count <= 256; ++count) {
            auto promotion = base;
            promotion.growToClassCount(count);
            const auto payload = promotion.buildExpandedData();
            ClassDataLayout classes;
            classes.count = count;
            classes.indexBase = 0x80243db0;
            classes.records = 0x80243df8;
            classes.appearances = 0x8024a000;
            classes.aliases = 0x80250000;
            classes.discovery = 0x80250800;
            const auto layout = ClassMenuLayout::build(classes, payload);
            QCOMPARE(layout.capacity, count + 1);
            QCOMPARE(layout.list, quint32(0x2528));
            const quint32 arrayBytes = (count + 1) * 2;
            QVERIFY(layout.list + arrayBytes <= layout.categoryClasses);
            QVERIFY(layout.categoryClasses + arrayBytes <= layout.currentCounts);
            QVERIFY(layout.currentCounts + arrayBytes <= layout.eligibleCounts);
            QVERIFY(layout.eligibleCounts + arrayBytes <= layout.ownerBytes);
            QCOMPARE((layout.categoryClasses | layout.currentCounts | layout.eligibleCounts |
                      layout.ownerBytes) &
                         15,
                     quint32(0));
            if (count == 166) {
                QCOMPARE(layout.categoryClasses, quint32(0x2680));
                QCOMPARE(layout.currentCounts, quint32(0x27d0));
                QCOMPARE(layout.eligibleCounts, quint32(0x2920));
                QCOMPARE(layout.ownerBytes, quint32(0x2a70));
            }
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (!tools.isEmpty() && (count == 166 || count == 255 || count == 256)) {
                Compiler compiler(tools);
                compiler.authenticate();
                QTemporaryDir work;
                for (const auto &entry : promotionOwners()["owners"].toArray()) {
                    const auto owner = entry.toObject();
                    if (!owner.contains("cSource"))
                        continue;
                    QFile source(":/game/promotion/" + owner["cSource"].toString());
                    QVERIFY(source.open(QIODevice::ReadOnly));
                    QMap<QString, quint32> symbols = layout.dataSymbols;
                    const auto bindings = owner["symbols"].toObject();
                    for (auto it = bindings.begin(); it != bindings.end(); ++it)
                        if (!symbols.contains(it.key()))
                            symbols[it.key()] = quint32(it.value().toDouble());
                    const auto input = source.readAll();
                    const auto code =
                        compiler.compileModule(input, work.path(), owner["name"].toString(),
                                               quint32(owner["start"].toInt()) + 0x801135a0U,
                                               {{"class_menu_config.h", layout.cHeader}}, symbols);
                    const auto size = owner.value("cExtent").toInt(owner["size"].toInt());
                    QVERIFY2(code.functionExtent <= quint32(size),
                             qPrintable(QString("%1 with %2 classes needs %3 bytes, allocation %4")
                                            .arg(owner["name"].toString())
                                            .arg(count)
                                            .arg(code.functionExtent)
                                            .arg(size)));
                    QCOMPARE(code.fitOwner(size).size(), size);
                    if (owner["cSource"] == "category-class-counts.c") {
                        QVector<RuntimeDataReference> references;
                        for (const auto &ref : code.addressReferences) {
                            if (ref.symbol == "lc_class_records")
                                references.append(
                                    {"classes", ref.highOffset, ref.lowOffset, ref.addend + 72});
                            else if (ref.symbol == "lc_named_class_map")
                                references.append(
                                    {"aliases", ref.highOffset, ref.lowOffset, ref.addend});
                            else if (ref.symbol == "lc_discovery_flags")
                                references.append(
                                    {"discovery", ref.highOffset, ref.lowOffset, ref.addend});
                        }
                        QVERIFY(references.size() >= 3);
                        const quint32 start = owner["start"].toInt();
                        const auto bytes = code.fitOwner(size);
                        const auto bases = ClassRuntimeInventory::dataBases(classes);
                        ClassRuntimeInventory inventory;
                        inventory.classCount = count;
                        inventory.owners.append(
                            ClassRuntimeInventory::capture(start, bytes, references, bases));
                        const auto saved = inventory.encode();
                        const auto restored = ClassRuntimeInventory::decode(saved);
                        QCOMPARE(restored.encode(), saved);
                        auto moved = bases;
                        moved["classes"] += 0x8440;
                        moved["aliases"] += 0x8100;
                        moved["discovery"] += 0x8040;
                        auto nextSymbols = symbols;
                        nextSymbols["lc_class_records"] = moved["classes"] + 72;
                        nextSymbols["lc_named_class_map"] = moved["aliases"];
                        nextSymbols["lc_discovery_flags"] = moved["discovery"];
                        const auto recompiled =
                            compiler
                                .compileModule(input, work.path(), "moved_counts", code.address,
                                               {{"class_menu_config.h", layout.cHeader}},
                                               nextSymbols)
                                .fitOwner(size);
                        QCOMPARE(restored.relocate(start, bytes, bases, moved), recompiled);
                        QCOMPARE(restored.relocate(start, recompiled, moved, bases), bytes);
                        auto image = syntheticRom();
                        image.replace(start, size, bytes);
                        restored.verify(Rom::fromBytes(image), bases);
                        const auto tableBytes =
                            compiler.compileData(payload.bytes, work.path(), "expanded_tables");
                        const auto slot =
                            ExpandedPromotionResource::encode(promotion, tableBytes, restored);
                        const auto reopened = ExpandedPromotionResource::decode(slot);
                        QCOMPARE(reopened.data.bytes, tableBytes);
                        QCOMPARE(reopened.inventory.encode(), saved);
                        image.replace(ExpandedPromotionResource::romOffset, slot.size(), slot);
                        const auto ordinary = Resource::load(Rom::fromBytes(image),
                                                             ExpandedPromotionResource::romOffset,
                                                             ExpandedPromotionResource::slotSize);
                        QCOMPARE(ordinary.decoded,
                                 tableBytes); // Metadata isn't decoded into game data.
                        auto damagedSlot = slot;
                        damagedSlot[8] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        constexpr int footerAt = ExpandedPromotionResource::slotSize - 64;
                        const auto metadataAt = u32(slot, footerAt + 40);
                        damagedSlot = slot;
                        damagedSlot[metadataAt + 24] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        damagedSlot = slot;
                        damagedSlot[footerAt - 1] = 0;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::decode(damagedSlot));
                        // Recompute footer CRC so this checks geometry, not only checksum
                        // rejection.
                        for (const auto pair :
                             {QPair<int, quint32>{40, 16}, {44, 0xfffffff0}, {24, 0}}) {
                            damagedSlot = slot;
                            put32(damagedSlot, footerAt + pair.first, pair.second);
                            put32(damagedSlot, footerAt + 60, crc32(damagedSlot.mid(footerAt, 60)));
                            QVERIFY_THROWS_EXCEPTION(
                                Error, ExpandedPromotionResource::decode(damagedSlot));
                        }
                        auto wrongInventory = restored;
                        wrongInventory.classCount = count == 256 ? 255 : 256;
                        QVERIFY_THROWS_EXCEPTION(Error, ExpandedPromotionResource::encode(
                                                            promotion, tableBytes, wrongInventory));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ExpandedPromotionResource::encode(
                                                     promotion, tableBytes.first(100), restored));
                        ClassRuntimeInventory empty;
                        empty.classCount = count;
                        QVERIFY_THROWS_EXCEPTION(Error, empty.verify(Rom::fromBytes(image), bases));
                        auto corrupt = bytes;
                        corrupt[0] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 restored.relocate(start, corrupt, bases, moved));
                        corrupt = saved;
                        corrupt[corrupt.size() - 1] ^= 1;
                        QVERIFY_THROWS_EXCEPTION(Error, ClassRuntimeInventory::decode(corrupt));
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 ClassRuntimeInventory::decode(saved.first(23)));
                        auto invalid = inventory;
                        invalid.owners.append(invalid.owners.first());
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        invalid = inventory;
                        invalid.owners[0].start += 4;
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        invalid = inventory;
                        invalid.owners[0].references[0].low = size;
                        QVERIFY_THROWS_EXCEPTION(Error, invalid.encode());
                        moved["aliases"] = 0x80400000;
                        QVERIFY_THROWS_EXCEPTION(Error,
                                                 restored.relocate(start, bytes, bases, moved));
                    }
                    const QSet<QString> discoveryReaders = {
                        "family-menu.c", "class-discovery.c", "equipment-class-menu.c",
                        "class-categories.c", "category-class-counts.c"};
                    if (discoveryReaders.contains(owner["cSource"].toString()))
                        QVERIFY(std::any_of(
                            code.addressReferences.begin(), code.addressReferences.end(),
                            [](const auto &ref) { return ref.symbol == "lc_discovery_flags"; }));
                }
            }
            if (count == 256) {
                QCOMPARE(layout.categoryClasses, quint32(0x2730));
                QCOMPARE(layout.currentCounts, quint32(0x2940));
                QCOMPARE(layout.eligibleCounts, quint32(0x2b50));
                QCOMPARE(layout.ownerBytes, quint32(0x2d60));
                auto mismatched = payload;
                --mismatched.classCount;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(classes, mismatched));
                mismatched = payload;
                ++mismatched.maskOffset;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(classes, mismatched));
                auto invalid = classes;
                invalid.discovery = 0x803ffff0;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                invalid = classes;
                invalid.aliases = classes.records + 16;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                invalid = classes;
                invalid.records |= 1;
                QVERIFY_THROWS_EXCEPTION(Error, ClassMenuLayout::build(invalid, payload));
                if (!tools.isEmpty()) {
                    Compiler compiler(tools);
                    compiler.authenticate();
                    QTemporaryDir work;
                    const auto configData = compiler.compile(
                        "#include \"class_menu_config.h\"\nunsigned int config[] = {"
                        "LC_CLASS_COUNT, LC_LIST_OFFSET, LC_CATEGORY_CLASSES_OFFSET, "
                        "LC_CATEGORY_CURRENT_OFFSET, LC_CATEGORY_ELIGIBLE_OFFSET, "
                        "LC_DISCOVERY_ADDRESS, LC_FAMILY_OFFSET, LC_MASK_RESOURCE_OFFSET, "
                        "LC_FLAGS_RESOURCE_OFFSET};\n",
                        work.path(), "menu_config", 0, ".data",
                        {{"class_menu_config.h", layout.cHeader}}, layout.dataSymbols);
                    QCOMPARE(configData.first(36),
                             QByteArray::fromHex("0000010000002528000027300000294000002b50802508000"
                                                 "000080000000c0000000d00"));
                }
            }
        }
    }
    void expandablePromotionData() {
        auto base = syntheticPromotion();
        const auto assignments = base.families["assignments"].toArray();
        auto legacy = base.json();
        legacy.remove("flags");
        auto legacyRestored = base;
        legacyRestored.apply(legacy);
        QCOMPARE(legacyRestored.flags, base.flags);
        auto model = base;
        QCOMPARE(model.appendClass(5), 165);
        QCOMPARE(model.classCount(), 166);
        QCOMPARE(model.gates.first(648), base.gates);
        QCOMPARE(model.masks.first(81), base.masks);
        QCOMPARE(model.flags.first(81), base.flags);
        QCOMPARE(model.gates.mid(81 * 8, 84 * 8), QByteArray(84 * 8, 0));
        QCOMPARE(model.masks.mid(81, 84), QByteArray(84, 0));
        QCOMPARE(model.flags.mid(81, 84), QByteArray(84, 0));
        for (int id = 81; id < 165; ++id)
            QCOMPARE(model.families["assignments"].toArray()[id].toInt(), 0);
        QCOMPARE(model.gates.mid(165 * 8, 8), base.gates.mid(5 * 8, 8));
        QCOMPARE(model.masks[165], base.masks[5]);
        QCOMPARE(model.flags[165], base.flags[5]);
        QCOMPARE(model.families["assignments"].toArray()[165], assignments[5]);
        const auto family = model.addFamily("New order", 1);
        model.assign(165, family);
        model.setGate(165, 0, 177);
        model.setMask(165, 0xa5);
        model.setEquipmentSetListed(165, true);
        QCOMPARE(u8(model.flags, 165), quint8(0xd0)); // Preserve unknown bits 4 and 6.
        const auto data = model.buildExpandedData();
        QCOMPARE(data.classCount, 166);
        QCOMPARE(data.familyOffset, 1328);
        QCOMPARE(data.maskOffset, 1992);
        QCOMPARE(data.flagsOffset, 2158);
        QCOMPARE(data.catalogOffset, 2324);
        QCOMPARE(u8(data.bytes, 1320), quint8(177));
        QCOMPARE(u32(data.bytes, 1328 + 165 * 4), quint32(0x10010));
        QCOMPARE(u8(data.bytes, 1992 + 165), quint8(0xa5));
        QCOMPARE(u8(data.bytes, 2158 + 165), quint8(0xd0));
        QCOMPARE(u32(data.bytes, 2324), quint32(0x4c43464d));
        QCOMPARE(u32(data.bytes, 2328), quint32(data.bytes.size() - 2332));
        QCOMPARE(QJsonDocument::fromJson(data.bytes.mid(2332)).object(), model.families);
        auto restored = base;
        restored.apply(model.json());
        QCOMPARE(restored.json(), model.json());
        restored = base;
        restored.applyExpandedData(data.bytes, 166);
        QCOMPARE(restored.json(), model.json());
        auto corrupt = data.bytes;
        corrupt[1328 + 165 * 4] ^= 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 166));
        corrupt = data.bytes;
        corrupt[2159] = 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 166));
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(data.bytes.first(2331), 166));
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(data.bytes, 167));
        QCOMPARE(restored.json(), model.json());
        auto padded = base;
        padded.growToClassCount(165);
        const auto paddedData = padded.buildExpandedData();
        QCOMPARE(paddedData.bytes.mid(2310, 2), QByteArray(2, 0));
        corrupt = paddedData.bytes;
        corrupt[2311] = 1;
        QVERIFY_THROWS_EXCEPTION(Error, restored.applyExpandedData(corrupt, 165));
        const auto before = model.json();
        QVERIFY_THROWS_EXCEPTION(Error, model.appendClass(166));
        QVERIFY_THROWS_EXCEPTION(Error, model.growToClassCount(164));
        QCOMPARE(model.json(), before);
        while (model.classCount() < 256)
            model.appendClass(165);
        const auto full = model.buildExpandedData();
        QCOMPARE(full.familyOffset, 2048);
        QCOMPARE(full.maskOffset, 3072);
        QCOMPARE(full.flagsOffset, 3328);
        QCOMPARE(full.catalogOffset, 3584);
        QCOMPARE(u8(full.bytes, 255 * 8), quint8(177));
        QCOMPARE(u32(full.bytes, 2048 + 255 * 4), quint32(0x10010));
        QCOMPARE(u8(full.bytes, 3072 + 255), quint8(0xa5));
        QCOMPARE(u8(full.bytes, 3328 + 255), quint8(0xd0));
        restored.applyExpandedData(full.bytes, 256);
        QCOMPARE(restored.json(), model.json());
        const auto atLimit = model.json();
        QVERIFY_THROWS_EXCEPTION(Error, model.appendClass(165));
        QCOMPARE(model.json(), atLimit);
        model.masks.chop(1);
        QVERIFY_THROWS_EXCEPTION(Error, model.buildExpandedData());
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
            auto maskOnly = p;
            auto maskModel = model;
            maskModel.setMask(5, 0xa5);
            const auto knightFlags = u8(maskModel.flags, 5);
            maskModel.setEquipmentSetListed(5, !(knightFlags & 0x80));
            maskOnly.setPromotion(maskModel.json());
            QTemporaryDir maskWork;
            const auto maskResult =
                rebuild(maskOnly, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            QCOMPARE(Promotion::load(Rom::fromBytes(maskResult.image)).json(), maskModel.json());
            QCOMPARE(maskResult.image.mid(0xbbd80, 44), p.rom().bytes().mid(0xbbd80, 44));
            QCOMPARE(u8(maskResult.image, 0xdbd2c + 5 * 2),
                     quint8(u8(p.rom().bytes(), 0xdbd2c + 5 * 2) ^ 0x80));
            QCOMPARE(maskResult.image.mid(0x27d0000, 65536), p.rom().bytes().mid(0x27d0000, 65536));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bbd80.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bf5dc.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bc0bc.compiler.s")));
            QVERIFY(QFileInfo::exists(maskWork.filePath("func_000bbdac.compiler.s")));
            QVERIFY(maskResult.image.mid(0xbf5dc, 332) != p.rom().bytes().mid(0xbf5dc, 332));
            // A compiled reader can precede relocation, or be rebuilt after it.
            auto namedMask = Project::fromRom(Rom::fromBytes(maskResult.image));
            namedMask.setClassNameBytes(5, "Steel Knight");
            const auto maskAndName =
                rebuild(namedMask, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            auto placedMask = Project::fromRom(Rom::fromBytes(maskAndName.image));
            placedMask.loadPromotion();
            auto nextMasks = *placedMask.promotion();
            nextMasks.setMask(5, 3);
            placedMask.setPromotion(nextMasks.json());
            const auto updatedMask =
                rebuild(placedMask, qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"), maskWork.path());
            auto maskReopened = Project::fromRom(Rom::fromBytes(updatedMask.image));
            maskReopened.loadPromotion();
            QCOMPARE(maskReopened.promotion()->masks, nextMasks.masks);
            QCOMPARE(maskReopened.classNameBytes(5), QByteArray("Steel Knight"));
            auto family = model.addFamily("Order of Dawn", 1);
            QCOMPARE(family, 16);
            model.assign(2, family);
            model.assign(5, family);
            model.setGate(5, 0, 90);
            model.setMask(5, 0xa5);
            model.setEquipmentSetListed(5, !(u8(model.flags, 5) & 0x80));
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
            const auto familyRom = Rom::fromBytes(result.image);
            const auto classes = ClassData::load(familyRom);
            Compiler compiler(qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"));
            compiler.authenticate();
            const auto runtime = buildClassRuntime(classes, compiler, repeatBuild.path(),
                                                   quint32(result.image.size()));
            const auto combined = linkClassRuntime(familyRom, runtime);
            QCOMPARE(Promotion::load(Rom::fromBytes(combined)).json(), model.json());
            auto altered = combined;
            // This is a relocated class operand in the shared eligibility owner.
            put32(altered, 0xc3998, u32(altered, 0xc3998) ^ 1);
            QVERIFY_THROWS_EXCEPTION(Error, Promotion::load(Rom::fromBytes(altered)));
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
