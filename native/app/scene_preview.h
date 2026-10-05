#pragma once
#include "engine/scene_actor_renderer.h"
#include "engine/scene_background_renderer.h"
#include "engine/scene_playback.h"
#include "engine/scene_programs.h"
#include "engine/scene_world_drawing.h"
#include "engine/sprite_renderer.h"
#include "engine/stage_orthographic.h"
#include <QMap>
#include <QWidget>
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class QGraphicsView;
class QGraphicsScene;
namespace lordly {
// Interactive PC reference stage. Unknown game services stop the preview.
// Completed draw snapshots keep ordinary UI repaints from advancing tint.
class ScenePreview : public QWidget {
    Q_OBJECT
  public:
    explicit ScenePreview(const Rom &, QWidget *parent = nullptr);
    void selectScene(const ScenePrograms &, SceneId, int mode);
    const QImage &renderedFrame() const {
        return frame_;
    }

  protected:
    void hideEvent(QHideEvent *) override;

  private:
    struct Snapshot {
        ScenePlayback playback;
        QImage frame;
        QString reason, backgroundName;
        bool stopped = false;
        std::optional<SceneBackgroundRenderer> backgrounds;
        std::optional<StageOrthographic> flatProps;
        int foregroundSelector = -1;
        quint32 previewRandom = 1;
    };
    void restart();
    void seek(int tick);
    void continueSeek();
    void step();
    void present();
    void stop();
    void render(Snapshot &, bool commitDraw);
    bool referenceCommand(Snapshot &, quint32, const QVector<quint32> &);
    Rom rom_;
    SceneActorRenderer renderer_;
    SpriteRenderer spriteRenderer_;
    StageFlatRenderer stageRenderer_;
    std::optional<Directors> resources_;
    std::optional<Snapshot> current_;
    QMap<int, Snapshot> checkpoints_;
    QImage frame_;
    int selector_ = -1, mode_ = -1, tick_ = 0;
    QSpinBox *position_, *rate_;
    QPushButton *play_, *step_;
    QLabel *status_, *context_;
    QTimer *timer_, *seekTimer_;
    int seekTarget_ = -1;
    QGraphicsScene *canvas_;
    QGraphicsView *view_;
};
} // namespace lordly
