#pragma once
#include "engine/actor_pose.h"
#include "engine/sprite_pose.h"
#include <QWidget>
#include <optional>
#include <variant>
class QCheckBox;
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

  private:
    void loadSources();
    void loadBank();
    void restart();
    void seek(int step);
    void step();
    void draw();
    void stop();
    int advancePose();
    bool suspendedPose() const;
    QString poseError() const;
    Rom rom_;
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
