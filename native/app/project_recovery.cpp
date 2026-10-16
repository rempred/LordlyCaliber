#include "app/main_window.h"
#include "app/diagnostics.h"
#include "app/pending_edits.h"
#include "app/recovery_draft.h"
#include "engine/recovery_store.h"
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QLockFile>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QSettings>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace lordly {
namespace {
QVector<RecoveryEntry> recoverable(const RecoveryStore &store) {
    QVector<RecoveryEntry> result;
    for(const auto &entry:store.entries()) {
        QLockFile lock(store.root()+"/.session-"+entry.session+".lock");
        if(lock.tryLock(0))result<<entry;
    }
    return result;
}
}
void MainWindow::initializeRecovery() {
    auto file=menuBar()->actions().front()->menu();
    file->addSeparator();
    auto recover=file->addAction("Recover unsaved work…",this,&MainWindow::showRecovery);
    recover->setObjectName("recoverProject");
    auto help=menuBar()->addMenu("&Help");
    auto logs=help->addAction("Open crash logs",this,[]{
        const auto folder=Diagnostics::folder();QDir().mkpath(folder);QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });logs->setObjectName("openCrashLogs");
    help->addAction("About recovery and crash logs…",this,[this]{
        QMessageBox::information(this,"Recovery and crash logs",
            "Project edits and unfinished animation/sprite dialog edits are backed up about every minute while unsaved changes exist. "
            "Recovery never changes your saved project or copies your ROM.\n\n"
            "Two snapshots per editing session are kept, up to 10 total and 128 MB including the next snapshot being written. "
            "Old snapshots are removed after 14 days when another backup is written. A single snapshot can use up to 64 MB. "
            "If a backup cannot be saved, the status bar reports it. Saving or deliberately discarding a project removes its temporary snapshots.\n\n"
            "Supported animation and sprite dialogs are included before Apply is clicked. Recovery identifies these unfinished edits and adds them to Undo history. "
            "Other unapplied forms, invalid text and Save Editor game-save changes are excluded; save game saves separately.\n\n"
            "After an interrupted session, use File → Recover unsaved work. The original source ROM is required.\n\n"
            "Crash reports and session logs are in:\n"+Diagnostics::folder()+
            "\n\nUse Help → Open crash logs and share the matching .crash.txt and .session.log files when reporting a problem. "
            "Logs may include local file paths. Nothing is uploaded automatically. Logs rotate and are limited to about 16 MB. "
            "Power loss or forcibly ending the process may leave recovery snapshots without a crash report.");
    });
    recoveryStatus_=new QLabel("Recovery ready");recoveryStatus_->setObjectName("recoveryStatus");
    recoveryStatus_->setToolTip("Automatic recovery includes unfinished animation and sprite dialogs. File → Recover unsaved work.");
    statusBar()->addPermanentWidget(recoveryStatus_);
    recoveryNotice_=addToolBar("Recovery available");recoveryNotice_->setObjectName("recoveryNotice");recoveryNotice_->setMovable(false);
    recoveryNotice_->addWidget(new QLabel("  A previous session has recovery snapshots or a crash report.  "));
    recoveryNotice_->addAction(recover);recoveryNotice_->addAction(logs);
    recoveryNotice_->addAction("Dismiss",this,[this]{recoveryNotice_->hide();
        const auto reports=Diagnostics::crashReports();if(!reports.isEmpty())QSettings().setValue("acknowledgedCrashReport",reports.front());});recoveryNotice_->hide();
    try {RecoveryStore store;const auto reports=Diagnostics::crashReports();
        const bool newCrash=!reports.isEmpty()&&reports.front()!=QSettings().value("acknowledgedCrashReport").toString();
        recoveryNotice_->setVisible(!recoverable(store).isEmpty()||newCrash);}
    catch(const std::exception &e){recoveryStatus_->setText("Recovery unavailable");recoveryStatus_->setToolTip(QString::fromUtf8(e.what()));qWarning()<<e.what();}
    auto timer=new QTimer(this);timer->setObjectName("recoveryTimer");timer->setInterval(60000);connect(timer,&QTimer::timeout,this,&MainWindow::autosaveRecovery);timer->start();
}
void MainWindow::beginRecoverySession() {
    recoverySession_=QUuid::createUuid().toString(QUuid::WithoutBraces);recoveryRevision_=projectRevision_;recoveryDrafts_={};projectLabel_.clear();
    try {RecoveryStore store;recoverySessionLock_=std::make_unique<QLockFile>(store.root()+"/.session-"+recoverySession_+".lock");
        if(!recoverySessionLock_->tryLock(0))throw Error("Cannot reserve recovery session");}
    catch(const std::exception &e){qWarning()<<"Recovery session:"<<e.what();}
}
void MainWindow::clearRecovery() {
    recoveryCancelled_->store(true);
    recoveryCancelled_=std::make_shared<std::atomic_bool>(false);
    recoveryDrafts_={};
    if(recoverySession_.isEmpty())return;
    try {RecoveryStore store;store.removeSession(recoverySession_);
        if(!recoveredSession_.isEmpty())store.removeSession(recoveredSession_);recoveredSession_.clear();}
    catch(const std::exception &e){qWarning()<<"Recovery cleanup:"<<e.what();}
    if(recoveryStatus_)recoveryStatus_->setText("Recovery ready");
}
void MainWindow::autosaveRecovery() {
    if(!project_||loading_)return;
    if(recoveryBusy_){recoveryAgain_=true;return;}
    QJsonArray drafts;
    try {drafts=captureRecoveryDrafts(workspaces_,*project_);}
    catch(const std::exception &e){recoveryStatus_->setText("Recovery failed — unfinished edit needs attention");recoveryStatus_->setToolTip(QString::fromUtf8(e.what()));qWarning()<<e.what();return;}
    if(undo_->isClean() && drafts.isEmpty()){clearRecovery();return;}
    if(projectRevision_==recoveryRevision_ && drafts==recoveryDrafts_)return;
    recoveryBusy_=true;
    const auto revision=projectRevision_;const auto session=recoverySession_;
    QStringList pending;for(const auto &entry:pendingEdits(workspaces_))pending<<entry.label;
    if(invalidEdits())pending<<"Invalid text input";
    if(auto modal=QApplication::activeModalWidget()) {
        bool covered=false;
        for(auto parent=modal;parent;parent=parent->parentWidget())
            if(!parent->findChildren<RecoveryDraft *>(QString(),Qt::FindDirectChildrenOnly).isEmpty())covered=true;
        if(!covered)pending<<"Unconfirmed edits in another open dialog";
    }
    recoveryStatus_->setText("Saving recovery…");
    struct Result {QString error;};auto watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,revision,session,pending,drafts]{
        const auto result=watcher->result();watcher->deleteLater();recoveryBusy_=false;
        if(recoveryAgain_){recoveryAgain_=false;QTimer::singleShot(0,this,&MainWindow::autosaveRecovery);}
        if(session!=recoverySession_) {
            try {RecoveryStore().removeSession(session);}catch(const std::exception &e){qWarning()<<e.what();}return;
        }
        if(!result.error.isEmpty()) {recoveryStatus_->setText("Recovery failed — save manually");recoveryStatus_->setToolTip(result.error);qWarning().noquote()<<result.error;}
        else {recoveryRevision_=revision;recoveryDrafts_=drafts;
            recoveryStatus_->setText("Recovery saved "+QTime::currentTime().toString("h:mm AP")+(!drafts.isEmpty()?" · unfinished art included":"")+(pending.isEmpty()?"":" · other drafts excluded"));
            recoveryStatus_->setToolTip(pending.isEmpty()?"Project edits and supported unfinished artwork are backed up. Save your project to keep it permanently.":"Not included: "+pending.join(", "));}
    });
    watcher->setFuture(QtConcurrent::run([snapshot=*project_,session,label=projectLabel_,pending,drafts,cancelled=recoveryCancelled_]{
        try {if(cancelled->load())return Result{};
            auto manifest=snapshot.manifest();
            if(!drafts.isEmpty())manifest["editorRecoveryDrafts"]=drafts;
            RecoveryStore store;store.save(session,manifest,label,pending);
            if(cancelled->load())store.removeSession(session);
            return Result{};}
        catch(const std::exception &e){return Result{QString::fromUtf8(e.what())};}
    }));
}
bool MainWindow::openRecoveryFile(const QString &path,const QString &sourceRom) {
    if(loading_||!confirmDiscard())return false;
    RecoveryStore store;auto manifest=store.load(path);QString oldSession;
    if(manifest.contains("editorRecoveryDrafts")&&!manifest["editorRecoveryDrafts"].isArray())throw Error("Invalid unfinished artwork snapshot");
    const auto drafts=manifest.take("editorRecoveryDrafts").toArray();
    for(const auto &entry:store.entries())if(entry.path==path)oldSession=entry.session;
    auto next=Project::fromRom(Rom::open(sourceRom),catalog_);
    next.loadTexts();
    if(next.rom().revision()==0){next.loadPromotion();next.loadShops();next.loadConsumableEffects();next.loadEncounters();next.loadScenarios();next.loadWorld();}
    next.applyManifest(manifest);validateRecoveryDrafts(next,drafts);installProject(std::move(next));
    recoveredSession_=oldSession;
    // Recovery is an unsaved baseline. Subsequent edits have normal Undo; the
    // baseline itself must not pretend to undo restored data.
    undo_->setClean();undo_->resetClean();
    restoreRecoveryDrafts(project_.get(),undo_,drafts);
    ++projectRevision_;
    recoveryStatus_->setText(drafts.isEmpty()?"Recovered — save your project":"Recovered unfinished edits — review with Undo, then save");updateStatus();
    recoveryNotice_->hide();return true;
}
void MainWindow::showRecovery() {
    try {
        RecoveryStore store;QDialog dialog(this);dialog.setWindowTitle("Recover unsaved work");dialog.resize(760,460);
        auto layout=new QVBoxLayout(&dialog);
        auto help=new QLabel("Recover using the original source ROM. Unfinished animation/sprite edits listed below are included as undoable changes. Your saved project is not overwritten; save the recovered project to keep it.");help->setWordWrap(true);layout->addWidget(help);
        auto list=new QListWidget;layout->addWidget(list);
        auto details=new QLabel;details->setTextFormat(Qt::PlainText);details->setWordWrap(true);layout->addWidget(details);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);
        auto recover=buttons->addButton("Recover selected…",QDialogButtonBox::ActionRole);
        auto remove=buttons->addButton("Delete selected",QDialogButtonBox::DestructiveRole);
        auto folder=buttons->addButton("Open recovery folder",QDialogButtonBox::ActionRole);
        auto locate=buttons->addButton("Choose source ROM…",QDialogButtonBox::ActionRole);
        QVector<RecoveryEntry> entries;QString selectedSource;
        auto refresh=[&]{entries=recoverable(store);list->clear();for(const auto &entry:entries)list->addItem(entry.created.toLocalTime().toString("MMM d, yyyy h:mm:ss AP")+" — "+(entry.label.isEmpty()?QFileInfo(entry.sourcePath).fileName():entry.label)+(entry.drafts.isEmpty()?"":" · unfinished edits"));
            recover->setEnabled(!entries.isEmpty());remove->setEnabled(!entries.isEmpty());locate->setEnabled(!entries.isEmpty());if(!entries.isEmpty())list->setCurrentRow(0);else details->setText("No recoverable snapshots. Snapshots from another running editor are not shown.");};
        const auto describe=[&](int row){if(row<0||row>=entries.size())return;
            details->setText("Source ROM: "+selectedSource+(entries[row].drafts.isEmpty()?"":"\nUnfinished edits included: "+entries[row].drafts.join(", "))+(entries[row].pending.isEmpty()?"":"\nNot included: "+entries[row].pending.join(", ")));};
        connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row){if(row<0||row>=entries.size())return;
            selectedSource=entries[row].sourcePath;describe(row);});
        connect(locate,&QPushButton::clicked,&dialog,[&]{
            const auto chosen=QFileDialog::getOpenFileName(&dialog,"Locate original source ROM",selectedSource,"N64 ROM (*.v64 *.z64 *.n64)");
            if(!chosen.isEmpty()){selectedSource=chosen;describe(list->currentRow());}
        });
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(folder,&QPushButton::clicked,&dialog,[&]{QDesktopServices::openUrl(QUrl::fromLocalFile(store.root()));});
        connect(remove,&QPushButton::clicked,&dialog,[&]{int row=list->currentRow();if(row<0)return;
            if(QMessageBox::question(&dialog,"Delete recovery snapshot","Delete only this selected recovery snapshot?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            try{store.remove(entries[row].path);refresh();}catch(const std::exception &e){report(QString::fromUtf8(e.what()));}});
        connect(recover,&QPushButton::clicked,&dialog,[&]{int row=list->currentRow();if(row<0)return;
            auto source=selectedSource;
            if(!QFileInfo::exists(source))source=QFileDialog::getOpenFileName(&dialog,"Locate original source ROM",{},"N64 ROM (*.v64 *.z64 *.n64)");
            if(source.isEmpty())return;
            try{if(openRecoveryFile(entries[row].path,source))dialog.accept();}catch(const std::exception &e){report(QString::fromUtf8(e.what()));}});
        refresh();dialog.exec();
    } catch(const std::exception &e){report(QString::fromUtf8(e.what()));}
}
}
