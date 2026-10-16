#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
namespace lordly {
struct RecoveryEntry {
    QString path, session, sourcePath, label;
    QDateTime created;
    QStringList pending, drafts;
};
class RecoveryStore {
  public:
    struct Limits { qint64 bytes=128ll*1024*1024; int entries=10, perSession=2, days=14; };
    explicit RecoveryStore(QString root = {});
    RecoveryStore(QString root, Limits limits);
    QString root() const { return root_; }
    QVector<RecoveryEntry> entries() const;
    QString save(const QString &session, const QJsonObject &manifest, const QString &label,
                 const QStringList &pending = {}) const;
    QJsonObject load(const QString &path) const;
    void removeSession(const QString &session) const;
    void remove(const QString &path) const;
  private:
    QString root_;
    Limits limits_;
};
}
