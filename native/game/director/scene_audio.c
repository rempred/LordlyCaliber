/* Scene handlers ROM28308C..28364C, at RAM802270BC..8022767C.
 * Portable reconstruction; not an accepted matching-C claim. */
#include "scene_audio.h"
static void bus(LordlySceneAudio *s, int mask, uint32_t value, LordlyAudioSink sink, void *user) {
    if (mask & 1)
        s->gains[0] = (uint16_t)value;
    if (mask & 2)
        s->gains[1] = (uint16_t)value;
    sink(user, LORDLY_AUDIO_BUS_GAIN, mask, value, 0);
}
int lordly_audio_command(LordlySceneAudio *s, const LordlyAudioInputs *in, uint32_t op,
                         const uint32_t a[4], LordlyAudioSink sink, void *user) {
    int context = (int32_t)a[0], first, end, i;
    if (context < 0 || context > 7)
        return 0;
    switch (op) {
    case 0x6e:
        if (context == 7) {
            if (a[1] == 1)
                sink(user, LORDLY_AUDIO_RESET, -1, 0, 0);
        } else if (a[1] == 0) {
            if (context < 3 && in->voices[context]) {
                if (!in->scene_present)
                    return 0;
                s->pending[context] = (uint8_t)a[2];
                sink(user, LORDLY_AUDIO_RELEASE, context, 0, 0);
            } else if (in->route == 0)
                sink(user, LORDLY_AUDIO_PENDING, context, a[2], 5);
            else
                sink(user, LORDLY_AUDIO_REGISTER, context, a[2], 0);
        } else if (a[1] == 1)
            sink(user, LORDLY_AUDIO_RELEASE, context, 0, 0);
        else if (context == 5)
            sink(user, LORDLY_AUDIO_DIRECT, context, a[2], 0);
        else
            sink(user, LORDLY_AUDIO_PENDING, context, a[2], 10);
        return 1;
    case 0x6f:
        sink(user, context == 7 ? LORDLY_AUDIO_BUS_GAIN_PENDING : LORDLY_AUDIO_GAIN,
             context == 7 ? 0 : context, a[1], 0);
        return 1;
    case 0x70:
        if (in->controller_suppressed == 1)
            return 1;
        first = context == 7 ? 0 : context;
        end = context == 7 ? 7 : context + 1;
        for (i = first; i < end; ++i)
            sink(user, LORDLY_AUDIO_CONTROL, i, a[1], 0);
        return 1;
    case 0x71:
        first = context == 7 ? 0 : context;
        end = context == 7 ? 7 : context + 1;
        for (i = first; i < end; ++i) {
            if (a[1] != UINT32_MAX)
                sink(user, LORDLY_AUDIO_TIME, i, a[1], 0);
            if (a[2] != UINT32_MAX)
                sink(user, LORDLY_AUDIO_PAN, i, a[2], 0);
            if (a[3] != UINT32_MAX)
                sink(user, LORDLY_AUDIO_PITCH, i, (uint32_t)((int32_t)a[3] / 128), 0);
        }
        return 1;
    case 0xb4: {
        int32_t half = (int32_t)a[2] / 2;
        if (!s->envelope_present) {
            uint32_t level;
            if (context >= 2 && context <= 6)
                level = 32767;
            else {
                if (a[1] > 1)
                    return 0;
                level = s->gains[a[1]];
            }
            if ((int32_t)a[2] > 0)
                level -= (uint32_t)half * a[3];
            s->level = (int16_t)(uint16_t)level;
            s->envelope_present = 1;
        }
        s->step = (int16_t)half;
        s->remaining = (int16_t)a[3];
        s->group = (int16_t)context;
        return 1;
    }
    default:
        return 0;
    }
}
void lordly_audio_advance(LordlySceneAudio *s, const LordlyAudioInputs *in, LordlyAudioSink sink,
                          void *user) {
    int i;
    if (in->scene_present)
        for (i = 0; i < 3; ++i) {
            if (!s->pending[i] || in->voices[i])
                continue;
            sink(user, LORDLY_AUDIO_REGISTER, i, s->pending[i], 0);
            bus(s, 2, 32767, sink, user);
            if (s->envelope_present) {
                s->envelope_present = 0;
                bus(s, 1, 32767, sink, user);
            }
            s->pending[i] = 0;
        }
    if (s->envelope_present && s->remaining > 0) {
        int level = s->level + s->step;
        --s->remaining;
        s->level = (int16_t)(level < 0 ? 0 : level > 32767 ? 32767 : level);
        bus(s, s->group == 7 ? 3 : s->group < 2 ? 2 : 1, (uint32_t)s->level, sink, user);
    }
}
