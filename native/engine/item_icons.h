#pragma once
#include "engine/image.h"
#include <array>
namespace lordly {
// CI8 item artwork with a shared RGBA5551 palette. Item bit0x200 selects
// consumables; low nine bits select the one-based item within its pack.
class ItemIcons {
  public:
    explicit ItemIcons(Rom rom) : rom_(std::move(rom)) {}
    RasterImage image(quint16 item) const;

  private:
    const QByteArray &pack(bool consumable) const;
    Rom rom_;
    mutable std::array<QByteArray, 2> packs_;
};
} // namespace lordly
