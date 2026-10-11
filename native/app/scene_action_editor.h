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
    void reportExit(SceneId, int word, const QSet<int> &movingActors, bool fade, int panUpdates, bool paused);
  signals:
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
    void changeDialogue(bool create, bool choice);
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
};
} // namespace lordly
