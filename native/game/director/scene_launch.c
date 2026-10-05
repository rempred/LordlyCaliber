#include "scene_launch.h"
static int16_t half(uint32_t value) {
    int32_t narrowed = value & 65535U;
    if (narrowed >= 32768) narrowed -= 65536;
    return (int16_t)narrowed;
}
int lordly_scene_launch_scan(const uint32_t *words, int count, LordlySceneLaunch *launch) {
    LordlySceneLaunch next;
    uint32_t flags = 0, terminal;
    int i;
    if (!words || !launch || count < 1) return 0;
    next = *launch;
    terminal = words[count - 1];
    next.resource_class = (terminal & 0xff000000U) == 0xff000000U ? terminal & 255 : 0;
    for (i = 0; i < count; ++i) {
        if (words[i] == 0x80000008U) { flags |= 4; next.capture = 1; }
        if (words[i] == 0x80000007U || words[i] == 0x80000006U) {
            if (i + 1 == count) return 0;
            if (words[i] == 0x80000007U) { flags |= 2; next.image_row = half(words[i + 1]); }
            else { flags |= 1; next.environment = half(words[i + 1]); }
        }
        if (flags == 4 || words[i] == 0x80000001U) break;
    }
    *launch = next;
    return 1;
}
