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
u8 *lc_audio_base(u8 *);
void lc_audio_start(u8 *,int);
int lc_effect_push(void *);
void lc_effect_pop(int);
void lc_strike_visual_list(u8 *);
void lc_strike_share_first_impact(const u8 *, u32);
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
#define pending_repeat (*(u32 *)(lc_attack_scratch + 80))
#define pending_donor (*(u32 *)(lc_attack_scratch + 84))
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
    int frame, hold, delay, strike, donor;
} VisualState;

/* The retail decorative scripts begin drawing without an impact command. */
static int native_without_contact(int effect) { return effect >= 93 && effect <= 96; }
static void frame_impacts(const u8 *targets,int strike) {
    int i;
    for (i = 0; i < (strike ? 1 : 20) && targets[i] != 255; i++) {
        if (targets[i] < 20)
            ((void (*)(int,int))0x801c893c)(targets[i],10);
    }
}
/* func_00279810 stores two native bytes, then waits for its projectile child.
 * Its first resume is the arrival cue even though it never calls the battle. */
typedef struct {
    u8 native[4];
    const u8 *targets;
    int strike;
} ProjectileCueState;
static void projectile_cue(int state,void *data) {
    ProjectileCueState *cue=(ProjectileCueState *)data;
    if(state==0x1001) frame_impacts(cue->targets,cue->strike);
    ((VisualCallback)0x80225080)(state,data);
}

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
        s->delay = s->config ? H(s->config, 18) : 0;
        s->donor = s->config ? H(s->config, 14) : pending_donor;
        s->strike = pending_repeat;
        /* Ordinary spell results are queued one battle tick after submission.
         * First-frame impacts must wait until they are available. */
        if (!s->strike && !s->delay &&
            ((s->config && H(s->config, 12)) || (s->extra && native_without_contact(s->donor))))
            s->delay = 1;
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
    if (!s->config || H(s->config, 12) == 0) {
        const u8 *firstFrameTargets = s->extra && native_without_contact(s->donor) ? s->targets : 0;
        int strike = s->strike;
        if (s->config) { lc_effect_start(s->config); lc_audio_start(s->config,s->source[0]); }
        index = s->donor;
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
            GLOBAL(0x8020a2d0) = index;
        }
        if (firstFrameTargets && index == 94) {
            ProjectileCueState *cue=(ProjectileCueState *)data;
            cue->targets=firstFrameTargets;
            cue->strike=strike;
            firstFrameTargets=0;
            original=projectile_cue;
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
        if (firstFrameTargets)
            frame_impacts(firstFrameTargets,strike);
        return;
    }
    if (!s->drawing) {
        lc_audio_start(s->config,s->source[0]);
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
        /* Imported sequences use their first visible frame as the impact.
         * Repeated playback passes a single victim. Ordinary playback can
         * present the same frame over several victims simultaneously. */
        frame_impacts(s->targets,s->strike);
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
/* Ordinary spell programs are authored for one simultaneous group. Serialize
 * complete single-target programs instead of feeding them duplicate actor IDs.
 * The six summon families already have a native per-impact driver. */
typedef struct {
    u8 *source, *targets, *extra, *config;
    u8 target[2], flag[2];
    u32 donor, ordinal, delay;
    void *child;
} RepeatVisual;
static void repeat_visual(int state,void *data) {
    RepeatVisual *s=(RepeatVisual *)data;
    u8 *task=(u8 *)GLOBAL(0x80220fe8);
    VisualCallback callback;
    int owner;
    if(state==256) {
        s->source=pending_source; s->targets=pending_targets;
        s->extra=pending_extra; s->config=pending_config;
        s->donor=pending_donor; s->ordinal=0; s->child=0; s->delay=2;
        ((void (*)(int))0x8020dc80)(4096);
        return;
    }
    if(state==257) { ((void (*)(void))0x8020dfb8)(); return; }
    if(state!=4096)return;
    /* Battle result commands follow the visual command by one stream tick. */
    if(s->delay) { --s->delay; return; }
    if(s->child && W(s->child,4) && W(s->child,20)==(u32)task)return;
    s->child=0;
    if(s->ordinal>=18 || s->targets[s->ordinal]==255) {
        ((void (*)(void))0x8020dfb8)(); return;
    }
    s->target[0]=s->targets[s->ordinal]; s->target[1]=255;
    s->flag[0]=s->extra?s->extra[s->ordinal]:0; s->flag[1]=255;
    ++s->ordinal;
    GLOBAL(0x80220f68)=(u32)s->source;
    GLOBAL(0x80220f6c)=(u32)s->target;
    if(s->donor>=97 && s->donor<=132) {
        GLOBAL(0x80220f70)=(u32)s->flag;
        *(u16 *)0x80220f74=(u16)s->donor;
    } else {
        *(u16 *)0x80220f70=(u16)s->donor;
        GLOBAL(0x8020a2d0)=s->donor;
    }
    callback=(VisualCallback)((u32 *)0x8021fcc0)[s->donor];
    if(s->config || native_without_contact(s->donor)) {
        pending_config=s->config; pending_source=s->source;
        pending_targets=s->target; pending_extra=s->flag;
        pending_donor=s->donor;
        pending_repeat=1;
        callback=visual_task;
    }
    owner=lc_effect_push(s->config);
    s->child=((void *(*)(VisualCallback,void *))0x8020dd60)(callback,task);
    lc_effect_pop(owner);
    pending_repeat=0;
}
static int native_strikes(int effect) {
    return (effect>=5 && effect<=7) || (effect>=13 && effect<=15) ||
           (effect>=21 && effect<=23) || (effect>=29 && effect<=31) ||
           (effect>=36 && effect<=38) || (effect>=44 && effect<=46);
}
static int native_first_victim_only(int effect) {
    /* These normal-combat scripts only deliver a primary contact to the first
     * victim, even when the battle supplies multiple occupied targets. A
     * separate caster return (for example Life Drain) is not a victim hit. */
    switch(effect) {
    case 0: case 4: case 8: case 16: case 20: case 24: case 28:
    case 39: case 43: case 51: case 58: case 65: case 72: case 79: case 86:
    case 97: case 98: case 99: case 102: case 103: case 104:
    case 106: case 108: case 110: case 111:
    case 125: case 126: case 127: case 128: case 129: case 130: case 131: case 132:
        return 1;
    }
    return 0;
}
static u32 native_covered_targets(int effect,int anchor) {
    float positions[4][3];
    u32 covered=1U<<anchor;
    int mode,i,actor;
    if(native_first_victim_only(effect))return covered;
    if(effect!=42 && effect!=68 && effect!=80)return 0xfffff;
    /* The fixed footprints in func_00250C24, func_00253D18 and
     * func_002550A4 use these same world-grid queries. Effect80 visits only
     * mode0, while the other two visit both sets of offsets. */
    for(mode=0;mode<(effect==80?1:2);mode++) {
        ((void (*)(u8,float *,int))0x801f79bc)(anchor,positions[0],mode);
        for(i=0;i<4;i++) {
            actor=((s16 (*)(float *))0x801f7738)(positions[i]);
            if(actor>=0 && actor<20)covered|=1U<<actor;
        }
    }
    return covered;
}
void lc_attack_visual_submit(int effect, u8 *source, u8 *targets, u8 *extra) {
    int i, donor, ringIndex, owner, repeated, imported;
    u8 *config = 0, *ring;
    u32 saved;
    u32 *table = (u32 *)0x8021fcc0;
    if (effect >= 136 && effect <= 254) {
        for (i = 1; i < 255; i++)
            if (lc_attack_settings[i * 32 + 21] == effect) {
                config = lc_audio_base((u8 *)W(lc_attack_settings + i * 32, 24));
                break;
            }
    }
    imported=config && H(config,12);
    donor=config?H(config,14):effect;
    repeated=extra && targets[0]!=255 && (extra[0]&128);
    if(!repeated && extra && targets[0]<20 && targets[1]!=255 && *(u8 *)0x800e9c12==1 && !imported)
        lc_strike_share_first_impact(targets,0xfffff & ~native_covered_targets(donor,targets[0]));
    if(repeated) {
        extra[0]&=127;
        lc_strike_visual_list(targets);
        if(imported || !native_strikes(donor) || *(u8 *)0x800e9c12!=1) {
            if(imported)donor=135;
            if(*(u8 *)0x800e9c12!=1)donor=135;
            pending_config=config; pending_source=source;
            pending_targets=targets; pending_extra=extra; pending_donor=donor;
            ringIndex=GLOBAL(0x80220db0);
            saved=table[donor]; table[donor]=(u32)repeat_visual;
            owner=lc_effect_push(config);
            lc_original_visual_submit(donor,source,targets,extra);
            lc_effect_pop(owner);
            table[donor]=saved;
            ring=(u8 *)0x80220db8+ringIndex*20;
            H(ring,2)|=1;
            return;
        }
    }
    if ((!config && !(extra && native_without_contact(donor))) || *(u8 *)0x800e9c12 != 1) {
        lc_original_visual_submit(donor, source, targets, extra);
        return;
    }
    if (imported) donor = 135;
    pending_config = config;
    pending_source = source;
    pending_targets = targets;
    pending_extra = extra;
    pending_donor = donor;
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
