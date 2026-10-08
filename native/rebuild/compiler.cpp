#include "engine/art_resource_links.h"
#include "rebuild/compiler.h"
#include "rebuild/animation_overrides.h"
#include "rebuild/body_markers.h"
#include "rebuild/body_selection.h"
#include "rebuild/pose_runtime.h"
#include "rebuild/body_visual_policy.h"
#include "engine/class_text.h"
#include "engine/dialogue_resources.h"
#include "engine/scene_program_resources.h"
#include "engine/shop_runtime.h"
#include "rebuild/class_runtime.h"
#include "rebuild/chaos_counter.h"
#include "rebuild/display_tools.h"
#include "rebuild/high_attack.h"
#include "rebuild/creature_rewards.h"
#include "rebuild/custom_encounters.h"
#include "rebuild/elf_references.h"
#include "rebuild/promotion_runtime.h"
#include "rebuild/scenario_rates.h"
#include "rebuild/shop_runtime.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <algorithm>

namespace lordly {
static QByteArray readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Cannot read " + path + ": " + file.errorString());
    auto result = file.readAll();
    if (result.size() != file.size())
        throw Error("Incomplete file read: " + path);
    return result;
}
Compiler::Compiler(QString toolsRoot) : root_(QDir(std::move(toolsRoot)).absolutePath()) {}
void Compiler::authenticate() const {
    auto pins = QJsonDocument::fromJson(readFile(":/rebuild/toolchain.json"))
                    .object()
                    .value("tools")
                    .toObject();
    if (pins.isEmpty())
        throw Error("Missing compiler authentication manifest");
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        auto actual = QString::fromLatin1(sha256(readFile(QDir(root_).filePath(it.key()))));
        if (actual != it.value().toString())
            throw Error("Unrecognized N64 toolchain file: " + it.key());
    }
}
void Compiler::run(const QString &tool, const QStringList &args, const QString &directory) const {
    QProcess p;
    p.setWorkingDirectory(directory);
    p.setProcessChannelMode(QProcess::MergedChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto &key : {"C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                            "DEPENDENCIES_OUTPUT", "SUNPRO_DEPENDENCIES"})
        environment.remove(QString::fromLatin1(key));
    p.setProcessEnvironment(environment);
    p.start(QDir(root_).filePath(tool), args);
    if (!p.waitForStarted(10000))
        throw Error("Cannot start " + tool + ": " + p.errorString());
    if (!p.waitForFinished(60000)) {
        p.kill();
        p.waitForFinished();
        throw Error(tool + " exceeded its build timeout");
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        throw Error(tool + " failed:\n" + QString::fromLocal8Bit(p.readAll()));
}
QByteArray Compiler::compile(const QByteArray &source, const QString &directory,
                             const QString &name, quint32 address, const QString &section,
                             const QMap<QString, QByteArray> &headers,
                             const QMap<QString, quint32> &symbols) const {
    if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]*$").match(name).hasMatch())
        throw Error("Invalid source unit name");
    if (section != ".text" && section != ".data")
        throw Error("Unsupported compiler output section");
    if (!QDir().mkpath(directory))
        throw Error("Cannot create build directory");
    QDir dir(directory);
    auto input = dir.absoluteFilePath(name + ".c");
    auto preprocessed = dir.absoluteFilePath(name + ".i");
    auto assembly = dir.absoluteFilePath(name + ".compiler.s");
    auto object = dir.absoluteFilePath(name + ".o");
    auto script = dir.absoluteFilePath(name + ".ld");
    auto elf = dir.absoluteFilePath(name + ".elf");
    auto binary = dir.absoluteFilePath(name + ".bin");
    atomicWrite(input, source);
    QTemporaryDir headerDirectory(dir.absoluteFilePath(name + "_headers_XXXXXX"));
    if (!headerDirectory.isValid())
        throw Error("Cannot create source header directory");
    QDir includes(headerDirectory.path());
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]*\\.h$").match(it.key()).hasMatch())
            throw Error("Invalid source header name");
        atomicWrite(includes.absoluteFilePath(it.key()), it.value());
    }
    // The executable runs on Windows; its input describes the N64 target.
    // Suppress host include paths and host predefined platform macros.
    run("cpp.exe",
        {"-P", "-undef", "-nostdinc", "-D__mips=3", "-D__mips__", "-D_MIPSEB", "-D__MIPSEB__",
         "-I-", "-I", includes.absolutePath(), input, preprocessed},
        directory);
    QStringList flags = {
        "-quiet",     "-O2", "-meb",     "-mips3",        "-mgp32",       "-mfp32",
        "-G",         "0",   "-fno-PIC", "-mno-abicalls", "-fno-builtin", "-funsigned-char",
        preprocessed, "-o",  assembly};
    run("cc1.exe", flags, directory);
    run("mips-kmc-elf-as.exe",
        {"-G", "0", "-mips3", "-mabi=32", "-force-n64align", "-EB", "-o", object, assembly},
        directory);
    QByteArray bindings;
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
        if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]*$").match(it.key()).hasMatch())
            throw Error("Invalid linked symbol name");
        bindings += QString("%1 = 0x%2;\n").arg(it.key()).arg(it.value(), 0, 16).toLatin1();
    }
    atomicWrite(script, bindings + QString("SECTIONS { %1 0x%2 : { *(%1) } }\n")
                                       .arg(section)
                                       .arg(address, 0, 16)
                                       .toLatin1());
    run("mips-kmc-elf-ld.exe", {"-EB", "-m", "elf32bmip", "-T", script, "-o", elf, object},
        directory);
    QStringList extract = {"-O", "binary"};
    // GNU 2.6 predates --only-section. Remove its known ancillary sections,
    // then verify the complete extracted owner against the expected result.
    for (const auto &other : {".text", ".data", ".bss", ".rdata", ".reginfo", ".pdr", ".mdebug",
                              ".comment", ".lit4", ".lit8"})
        if (section != QLatin1String(other))
            extract << "--remove-section=" + QString::fromLatin1(other);
    extract << elf << binary;
    run("mips-kmc-elf-objcopy.exe", extract, directory);
    return readFile(binary);
}
CompiledModule Compiler::compileModule(const QByteArray &source, const QString &directory,
                                       const QString &name, quint32 address,
                                       const QMap<QString, QByteArray> &headers,
                                       const QMap<QString, quint32> &symbols) const {
    CompiledModule result{
        compile(source, directory, name, address, ".text", headers, symbols), address, {}};
    const auto elf = readFile(QDir(directory).filePath(name + ".elf"));
    auto range = [&](quint64 offset, quint64 length) {
        if (offset > quint64(elf.size()) || length > quint64(elf.size()) - offset)
            throw Error("Linked module contains a section outside its ELF file");
        return elf.mid(qsizetype(offset), qsizetype(length));
    };
    if (range(0, 7) != QByteArray::fromHex("7f454c46010201") || u16(elf, 16) != 2 ||
        u16(elf, 18) != 8 || u32(elf, 20) != 1 || u16(elf, 40) != 52 || u16(elf, 46) != 40)
        throw Error("Expected a linked ELF32 big-endian MIPS module");
    const quint32 sectionOffset = u32(elf, 32);
    const auto count = u16(elf, 48), nameIndex = u16(elf, 50);
    range(sectionOffset, quint64(count) * 40);
    auto section = [&](quint32 index) {
        if (index >= count)
            throw Error("Linked module has an invalid section index");
        return range(quint64(sectionOffset) + index * 40, 40);
    };
    auto contents = [&](const QByteArray &header) {
        return range(u32(header, 16), u32(header, 20));
    };
    auto string = [](const QByteArray &table, quint32 offset) {
        const auto end = table.indexOf(char(0), offset);
        if (offset >= quint32(table.size()) || end < 0)
            throw Error("Linked module contains an invalid string offset");
        return table.mid(offset, end - offset);
    };
    const auto names = contents(section(nameIndex));
    int textIndex = -1;
    for (int index = 0; index < count; ++index) {
        const auto header = section(index);
        const auto label = string(names, u32(header, 0));
        if (label == ".text") {
            if (textIndex >= 0 || u32(header, 12) != address || contents(header) != result.bytes)
                throw Error("Linked text does not match the extracted module");
            textIndex = index;
        } else if ((u32(header, 8) & 2) && u32(header, 20) && label != ".reginfo")
            throw Error("Code module requires a separate runtime data allocation: " +
                        QString::fromLatin1(label));
    }
    if (textIndex < 0 || result.bytes.isEmpty() ||
        quint64(address) + result.bytes.size() > 0x100000000ULL)
        throw Error("Missing or invalid linked text allocation");
    for (int index = 0; index < count; ++index) {
        const auto header = section(index);
        if (u32(header, 4) != 2)
            continue;
        const auto table = contents(header), labels = contents(section(u32(header, 24)));
        if (u32(header, 36) != 16 || table.size() % 16)
            throw Error("Invalid ELF32 symbol table");
        for (qsizetype offset = 0; offset < table.size(); offset += 16) {
            const auto info = u8(table, offset + 12);
            if ((info & 15) != 2)
                continue;
            const auto symbolName = QString::fromLatin1(string(labels, u32(table, offset)));
            const auto value = u32(table, offset + 4), size = u32(table, offset + 8);
            if (u16(table, offset + 14) != textIndex || value < address ||
                quint64(value) >= quint64(address) + result.bytes.size() || (value & 3) || !size ||
                size > quint64(address) + result.bytes.size() - value || symbolName.isEmpty() ||
                result.entries.contains(symbolName))
                throw Error("Invalid exported module function: " + symbolName);
            result.functionExtent = std::max(result.functionExtent, value - address + size);
            // Local functions constrain the extent but are not public entries.
            if ((info >> 4) == 1)
                result.entries[symbolName] = value;
        }
    }
    if (result.entries.isEmpty())
        throw Error("Compiled module has no exported functions");
    result.addressReferences =
        readMipsAddressReferences(readFile(QDir(directory).filePath(name + ".o")), result, symbols);
    return result;
}
QByteArray CompiledModule::fitOwner(qsizetype size) const {
    if (size < 0 || size < functionExtent || !functionExtent)
        throw Error("Compiled function exceeds its owner allocation");
    for (qsizetype offset = size; offset < bytes.size(); ++offset)
        if (bytes[offset] != 0)
            throw Error("Nonzero compiled bytes outside the function owner");
    auto fitted = bytes.first(std::min(size, bytes.size()));
    fitted.append(QByteArray(size - fitted.size(), 0));
    return fitted;
}
QByteArray Compiler::compileData(const QByteArray &bytes, const QString &directory,
                                 const QString &name) const {
    QByteArray source =
        "unsigned char " + name.toLatin1() + "[" + QByteArray::number(bytes.size()) + "] = {\n";
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        source += QByteArray::number(u8(bytes, i));
        source += ",";
        if (i % 24 == 23)
            source += "\n";
    }
    source += "\n};\n";
    auto output = compile(source, directory, name, 0, ".data");
    if (output.size() < bytes.size() || output.first(bytes.size()) != bytes)
        throw Error("Compiled data differs from the authored records");
    for (qsizetype i = bytes.size(); i < output.size(); ++i)
        if (u8(output, i) != 0)
            throw Error("Unexpected nonzero compiler alignment bytes");
    return output.first(bytes.size());
}
QByteArray Compiler::assembleOwner(const QByteArray &source, const QString &directory,
                                   const QString &name, quint32 address,
                                   const QMap<QString, quint32> &symbols,
                                   bool preserveInstructionPositions) const {
    QDir dir(directory);
    if (!dir.mkpath("."))
        throw Error("Cannot create assembly build directory");
    auto input = dir.absoluteFilePath(name + ".s"), object = dir.absoluteFilePath(name + ".o");
    auto script = dir.absoluteFilePath(name + ".ld"), elf = dir.absoluteFilePath(name + ".elf");
    auto binary = dir.absoluteFilePath(name + ".bin");
    atomicWrite(input, ".set noat\n.set noreorder\n.section .owner,\"ax\"\n.balign 4\n" + source);
    QStringList arguments = {"-G", "0", "-mips3", "-mabi=32", "-EB", "-o", object, input};
    if (preserveInstructionPositions)
        arguments.prepend("--vr4300mul-off");
    run("mips-kmc-elf-as.exe", arguments, directory);
    QByteArray bindings;
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
        if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]*$").match(it.key()).hasMatch())
            throw Error("Invalid linked assembly symbol name");
        bindings += QString("%1 = 0x%2;\n").arg(it.key()).arg(it.value(), 0, 16).toLatin1();
    }
    atomicWrite(script,
                bindings +
                    QString("FAMILY_LOOKUP = 0x801CF2F0; CHECK_CHANGE = 0x801D2358;\nSECTIONS "
                            "{ .owner 0x%1 : { *(.owner) } }\n")
                        .arg(address, 0, 16)
                        .toLatin1());
    run("mips-kmc-elf-ld.exe", {"-EB", "-T", script, "-o", elf, object}, directory);
    QStringList extract = {"-O", "binary"};
    for (auto section : {".text", ".data", ".bss", ".reginfo", ".mdebug", ".pdr", ".comment"})
        extract << "--remove-section=" + QString::fromLatin1(section);
    extract << elf << binary;
    run("mips-kmc-elf-objcopy.exe", extract, directory);
    return readFile(binary);
}
static void rebuildPromotion(const Project &p, const Compiler &compiler, const QString &directory,
                             BuildResult &result) {
    if (!p.promotion() || !p.promotion()->modified())
        return;
    const auto &promotion = *p.promotion();
    if (promotion.classCount() != 81)
        throw Error("Expanded promotion data must use the coordinated class runtime builder");
    bool family = promotion.expanded || promotion.directGates ||
                  promotion.families != promotion.originalFamilies ||
                  encodeLzss(promotion.gates).size() > promotion.retailResource.capacity;
    if (family) {
        const auto &bytes = p.rom().bytes();
        if (!promotion.expanded) {
            auto free = bytes.mid(0x27d0000, 65536);
            if (free.size() != 65536 ||
                std::any_of(free.begin(), free.end(), [](char c) { return quint8(c) != 255; }))
                throw Error("Promotion rebuild allocation is occupied");
        }
        const auto legacy = bytes.mid(0x27b0800, 1024);
        if (!promotion.directGates &&
            std::any_of(legacy.begin(), legacy.end(), [](char c) { return quint8(c) != 255; }))
            throw Error("Legacy promotion allocation needs migration before this export");
        if (promotion.directGates)
            result.image.replace(0x27b0800, 1024, QByteArray(1024, char(-1)));
        auto compiled =
            compiler.compileData(promotion.decodedFamilies(), directory, "class_families");
        result.image.replace(0x27d0000, 65536, promotion.familyContainer(compiled));
        const auto owners = promotionOwners()["owners"].toArray();
        const auto classPlacement = ClassPlacement::read(p.rom());
        const QMap<int, QByteArray> replacements = {{0xc38c0, "bne $v1, $v0, .+32"},
                                                    {0xc3a88, "bne $v1, $v0, .+32"},
                                                    {0xcce00, "move $v0, $v1"}};
        for (auto v : owners) {
            auto owner = v.toObject();
            auto start = owner["start"].toInt(), size = owner["size"].toInt();
            auto original = bytes.mid(start, size);
            if (classPlacement)
                original = classPlacement->canonicalReferences(original, start);
            auto hash = QString::fromLatin1(sha256(canonicalArtOwner(original,start)));
            if (hash != owner["retailSha256"].toString() &&
                hash != owner["familySha256"].toString() &&
                !(owner.contains("cSourceSha256") && hash == owner["cSourceSha256"].toString()) &&
                !(promotion.directGates && owner.contains("directGateSha256") &&
                  hash == owner["directGateSha256"].toString()))
                throw Error("Promotion source owner was modified: " + owner["name"].toString());
            QByteArray source, built;
            auto expected = owner["familySha256"].toString().toLatin1();
            if (owner.contains("cFragment")) {
                const auto blockStart = owner["fragmentStart"].toInt();
                const auto blockEnd = owner["fragmentEnd"].toInt();
                if (blockStart < start || blockEnd > start + size || blockEnd <= blockStart + 16)
                    throw Error("Invalid promotion source fragment allocation");
                const quint32 entryAddress = quint32(blockStart + 16) + 0x801135a0u;
                QMap<QString, quint32> symbols;
                const auto bindings = owner["symbols"].toObject();
                for (auto it = bindings.begin(); it != bindings.end(); ++it)
                    symbols.insert(it.key(), quint32(it.value().toDouble()));
                auto fragment = compiler.compile(
                    readFile(":/game/promotion/" + owner["cFragment"].toString()), directory,
                    owner["name"].toString() + "_rows", entryAddress, ".text",
                    {{"class_menu_config.h", readFile(":/game/promotion/class_menu_config.h")}},
                    symbols);
                if (fragment.size() > blockEnd - blockStart - 16)
                    throw Error("Compiled class renderer exceeds its existing allocation");
                auto entry = compiler.assembleOwner(
                    readFile(":/game/promotion/" + owner["fragmentEntry"].toString()), directory,
                    owner["name"].toString() + "_entry", quint32(blockStart) + 0x801135a0u,
                    {{"CLASS_RENDER_ROWS", entryAddress},
                     {"CLASS_RENDER_CONTINUE", quint32(blockEnd) + 0x801135a0u}});
                if (entry.size() != 16)
                    throw Error("Unexpected class renderer entry size");
                entry += fragment;
                entry.append(QByteArray(blockEnd - blockStart - entry.size(), 0));
                built = original;
                built.replace(blockStart - start, entry.size(), entry);
                const auto styleStart = owner["styleStart"].toInt();
                const auto styleSize = owner["styleSize"].toInt();
                if (styleStart < start || styleSize <= 0 || styleStart + styleSize > start + size)
                    throw Error("Invalid class drawing source allocation");
                const auto style = compiler.assembleOwner(
                    "WIDE_CLASSES = 0\n" +
                        readFile(":/game/promotion/" + owner["styleSource"].toString()),
                    directory, owner["name"].toString() + "_style",
                    quint32(styleStart) + 0x801135a0u,
                    {{"CLASS_DRAW_STYLE", 0x8019b5c4u}, {"DRAW_CLASS_SPRITE", 0x801ba900u}});
                if (style.size() != styleSize)
                    throw Error("Unexpected class drawing source size");
                built.replace(styleStart - start, styleSize, style);
                expected = owner["cSourceSha256"].toString().toLatin1();
            } else if (owner.contains("cSource")) {
                QMap<QString, quint32> symbols;
                const auto bindings = owner["symbols"].toObject();
                for (auto it = bindings.begin(); it != bindings.end(); ++it)
                    symbols.insert(it.key(), quint32(it.value().toDouble()));
                const auto module = compiler.compileModule(
                    readFile(":/game/promotion/" + owner["cSource"].toString()), directory,
                    owner["name"].toString(), quint32(start) + 0x801135a0u,
                    {{"class_menu_config.h", readFile(":/game/promotion/class_menu_config.h")}},
                    symbols);
                // Some accepted owners include the next routine's split entry.
                // Keep that authenticated suffix outside the C allocation.
                const auto extent = owner.value("cExtent").toInt(size);
                if (extent <= 0 || extent > size || (extent & 3))
                    throw Error("Invalid C function allocation within its runtime owner");
                built = module.fitOwner(extent) + original.sliced(extent);
                expected = owner["cSourceSha256"].toString().toLatin1();
            } else if (start == 0xbbd50)
                source = readFile(":/game/promotion/family-lookup.s");
            else
                for (int offset = 0; offset < size; offset += 4) {
                    auto site = start + offset;
                    if (replacements.contains(site))
                        source += replacements[site];
                    else if (owner.contains("keySite") && site == owner["keySite"].toInt())
                        source += "lui $a0, 0x0223";
                    else if (owner.contains("keySite") && site == owner["keySite"].toInt() + 4)
                        source += "ori $a0, $a0, 0xbd80";
                    else
                        source += ".word 0x" + QByteArray::number(u32(original, offset), 16);
                    source += '\n';
                }
            if (built.isEmpty())
                built = compiler.assembleOwner(source, directory, owner["name"].toString(),
                                               quint32(start) + 0x801135a0u);
            qsizetype fragmentEnd = 0;
            for (const auto &value : owner["assemblyFragments"].toArray()) {
                const auto fragment = value.toObject();
                const auto offset = fragment["offset"].toInt(-1);
                const auto length = fragment["size"].toInt();
                if (offset < fragmentEnd || length <= 0 || offset + length > built.size() ||
                    ((offset | length) & 3))
                    throw Error("Invalid or overlapping maintained assembly fragment");
                const auto code = compiler.assembleOwner(
                    readFile(":/game/promotion/" + fragment["source"].toString()), directory,
                    owner["name"].toString() + "_fragment_" + QString::number(offset),
                    quint32(start + offset) + 0x801135a0u);
                if (code.size() != length)
                    throw Error("Assembly fragment exceeds its instruction allocation");
                built.replace(offset, length, code);
                fragmentEnd = offset + length;
            }
            if (built.size() != size || sha256(canonicalArtOwner(built,start)) != expected)
                throw Error("Compiled promotion owner differs from validated source: " +
                            owner["name"].toString());
            if (classPlacement)
                built = classPlacement->placedReferences(built, start);
            result.image.replace(start, size, built);
        }
        result.report << QString("Compiled promotion data, menu, selection, discovery and "
                                 "class-row C; rebuilt %1 "
                                 "complete runtime owners within their original extents")
                             .arg(owners.size());
    } else if (promotion.gates != promotion.originalGates) {
        auto compiled = compiler.compileData(promotion.gates, directory, "promotion_gates");
        auto container = promotion.retailResource.container(compiled);
        result.image.replace(promotion.retailResource.romOffset, container.size(), container);
        result.report << "Compiled promotion requirements into the retail resource";
    }
    if (promotion.masks != promotion.originalMasks || promotion.flags != promotion.originalFlags) {
        auto data = p.rom().bytes().mid(0xdbd2c, 162);
        for (int i = 0; i < 81; ++i) {
            data[i * 2 + 1] = promotion.masks[i];
            data[i * 2] = char((quint8(data[i * 2]) & 15) | quint8(promotion.flags[i]));
        }
        auto compiled = compiler.compileData(data, directory, "promotion_compatibility");
        result.image.replace(0xdbd2c, compiled.size(), compiled);
        if (!family) {
            const auto classPlacement = ClassPlacement::read(p.rom());
            for (const auto &value : promotionOwners()["owners"].toArray()) {
                const auto owner = value.toObject();
                const auto start = owner["start"].toInt(), size = owner["size"].toInt();
                if (!(owner["onMaskEdit"].toBool() && promotion.masks != promotion.originalMasks) &&
                    !(owner["onFlagsEdit"].toBool() && promotion.flags != promotion.originalFlags))
                    continue;
                auto prior = p.rom().bytes().mid(start, size);
                if (classPlacement)
                    prior = classPlacement->canonicalReferences(prior, start);
                const auto priorHash = QString::fromLatin1(sha256(prior));
                if (priorHash != owner["retailSha256"].toString() &&
                    priorHash != owner["cSourceSha256"].toString())
                    throw Error("Class-line reader was modified: " + owner["name"].toString());
                QMap<QString, quint32> symbols;
                const auto bindings = owner["symbols"].toObject();
                for (auto it = bindings.begin(); it != bindings.end(); ++it)
                    symbols.insert(it.key(), quint32(it.value().toDouble()));
                const auto module = compiler.compileModule(
                    readFile(":/game/promotion/" + owner["cSource"].toString()), directory,
                    owner["name"].toString(), quint32(start) + 0x801135a0u,
                    {{"class_menu_config.h", readFile(":/game/promotion/class_menu_config.h")}},
                    symbols);
                auto reader = module.fitOwner(size);
                if (sha256(reader) != owner["cSourceSha256"].toString().toLatin1())
                    throw Error("Compiled class-line reader differs from validated source");
                if (classPlacement)
                    reader = classPlacement->placedReferences(reader, start);
                result.image.replace(start, size, reader);
            }
            result.report << "Compiled class-line data and its affected C readers";
        }
    }
}
BuildResult rebuild(const Project &input, const QString &toolsRoot, const QString &directory) {
    auto p = input;
    p.validateItemReferences();
    Compiler compiler(toolsRoot);
    compiler.authenticate();
    BuildResult result{p.rom().bytes(), {}};
    if(p.rom().revision()==0)p.loadAnimationOverrides();
    for (const auto &r : p.records(Table::Classes))
        if (r.original.size() == r.bytes.size() &&
            (r.original[ClassData::unitSizeOffset] != r.bytes[ClassData::unitSizeOffset] ||
             r.original[ClassData::itemCapacityOffset] != r.bytes[ClassData::itemCapacityOffset])) {
            p.loadScenarios();
            break;
        }
    if (p.scenarios())
        p.scenarios()->validateForExport(p.classData());
    if (p.rom().revision() == 0) {
        QVector<int> changedLanes;
        for (const auto &row : p.records(Table::Classes))
            if ((u8(row.bytes, ClassData::unitSizeOffset) == 2) !=
                (u8(row.original, ClassData::unitSizeOffset) == 2))
                changedLanes.append(row.id);
        if (!changedLanes.isEmpty()) {
            p.loadArmyArtwork();
            const auto classes = p.classData();
            for (const auto id : changedLanes) {
                const auto &asset = classes.entries()[id];
                const auto lane = ArmyArtwork::classLane(asset),
                           model = ArmyArtwork::classModel(asset);
                for (int side = 0; side < 2; ++side)
                    if (!p.armyArtwork()->present(lane * 2 + side, model))
                        throw Error(
                            QString("Class %1's new unit size selects a missing %2 Army model "
                                    "0x%3; assign a compatible model or create that plane")
                                .arg(id)
                                .arg(side ? "player-facing" : "front-facing")
                                .arg(model, 2, 16, QChar('0')));
            }
        }
    }
    const auto priorClassPlacement = ClassPlacement::read(p.rom());
    const auto priorShopPlacement = ShopPlacement::read(p.rom());
    const auto priorCustom = CustomPlacement::read(p.rom());
    if (priorCustom)
        p.loadEncounters();
    const bool customSquads = p.encounters() && (p.encounters()->customSquads().hasProfiles() || p.encounters()->slotsPerPool() > 10);
    if (customSquads)
        p.encounters()->customSquads().validateFormations(p.classData());
    if (priorCustom)
        result.image = removeCustomInstallation(p.rom(), *priorCustom, compiler, directory);
    if (priorShopPlacement && priorShopPlacement->version < 4)
        p.loadShops();
    const bool expandedClasses =
        p.records(Table::Classes).size() > 165 ||
        (p.promotion() && p.promotion()->classCount() != 81) ||
        (priorClassPlacement && !priorClassPlacement->runtimeInventory.isEmpty());
    if (expandedClasses) {
        p.loadPromotion();
        p.loadTexts();
        if (p.promotion()->classCount() != p.records(Table::Classes).size() ||
            p.textBanks()["classes"].entries.size() != p.records(Table::Classes).size() + 60)
            throw Error("Expanded class records, promotion data and descriptions must have "
                        "matching counts");
    }
    const bool avatarEdits = p.avatarArtwork() && p.avatarArtwork()->modified();
    const bool armyRoutes = p.armyArtwork() && !p.armyArtwork()->routes().isEmpty();
    if (armyRoutes)
        p.armyArtwork()->validateRoutes(p.classData());
    const bool relocateItems =
        p.itemNamesModified() || (priorClassPlacement && priorClassPlacement->layout.items);
    std::optional<ItemData> itemData;
    if (relocateItems) {
        itemData = p.itemData();
        p.loadTexts();
        p.loadIconArtwork();
        p.loadAnimationRoutes();
        const int count = int(itemData->entries().size());
        if (p.textBanks()["items"].entries.size() != count ||
            p.iconArtwork()->equipmentCount() != count - 1 ||
            p.animationRoutes()->section(2).size() < count)
            throw Error("Item records, descriptions, icons and weapon appearances must have "
                        "matching counts");
    }
    const bool relocateClasses = relocateItems || priorClassPlacement.has_value() ||
                                 p.classNamesModified() || expandedClasses || customSquads ||
                                 avatarEdits || armyRoutes;
    // Retained sections must become actual native owners before they can enter an export.
    const auto pending = p.pendingLegacySections();
    if (!pending.isEmpty())
        throw Error("Imported sections still need native export support: " + pending.join(", "));
    for (auto table : {Table::Classes, Table::Items, Table::Consumables}) {
        if ((table == Table::Classes && relocateClasses) ||
            (table == Table::Items && relocateItems))
            continue;
        const auto &records = p.records(table);
        bool changed = false;
        for (const auto &r : records)
            if (r.bytes != r.original)
                changed = true;
        if (!changed)
            continue;
        QByteArray data;
        for (const auto &r : records)
            data += r.bytes;
        auto compiled = compiler.compileData(data, directory, tableKey(table) + "_records");
        auto offset = records.front().romOffset;
        if (offset < 0 || offset + compiled.size() > result.image.size())
            throw Error("Built records exceed their ROM allocation");
        result.image.replace(offset, compiled.size(), compiled);
        result.report << QString("Compiled %1 records from generated C (%2 bytes)")
                             .arg(tableKey(table))
                             .arg(compiled.size());
    }
    for (const auto &bank : p.textBanks()) {
        if (!bank.modified())
            continue;
        auto compiled =
            compiler.compileData(bank.serialized(), directory, "descriptions_" + bank.spec.id);
        if (bank.spec.id == "classes" && p.rom().revision() == 0 &&
            (ClassTextPlacement::read(p.rom()) || bank.entries.size() != 225 ||
             encodeLzss(compiled).size() > bank.resource.capacity)) {
            result.image =
                linkClassText(Rom::fromBytes(result.image), compiled, int(bank.entries.size()));
        } else if (bank.spec.id == "items" && !p.rom().revision() &&
                   (ClassTextPlacement::read(p.rom(), true) || bank.entries.size() != 278 ||
                    encodeLzss(compiled).size() > bank.resource.capacity)) {
            result.image = linkClassText(Rom::fromBytes(result.image), compiled,
                                         int(bank.entries.size()), true);
        } else {
            auto container = bank.resource.container(compiled);
            result.image.replace(bank.resource.romOffset, container.size(), container);
        }
        result.report << "Rebuilt " + bank.spec.label + " from edited C data";
    }
    if (p.encounters() && p.encounters()->modified()) {
        if (p.encounters()->rewards().modified()) {
            result.image = rebuildCreatureRewards(Rom::fromBytes(result.image),
                                                  p.encounters()->rewards(), compiler, directory);
            result.report << "Compiled weighted creature rewards for every byte-sized class ID; "
                             "selection stays inside its original function and stack frame";
        }
        Encounters::verifyReader(p.rom());
        auto write = [&](const QByteArray &bytes, int offset, const QString &name) {
            const auto built = compiler.compileData(bytes, directory, name);
            if (built != bytes)
                throw Error("Compiled encounter table differs from project");
            result.image.replace(offset, bytes.size(), built);
        };
        write(p.encounters()->retailPoolBytes(), Encounters::poolOffset, "encounter_pools");
        write(p.encounters()->rateBytes(), Encounters::rateOffset, "encounter_terrain_rates");
        write(p.encounters()->slotBytes(), Encounters::slotOffset, "encounter_terrain_slots");
        if (p.encounters()->scenarioRates().modified() ||
            (p.encounters()->globalRate().modified() && ScenarioRates::placement(p.rom()))) {
            result.image =
                rebuildScenarioRates(Rom::fromBytes(result.image), p.encounters()->scenarioRates(),
                                     p.encounters()->globalRate(), compiler, directory);
            result.report << "Rebuilt independent scenario rates and inherited global values "
                             "inside the original dispatcher";
        } else if (p.encounters()->globalRate().modified()) {
            const auto &rate = p.encounters()->globalRate();
            const auto code = compiler.assembleOwner(
                rate.assemblyDefinitions() + readFile(":/game/encounters/dispatcher.s"), directory,
                "encounter_dispatcher", EncounterRate::ramAddress,
                EncounterRate::serviceAddresses(), true);
            if (code.size() != EncounterRate::codeSize)
                throw Error("Encounter dispatcher exceeds its original allocation");
            result.image.replace(EncounterRate::romOffset, EncounterRate::codeSize, code);
            if (EncounterRate::load(Rom::fromBytes(result.image)).json() != rate.json())
                throw Error("Compiled encounter rate differs from project");
            result.report << "Rebuilt the encounter dispatcher with independent 32-bit pass counts "
                             "in its original instruction space";
        }
        result.report << "Compiled the original 45 scenario groups and 32 terrain rates/assignments into their "
                         "original allocations";
    }
    if (customSquads && !CreatureRewards::placement(Rom::fromBytes(result.image))) {
        result.image = rebuildCreatureRewards(Rom::fromBytes(result.image),
                                              p.encounters()->rewards(), compiler, directory);
        result.report << "Rebuilt class reward rows alongside custom encounter rewards";
    }
    if (p.tools() && p.tools()->experienceModified()) {
        if (!ToolSettings::load(p.rom()).available())
            throw Error("Experience source code changed outside its supported model");
        const auto sites = ToolSettings::experienceSites();
        for (int i = 0; i < sites.size(); ++i) {
            const auto site = sites[i].toObject();
            const int offset = site["rom"].toInt(), size = site["size"].toInt();
            if (result.image.mid(offset, size) != p.rom().bytes().mid(offset, size))
                throw Error("Another rebuild owner overlaps the experience setting");
            const auto code = compiler.assembleOwner(
                p.tools()->assemblyDefinitions(i) + readFile(":/game/tools/experience.s"),
                directory, QString("experience_%1").arg(i), quint32(site["ram"].toInteger()),
                {{"XP_RETAIL_WEIGHTED", 0x801d604c}}, true);
            p.tools()->validateCompiled(i, code);
            result.image.replace(offset, size, code);
        }
        const auto reopened = ToolSettings::load(Rom::fromBytes(result.image));
        if (!reopened.available() || reopened.experiencePercent() != p.tools()->experiencePercent())
            throw Error("Rebuilt experience setting failed readback");
        result.report << QString("Compiled experience size-weight scale at %1% in its original "
                                 "combat code allocation")
                             .arg(p.tools()->experiencePercent());
    }
    if (p.consumableEffects() && p.consumableEffects()->modified()) {
        const auto &effects = *p.consumableEffects();
        ConsumableEffects::load(p.rom()); // Authenticate the complete owner and dispatch table.
        const auto built = compiler.assembleOwner(
            effects.assemblyDefinitions() + readFile(":/game/consumables/effects.s"), directory,
            "consumable_effects", ConsumableEffects::ramAddress,
            ConsumableEffects::serviceAddresses(), true);
        if (built != effects.expectedCode(p.rom().bytes().mid(ConsumableEffects::romOffset,
                                                              ConsumableEffects::codeSize)))
            throw Error("Compiled consumable effects differ outside their authored parameters");
        result.image.replace(ConsumableEffects::romOffset, ConsumableEffects::codeSize, built);
        result.report << "Compiled all ten consumable effect models from maintained assembly "
                         "within the original resident allocation";
    }
    if (p.shops() &&
        (p.shops()->modified() || (priorShopPlacement && priorShopPlacement->version < 4))) {
        result.image = rebuildShops(Rom::fromBytes(result.image), *p.shops(), compiler, directory);
        result.report << "Compiled shop stock, producer and halfword readers; retained original "
                         "RAM allocations";
    }
    if (!expandedClasses)
        rebuildPromotion(p, compiler, directory, result);
    if (relocateClasses) {
        auto classData = p.classData();
        if (avatarEdits) {
            auto bundles = p.avatarArtwork()->decodedBundles();
            for (auto it = bundles.begin(); it != bundles.end(); ++it)
                it.value() = compiler.compileData(it.value(), directory,
                                                  QString("portrait_%1").arg(it.key()));
            result.image =
                p.avatarArtwork()->rebuild(Rom::fromBytes(result.image), classData, bundles);
            result.report << QString(
                                 "Compiled %1 independent portraits and rebuilt their directory "
                                 "and class selectors")
                                 .arg(bundles.size());
        }
        auto offset = priorClassPlacement ? priorClassPlacement->dataRomOffset
                                          : quint32((result.image.size() + 15) & ~qsizetype(15));
        const auto &data = classData;
        const auto classImage = data.build(0x80243db0);
        const auto dataSize = classImage.bytes.size() +
                              (itemData ? itemData->build(classImage.layout.end).size() : 0);
        if (priorClassPlacement &&
            dataSize > priorClassPlacement->layout.end - priorClassPlacement->layout.indexBase &&
            quint64(priorClassPlacement->dataRomOffset) + priorClassPlacement->layout.end -
                    priorClassPlacement->layout.indexBase !=
                quint64(result.image.size()))
            offset = quint32((result.image.size() + 15) & ~qsizetype(15));
        std::optional<CustomRuntimeBuild> custom;
        std::function<QByteArray(quint32, quint32)> residentBuilder;
        if (customSquads) {
            const auto classEnd = quint64(offset) + dataSize;
            const auto prospectiveEnd = qMax(quint64(result.image.size()), classEnd);
            auto index = quint32((prospectiveEnd + 15) & ~quint64(15));
            const int poolSlots = p.encounters()->slotsPerPool();
            const auto bytes = p.encounters()->customSquads().bytes(0x2800000, poolSlots).size() +
                               (poolSlots > 10 ? (45 * poolSlots * 2 + 3) & ~3 : 0);
            if (priorCustom) {
                const auto before = priorCustom->dataOffset;
                const bool separate = quint64(before) + bytes <= offset || before >= classEnd;
                const bool append = before >= prospectiveEnd;
                const bool reuse = bytes <= priorCustom->dataSize &&
                                   quint64(before) + bytes <= quint64(result.image.size()) &&
                                   result.image.mid(before, bytes) == QByteArray(bytes, char(-1));
                if (separate && (append || reuse))
                    index = before;
            }
            residentBuilder = [&, index](quint32 address, quint32 capacity) {
                custom = buildCustomRuntime(compiler, directory, address, capacity, index,
                                            p.encounters()->slotsPerPool(),
                                            int(p.encounters()->customSquads().profiles().size()));
                return custom->bytes;
            };
        }
        auto classes = buildClassRuntime(
            data, compiler, directory, offset,
            expandedClasses || (priorClassPlacement && priorClassPlacement->saves.installed()),
            residentBuilder, itemData ? &*itemData : nullptr);
        classes.removeResident = priorCustom.has_value() && !customSquads;
        const auto menus =
            expandedClasses
                ? std::optional<PromotionRuntimeBuild>(buildPromotionRuntime(
                      p.rom(), classes.data.layout, *p.promotion(), compiler, directory))
                : std::nullopt;
        result.image =
            linkClassRuntime(Rom::fromBytes(result.image), classes, menus ? &*menus : nullptr);
        if (custom) {
            result.image = installCustomEncounters(Rom::fromBytes(result.image),
                                                   p.encounters()->customSquads(), *custom,
                                                   compiler, directory, priorCustom, p.encounters()->poolBytes(),
                                                   p.encounters()->slotsPerPool());
            result.report << QString(
                                 "Compiled %1 custom squads and their battle behavior; %2 bytes "
                                 "reserved in reclaimed resident storage within 4 MB RAM")
                                 .arg(p.encounters()->customSquads().profiles().size())
                                 .arg(custom->bytes.size());
        } else if (priorCustom) {
            result.report << "Removed custom squad callers and returned their resident allocation "
                             "to the heap";
        }
        if (menus)
            result.report
                << QString(
                       "Compiled expanded promotion, selection and category menus for %1 classes; "
                       "rebuilt description routing and SRAM/Controller Pak conversion")
                       .arg(classes.data.layout.count);
        result.report << QString("Rebuilt %1 classes with relocated names, appearance data and "
                                 "source-built initialization (%2 bytes of RAM)")
                             .arg(classes.data.layout.count)
                             .arg(classes.data.bytes.size());
    }
    if (relocateItems)
        result.report << QString("Rebuilt %1 equipment records, names and their runtime references "
                                 "within 4 MB RAM")
                             .arg(itemData->entries().size());
    if (p.rom().revision() == 0 &&
        !(p.promotion() && (p.promotion()->expanded || p.promotion()->modified())) &&
        p.manifest().value("records").toObject().value("classes").toObject().size() > 0) {
        auto source = readFile(":/game/accepted/func_000bbd50.c");
        auto function = compiler.compile(source, directory, "func_000bbd50", 0x801cf2f0, ".text");
        if (function.size() != 0x30 || function != p.rom().bytes().mid(0xbbd50, 0x30))
            throw Error("Source-built class-family getter differs from the loaded ROM; this "
                        "runtime needs its own source owner");
        result.image.replace(0xbbd50, 0x30, function);
        result.report << "Compiled accepted class-family C at Army RAM 0x801CF2F0; verified its "
                         "complete 48-byte owner";
    }
    if (p.scenarios() && p.scenarios()->modified()) {
        auto plan = p.scenarios()->plan(p.classData());
        const auto resources = ScenarioResources::load(Rom::fromBytes(result.image));
        // The user-authored library and each scenario compile during this export.
        if (plan.enemies != resources.enemyData())
            plan.enemies = compiler.compileData(plan.enemies, directory, "scenario_squads");
        for (auto it = plan.esets.begin(); it != plan.esets.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("scenario_%1").arg(it.key()));
        result.image = resources.rebuild(Rom::fromBytes(result.image), plan.enemies, plan.esets);
        result.report << QString("Rebuilt %1 scenarios and %2 squad templates through original "
                                 "resource links; shared scenarios retain independent edits")
                             .arg(plan.esets.size())
                             .arg(plan.enemies.size() / 35);
    }
    if (p.world() && p.world()->modified()) {
        p.loadShops();
        for (int id = 1; id <= ScenarioWorld::townCount; ++id)
            if (u8(p.world()->town(id), 27) >= p.shops()->size())
                throw Error(QString("Stronghold %1 references a missing shop").arg(id));
        auto plan = p.world()->plan();
        const auto worldRom = Rom::fromBytes(result.image);
        const auto resources = WorldResources::load(worldRom);
        if (plan.towns != resources.towns(worldRom))
            plan.towns = compiler.compileData(plan.towns, directory, "scenario_towns");
        for (auto it = plan.treasures.begin(); it != plan.treasures.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("treasure_%1").arg(it.key()));
        for (auto it = plan.sites.begin(); it != plan.sites.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("sites_%1").arg(it.key()));
        result.image = resources.rebuild(worldRom, plan.towns, plan.treasures, plan.sites);
        result.report << QString("Rebuilt global sites, %1 treasure lists and %2 allegiance lists "
                                 "through original resource links")
                             .arg(plan.treasures.size())
                             .arg(plan.sites.size());
    }
    if ((p.scenePrograms() && p.scenePrograms()->modified()) ||
        (p.sceneEvents() && p.sceneEvents()->modified())) {
        p.scenePrograms()->validateReferences(Rom::fromBytes(result.image),
                                              p.dialogues() ? &*p.dialogues() : nullptr);
        auto plan = p.scenePrograms()->plan();
        for (auto it = plan.begin(); it != plan.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("scene_%1").arg(it.key()));
        auto authored = p.scenePrograms()->authoredPlan();
        for (auto &scene : authored)
            scene.program = compiler.compileData(scene.program, directory, "scene_" + scene.id);
        const auto events = p.sceneEvents() ? *p.sceneEvents() : SceneEvents::load(p.rom());
        events.validate(*p.scenePrograms());
        auto eventPlan = events.plan();
        for (auto it = eventPlan.rows.begin(); it != eventPlan.rows.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("event_%1").arg(it.key()));
        result.image = SceneProgramResources::rebuild(Rom::fromBytes(result.image), plan, authored,
                                                      &eventPlan);
        result.report
            << QString("Compiled and rebuilt %1 event launch rows").arg(eventPlan.rows.size());
        result.report
            << QString("Compiled and rebuilt %1 scene programs through original resource links")
                   .arg(plan.size());
    }
    if (p.dialogues() && p.dialogues()->modified()) {
        auto plan = p.dialogues()->plan();
        for (auto it = plan.begin(); it != plan.end(); ++it)
            it.value() =
                compiler.compileData(it.value(), directory, QString("dialogue_%1").arg(it.key()));
        result.image = DialogueResources::rebuild(Rom::fromBytes(result.image), plan);
        result.report
            << QString("Compiled and rebuilt %1 dialogue archives through original resource links")
                   .arg(plan.size());
    }
    if(p.iconArtwork() && p.iconArtwork()->modified()) {
        auto packs=p.iconArtwork()->decodedPacks();
        for(int i=0;i<2;++i)packs[i]=compiler.compileData(packs[i],directory,QString("icon_pack_%1").arg(i));
        result.image=p.iconArtwork()->rebuild(Rom::fromBytes(result.image),packs);
        result.report << "Compiled and rebuilt both shared item-icon packs through their original resource loaders";
    }
    if(p.armyArtwork() && p.armyArtwork()->pixelsModified()) {
        auto packs=p.armyArtwork()->decodedPacks();
        for(int i=0;i<6;++i)packs[i]=compiler.compileData(packs[i],directory,QString("army_atlas_%1").arg(i));
        result.image=p.armyArtwork()->rebuild(Rom::fromBytes(result.image),packs);
        result.report << "Compiled all six Army/formation atlases and retained shared front/back resource operands";
    }
    if (p.animationOverrides() && p.animationOverrides()->available() &&
        (p.animationOverrides()->modified() || !p.animationOverrides()->entries().isEmpty())) {
        const auto classes = p.classData();
        auto overrides = *p.animationOverrides();
        if (!overrides.entries().isEmpty()) {
            p.loadAnimationRoutes();
            overrides.setEntries(overrides.entries(), p.rom(), classes, *p.animationRoutes());
        }
        if (overrides.needsRebuild(classes)) {
            result.image = rebuildAnimationOverrides(Rom::fromBytes(result.image), overrides,
                                                     classes, compiler, directory);
            result.report << (overrides.entries().isEmpty()
                                  ? "Removed animation overrides and restored source-built "
                                    "original instructions"
                                  : QString(
                                        "Compiled %1 Normal/Blocked animation overrides from "
                                        "maintained sources; the existing patch requires 8 MB RAM")
                                        .arg(overrides.entries().size()));
        }
    }
    if(p.rom().bytes().mid(AnimationSequences::descriptorOffset,8)=="LC05ASEQ")p.loadAnimationRoutes();
    auto exportAnimationRoutes=p.animationRoutes();
    if(p.animationSequences()&&(p.animationSequences()->modified()||!p.animationSequences()->groups().isEmpty())) {
        p.loadAnimationRoutes();
        const auto &sequences=*p.animationSequences();
        p.validateAnimationAssignments();
        sequences.validate(*p.animationRoutes(),p.records(Table::Classes).size());
        const auto beforeMarkers=result.image;
        result.image=rebuildBodyMarkers(Rom::fromBytes(result.image),p.rom(),sequences.requiresMarkerRuntime(),compiler,directory);
        if(result.image!=beforeMarkers)result.report<<(sequences.requiresMarkerRuntime()?"Compiled wide-frame equipment marker scans within the original code allocation":"Restored source-built original equipment marker scans");
        const auto plan=sequences.plan(Rom::fromBytes(result.image));
        const auto compiled=plan.payload.isEmpty()?QByteArray{}:compiler.compileData(plan.payload,directory,"private_animation_resources");
        result.image=sequences.materialize(Rom::fromBytes(result.image),plan,compiled);
        const auto root=compiler.compileData(plan.directory.decoded(),directory,"private_animation_directory");
        result.image=plan.directory.rebuild(Rom::fromBytes(result.image),root);
        exportAnimationRoutes=sequences.exportRoutes(*p.animationRoutes(),plan);
        const auto actual=AnimationSequences::load(Rom::fromBytes(result.image));
        if(actual.groups().size()!=sequences.groups().size())throw Error("Private sequence count differs after export");
        for(auto it=sequences.groups().cbegin();it!=sequences.groups().cend();++it){
            const auto *reopened=actual.find(it.key()/4,it.key()%4);
            if(!reopened||reopened->controls()!=it->descriptor.controls()||reopened->members().size()!=it->descriptor.members().size())
                throw Error("Private animation controls differ after export");
            for(int art=0;art<reopened->members().size()-4;++art)
                if(it->descriptor.members()[art+4]&&reopened->sprite(art)!=it->descriptor.sprite(art))throw Error("Private animation pixels differ after export");
        }
        result.report<<QString("Compiled %1 private class/body animation descriptors and isolated their controls and sprites").arg(sequences.groups().size());
    }
    if (exportAnimationRoutes && p.animationSequences())
        exportAnimationRoutes->setPrivatePoses(p.privatePoseMappings());
    if (exportAnimationRoutes && exportAnimationRoutes->modified()) {
        const auto compiled = compiler.compileData(exportAnimationRoutes->decodedResource(),
                                                   directory, "class_animation_routes");
        const auto copyLoads = compiler.assembleOwner(
            readFile(":/game/art/animation-copy.s"), directory, "class_animation_copy", 0x80000000);
        result.image =
            exportAnimationRoutes->rebuild(Rom::fromBytes(result.image), compiled, copyLoads);
        result.report << QString("Compiled class-animation routes for %1 table rows and preserved "
                                 "the other compound-resource sections")
                             .arg(exportAnimationRoutes->rowCount());
    }
    if (exportAnimationRoutes) {
        const auto beforeSelection = result.image;
        result.image = rebuildBodySelection(Rom::fromBytes(result.image), p.rom(),
                                            exportAnimationRoutes->privatePoses(), compiler, directory);
        if (result.image != beforeSelection)
            result.report << "Compiled linked-body and leader selection from C within their original code allocations";
        const auto beforePose = result.image;
        result.image = rebuildPoseRuntime(Rom::fromBytes(result.image), p.rom(),
            exportAnimationRoutes->privatePoses(), p.animationSequences() && p.animationSequences()->requiresWideStates(),
            exportAnimationRoutes->hasBodyPolicies(), compiler, directory);
        if (result.image != beforePose)
            result.report << "Compiled private-pose execution and class record lookup within their original allocations";
        const auto beforeVisual = result.image;
        result.image = rebuildBodyVisualPolicy(Rom::fromBytes(result.image), p.rom(),
            *exportAnimationRoutes, compiler, directory);
        if (result.image != beforeVisual)
            result.report << "Compiled per-class equipment artwork policies within the original readers";
    }
    if (p.combatArtwork() && p.combatArtwork()->modified()) {
        auto resources = p.combatArtwork()->decodedResources();
        int ordinal = 0;
        for (auto it = resources.begin(); it != resources.end(); ++it)
            it.value() = compiler.compileData(it.value(), directory, QString("combat_art_%1").arg(ordinal++));
        result.image = p.combatArtwork()->rebuild(Rom::fromBytes(result.image), resources);
        result.report << QString("Compiled %1 combat artwork resources and preserved descriptor-local sharing").arg(resources.size());
    }
    rebuildChaosCounter(p, compiler, directory, result);
    rebuildDisplayTools(p, compiler, directory, result);
    rebuildHighAttack(p, compiler, directory, result);
    if (result.image != p.rom().bytes())
        Rom::updateChecksum(result.image);
    auto reopened = Project::fromRom(Rom::fromBytes(result.image), p.catalog());
    if (p.world()) {
        reopened.loadWorld();
        if (reopened.world()->json() != p.world()->json())
            throw Error("Exported site and treasure data differ from the project");
    }
    if (p.scenarios()) {
        reopened.loadScenarios();
        if (reopened.scenarios()->json() != p.scenarios()->json())
            throw Error("Exported scenarios differ from the project");
    }
    if (p.encounters()) {
        reopened.loadEncounters();
        if (reopened.encounters()->json() != p.encounters()->json())
            throw Error("Exported encounters differ from the project");
    }
    if (p.consumableEffects()) {
        reopened.loadConsumableEffects();
        if (reopened.consumableEffects()->json() != p.consumableEffects()->json())
            throw Error("Exported consumable effects differ from the project");
    }
    if (p.shops()) {
        reopened.loadShops();
        if (reopened.shops()->json() != p.shops()->json())
            throw Error("Exported shop stock differs from the project");
    }
    if (p.promotion()) {
        reopened.loadPromotion();
        if (reopened.promotion()->json() != p.promotion()->json())
            throw Error("Exported promotion data differs from the project");
    }
    if (!p.textBanks().isEmpty()) {
        reopened.loadTexts();
        for (auto bank = p.textBanks().begin(); bank != p.textBanks().end(); ++bank)
            if (reopened.textBanks().value(bank.key()).serialized() != bank->serialized())
                throw Error("Exported descriptions differ from project");
    }
    for (auto table : {Table::Classes, Table::Items, Table::Consumables})
        for (int row = 0; row < p.records(table).size(); ++row)
            if (((table == Table::Classes && relocateClasses) ||
                 (table == Table::Items && relocateItems))
                    ? (reopened.record(table, row).bytes.sliced(4) !=
                           p.record(table, row).bytes.sliced(4) ||
                       (table == Table::Classes
                            ? reopened.classNameBytes(row) != p.classNameBytes(row)
                            : reopened.itemNameBytes(row) != p.itemNameBytes(row)))
                    : reopened.record(table, row).bytes != p.record(table, row).bytes)
                throw Error("Export reopen differs from the project");
    if (avatarEdits) {
        const auto actual = AvatarLibrary::load(reopened.rom());
        const auto actualClasses = reopened.classData(), intended = p.classData();
        for (int id = 0; id < intended.entries().size(); ++id) {
            if (actualClasses.entries()[id].appearance.mid(4) !=
                intended.entries()[id].appearance.mid(4))
                throw Error("Portrait export changed the Army appearance bytes");
            for (int selector = 0; selector < 2; ++selector)
                if (actual.words(u16(actualClasses.entries()[id].appearance, selector * 2)) !=
                    p.avatarArtwork()->words(intended, id, selector))
                    throw Error("Reopened portrait selector differs from the project");
        }
    }
    if (armyRoutes) {
        const auto actual = reopened.classData(), intended = p.classData();
        for (int id = 0; id < intended.entries().size(); ++id)
            if (actual.entries()[id].appearance.mid(4) != intended.entries()[id].appearance.mid(4))
                throw Error("Reopened Army class selectors differ from the project");
        result.report << "Rebuilt Army class selectors with the source-built appearance table";
    }
    result.report << "Reopened every class, equipment and consumable record; CIC-6102 checksum "
                     "updated when required";
    return result;
}
void exportRom(const Project &p, const BuildResult &result, const QString &path) {
    auto source = QFileInfo(p.rom().sourcePath()).canonicalFilePath();
    auto output = QFileInfo(path).canonicalFilePath();
    if (!source.isEmpty() && (output == source || QFileInfo(path).absoluteFilePath().compare(
                                                      source, Qt::CaseInsensitive) == 0))
        throw Error("Choose an output path different from the source ROM");
    atomicWrite(path, p.rom().serialize(result.image));
}
} // namespace lordly
