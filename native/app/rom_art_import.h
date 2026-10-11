#pragma once
#include "engine/sprite_library.h"
#include <QDialog>
#include <functional>
namespace lordly {
class RomArtImport : public QDialog {
    Q_OBJECT
  public:
    RomArtImport(const Rom &, std::function<void(SpriteAsset)>, QWidget *parent = nullptr);
};
} // namespace lordly
