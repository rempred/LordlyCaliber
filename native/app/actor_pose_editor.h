#pragma once
#include "engine/actor_pose.h"
#include "engine/actor_renderer.h"
#include "engine/director.h"
#include "engine/sprite_pose.h"
#include <QWidget>
#include <optional>
#include <variant>
class QCheckBox;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QListWidget;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QTimer;
namespace lordly {
class ActorPoseEditor : public QWidget {
    Q_OBJECT
  public:
    explicit ActorPoseEditor(const Rom &rom, QWidget *parent = nullptr);
    void selectCommand(const DirectorProgram &, int command);

  private:
    void loadSources();
    void loadBank();
    void restart();
    void seek(int step);
    void step();
    void draw();
    void drawProjected();
    void stop();
    int advancePose();
    bool suspendedPose() const;
    QString poseError() const;
    Rom rom_;
    ActorRenderer renderer_;
    QCheckBox *projected_, *shadows_;
    QComboBox *placementMode_;
    QWidget *placementControls_;
    std::array<QDoubleSpinBox *, 3> placement_;
    QDoubleSpinBox *size_, *cameraYaw_, *cameraHeight_, *cameraDistance_;
    QSpinBox *facing_;
    int previousPlacementMode_ = 0;
    std::array<std::array<double, 3>, 2> previewCameras_{{{0, 80, 300}, {65, 80, 300}}};
    std::optional<ActorArt> art_;
    std::optional<std::variant<ActorPosePlayer, SpritePosePlayer>> player_;
    QCheckBox *sepia_;
    QComboBox *kinds_, *banks_, *poses_, *owners_, *flags_;
    QSpinBox *variant_, *position_, *speed_;
    QPushButton *play_;
    QLabel *status_, *selection_;
    QListWidget *layers_;
    QGraphicsView *view_;
    QGraphicsScene *canvas_;
    QTimer *timer_;
    QVector<QGraphicsPixmapItem *> items_;
    bool fit_ = true;
    int step_ = 0;
};
} // namespace lordly
