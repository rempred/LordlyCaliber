#include "game/effect_model_e06_internal.h"

extern const double D_80220B90;
extern const double D_80220B98;

s32 func_00267928(s32 value, E06ResourcePair *pairs)
{
    s32 i = 0;
    for (;;) {
        if (pairs[i].unk00 == -1 && pairs[i].unk02 == 0) {
            i--;
            if (i < 0) i = 0;
            break;
        }
        if (value == pairs[i].unk00) break;
        if (value < pairs[i].unk00) {
            i--;
            if (i < 0) i = 0;
            break;
        }
        i++;
    }
    return pairs[i].unk02;
}
/* The second logical body starts at ROM267990, before the physical owner marker.
 * Keep the complete static function beside the global lookup producer. */
static float func_00267990(Func00269F00Curve *curve, float value)
{
    E06CurveKey *keys;
    E06CurveKey *first;
    E06CurveKey *second;
    float width;
    float slope;
    float range;
    float first_time;
    float second_time;
    float first_value;
    float second_value;
    float first_slope;
    float second_slope;
    float delta2;
    float delta3;
    float inverse;
    float inverse2;
    float cubic;
    float twice;
    float triple;
    float quadratic;
    float part;
    s32 low;
    s32 high;
    s32 mid;

    if (curve == 0 || curve->count == 0) return 0.0f;
    keys = curve->keys;
    if (value < keys[0].time) {
        switch (curve->before) {
        case 0:
            return keys[0].value;
        case 1:
            if (curve->count == 1) return keys[0].value;
            first = keys;
            width = first[1].time - first[0].time;
            if (curve->mode == 1 && width != 0.0) {
                slope = (first[1].value - first[0].value) / width;
            } else {
                slope = first[0].unk0C;
            }
            return first[0].value - (first[0].time - value) * slope;
        case 2:
            if (curve->count == 1) return keys[0].value;
            width = keys[0].time;
            range = keys[curve->count - 1].time - width;
            while (value < width) value += range;
            break;
        default:
            return 0.0f;
        }
    }
    if (curve->keys[curve->count - 1].time <= value) {
        switch (curve->after) {
        case 0:
            return curve->keys[curve->count - 1].value;
        case 1:
            if (curve->count == 1) return curve->keys[0].value;
            first = &curve->keys[curve->count - 2];
            second = &curve->keys[curve->count - 1];
            width = second->time - first->time;
            if (curve->mode == 1 && width != 0.0) {
                slope = (second->value - first->value) / width;
            } else {
                slope = second->unk08;
            }
            return second->value + (value - second->time) * slope;
        case 2:
            if (curve->count == 1) return curve->keys[0].value;
            width = curve->keys[curve->count - 1].time;
            range = width - curve->keys[0].time;
            while (width < value) value -= range;
            break;
        default:
            return 0.0f;
        }
    }
    low = 1;
    high = curve->count - 1;
    while (low < high) {
        mid = (low + high) / 2;
        first = &curve->keys[mid];
        if (value < first->time) high = mid;
        else low = mid + 1;
    }
    first = &curve->keys[low];
    second_time = first->time;
    if (value == second_time) return first->value;
    second_value = first->value;
    second_slope = first->unk0C;
    first--;
    first_time = first->time;
    first_value = first->value;
    first_slope = first->unk08;
    switch (first->mode) {
    case 0:
        return first_value;
    case 1:
        return first_value + (second_value - first_value) * (value - first_time) / (second_time - first_time);
    case 2:
        inverse = D_80220B90 / (double)(second_time - first_time);
        width = value - first_time;
        delta2 = width * width;
        delta3 = delta2 * width;
        inverse2 = inverse * inverse;
        cubic = delta3 * inverse2;
        twice = (double)cubic * 2.0 * (double)inverse;
        triple = (double)delta2 * D_80220B98 * (double)inverse2;
        quadratic = delta2 * inverse;
        part = cubic - quadratic;
        return (twice - triple) * first_value + first_value
            + second_value * (-twice + triple)
            + first_slope * (part - quadratic + width)
            + second_slope * part;
    default:
        return 0.0f;
    }
}
