#pragma once
#include "app/record_model.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QMainWindow>
#include <memory>
#include <atomic>
class QTabWidget;
class QLabel;
class QUndoGroup;
class QLockFile;
class QTimer;
class QToolBar;
namespace lordly {
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void loadRom(const QString &path);
    bool openProjectFile(const QString &path);
    void navigateTo(const EditLocation &);
    bool openRecoveryFile(const QString &path, const QString &sourceRom);

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    void installProject(Project project);
    void addRecordWorkspace(Table table, const QString &title);
    bool confirmDiscard();
    void openProject();
    void saveProject(bool archive);
    void buildRom();
    bool prepareBuild(const QString &action);
    void buildForTesting(int choice);
    void compileRom(const QString &path, int testChoice = -2);
    void showUses();
    void updateStatus();
    bool invalidEdits() const;
    void report(const QString &text);
    void initializeRecovery();
    void autosaveRecovery();
    void showRecovery();
    void clearRecovery();
    void beginRecoverySession();
    QJsonObject catalog_;
    std::unique_ptr<Project> project_;
    QUndoStack *undo_;
    QUndoGroup *undoGroup_;
    QTabWidget *workspaces_;
    QLabel *summary_;
    bool loading_ = false;
    quint64 projectRevision_ = 0;
    QString testBuildPath_, testBuildSettingsKey_;
    std::shared_ptr<QLockFile> testBuildLock_;
    QString recoverySession_, recoveredSession_, projectLabel_;
    quint64 recoveryRevision_ = 0;
    bool recoveryBusy_ = false;
    bool recoveryAgain_ = false;
    QJsonArray recoveryDrafts_;
    QLabel *recoveryStatus_ = nullptr;
    QToolBar *recoveryNotice_ = nullptr;
    std::unique_ptr<QLockFile> recoverySessionLock_;
    std::shared_ptr<std::atomic_bool> recoveryCancelled_ = std::make_shared<std::atomic_bool>(false);
};
} // namespace lordly
