#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>

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
                             const QString &name, quint32 address, const QString &section) const {
    if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]*$").match(name).hasMatch())
        throw Error("Invalid source unit name");
    if (section != ".text" && section != ".data")
        throw Error("Unsupported compiler output section");
    if (!QDir().mkpath(directory))
        throw Error("Cannot create build directory");
    QDir dir(directory);
    auto input = dir.absoluteFilePath(name + ".c");
    auto assembly = dir.absoluteFilePath(name + ".compiler.s");
    auto object = dir.absoluteFilePath(name + ".o");
    auto script = dir.absoluteFilePath(name + ".ld");
    auto elf = dir.absoluteFilePath(name + ".elf");
    auto binary = dir.absoluteFilePath(name + ".bin");
    atomicWrite(input, source);
    QStringList flags = {
        "-quiet", "-O2", "-meb",     "-mips3",        "-mgp32",       "-mfp32",
        "-G",     "0",   "-fno-PIC", "-mno-abicalls", "-fno-builtin", "-funsigned-char",
        input,    "-o",  assembly};
    run("cc1.exe", flags, directory);
    run("mips-kmc-elf-as.exe",
        {"-G", "0", "-mips3", "-mabi=32", "-force-n64align", "-EB", "-o", object, assembly},
        directory);
    atomicWrite(
        script,
        QString("SECTIONS { %1 0x%2 : { *(%1) } }\n").arg(section).arg(address, 0, 16).toLatin1());
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
BuildResult rebuild(const Project &p, const QString &toolsRoot, const QString &directory) {
    Compiler compiler(toolsRoot);
    compiler.authenticate();
    BuildResult result{p.rom().bytes(), {}};
    // Retained sections must become actual native owners before they can enter an export.
    const auto pending = p.pendingLegacySections();
    if (!pending.isEmpty())
        throw Error("Imported sections still need native export support: " + pending.join(", "));
    for (auto table : {Table::Classes, Table::Items}) {
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
    if (p.rom().revision() == 0 &&
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
    for (auto table : {Table::Classes, Table::Items})
        for (int row = 0; row < p.records(table).size(); ++row)
            if (reopened.record(table, row).bytes != p.record(table, row).bytes)
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
