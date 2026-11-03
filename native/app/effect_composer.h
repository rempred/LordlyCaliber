#pragma once
#include "engine/effect_composition.h"
#include "engine/project.h"
#include <QDialog>
#include <QElapsedTimer>
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QListWidget;
class QLabel;
class QPushButton;
class QSlider;
class QUndoStack;
namespace lordly {
class EffectCanvas;
class EffectComposer : public QDialog {
    Q_OBJECT
  public:
    EffectComposer(Project *, EffectComposition, QWidget * = nullptr, bool allowAssign = false);
    const EffectComposition &composition() const {
        return model_;
    }
    SpriteAsset asset() const {
        return model_.bake();
    }
    void addPiece(SpriteAsset);
    void replacePiece(int index, SpriteAsset);
    void setComposition(const EffectComposition &);
    bool assignAfterSave() const {
        return assignAfter_;
    }

  protected:
    void keyPressEvent(QKeyEvent *) override;

  private:
    void record(EffectComposition);
    void history(int);
    void refresh();
    void updatePreview();
    void editPiece(const std::function<void(EffectPiece &)> &);
    void chooseLibrary();
    void chooseEffect();
    void chooseFile();
    void importPiece(SpriteAsset);
    void guarded(const std::function<void()> &);
    void finish(bool assign);
    Project *project_;
    EffectComposition model_;
    QVector<EffectComposition> history_;
    int historyAt_ = 0;
    bool refreshing_ = false, playing_ = false, editEnd_ = false, assignAfter_ = false;
    bool replacing_ = false;
    int playFrom_ = 0;
    QElapsedTimer clock_;
    EffectCanvas *canvas_;
    QListWidget *pieces_;
    QLineEdit *name_, *pieceName_;
    QSpinBox *width_, *height_, *length_, *start_, *duration_, *fromX_, *fromY_, *toX_, *toY_;
    QSpinBox *fromScale_, *toScale_, *fromAngle_, *toAngle_, *fromOpacity_, *toOpacity_;
    QSpinBox *speed_;
    QComboBox *easing_, *placement_;
    QCheckBox *loop_, *playbackLoop_;
    QWidget *pieceControls_;
    QPushButton *play_, *undo_, *redo_;
    QSlider *time_;
    QLabel *timeLabel_, *notice_;
};
// One project undo command, with an independent copy of the composed artwork.
int assignComposedEffect(Project *, QUndoStack *, const SpriteAsset &, int attack, bool duplicate);
void chooseEffectAttack(Project *, QUndoStack *, const SpriteAsset &, QWidget *parent);
} // namespace lordly
