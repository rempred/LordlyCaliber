#pragma once
#include "engine/actor_pose.h"
#include "engine/sprite_pose.h"
#include <QWidget>
#include <QFutureWatcher>
#include <variant>
class QTabWidget; class QComboBox; class QListWidget; class QLabel; class QTimer; class QPushButton;class QCheckBox;
namespace lordly {
class SceneKeyPicker : public QWidget {
    Q_OBJECT
  public:
    explicit SceneKeyPicker(const Rom &, QWidget *parent=nullptr);
    QWidget *auditionWidget() const { return audition_; }
    void setArtwork(const ActorArtworkEdits &);
    QVector<int> selectedKey() const;
    void openLibrary(bool browseArtwork);
    void selectArtwork(int bank,int appearance,int animation,int facing);
  signals:
    void keySelected(bool sprite, int bank, int animation, int facing, int variant);
    void placementCancelled();
  protected:
    void hideEvent(QHideEvent *) override;
    void showEvent(QShowEvent *) override;
  private:
    void loadNextGalleryBank();
    void resetGallery();
    void selectFirstVisibleKey();
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
    QListWidget *keys_, *gallery_;
    QTabWidget *pages_;
    struct GalleryBank {
        int generation, bank;
        QVector<QImage> thumbnails;
    };
    QFutureWatcher<GalleryBank> *galleryLoader_;
    int galleryGeneration_ = 0, nextBank_ = 1;
    bool loadingBank_ = false;
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
