#pragma once
#include "engine/dialogue_session.h"
#include "engine/scene_actor_renderer.h"
#include "engine/scene_background_renderer.h"
#include "engine/scene_playback.h"
#include "engine/scene_party.h"
#include "engine/scene_programs.h"
#include "engine/scene_selection.h"
#include "engine/scene_world_drawing.h"
#include "engine/sprite_renderer.h"
#include "engine/stage_object_renderer.h"
#include "engine/stage_orthographic.h"
#include <QMap>
#include <QWidget>
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsRectItem;
class QComboBox;
class QLineEdit;
namespace lordly {
class SaveDocument;
// Interactive PC reference stage. Unknown game services stop the preview.
// Completed draw snapshots keep ordinary UI repaints from advancing tint.
class ScenePreview : public QWidget {
    Q_OBJECT
  public:
    explicit ScenePreview(const Rom &, QWidget *parent = nullptr);
    ~ScenePreview() override;
    void selectScene(const ScenePrograms &, SceneId, int mode, std::optional<Dialogues> = {},
                     std::optional<SceneEvents> = {});
    void setDialogues(std::optional<Dialogues>);
    void setPortraitProvider(DialogueDrawing::PortraitProvider);
    void refreshPortraits();
    void setArtwork(const ActorArtworkEdits &);
    void setBodyAnimations(BodyAnimationData::Pointer);
    void setSaveProvider(std::function<const SaveDocument *()> provider);
    void setObjectNavigationEnabled(bool enabled);
    void beginPositionPick(int command, int slot);
    void seekToCommand(int word);
    const QMap<int, int> &commandTimes() const { return commandTimes_; }
    int activeWord() const { return current_ ? current_->activeWord : -1; }
    int actorAt(QPoint) const;
    int spriteAt(QPoint) const;
    SceneSelectionTarget objectAt(QPoint) const;
  signals:
    void findActorAction(int selector, int slot);
    void findSpriteAction(int selector, int slot);
    void positionPicked(int command, SceneVector position);
    void timelineChanged(SceneId, const QMap<int, int> &commandTimes, int activeWord, int tick);

  public:
    const QImage &renderedFrame() const {
        return frame_;
    }

  protected:
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    struct Snapshot {
        ScenePlayback playback;
        QImage frame;
        QString reason, backgroundName;
        bool stopped = false;
        SceneSelection selection;
        std::optional<SceneBackgroundRenderer> backgrounds;
        QMap<quint32, SceneBackgroundChannel> backgroundResources;
        std::array<quint32, 20> backgroundHandles{};
        quint32 nextBackgroundHandle = 3;
        std::optional<StageOrthographic> flatProps;
        std::optional<StagePerspective> worldProps;
        int foregroundSelector = -1;
        int environmentSelector = -1;
        quint32 previewRandom = 1;
        quint32 texturePhase = 0;
        std::optional<DialoguePlayback> dialogue;
        int activeWord = -1;
    };
    void restart();
    void seek(int tick);
    void beginSeek(int tick, int commandWord);
    void continueSeek();
    void step();
    void present();
    void presentObjects();
    void presentSelectedObject();
    void presentSelectionOutline();
    void stop();
    void render(Snapshot &, bool commitDraw);
    bool referenceCommand(Snapshot &, quint32, const QVector<quint32> &, int *value = nullptr);
    void retainBackgrounds(Snapshot &);
    void convertBackgrounds(Snapshot &);
    int activeChannels(const Snapshot &) const;
    void imageButton(quint16 bit, bool down);
    void imageInput(LordlyImageInput);
    void branchInput();
    void skipScene();
    void panelButton(quint16);
    bool hasDialogue() const;
    std::optional<QPointF> dialoguePoint(const Snapshot &, int slot) const;
    Rom rom_;
    BodyAnimationData::Pointer bodyAnimations_;
    DialogueDrawing::PortraitProvider portraits_;
    SceneActorRenderer renderer_;
    SpriteRenderer spriteRenderer_;
    StageFlatRenderer stageRenderer_;
    StageObjectRenderer objectRenderer_;
    std::optional<Directors> resources_;
    std::optional<Dialogues> dialogues_;
    std::optional<DialogueDrawing> dialogueDrawing_;
    DialogueTextEnvironment dialogueEnvironment_;
    std::optional<Snapshot> current_;
    QMap<int, Snapshot> checkpoints_;
    QMap<int, int> commandTimes_;
    SceneId sceneId_;
    int commandSeekWord_ = -1;
    QString commandSeekNotice_;
    QMap<int, LordlyImageInput> inputHistory_;
    QMap<int, quint16> pressedHistory_;
    LordlyImageInput liveInput_{};
    quint16 pendingPressed_ = 0;
    QImage frame_;
    int selector_ = -1, mode_ = -1, tick_ = 0;
    QSpinBox *position_, *rate_;
    QPushButton *play_, *step_, *skip_;
    QLabel *status_, *context_;
    QComboBox *liveObjects_;
    QLabel *objectState_;
    QPushButton *findObject_;
    SceneSelectionTarget selectedObject_;
    void findSelectedAction();
    void cancelPositionPick();
    int placementCommand_ = -1, placementSlot_ = -1;
    QLabel *placementHelp_;
    QPushButton *cancelPlacement_;
    bool objectNavigationEnabled_ = false;
    std::optional<QPoint> selectionPress_;
    QTimer *timer_, *seekTimer_;
    int seekTarget_ = -1;
    QGraphicsScene *canvas_;
    QGraphicsRectItem *selectionOutline_ = nullptr;
    QGraphicsView *view_;
    QWidget *imageControlsUi_;
    QWidget *panelControlsUi_;
    QWidget *launchImageUi_;
    QComboBox *launchImage_;
    QWidget *storyCastUi_;
    std::array<QComboBox *, 5> storyCast_{}, storySecondary_{};
    std::function<const SaveDocument *()> saveProvider_;
    QPushButton *savedCast_, *armyCast_;
    bool armyCastActive_ = true;
    void loadArmyCast();
    QLabel *castSource_;
    int classCount_ = 0;
    bool storyCastEnabled_ = false;
    bool classDefaultsEnabled_ = false;
    std::optional<SceneParty> party_;
    QVector<SceneLaunchPlan> launchPlans_;
    QWidget *launchContextUi_;
    QComboBox *launchContext_;
    bool preparing_ = false;
    QString contextSummary_;
    QWidget *dialogueNamesUi_;
    QWidget *rewardUi_;
    QComboBox *reward_;
    std::array<QLineEdit *, 4> dialogueNames_{};
};
} // namespace lordly
