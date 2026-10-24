#pragma once
#include "engine/sprite_library.h"
#include <QWidget>
class QCheckBox;
class QSpinBox;
namespace lordly {
class AnimationColorComparison : public QWidget {
    Q_OBJECT
  public:
    explicit AnimationColorComparison(QWidget *parent = nullptr);
    void setAsset(const SpriteAsset &);
    void clear();
    void setFrame(int);
    void setFrameSelectorVisible(bool);
    QCheckBox *dither() const {
        return dither_;
    }
    QImage originalImage() const {
        return original_;
    }
    QImage gameImage() const {
        return game_;
    }

  private:
    void refresh();
    SpriteAsset asset_;
    QImage original_, game_;
    QWidget *canvas_, *frameRow_;
    QCheckBox *dither_;
    QSpinBox *frame_;
};
} // namespace lordly
