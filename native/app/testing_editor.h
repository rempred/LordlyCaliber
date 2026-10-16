#pragma once
#include "engine/scenario_seeder.h"
#include <QWidget>
#include <functional>
#include <memory>
#include <utility>
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QComboBox;
class QCheckBox;
namespace lordly {
class TestingEditor : public QWidget {
    Q_OBJECT
  public:
    explicit TestingEditor(const QString &romPath, QWidget *parent = nullptr);
    void loadRom(const QString &);
    void setDefaultArmy(const QByteArray &);
    void loadSave(const QString &);
    void loadSaveRom(const QString &);
    void useExport(const QString &path, quint64 projectRevision, const QByteArray &digest);
    void useTestBuild(const QString &path, quint64 projectRevision, const QByteArray &digest,
                      int choice);
    void launchPrepared(int choice);
    void buildFailed(const QString &message);
    bool currentProjectReady() const;
    void setBuildPath(const QString &path);
    // The launcher is injectable so the complete build/seed workflow can be checked
    // without controlling an emulator. Returns the previous save's backup path.
    using Launcher = std::function<QString(const Rom &, const QString &romPath,
                                           const QString &executable, const QByteArray &save,
                                           const QString &destination, bool merge, bool start)>;
    void setLauncher(Launcher launcher) {
        launcher_ = std::move(launcher);
    }
    void setProjectRevision(quint64 revision, bool pending);
    void selectScenario(int key);
    bool ready() const {
        return bool(seeder_);
    }
    QString defaultArmyPath() const;
  signals:
    void editDefaultArmy(const QByteArray &);
    void testProjectRequested(int choice);
    void buildLocationRequested();
    void assignmentFinished(const QString &message);

  protected:
    void showEvent(QShowEvent *) override;

  private:
    void filter();
    void guarded(const std::function<void()> &, QLabel *feedback = nullptr);
    void chooseArmy();
    void useSave();
    void assignCompleteSave();
    void installPreparedSave(const Rom &, const QString &romPath, const QByteArray &save,
                             const QString &title, bool merge, bool start, qint64 size,
                             qint64 time);
    void requestLaunch(int choice);
    void addLaunchButton(QTreeWidgetItem *);
    void checkRom() const;
    void updateFreshness();
    void updateLaunchControls();
    void updateArmyLabel();
    bool matchesExport() const;
    std::optional<SaveDocument> army() const;
    std::shared_ptr<ScenarioSeeder> seeder_;
    QString initialRom_, romPath_, project64_, armyPath_, savePath_, saveRomPath_;
    QString exportPath_, pendingRom_;
    QByteArray exportDigest_, loadedDigest_;
    quint64 projectRevision_ = 0, exportRevision_ = 0;
    bool pendingEdits_ = false;
    int requestedScenario_ = -1;
    int pendingLaunch_ = -2;
    int pendingNode_ = -1, pendingEvent_ = -1;
    Launcher launcher_;
    QLabel *freshness_;
    QLabel *buildPathLabel_;
    QCheckBox *launchAfterSeed_;
    int armySlot_ = 0;
    qint64 romSize_ = 0, romTime_ = 0;
    bool initialized_ = false, busy_ = false;
    QLabel *romLabel_, *p64Label_, *armyLabel_, *saveLabel_, *saveRomLabel_, *saveStatus_, *status_,
        *detail_;
    QComboBox *sourceMode_;
    QLineEdit *search_;
    QTreeWidget *missions_;
    QWidget *controls_;
};
} // namespace lordly
