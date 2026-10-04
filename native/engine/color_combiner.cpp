#include "engine/color_combiner.h"
#include <algorithm>
namespace lordly {
ColorCombiner::ColorCombiner(quint32 high, quint32 low, bool twoCycles) : twoCycles_(twoCycles) {
    if ((high >> 24) != 0xfc)
        throw Error("Color combiner requires a SetCombine command");
    cycles_[0].color = {int(high >> 20 & 15), int(low >> 28 & 15), int(high >> 15 & 31),
                        int(low >> 15 & 7)};
    cycles_[0].alpha = {int(high >> 12 & 7), int(low >> 12 & 7), int(high >> 9 & 7),
                        int(low >> 9 & 7)};
    cycles_[1].color = {int(high >> 5 & 15), int(low >> 24 & 15), int(high & 31),
                        int(low >> 6 & 7)};
    cycles_[1].alpha = {int(low >> 21 & 7), int(low >> 3 & 7), int(low >> 18 & 7), int(low & 7)};
}
RenderColor ColorCombiner::evaluate(const CombineInputs &input) const {
    std::array<double, 4> combined{};
    for (int cycle = twoCycles_ ? 0 : 1; cycle < 2; ++cycle) {
        // In two-cycle mode, the primitive tile is TEXEL1 during cycle2.
        const bool swap = twoCycles_ && cycle == 1;
        const auto &texture0 = swap ? input.texel1 : input.texel0;
        const auto &texture1 = swap ? input.texel0 : input.texel1;
        auto ordinary = [&](int selector, int channel) -> double {
            switch (selector) {
            case 0:
                return combined[channel];
            case 1:
                return texture0[channel];
            case 2:
                return texture1[channel];
            case 3:
                return input.primitive[channel];
            case 4:
                return input.shade[channel];
            case 5:
                return input.environment[channel];
            }
            throw Error("Unresolved color-combiner input");
        };
        auto color = [&](int term, int selector, int channel) -> double {
            if (selector <= 5)
                return ordinary(selector, channel);
            if (term == 2) {
                if (selector >= 7 && selector <= 12)
                    return ordinary(selector - 7, 3);
                if (selector >= 16)
                    return 0;
                throw Error("Color combiner requires chroma-key, LOD or conversion input");
            }
            if (term == 3)
                return selector == 6 ? 255. : 0.;
            if (selector >= 8)
                return 0;
            if (term == 0 && selector == 6)
                return 255.;
            throw Error("Color combiner requires noise or chroma-key input");
        };
        auto alpha = [&](int term, int selector) -> double {
            if (selector == 7)
                return 0;
            if (term == 2 && (selector == 0 || selector == 6))
                throw Error("Alpha combiner requires an explicit LOD fraction");
            return selector == 6 ? 255. : ordinary(selector, 3);
        };
        std::array<double, 4> next;
        const auto &selected = cycles_[cycle];
        for (int channel = 0; channel < 3; ++channel) {
            const auto &s = selected.color;
            next[channel] = (color(0, s[0], channel) - color(1, s[1], channel)) *
                                color(2, s[2], channel) / 255. +
                            color(3, s[3], channel);
        }
        const auto &s = selected.alpha;
        next[3] = (alpha(0, s[0]) - alpha(1, s[1])) * alpha(2, s[2]) / 255. + alpha(3, s[3]);
        for (int i = 0; i < 4; ++i)
            combined[i] = std::clamp(next[i], 0., 255.);
    }
    RenderColor result;
    for (int i = 0; i < 4; ++i)
        result[i] = quint8(combined[i]);
    return result;
}
} // namespace lordly
