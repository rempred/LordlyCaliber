/* Eight complete constructor bodies, independently reconstructed from ROM.
 * Explicit inline preserves the caller's observed embedded LP-filter setup
 * while retaining the callable global helper under the legacy profile. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct L4Filter {
    void *input, *pull, *param;
    short field0c, field0e;
    int field10;
} L4Filter;
typedef struct L4Lowpass {
    short cutoff, gain;
    int field04;
    short coefficients[16];
    void *state;
    int first;
} L4Lowpass;
typedef struct L4Resample {
    L4Filter filter;
    void *state;
    float ratio;
    int field1c, field20, first, field28, field2c, field30;
} L4Resample;
typedef struct L4Delay {
    u32 input, output;
    short field08, field0a, field0c, field0e;
    float rate, ratio;
    int field18;
    float depth;
    L4Lowpass *lowpass;
    L4Resample *resample;
} L4Delay;
typedef struct L4Effect {
    L4Filter filter;
    short *buffer, *cursor;
    u32 length;
    L4Delay *delays;
    u8 count, pad25[3];
    void *paramHandler;
} L4Effect;
typedef struct L4Config {
    u8 field00[0x18];
    int rate;
    u8 effectType, pad1d[3];
    int *effectParams;
} L4Config;
typedef struct L4Envelope {
    L4Filter filter;
    void *state;
    short field18, field1a, field1c, field1e, field20, field22;
    short field24, field26, field28, field2a, field2c, field2e;
    int field30, field34, field38, field3c, field40, field44, field48;
} L4Envelope;
typedef struct L4Load {
    L4Filter filter;
    void *state14, *state18;
    int field1c, field20, field24, field28, field2c;
    void *dma;
    int field34, field38, field3c, first, field44;
} L4Load;
typedef struct L4Bus {
    L4Filter filter;
    int count, capacity;
    void *sources;
} L4Bus;
typedef struct L4Save { L4Filter filter; void *destination; int first; } L4Save;

extern void func_0001eaf0(void *, void *, void *, int);
extern void *bump_alloc(void *, int, void *, int, int);
extern void func_0001ef30(void), func_0001f2a4(void), func_0001f2b8(void);
extern void func_0001de40(void), func_0001e340(void);
extern void func_0001d220(void), func_0001da00(void);
extern void func_0001ec80(void), func_0001ee6c(void);
extern void func_0001dd30(void), func_0001de0c(void);
extern void func_0001eb10(void), func_0001ec50(void);
extern void func_0001fad0(void), func_0001fb78(void);
extern int data_800AA460[], data_800AA4C8[], data_800AA550[];
extern int data_800AA578[], data_800AA5A0[], data_800AA5C8[];

inline void func_0001c9f0(L4Lowpass *filter)
{
    int cutoff;
    short coefficient;
    int i;
    double value, multiplier;

    filter->first = 1;
    cutoff = (filter->cutoff * 0x4000) >> 15;
    coefficient = cutoff;
    filter->gain = 0x4000 - cutoff;
    for (i = 7; i >= 0; i--)
        filter->coefficients[i] = 0;
    filter->coefficients[8] = coefficient;
    value = coefficient / 16384.0;
    multiplier = value;
    for (i = 9; i < 16; i++) {
        value *= multiplier;
        filter->coefficients[i] = value * 16384.0;
    }
}

void func_0001ca94(L4Effect *effect, L4Config *config, void *heap)
{
    int *params;
    u16 i, j, p;
    L4Delay *delay;

    func_0001eaf0(&effect->filter, 0, func_0001f2a4, 5);
    effect->filter.pull = func_0001ef30;
    effect->paramHandler = func_0001f2b8;
    switch (config->effectType) {
    case 1: params = data_800AA460; break;
    case 2: params = data_800AA4C8; break;
    case 5: params = data_800AA550; break;
    case 3: params = data_800AA578; break;
    case 4: params = data_800AA5A0; break;
    case 6: params = config->effectParams; break;
    default: params = data_800AA5C8; break;
    }
    effect->count = params[0];
    effect->length = params[1];
    effect->delays = bump_alloc(0, 0, heap, effect->count, sizeof(L4Delay));
    effect->buffer = bump_alloc(0, 0, heap, effect->length, sizeof(short));
    effect->cursor = effect->buffer;
    p = 2;
    for (i = 0; i < effect->length; i++)
        effect->buffer[i] = 0;
    for (j = 0; j < effect->count; j++) {
        delay = &effect->delays[j];
        delay->input = params[p++];
        delay->output = params[p++];
        delay->field0a = params[p++];
        delay->field08 = params[p++];
        delay->field0c = params[p++];
        if (params[p]) {
            delay->rate = 2.0 * ((float)params[p++] / 1000.0f) / config->rate;
            delay->depth = ((float)params[p++] / 173123.404906676) *
                           (delay->output - delay->input);
            delay->ratio = 1.0f;
            delay->field18 = 0;
            delay->resample = bump_alloc(0, 0, heap, 1, sizeof(L4Resample));
            delay->resample->state = bump_alloc(0, 0, heap, 1, 32);
            delay->resample->field20 = 0;
            delay->resample->first = 1;
        } else {
            delay->resample = 0;
            p += 2;
        }
        if (params[p]) {
            delay->lowpass = bump_alloc(0, 0, heap, 1, sizeof(L4Lowpass));
            delay->lowpass->state = bump_alloc(0, 0, heap, 1, 8);
            delay->lowpass->cutoff = params[p++];
            func_0001c9f0(delay->lowpass);
        } else {
            delay->lowpass = 0;
            p++;
        }
    }
}

void func_0001cf2c(L4Envelope *envelope, void *heap)
{
    func_0001eaf0(envelope, func_0001de40, func_0001e340, 4);
    envelope->state = bump_alloc(0, 0, heap, 1, 80);
    envelope->field38 = 1;
    envelope->field48 = 0;
    envelope->field1a = 1;
    envelope->field28 = 1;
    envelope->field2e = 1;
    envelope->field1c = 1;
    envelope->field1e = 1;
    envelope->field20 = 0;
    envelope->field22 = 0;
    envelope->field26 = 1;
    envelope->field24 = 0;
    envelope->field26 = 1;
    envelope->field24 = 0;
    envelope->field30 = 0;
    envelope->field34 = 0;
    envelope->field18 = 0;
    envelope->field3c = 0;
    envelope->field40 = 0;
    envelope->field44 = 0;
}

void func_0001cfe4(L4Load *load, void *(*createDma)(int *), void *heap)
{
    func_0001eaf0(load, func_0001d220, func_0001da00, 0);
    load->state14 = bump_alloc(0, 0, heap, 1, 32);
    load->state18 = bump_alloc(0, 0, heap, 1, 32);
    load->dma = createDma(&load->field34);
    load->field3c = 0;
    load->first = 1;
    load->field44 = 0;
}

void func_0001d094(L4Resample *resample, void *heap)
{
    func_0001eaf0(resample, func_0001ec80, func_0001ee6c, 1);
    resample->state = bump_alloc(0, 0, heap, 1, 32);
    resample->field20 = 0;
    resample->first = 1;
    resample->field30 = 0;
    resample->field1c = 0;
    resample->field28 = 0;
    resample->field2c = 0;
    resample->ratio = 1.0f;
}

void func_0001d120(L4Bus *bus, void *sources, int capacity)
{
    func_0001eaf0(bus, func_0001dd30, func_0001de0c, 6);
    bus->count = 0;
    bus->capacity = capacity;
    bus->sources = sources;
}

void func_0001d17c(L4Bus *bus, void *sources, int capacity)
{
    func_0001eaf0(bus, func_0001eb10, func_0001ec50, 7);
    bus->count = 0;
    bus->capacity = capacity;
    bus->sources = sources;
}

void func_0001d1d8(L4Save *save)
{
    func_0001eaf0(save, func_0001fad0, func_0001fb78, 3);
    save->destination = 0;
    save->first = 1;
}






