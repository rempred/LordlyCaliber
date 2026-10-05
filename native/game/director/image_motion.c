#include "image_motion.h"
#include <math.h>
/* Interpret target word bits without implementation-defined unsigned narrowing. */
static double signed_word(uint32_t word) {
    return word < 0x80000000U ? (double)word : (double)word - 4294967296.0;
}
int lordly_image_motion_start(LordlyImageMotion *motion, LordlyImageZoom *zoom,
                              const int32_t args[7], float current_x, float current_y) {
    LordlyImageMotion next;
    LordlyImageZoom z = *zoom;
    float x = args[0] == -1 && args[1] == -1 ? current_x : (float)args[0];
    float y = args[0] == -1 && args[1] == -1 ? current_y : (float)args[1];
    x = -x + (float)args[3];
    y = -y + (float)args[4];
    next.step_x = x > 1.f || x < -1.f ? x / (float)args[6] : 0.f;
    next.step_y = y > 1.f || y < -1.f ? y / (float)args[6] : 0.f;
    if (!isfinite(x) || !isfinite(y) || !isfinite(next.step_x) || !isfinite(next.step_y))
        return 0;
    next.duration = args[6];
    next.elapsed = 0;
    if (!args[5]) {
        if (z.state == 3) {
            z.state = 2; z.elapsed = 0; z.duration = (uint32_t)args[6];
        } else if (z.state == 1) {
            z.state = 2; z.elapsed = z.duration - z.elapsed;
        }
    } else {
        if (z.state == 4) {
            z.state = 1; z.elapsed = 0; z.duration = (uint32_t)args[6];
        } else if (z.state == 2) {
            z.state = 1; z.elapsed = z.duration - z.elapsed;
        }
    }
    *motion = next; *zoom = z;
    return 1;
}
int lordly_image_zoom_advance(LordlyImageZoom *zoom, float *scale,
                               const double constants[4]) {
    LordlyImageZoom z = *zoom;
    float value = *scale;
    if (z.state == 1 || z.state == 2) {
        double ratio;
        if (!z.duration) return 0;
        ratio = signed_word(z.elapsed) * constants[z.state == 1 ? 0 : 2];
        ratio = ratio / signed_word(z.duration);
        value = (float)(z.state == 1 ? constants[1] - ratio : constants[3] + ratio);
        if (!isfinite(value)) return 0;
        if (z.elapsed == z.duration) z.state += 2;
        ++z.elapsed;
    }
    *zoom = z; *scale = value;
    return 1;
}
int lordly_image_motion_advance(LordlyImageMotion *motion, LordlyImageZoom *zoom,
                                float *x, float *y, float *scale, const double constants[4]) {
    LordlyImageMotion m = *motion;
    LordlyImageZoom z = *zoom;
    float nx = *x, ny = *y, ns = *scale;
    int present = m.elapsed != (uint32_t)m.duration;
    if (!lordly_image_zoom_advance(&z, &ns, constants)) return -1;
    if (present) {
        ++m.elapsed;
        nx += m.step_x; ny += m.step_y;
        if (!isfinite(nx) || !isfinite(ny)) return -1;
    }
    *motion = m; *zoom = z; *x = nx; *y = ny; *scale = ns;
    return present;
}
