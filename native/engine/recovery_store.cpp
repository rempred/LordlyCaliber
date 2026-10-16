#include "engine/recovery_store.h"
#include "engine/project.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
namespace lordly {
namespace {
constexpr auto magic="LCRECOVERY1\n";
QJsonObject header(QFile &file) {
    if(file.readLine(32)!=magic) throw Error("Not a recovery snapshot");
    const auto line=file.readLine(65536);
    if(!line.endsWith('\n')) throw Error("Damaged recovery header");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(line,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())throw Error("Damaged recovery header");
    if(!QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(doc.object()["session"].toString()).hasMatch())
        throw Error("Invalid recovery session");
    return doc.object();
}
void lockStore(QLockFile &lock) {
    if(!lock.tryLock(0))throw Error("Recovery storage is busy; the next autosave will retry");
}
}
RecoveryStore::RecoveryStore(QString root):RecoveryStore(std::move(root),Limits{}){}
RecoveryStore::RecoveryStore(QString root,Limits limits):root_(std::move(root)),limits_(limits) {
    if(root_.isEmpty())root_=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Recovery";
    if(!QDir().mkpath(root_))throw Error("Cannot create recovery folder: "+root_);
}
QVector<RecoveryEntry> RecoveryStore::entries() const {
    QVector<RecoveryEntry> result;
    for(const auto &info:QDir(root_).entryInfoList({"*.lcrecovery"},QDir::Files,QDir::Name|QDir::Reversed)) {
        try {
            QFile file(info.absoluteFilePath());if(!file.open(QIODevice::ReadOnly))continue;
            const auto h=header(file);QStringList pending,drafts;
            for(const auto &text:h["pending"].toArray())pending<<text.toString();
            for(const auto &text:h["drafts"].toArray())drafts<<text.toString();
            result.append({info.absoluteFilePath(),h["session"].toString(),h["sourcePath"].toString(),h["label"].toString(),
                           QDateTime::fromString(h["created"].toString(),Qt::ISODateWithMs),pending,drafts});
        } catch(const Error &) { /* A damaged entry stays available for diagnosis. */ }
    }
    return result;
}
QString RecoveryStore::save(const QString &session,const QJsonObject &manifest,const QString &label,const QStringList &pending) const {
    if(!QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(session).hasMatch())throw Error("Invalid recovery session");
    auto payload=QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    if(payload.size()>512ll*1024*1024)throw Error("Project is too large for automatic recovery; save it manually");
    payload="LC05PACK\n"+qCompress(payload,6);
    QJsonObject h{{"session",session},{"created",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"sourcePath",manifest["sourcePath"]},{"label",label},{"pending",QJsonArray::fromStringList(pending)},
        {"sha256",QString::fromLatin1(sha256(payload))}};
    QJsonArray drafts;for(const auto &draft:manifest["editorRecoveryDrafts"].toArray())drafts.append(draft.toObject()["label"]);
    if(!drafts.isEmpty())h["drafts"]=drafts;
    const auto metadata=QJsonDocument(h).toJson(QJsonDocument::Compact);
    if(metadata.size()>=65535)throw Error("Recovery details exceed their size limit; save the project manually");
    const auto bytes=QByteArray(magic)+metadata+'\n'+payload;
    // Half the budget leaves room for the last good snapshot while staging its successor.
    if(bytes.size()>limits_.bytes/2)throw Error("Recovery snapshot exceeds its size limit; save the project manually. The previous snapshot is retained.");
    QLockFile lock(root_+"/.lock");lockStore(lock);
    // QSaveFile staging files can survive a process crash. The store lock
    // excludes active writers, so these abandoned files can be removed safely.
    for(const auto &file:QDir(root_).entryInfoList({"*.lcrecovery.*"},QDir::Files|QDir::Hidden))
        if(!QFile::remove(file.absoluteFilePath()))throw Error("Cannot clean an interrupted recovery write");
    auto files=QDir(root_).entryInfoList({"*.lcrecovery"},QDir::Files,QDir::Name);
    QString keep;
    for(const auto &entry:entries())if(entry.session==session && keep.isEmpty())keep=entry.path;
    qint64 total=0;for(const auto &file:files)total+=file.size();
    int count=files.size();
    const auto expired=QDateTime::currentDateTimeUtc().addDays(-limits_.days);
    for(const auto &file:files) {
        if(file.absoluteFilePath()==keep)continue;
        if(total+bytes.size()<=limits_.bytes && count<limits_.entries && file.lastModified()>expired)continue;
        if(!QFile::remove(file.absoluteFilePath()))throw Error("Cannot clean old recovery snapshots");
        total-=file.size();--count;
    }
    if(total+bytes.size()>limits_.bytes)throw Error("Recovery storage is full; previous recovery is retained. Save manually.");
    const auto path=QDir(root_).filePath(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".lcrecovery");
    atomicWrite(path,bytes);
    int retained=0;
    for(const auto &entry:entries())if(entry.session==session && ++retained>limits_.perSession && !QFile::remove(entry.path))
        throw Error("Recovery saved, but an old snapshot could not be removed; check the recovery folder");
    for(const auto &file:QDir(root_).entryInfoList({".session-*.lock"},QDir::Files|QDir::Hidden)) {
        QLockFile sessionLock(file.absoluteFilePath());
        if(sessionLock.tryLock(0))sessionLock.unlock(); // Removes abandoned locks, leaves active sessions alone.
    }
    return path;
}
QJsonObject RecoveryStore::load(const QString &path) const {
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>limits_.bytes)throw Error("Cannot read recovery snapshot");
    const auto h=header(file);const auto payload=file.readAll();
    if(sha256(payload)!=h["sha256"].toString().toLatin1())throw Error("Recovery snapshot is damaged; try the previous snapshot");
    return decodeProject(payload);
}
void RecoveryStore::removeSession(const QString &session) const {
    QLockFile lock(root_+"/.lock");lockStore(lock);
    for(const auto &entry:entries())if(entry.session==session && !QFile::remove(entry.path))throw Error("Cannot remove recovery snapshot");
}
void RecoveryStore::remove(const QString &path) const {
    if(QFileInfo(path).absolutePath()!=QFileInfo(root_).absoluteFilePath())throw Error("Snapshot is outside the recovery folder");
    QLockFile lock(root_+"/.lock");lockStore(lock);
    if(!QFile::remove(path))throw Error("Cannot remove recovery snapshot");
}
}
