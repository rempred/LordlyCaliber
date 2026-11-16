#pragma once
#include <QByteArray>
#include <QMap>
#include <QStringList>

namespace lordly {
// Disposable compiler artifacts, never project state or a partially patched ROM.
class CompilationCache {
  public:
    void enable(const QByteArray &authenticatedToolchain);
    QByteArray key(const QByteArray &input, const QByteArray &recipe) const;
    QMap<QString, QByteArray> load(const QByteArray &key, const QStringList &files) const;
    void store(const QByteArray &key, const QMap<QString, QByteArray> &files) const;
    int hits() const { return hits_; }
    int misses() const { return misses_; }
  private:
    void trim() const;
    QString directory_;
    QByteArray toolchain_;
    mutable int hits_ = 0, misses_ = 0;
    mutable qint64 written_ = 0;
};
} // namespace lordly
