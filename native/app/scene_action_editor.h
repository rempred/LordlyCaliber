#pragma once
#include "engine/project.h"
#include "engine/scene_actions.h"
#include "engine/scene_camera.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QComboBox;
class QListWidget;
class QFormLayout;
class QLabel;
class QPushButton;
namespace lordly {
class SceneActionEditor : public QWidget {
    Q_OBJECT
  public:
    SceneActionEditor(Project *, QUndoStack *, std::function<void()> changed,
                      QWidget *parent = nullptr);
    void selectScene(SceneId, int command);
    void selectCommand(int command);
    bool selectActor(int slot);
    bool selectSprite(int slot);
    void applyPosition(int command, SceneVector);
    void updateTimeline(SceneId, const QMap<int, int> &, int activeWord, int tick);
    void placeActorAtPlayhead(int slot, SceneVector);
    void setPlacementMode(bool move, int duration, bool wait);
    void addAtPlayhead(quint32 kind);
    QVector<QPair<QString,int>> storyDestinations() const;
    void addFromWorkspace(quint32 kind, int where, int before = -1);
    void playObjectKey(bool sprite,int slot,int bank,int animation,int facing,const QByteArray &record,int variant=-1);
    void addAsset(bool sprite, int bank, int animation, int facing, SceneVector position,int variant=0,int slot=-1);
    void placeSpriteAtPlayhead(int slot, SceneVector position);
    void reportExit(SceneId, int word, const QSet<int> &movingActors, bool fade, int panUpdates, bool paused);
  signals:
    void dialogueLabelsChanged(SceneId id, const QMap<int, QString> &labels,
                               const QMap<int, QString> &branchAnswers);
    void stageFeedback(const QString &message);
    void assetAdded(bool sprite, int slot);
    void commandSelected(int command);
    void actionActivated(int command);
    void previewUpdateRequested(int tick);
    void editDialogue(int selector, int entry);
    void validityChanged();
    void choosePosition(int command, int slot);

  private:
    void refresh(int command);
    void showAction();
    void publishFields();
    void refreshClassChoices();
    void validity(bool valid);
    void updateStructureControls();
    enum class Change { Add, Remove, Move };
    void changeStructure(Change);
    void insertAtPlayhead();
    void holdBeforeExit();
    void changeDialogue(bool create, bool choice, bool atPlayhead = false);
    QComboBox *kind_, *position_;
    QPushButton *insert_, *remove_, *move_, *text_, *positionPick_;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    SceneId key_;
    QVector<SceneAction> actions_;
    QListWidget *list_;
    QFormLayout *fields_;
    QLabel *notice_;
    QPushButton *discard_;
    bool editing_ = false;
    QMap<int, int> commandTimes_;
    int activeWord_ = -1, previewTick_ = 0;
    QLabel *exitWarning_ = nullptr;
    QPushButton *hold_ = nullptr, *insertTimed_ = nullptr;
    int exitWord_ = -1, unfinishedPan_ = 0;
    QSet<int> unfinishedActors_;
    bool unfinishedFade_ = false;
    bool moveTool_ = false, waitForMove_ = false;
    int moveDuration_ = 30;
};
} // namespace lordly
