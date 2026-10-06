#pragma once
#include "engine/scene_images.h"
#include "engine/scene_background_edit.h"
#include "engine/scene_programs.h"
#include <QWidget>
#include <functional>
#include <optional>
class QUndoStack;
class QPushButton;
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
class SceneActionEditor;
class TransformPreview;
class ScenePreview;
class CutsceneEditor : public QWidget {
    Q_OBJECT
  public:
    explicit CutsceneEditor(const Rom &rom, QWidget *parent = nullptr, Project *project = nullptr,
                            QUndoStack *undo = nullptr, std::function<void()> changed = {});

  private:
    void selectScene();
    SceneId selectedScene() const;
    const ScenePrograms &programs() const;
    void reloadScenes(SceneId preferred = {});
    void createScene(bool blank);
    void renameScene();
    void removeScene();
    void wipeScene();
    void selectRequest();
    void selectImage(const QString &id);
    void showImage();
    QWidget *createBackgroundControls();
    void refreshBackgroundEdit();
    void previewBackgroundChoice();
    void applyBackgroundChoice(bool restore);
    bool invalidSceneInput() const;
    void refreshSceneInputState();
    Rom rom_;
    Project *project_ = nullptr;
    QUndoStack *undo_ = nullptr;
    std::optional<ScenePrograms> readOnlyPrograms_;
    QString sceneCatalogSignature_;
    QPushButton *newScene_ = nullptr, *duplicateScene_ = nullptr, *renameScene_ = nullptr,
                *removeScene_ = nullptr, *wipeScene_ = nullptr;
    SceneCommandEditor *commandEditor_ = nullptr;
    SceneActionEditor *actionEditor_ = nullptr;
    TransformPreview *transformPreview_ = nullptr;
    ScenePreview *scenePreview_ = nullptr;
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
    BackgroundEdit backgroundEdit_;
    QMap<int, QVector<BackgroundChoice>> backgroundOptions_;
    QComboBox *backgroundChoice_ = nullptr;
    QPushButton *backgroundApply_ = nullptr, *backgroundRestore_ = nullptr;
    QLabel *backgroundHelp_ = nullptr;
};
} // namespace lordly
