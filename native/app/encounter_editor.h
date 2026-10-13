#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QListWidget;
class QLineEdit;
class QLabel;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QTableWidget;
class QDialog;
namespace lordly {
class EncounterEditor : public QWidget {
  public:
    EncounterEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                    QWidget *parent = nullptr);

  private:
    void refresh();
    void filter();
    void chooseCreatures();
    void assignSquad();
    void editSquad();
    void copyEncounters();
    void showTerrain();
    int selectedKey() const;
    int selectedSlot() const;
    void changeTerrain(int terrain);
    void showGlobalRatio();
    void refreshPoolRows();
    void editTerrainRules(int slot);
    QString poolContents(int slot) const;
    QStringList squadSummaries(int slot) const;
    void applyGlobalRate(bool restore);
    QWidget *createRewardsPage();
    void refreshRewardClasses();
    void showReward();
    void showRewardOdds();
    void applyReward(bool restore);
    void filterRewards();
    QWidget *createScenarioPage();
    void refreshScenarios();
    void filterScenarios();
    void showScenario();
    void showScenarioOdds();
    void applyScenario(bool restore);
    void commit(const Encounters &next, const QString &label);
    int selected() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *pools_;
    QLineEdit *search_;
    QLabel *heading_, *notice_, *terrainContext_;
    QComboBox *mapKey_;
    QLabel *selection_, *scope_;
    QDialog *terrainDialog_;
    QTableWidget *pairTable_, *terrainTable_, *estimates_ = nullptr;
    QListWidget *poolTiles_;
    QSpinBox *deployed_;
    QDoubleSpinBox *checks_;
    QVector<QSpinBox *> rates_;
    QDoubleSpinBox *divisor_, *normalPass_, *alternatePass_;
    QCheckBox *linkedRates_;
    QLabel *globalRatio_ = nullptr, *globalNotice_ = nullptr;
    QListWidget *rewardClasses_ = nullptr;
    QLineEdit *rewardSearch_ = nullptr;
    QComboBox *rewardCopyFrom_ = nullptr;
    QVector<QComboBox *> rewardItems_;
    QVector<QSpinBox *> rewardWeights_;
    QVector<QLabel *> rewardOdds_;
    QLabel *rewardNotice_ = nullptr;
    QWidget *rewardDetails_ = nullptr;
    QListWidget *scenarios_ = nullptr;
    QLineEdit *scenarioSearch_ = nullptr;
    QWidget *scenarioDetails_ = nullptr;
    QVector<QComboBox *> scenarioModes_;
    QVector<QDoubleSpinBox *> scenarioDivisors_, scenarioPasses_;
    QVector<QLabel *> scenarioOdds_;
    QLabel *scenarioNotice_ = nullptr;
};
} // namespace lordly
