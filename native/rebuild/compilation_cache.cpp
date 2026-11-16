#include "rebuild/compilation_cache.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace lordly {
namespace {
constexpr qint64 entryLimit = 64 * 1024 * 1024;
constexpr qint64 cacheLimit = 256 * 1024 * 1024;
const QByteArray magic = "LCCACHE1";
QByteArray digest(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}
}
void CompilationCache::enable(const QByteArray &authenticatedToolchain) {
    directory_.clear();
    toolchain_.clear();
    if (qEnvironmentVariableIntValue("LORDLY_DISABLE_COMPILER_CACHE") != 0) return;
    auto root = qEnvironmentVariable("LORDLY_COMPILER_CACHE_DIR");
    if (root.isEmpty()) {
        const auto base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (base.isEmpty()) return;
        root = QDir(base).filePath("LordlyCaliber/rom-compiler-v1");
    }
    if (!QDir().mkpath(root)) return;
    directory_ = QDir(root).absolutePath();
    toolchain_ = authenticatedToolchain;
    trim();
}
QByteArray CompilationCache::key(const QByteArray &input, const QByteArray &recipe) const {
    if (directory_.isEmpty()) return {};
    return digest(magic + digest(toolchain_) + digest(input) + digest(recipe));
}
QMap<QString, QByteArray> CompilationCache::load(const QByteArray &key,
                                               const QStringList &files) const {
    if (key.isEmpty()) return {};
    ++misses_;
    QFile file(QDir(directory_).filePath(QString::fromLatin1(key) + ".cache"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > entryLimit || file.size() < 136) return {};
    const auto bytes = file.readAll();
    if (bytes.size() != file.size() || bytes.size() < 136) return {};
    const auto body = bytes.sliced(136);
    if (bytes.first(8) != magic || bytes.mid(8, 64) != key ||
        bytes.mid(72, 64) != digest(body)) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto object = document.object();
    if (object.size() != files.size()) return {};
    QMap<QString, QByteArray> result;
    for (const auto &name : files) {
        if (!object.value(name).isString()) return {};
        const auto decoded = QByteArray::fromBase64Encoding(object[name].toString().toLatin1(),
                                                           QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return {};
        result[name] = decoded.decoded;
    }
    // LRU maintenance is best effort. Read-only caches can still supply results.
    file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    --misses_;
    ++hits_;
    return result;
}
void CompilationCache::store(const QByteArray &key, const QMap<QString, QByteArray> &files) const {
    if (key.isEmpty()) return;
    QJsonObject object;
    for (auto it = files.begin(); it != files.end(); ++it)
        object[it.key()] = QString::fromLatin1(it.value().toBase64());
    const auto body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const auto bytes = magic + key + digest(body) + body;
    if (bytes.size() > entryLimit) return;
    QSaveFile file(QDir(directory_).filePath(QString::fromLatin1(key) + ".cache"));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return;
    written_ += bytes.size();
    if (written_ >= 16 * 1024 * 1024) { trim(); written_ = 0; }
}
void CompilationCache::trim() const {
    const auto entries = QDir(directory_).entryInfoList({"*.cache"}, QDir::Files, QDir::Time);
    qint64 total = 0;
    for (const auto &entry : entries) {
        total += entry.size();
        if (total > cacheLimit) QFile::remove(entry.absoluteFilePath());
    }
}
} // namespace lordly
