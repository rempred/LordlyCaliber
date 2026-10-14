#pragma once
#include "engine/scenario_seeder.h"
#include <QWidget>
#include <functional>
#include <memory>
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
namespace lordly {
class TestingEditor : public QWidget {
    Q_OBJECT
  public:
    explicit TestingEditor(const QString &romPath, QWidget *parent = nullptr);
    void loadRom(const QString &);
    void setDefaultArmy(const QByteArray &);
    void loadSave(const QString &);
    void useExport(const QString &path, quint64 projectRevision, const QByteArray &digest);
    void setProjectRevision(quint64 revision, bool pending);
    void selectScenario(int key);
    bool ready() const {
        return bool(seeder_);
    }
    QString defaultArmyPath() const;
  signals:
    void editDefaultArmy(const QByteArray &);
    void exportRequested();

  protected:
    void showEvent(QShowEvent *) override;

  private:
    void filter();
    void guarded(const std::function<void()> &);
    void chooseArmy();
    void useSave();
    void coldBoot(QTreeWidgetItem *);
    void addLaunchButton(QTreeWidgetItem *);
    void checkRom() const;
    void updateFreshness();
    QString destination() const;
    std::optional<SaveDocument> army() const;
    std::shared_ptr<ScenarioSeeder> seeder_;
    QString initialRom_, romPath_, project64_, armyPath_, savePath_;
    QString exportPath_, pendingRom_;
    QByteArray exportDigest_, loadedDigest_;
    quint64 projectRevision_ = 0, exportRevision_ = 0;
    bool pendingEdits_ = false;
    int requestedScenario_ = -1;
    QLabel *freshness_;
    int armySlot_ = 0;
    qint64 romSize_ = 0, romTime_ = 0;
    bool initialized_ = false, busy_ = false;
    QLabel *romLabel_, *p64Label_, *armyLabel_, *status_, *detail_;
    QLineEdit *search_;
    QTreeWidget *missions_;
    QWidget *controls_;
};
} // namespace lordly
