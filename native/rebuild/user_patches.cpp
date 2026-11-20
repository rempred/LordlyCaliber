#include "rebuild/user_patches.h"
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <limits>

namespace lordly {
namespace {
qint64 number(const QJsonObject &o, const QString &key, qint64 high) {
    const auto v = o.value(key);
    const auto n = v.toInteger(-1);
    if (!v.isDouble() || n < 0 || n > high || v.toDouble() != double(n))
        throw Error("Invalid code patch " + key);
    return n;
}
const QRegularExpression symbol(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
void checkC(const QString &source, const QJsonObject &headers) {
    // Bind every include to a file carried in the project. Do not allow computed
    // includes, alternative directive spellings or directives that open host paths.
    auto logical = source;
    logical.replace("\r\n", "\n").replace('\r', '\n').replace("\\\n", "");
    if (logical.contains("??") || logical.contains("%:") ||
        logical.contains(QRegularExpression("[\\x00-\\x08\\x0b-\\x0c\\x0e-\\x1f]")))
        throw Error("Unsupported preprocessor spelling in code patch");
    // Remove comments while respecting C string and character literal boundaries.
    // Preserve newlines so each real directive is validated independently.
    QString clean;
    QChar quote;
    bool block = false;
    for (qsizetype i = 0; i < logical.size(); ++i) {
        const QChar c = logical[i], next = i + 1 < logical.size() ? logical[i + 1] : QChar{};
        if (block) {
            if (c == '*' && next == '/') {
                block = false;
                ++i;
            } else if (c == '\n')
                clean += c;
        } else if (!quote.isNull()) {
            clean += c;
            if (c == '\\' && !next.isNull())
                clean += logical[++i];
            else if (c == quote)
                quote = {};
            else if (c == '\n')
                throw Error("Unterminated C literal in code patch");
        } else if (c == '/' && next == '*') {
            block = true;
            ++i;
            clean += ' ';
        } else if (c == '/' && next == '/') {
            while (i + 1 < logical.size() && logical[i + 1] != '\n')
                ++i;
        } else {
            clean += c;
            if (c == '"' || c == '\'')
                quote = c;
        }
    }
    if (block || !quote.isNull())
        throw Error("Unterminated C comment or literal in code patch");
    const QRegularExpression include(
        "^\\s*#\\s*include\\s*([<\"])([A-Za-z_][A-Za-z0-9_/.-]*)[>\"]\\s*$");
    const QRegularExpression directive(
        "^\\s*#\\s*(define|undef|if|ifdef|ifndef|else|elif|endif|error)\\b");
    for (auto line : clean.split('\n')) {
        line = line.trimmed();
        if (!line.startsWith('#'))
            continue;
        if (directive.match(line).hasMatch())
            continue;
        const auto match = include.match(line);
        if (!match.hasMatch() || !headers.contains(match.captured(2)))
            throw Error("Code patch requires a project header or an unsupported directive: " +
                        line);
    }
}
void checkAssembly(const QString &source) {
    // This input is an in-place instruction body. A new section would be discarded
    // by assembleOwner; includes and macros could escape the project workspace.
    const QRegularExpression label("^(?:[A-Za-z_.$][\\w.$]*|\\d+)\\s*:\\s*");
    const QRegularExpression operation("^[a-zA-Z][a-zA-Z0-9.]*\\b");
    const QRegularExpression data(
        "^\\.(?:word|half|byte|float|double|balign|align|globl|global)\\s+");
    auto logical = source;
    logical.replace("\r\n", "\n").replace('\r', '\n');
    if (logical.contains(QRegularExpression("[\\x00-\\x08\\x0b-\\x0c\\x0e-\\x1f]")))
        throw Error("Unsupported control character in assembly");
    const QRegularExpression mode("^\\.set\\s+(?:noat|at|noreorder|reorder|nomacro|macro)$");
    for (auto line : logical.split('\n')) {
        line = line.section('#', 0, 0);
        for (auto statement : line.split(';')) {
            statement = statement.trimmed();
            while (label.match(statement).hasMatch())
                statement = statement.mid(label.match(statement).capturedLength()).trimmed();
            if (statement.isEmpty())
                continue;
            if (mode.match(statement).hasMatch())
                continue;
            if (statement.contains('\\') || statement.contains('"') ||
                !(operation.match(statement).hasMatch() || data.match(statement).hasMatch()))
                throw Error("Unsupported instruction-body statement: " + statement);
        }
    }
}
} // namespace
void validateModuleSource(const QString &source, const QMap<QString, QByteArray> &headers) {
    QJsonObject names;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        names[it.key()] = true;
    checkC(source, names);
}
void validateUserPatches(const QJsonValue &value) {
    if (!value.isArray())
        throw Error("codePatches must be an array");
    QSet<QString> ids;
    for (const auto &v : value.toArray()) {
        const auto p = v.toObject();
        const auto id = p["id"].toString();
        const auto lang = p["language"].toString();
        if (!symbol.match(id).hasMatch() || id.size() > 96 || ids.contains(id))
            throw Error("Code patches need unique C identifier names");
        ids.insert(id);
        if ((lang != "c" && lang != "asm") || !p["source"].isString() ||
            p["source"].toString().trimmed().isEmpty())
            throw Error("Code patch " + id + " needs C or assembly source");
        if (p.contains("enabled") && !p["enabled"].isBool())
            throw Error("Invalid code patch enabled flag");
        if (p.contains("module")) {
            if (lang != "c" || !symbol.match(p["module"].toString()).hasMatch() ||
                !QRegularExpression("^[A-Fa-f0-9]{64}$")
                     .match(p["expectedSourceSha256"].toString())
                     .hasMatch())
                throw Error(
                    "A build module needs C source, its build name and the original source digest");
            continue;
        }
        const auto start = number(p, "offset", 0x7fffffff);
        const auto address = number(p, "address", 0xffffffff);
        const auto size = number(p, "size", 0x800000);
        if (start < 0x40 || (start | address | size) % 4 || size < 4 || address < 0x80000000 ||
            address + size > 0x80800000)
            throw Error("Code patch requires an aligned ROM allocation and game RAM address");
        if (!QRegularExpression("^[a-fA-F0-9]{64}$")
                 .match(p["expectedSha256"].toString())
                 .hasMatch())
            throw Error("Code patch needs the expected digest of its original allocation");
        if (p.contains("bindings") && !p["bindings"].isObject())
            throw Error("Invalid code patch bindings");
        const auto bindings = p["bindings"].toObject();
        for (const auto &key : bindings.keys()) {
            if (!symbol.match(key).hasMatch())
                throw Error("Invalid code patch symbol " + key);
            number(bindings, key, 0xffffffff);
        }
        if (p.contains("headers") && !p["headers"].isObject())
            throw Error("Invalid code patch headers");
        const auto headers = p["headers"].toObject();
        for (const auto &key : headers.keys()) {
            if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_/-]*\\.h$").match(key).hasMatch() ||
                !headers[key].isString())
                throw Error("Invalid code patch header " + key);
            checkC(headers[key].toString(), headers);
        }
        if (lang == "c") {
            if (!symbol.match(p["entry"].toString()).hasMatch())
                throw Error("C patch needs an entry function");
            checkC(p["source"].toString(), headers);
        } else
            checkAssembly(p["source"].toString());
    }
}
void addUserPatches(ExportPlan &plan, const QJsonArray &patches, const Compiler &compiler,
                    const QString &directory, QStringList &report) {
    validateUserPatches(patches);
    QVector<ExportRange> used;
    for (const auto &value : patches) {
        const auto p = value.toObject();
        if (!p["enabled"].toBool(true) || p.contains("module"))
            continue;
        const auto start = p["offset"].toInteger(), size = p["size"].toInteger();
        const auto address = quint32(p["address"].toInteger());
        const ExportRange range{start, start + size};
        const auto id = p["id"].toString();
        for (const auto &other : used)
            if (range.begin < other.end && other.begin < range.end)
                throw Error("Code patch " + id + " overlaps another enabled code patch");
        used.append(range);
        QStringList dependencies{"scene-logic"};
        const auto after = p["after"].toString();
        if (!after.isEmpty() && !dependencies.contains(after))
            dependencies.append(after);
        const auto step = "user-" + id;
        plan.add(step, dependencies,
                 [=, &compiler, &report](QByteArray &image) {
                     if (start + size > image.size() ||
                         sha256(image.mid(start, size)) !=
                             p["expectedSha256"].toString().toLatin1().toLower())
                         throw Error("Code patch " + id +
                                     " conflicts with the current ROM bytes at z64 0x" +
                                     QString::number(start, 16));
                     QMap<QString, quint32> symbols;
                     const auto bindings = p["bindings"].toObject();
                     for (const auto &key : bindings.keys())
                         symbols[key] = quint32(bindings[key].toInteger());
                     const auto source = p["source"].toString().toUtf8();
                     const auto dir = QDir(directory).filePath("user-patches/" + id);
                     QByteArray bytes;
                     if (p["language"] == "c") {
                         QMap<QString, QByteArray> includes;
                         const auto headers = p["headers"].toObject();
                         for (const auto &key : headers.keys())
                             includes[key] = headers[key].toString().toUtf8();
                         const auto module =
                             compiler.compileModule(source, dir, "lc_user_" + id, address, includes,
                                                    symbols, p["inlineConstants"].toBool(false));
                         if (module.entries.value(p["entry"].toString()) != address)
                             throw Error(
                                 "Code patch entry must start at the declared RAM address: " + id);
                         bytes = module.bytes.size() > size ? module.fitOwner(size) : module.bytes;
                     } else {
                         bytes = compiler.assembleOwner(source, dir, "lc_user_" + id, address,
                                                        symbols, true);
                     }
                     if (bytes.isEmpty() || bytes.size() > size || (bytes.size() % 4))
                         throw Error("Code patch " + id + " does not fit its ROM allocation (" +
                                     QString::number(bytes.size()) + " compiled bytes)");
                     image.replace(start, bytes.size(), bytes);
                     report << QString("Compiled %1 patch %2 at z64 0x%3, RAM 0x%4 (%5 bytes)")
                                   .arg(p["language"].toString(), id)
                                   .arg(start, 0, 16)
                                   .arg(address, 0, 16)
                                   .arg(bytes.size());
                 },
                 {range});
        if (!after.isEmpty())
            plan.compose(step, after, ExportPlan::Ranges{range});
    }
}
} // namespace lordly
