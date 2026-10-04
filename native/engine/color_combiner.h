#pragma once
#include "engine/rom.h"
#include <array>
namespace lordly {
using RenderColor = std::array<quint8, 4>;
struct CombineInputs {
    RenderColor texel0{}, texel1{}, primitive{}, environment{}, shade{};
};
// Desktop arithmetic model of the RGB/alpha combiner, independent of geometry.
// Retains command muxes and two-cycle texture order; floating arithmetic and
// final8-bit clipping are approximations of RDP intermediate precision.
class ColorCombiner {
  public:
    ColorCombiner(quint32 high, quint32 low, bool twoCycles);
    RenderColor evaluate(const CombineInputs &) const;

  private:
    struct Cycle {
        std::array<int, 4> color{}, alpha{};
    };
    std::array<Cycle, 2> cycles_;
    bool twoCycles_;
};
} // namespace lordly
