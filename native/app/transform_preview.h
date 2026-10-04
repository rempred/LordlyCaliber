#pragma once
#include "engine/scene_images.h"
#include "engine/scene_transforms.h"
#include <QImage>
#include <QMap>
#include <QWidget>
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class QGraphicsView;
class QGraphicsScene;
namespace lordly {
// Isolated transform/resource inspection. Does not execute the other scene actions.
class TransformPreview : public QWidget {
    Q_OBJECT
  public:
    explicit TransformPreview(const Rom &, QWidget *parent = nullptr);
    void selectSequence(int resource, int group, qint32 duration, SceneCamera camera);
    void selectCommand(const DirectorProgram &, int command);
    void setImage(std::optional<DecodedImage> image);
    void clear();
    const QImage &renderedFrame() const {
        return frame_;
    }

  protected:
    void hideEvent(QHideEvent *) override;

  private:
    void reload(bool newResource);
    void seek(int tick);
    void draw();
    void stop();
    void fail(const std::exception &);
    Rom rom_;
    SceneCamera camera_;
    std::optional<SceneTransformAsset> asset_;
    std::optional<SceneTransforms> state_;
    std::optional<DecodedImage> image_;
    QMap<int, SceneTransforms> checkpoints_;
    QImage frame_;
    int tick_ = 0;
    QSpinBox *resource_, *group_, *duration_, *channel_, *position_, *rate_;
    QPushButton *play_, *step_;
    QTimer *timer_;
    QLabel *status_, *context_;
    QGraphicsScene *canvas_;
    QGraphicsView *view_;
};
} // namespace lordly
