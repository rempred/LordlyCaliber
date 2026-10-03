#include "rebuild/compiler.h"
#include "rebuild/class_runtime.h"
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
                                   const QMap<QString, quint32> &symbols) const {
    QDir dir(directory);
    if (!dir.mkpath("."))
        throw Error("Cannot create assembly build directory");
    auto input = dir.absoluteFilePath(name + ".s"), object = dir.absoluteFilePath(name + ".o");
    auto script = dir.absoluteFilePath(name + ".ld"), elf = dir.absoluteFilePath(name + ".elf");
    auto binary = dir.absoluteFilePath(name + ".bin");
    atomicWrite(input, ".set noat\n.set noreorder\n.section .owner,\"ax\"\n.balign 4\n" + source);
    run("mips-kmc-elf-as.exe", {"-G", "0", "-mips3", "-mabi=32", "-EB", "-o", object, input},
        directory);
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
                                                    {0xcce00, "move $v0, $v1"},
                                                    {0xd0328, "addiu $v1, $v1, -1"},
                                                    {0xd0334, "srl $v0, $v0, 16"}};
        for (auto v : owners) {
            auto owner = v.toObject();
            auto start = owner["start"].toInt(), size = owner["size"].toInt();
            auto original = bytes.mid(start, size);
            if (classPlacement)
                original = classPlacement->canonicalReferences(original, start);
            auto hash = QString::fromLatin1(sha256(original));
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
                    {{"CLASS_TEXT_STYLE", 0x8019b5c4u}, {"DRAW_CLASS_NAME", 0x801ba900u}});
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
                built = module.fitOwner(size);
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
            if (built.size() != size || sha256(built) != expected)
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
    if (promotion.masks != promotion.originalMasks) {
        auto data = p.rom().bytes().mid(0xdbd2c, 162);
        for (int i = 0; i < 81; ++i)
            data[i * 2 + 1] = promotion.masks[i];
        auto compiled = compiler.compileData(data, directory, "promotion_compatibility");
        result.image.replace(0xdbd2c, compiled.size(), compiled);
        if (!family) {
            // A standalone mask edit still rebuilds its accepted C reader.
            const auto module = compiler.compileModule(
                readFile(":/game/promotion/compatibility-mask.c"), directory, "class_compatibility",
                0x801cf320u,
                {{"class_menu_config.h", readFile(":/game/promotion/class_menu_config.h")}});
            const auto reader = module.fitOwner(44);
            if (reader != p.rom().bytes().mid(0xbbd80, 44))
                throw Error("Source-built compatibility reader differs from the source ROM");
            result.image.replace(0xbbd80, 44, reader);
            result.report << "Compiled compatibility masks and their accepted C lookup";
        }
    }
}
BuildResult rebuild(const Project &p, const QString &toolsRoot, const QString &directory) {
    Compiler compiler(toolsRoot);
    compiler.authenticate();
    BuildResult result{p.rom().bytes(), {}};
    const auto priorClassPlacement = ClassPlacement::read(p.rom());
    const bool relocateClasses = priorClassPlacement.has_value() || p.classNamesModified();
    // Retained sections must become actual native owners before they can enter an export.
    const auto pending = p.pendingLegacySections();
    if (!pending.isEmpty())
        throw Error("Imported sections still need native export support: " + pending.join(", "));
    for (auto table : {Table::Classes, Table::Items}) {
        if (table == Table::Classes && relocateClasses)
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
        auto container = bank.resource.container(compiled);
        result.image.replace(bank.resource.romOffset, container.size(), container);
        result.report << "Rebuilt " + bank.spec.label + " from edited C data";
    }
    rebuildPromotion(p, compiler, directory, result);
    if (relocateClasses) {
        auto offset = priorClassPlacement ? priorClassPlacement->dataRomOffset
                                          : quint32((result.image.size() + 15) & ~qsizetype(15));
        const auto data = p.classData();
        if (priorClassPlacement &&
            data.build(0x80243db0).bytes.size() >
                priorClassPlacement->layout.end - priorClassPlacement->layout.indexBase &&
            quint64(priorClassPlacement->dataRomOffset) + priorClassPlacement->layout.end -
                    priorClassPlacement->layout.indexBase !=
                quint64(result.image.size()))
            offset = quint32((result.image.size() + 15) & ~qsizetype(15));
        const auto classes = buildClassRuntime(data, compiler, directory, offset);
        result.image = linkClassRuntime(Rom::fromBytes(result.image), classes);
        result.report << QString("Rebuilt %1 classes with relocated names, appearance data and "
                                 "source-built initialization (%2 bytes of RAM)")
                             .arg(classes.data.layout.count)
                             .arg(classes.data.bytes.size());
    }
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
    if (result.image != p.rom().bytes())
        Rom::updateChecksum(result.image);
    auto reopened = Project::fromRom(Rom::fromBytes(result.image), p.catalog());
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
    for (auto table : {Table::Classes, Table::Items})
        for (int row = 0; row < p.records(table).size(); ++row)
            if (table == Table::Classes && relocateClasses
                    ? (reopened.record(table, row).bytes.sliced(4) !=
                           p.record(table, row).bytes.sliced(4) ||
                       reopened.classNameBytes(row) != p.classNameBytes(row))
                    : reopened.record(table, row).bytes != p.record(table, row).bytes)
                throw Error("Export reopen differs from the project");
    result.report
        << "Reopened every class and item record; CIC-6102 checksum updated when required";
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
