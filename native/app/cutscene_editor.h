#pragma once
#include "engine/scene_images.h"
#include <QWidget>
#include <functional>
#include <optional>
class QUndoStack;
class QComboBox;
class QLabel;
class QListWidget;
class QTableWidget;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QTabWidget;
namespace lordly {
class Project;
class SceneCommandEditor;
class CutsceneEditor : public QWidget {
    Q_OBJECT
  public:
    explicit CutsceneEditor(const Rom &rom, QWidget *parent = nullptr, Project *project = nullptr,
                            QUndoStack *undo = nullptr, std::function<void()> changed = {});

  private:
    void selectScene();
    void selectRequest();
    void selectImage(const QString &id);
    void showImage();
    Rom rom_;
    Project *project_ = nullptr;
    SceneCommandEditor *commandEditor_ = nullptr;
    std::optional<Directors> directors_;
    std::optional<DecodedImage> image_;
    SceneBackgrounds backgrounds_;
    quint64 generation_ = 0;
    bool decoding_ = false;
    QString imageId_;
    QListWidget *scenes_, *assets_, *layers_;
    QComboBox *mode_, *requests_, *images_, *frames_;
    QLabel *sceneStatus_, *imageStatus_;
    QTableWidget *commands_;
    QGraphicsView *view_;
    QGraphicsScene *canvas_;
    QTabWidget *catalogTabs_;
    QVector<QGraphicsPixmapItem *> layerItems_;
};
} // namespace lordly
