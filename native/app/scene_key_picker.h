#pragma once
#include "engine/actor_pose.h"
#include "engine/sprite_pose.h"
#include <QWidget>
#include <variant>
class QComboBox; class QListWidget; class QLabel; class QTimer; class QPushButton;class QCheckBox;
namespace lordly {
class SceneKeyPicker : public QWidget {
    Q_OBJECT
  public:
    explicit SceneKeyPicker(const Rom &, QWidget *parent=nullptr);
    QWidget *auditionWidget() const { return audition_; }
    void setArtwork(const ActorArtworkEdits &);
    QVector<int> selectedKey() const;
  signals:
    void keySelected(bool sprite, int bank, int animation, int facing, int variant);
    void placementCancelled();
  protected:
    void hideEvent(QHideEvent *) override;
  private:
    void loadBank();
    void loadKeys();
    void selectKey();
    void restart();
    void step();
    void draw();
    QImage firstFrame(int state, int variant) const;
    Rom rom_;
    ActorArtworkEdits artwork_;
    std::optional<ActorArt> art_;
    std::variant<std::monostate,ActorPosePlayer,SpritePosePlayer> player_;
    QComboBox *kind_, *banks_, *variants_;
    QListWidget *keys_;
    QWidget *audition_;
    QLabel *image_, *status_;
    QPushButton *play_;
    QCheckBox *loop_;
    QTimer *timer_;
    int state_=-1, ticks_=0;
    bool finished_=false;
    QSet<QString> visited_;
};
}
