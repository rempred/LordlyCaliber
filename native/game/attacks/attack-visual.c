/* Independent visual programs. The original battle appender, lifecycle,
 * texture parser and image drawing callback are retained. Imported images are
 * bounded native RGBA5551 or RGBA32 frames; attack mechanics never read these settings. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
extern u8 lc_attack_settings[], lc_attack_scratch[];
void lc_original_visual_submit(int, u8 *, u8 *, u8 *);
void lc_original_visual_normalize(float *);
int lc_original_visual_travel(int, void *);
float lc_original_visual_distance(float *);
int lc_original_visual_projectile(int, void *);
void lc_effect_bind_current(void *);
void lc_effect_start(void *);
int lc_effect_push(void *);
void lc_effect_pop(int);
#define W(p, n) (*(u32 *)((u8 *)(p) + (n)))
#define H(p, n) (*(u16 *)((u8 *)(p) + (n)))
#define F(p, n) (*(float *)((u8 *)(p) + (n)))
#define GLOBAL(a) (*(u32 *)(a))

/* A self-targeted projectile has no direction. Keep a finite unit axis for its
 * orientation; retain the exact native normalization for every nonzero vector. */
void lc_attack_visual_normalize(float *vector) {
    if (vector[0] == 0.0f && vector[1] == 0.0f && vector[2] == 0.0f) {
        vector[0] = 1.0f;
        return;
    }
    lc_original_visual_normalize(vector);
}
/* Some projectiles take their own reciprocal of this length. A coincident
 * endpoint needs a finite denominator but still has a zero movement vector. */
float lc_attack_visual_distance(float *vector) {
    float length = lc_original_visual_distance(vector);
    return length == 0.0f ? 1.0f : length;
}
/* This travel task normally signals its waiting parent as distance crosses
 * 26..52 units, after two 26-unit updates. A shorter path skips that interval.
 * Signal once at the initial drawing step, then retain its native lifecycle. */
int lc_attack_visual_travel(int state, void *data) {
    int short_path = (state == 4097 && F(data, 40) < 78.0f);
    int result = lc_original_visual_travel(state, data);
    if (short_path)
        ((void (*)(void))0x8020e0d0)();
    return result;
}
/* The shared projectile initializes and takes its first step in state4096.
 * A path shorter than one step bypasses its near-impact parent notification. */
int lc_attack_visual_projectile(int state, void *data) {
    u8 *task = (u8 *)GLOBAL(0x80220fe8);
    int result = lc_original_visual_projectile(state, data);
    if (state == 4096 && F(data, 40) <= F(data, 48))
        ((void (*)(u32, int))0x8020dcac)(W(task, 20), 4099);
    return result;
}
typedef void (*VisualCallback)(int, void *);
int lc_visual_image_commands_format(u32 *, u32, int, int, int, int, int, int);
int lc_visual_origins(int, const u8 *, const u8 *, const float *, u32, u32, const float *,
                      const float *, int *);
#define pending_config (*(u8 **)(lc_attack_scratch + 64))
#define pending_source (*(u8 **)(lc_attack_scratch + 68))
#define pending_targets (*(u8 **)(lc_attack_scratch + 72))
#define pending_extra (*(u8 **)(lc_attack_scratch + 76))
static void visual_draw(void *node) {
    int words, i, count, used = 0, origins[40];
    float positions[60];
    u32 occupied = 0, opponents = 0;
    u8 *actor;
    u8 *config = (u8 *)W(node, 24);
    u32 *list = (u32 *)GLOBAL(0x800e9ba0);
    u32 *commands = (u32 *)W(node, 36);
    const int placement = H(config, 30) >> 8;
    if (placement)
        for (i = 0; i < 20; ++i) {
            actor = (u8 *)(GLOBAL(0x801ce8bc) + 0x1c4 + i * 0xf8);
            /* Accepted func_0020C478 uses +48 to distinguish an empty actor slot.
             * Never call the native position reader for an empty slot. */
            if (!W(actor, 0x48))
                continue;
            occupied |= 1U << i;
            if (W(actor, 0x40) & 0x100)
                opponents |= 1U << i;
            ((void (*)(int, float *, int))0x801f7344)(i, positions + i * 3, 0x10);
        }
    count = lc_visual_origins(placement, (u8 *)W(node, 28), (u8 *)W(node, 32), positions, occupied,
                              opponents, (float *)0x80220e20, (float *)0x80220e90, origins);
    for (i = 0; i < count; ++i) {
        words = lc_visual_image_commands_format(
            commands + used, W(node, 20) + 8, H(config, 20), H(config, 22),
            (s16)H(config, 24) + 160 - origins[i * 2],
            (s16)H(config, 26) + 120 - origins[i * 2 + 1], 2048, (H(config, 30) & 255) == 3);
        if (words > 0)
            used += words;
    }
    commands[used++] = 0xdf000000;
    commands[used] = 0;
    /* One bounded private display list avoids multiplying the battle's shared
     * command-buffer consumption for multi-target effects. */
    list[0] = 0xde000000;
    list[1] = (u32)commands;
    GLOBAL(0x800e9ba0) = (u32)(list + 2);
}

/* data fits the native task's 120-byte private storage. */
typedef struct {
    u8 *config, *source, *targets, *extra;
    u8 *pixels;
    void *drawing;
    u32 *commands;
    int frame, hold, delay;
} VisualState;

static int unpack(const u8 *in, int length, u8 *out, int size) {
    int at = 0, used = 0, b, n, d, j, value;
    while (used < size && at < length) {
        b = in[at++];
        d = 0;
        n = 0;
        value = 0;
        if (b >= 128) {
            if (at >= length)
                return 0;
            d = ((b & 7) * 256 + in[at++]) + 1;
            n = ((b >> 3) & 15) + 3;
        } else if (b >= 64) {
            n = (b & 63) + 1;
            if (at + n > length || used + n > size)
                return 0;
            for (j = 0; j < n; j++)
                out[used++] = in[at++];
            continue;
        } else if (b >= 32)
            n = (b & 31) + 2;
        else if (b >= 16) {
            if (at + 2 > length)
                return 0;
            j = in[at++];
            n = (b & 15) + ((j & 192) >> 2) + 4;
            d = ((j & 63) * 256 + in[at++]) + 1;
        } else if (b == 0) {
            if (at + 3 > length)
                return 0;
            n = in[at++] + 5;
            d = in[at++] * 256;
            d += in[at++] + 1;
        } else if (b == 1 || b == 2) {
            if (at >= length)
                return 0;
            n = in[at++] + 3;
            value = b == 1 ? 255 : 0;
        }
        if (n > size - used)
            return 0;
        for (j = 0; j < n; j++) {
            out[used] = d ? (used >= d ? out[used - d] : 0) : value;
            used++;
        }
    }
    return used == size;
}
static void hide(VisualState *s) {
    if (s->drawing)
        W(s->drawing, 12) = 0;
}
static void finish(VisualState *s) {
    u32 index;
    if (s->drawing) {
        hide(s);
        index = ((u32)s->drawing - GLOBAL(0x80220da8)) / 120;
        H(s->drawing, 0) = *(u16 *)0x80220da0;
        *(u16 *)0x80220da0 = (u16)index;
        s->drawing = 0;
    }
    if (s->pixels) {
        ((void (*)(void *))0x800712c4)(s->pixels);
        s->pixels = 0;
    }
    if (s->commands) {
        ((void (*)(void *))0x800712c4)(s->commands);
        s->commands = 0;
    }
    ((void (*)(void))0x8020dfb8)();
}
static int load_frame(VisualState *s) {
    u8 *row = s->config + 32 + s->frame * 16;
    if (!unpack(s->config + W(row, 0), W(row, 4), s->pixels, W(row, 12)))
        return 0;
    s->hold = H(row, 8);
    return s->hold > 0;
}
static void visual_task(int state, void *data) {
    VisualState *s = (VisualState *)data;
    u8 *task;
    void *drawing;
    int i, index;
    u32 *table;
    VisualCallback original;
    if (state == 256) {
        s->config = pending_config;
        s->source = pending_source;
        s->targets = pending_targets;
        s->extra = pending_extra;
        s->pixels = 0;
        s->drawing = 0;
        s->commands = 0;
        s->frame = 0;
        s->hold = 0;
        s->delay = H(s->config, 18);
        lc_effect_bind_current(s->config);
        ((void (*)(int))0x8020dc80)(4096);
        return;
    }
    if (state == 257) {
        finish(s);
        return;
    }
    if (state != 4096)
        return;
    if (s->delay) {
        s->delay--;
        return;
    }
    if (H(s->config, 12) == 0) {
        lc_effect_start(s->config);
        index = H(s->config, 14);
        table = (u32 *)0x8021fcc0;
        original = (VisualCallback)table[index];
        task = (u8 *)GLOBAL(0x80220fe8);
        GLOBAL(0x80220f68) = (u32)s->source;
        GLOBAL(0x80220f6c) = (u32)s->targets;
        if (index >= 97 && index <= 132) {
            GLOBAL(0x80220f70) = (u32)s->extra;
            *(u16 *)0x80220f74 = (u16)index;
        } else {
            *(u16 *)0x80220f70 = (u16)index;
            *(u16 *)0x8020a2d0 = (u16)index;
        }
        W(task, 4) = (u32)original;
        for (i = 0; i < 4; i++) {
            u8 *ring = (u8 *)0x80220db8 + i * 20;
            if (W(ring, 4) == (u32)task)
                W(ring, 8) = (u32)original;
        }
        original(256, data);
        if (H(task, 8) >= 256)
            original(H(task, 8), data);
        return;
    }
    if (!s->drawing) {
        s->pixels = (u8 *)((void *(*)(int))0x80070f30)(
            8 + ((H(s->config, 20) + 3) & ~3) * ((H(s->config, 30) & 255) == 3 ? 4 : 2) *
                    H(s->config, 22));
        if (!s->pixels) {
            finish(s);
            return;
        }
        s->commands = (u32 *)((void *(*)(int))0x80070f30)(
            ((H(s->config, 30) >> 8) == 1 ? 20 * 2048 + 2 : 2048 + 2) * 4);
        if (!s->commands) {
            finish(s);
            return;
        }
        if (*(u16 *)0x80220da0 == 65535) {
            finish(s);
            return;
        }
        drawing = ((void *(*)(u32, int))0x8020e570)((u32)visual_draw, 1);
        s->drawing = drawing;
        if (!drawing) {
            finish(s);
            return;
        }
        F(drawing, 8) = 1000.0f;
        W(drawing, 20) = (u32)s->pixels;
        W(drawing, 24) = (u32)s->config;
        W(drawing, 28) = (u32)s->source;
        W(drawing, 32) = (u32)s->targets;
        W(drawing, 36) = (u32)s->commands;
        if (!load_frame(s)) {
            finish(s);
            return;
        }
    } else if (!s->hold) {
        s->frame++;
        if (s->frame >= H(s->config, 28)) {
            finish(s);
            return;
        }
        if (!load_frame(s)) {
            finish(s);
            return;
        }
    }
    s->hold--;
}
void lc_attack_visual_submit(int effect, u8 *source, u8 *targets, u8 *extra) {
    int i, donor, ringIndex, owner;
    u8 *config = 0, *ring;
    u32 saved;
    u32 *table = (u32 *)0x8021fcc0;
    if (effect >= 136 && effect <= 254) {
        for (i = 1; i < 255; i++)
            if (lc_attack_settings[i * 32 + 21] == effect) {
                config = (u8 *)W(lc_attack_settings + i * 32, 24);
                break;
            }
    }
    if (!config || *(u8 *)0x800e9c12 != 1) {
        lc_original_visual_submit(config ? H(config, 14) : effect, source, targets, extra);
        return;
    }
    donor = H(config, 12) ? 135 : H(config, 14);
    pending_config = config;
    pending_source = source;
    pending_targets = targets;
    pending_extra = extra;
    ringIndex = GLOBAL(0x80220db0);
    saved = table[donor];
    table[donor] = (u32)visual_task;
    /* Scope native initialization and child resource loads from submission. */
    owner = lc_effect_push(config);
    lc_original_visual_submit(donor, source, targets, extra);
    lc_effect_pop(owner);
    table[donor] = saved;
    ring = (u8 *)0x80220db8 + ringIndex * 20;
    H(ring, 2) |= 1;
}
