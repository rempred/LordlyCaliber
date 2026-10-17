/* Seven-body envelope producer ROM1DE40..1EAF0, derived from ROM.
 * The original power table remains under its original writable-data owner. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct L4Command { u32 word0, word1; } L4Command;
typedef struct L4Filter L4Filter;
struct L4Filter {
    L4Filter *input;
    L4Command *(*pull)(void *, short *, int, int, L4Command *);
    int (*param)(void *, int, void *);
    short field0c, field0e;
    int field10;
};
typedef struct L4Param L4Param;
struct L4Param {
    L4Param *next;
    int time;
    short type, field0a;
    union {
        struct { int value, time; } general;
        struct { int pitch; short volume; u8 pan, effect; int samples; void *wave; } start;
        void *pointer;
    } data;
};
typedef struct L4Envelope {
    L4Filter filter;
    void *state;
    short pan, volume, left, right, dry, wet;
    u16 leftFraction;
    short leftRate, leftTarget;
    u16 rightFraction;
    short rightRate, rightTarget;
    int elapsed, duration, first;
    L4Param *head, *tail;
    int field44, playing;
} L4Envelope;
typedef struct L4Voice { char fields00[0xd8]; int fieldd8; } L4Voice;
extern short powerTable[128];
extern void *alGlobals;
extern void _freePVoice(void *, void *);
extern void __freeParam(L4Param *);
extern u32 osVirtualToPhysical(void *);

#define SET_BUFFER(packet, flags, in, out, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x08000000 | ((flags) << 16) | ((in) & 0xffff); \
    command->word1 = ((out) << 16) | ((length) & 0xffff); \
}
#define SET_VOLUME(packet, flags, volume, rate, fraction) { \
    L4Command *command = (packet); \
    command->word0 = 0x09000000 | ((flags) << 16) | ((volume) & 0xffff); \
    command->word1 = ((u16)(rate) << 16) | ((fraction) & 0xffff); \
}
L4Command *func_0001e414(void *, short *, short *, int, int, L4Command *);
short func_0001e7ec(double, double, int, u16 *);
float func_0001ea6c(float, int, short, u16);

L4Command *func_0001de40(void *filter, short *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Envelope *env = filter;
    L4Command *commands = inputCommands;
    short inputBuffer, outputBuffer;
    int time = offset;
    outputBuffer = 0;
    inputBuffer = 0;
    while (env->head) {
        /* Keep the old cursor separate from the new event time. */
        int previous = time;
        int delta;
        time = env->head->time;
        delta = time - previous;
        if (delta > count)
            break;
        switch (env->head->type) {
        case 13: {
            L4Param *param = env->head;
            L4Filter *control = &env->filter;
            if (param->field0a)
                env->filter.param(env, 8, 0);
            env->filter.param(env, 5, param->data.start.wave);
            env->filter.param(env, 9, 0);
            env->first = 1;
            env->elapsed = 0;
            env->duration = param->data.start.samples;
            env->volume = (param->data.start.volume * param->data.start.volume) >> 15;
            env->pan = param->data.start.pan;
            env->dry = powerTable[param->data.start.effect];
            env->wet = powerTable[127 - param->data.start.effect];
            if (param->data.start.samples) {
                env->left = 1;
                env->right = 1;
            } else {
                env->left = (env->volume * powerTable[env->pan]) >> 15;
                env->right = (env->volume * powerTable[127-env->pan]) >> 15;
            }
            if (control->input)
                control->input->param(control->input, 7, (void *)param->data.start.pitch);
            break;
        }
        case 11:
        case 12:
        case 16:
            commands = func_0001e414(env, &inputBuffer, &outputBuffer, delta, offset, commands);
            if (env->elapsed >= env->duration) {
                env->leftTarget = (env->volume * powerTable[env->pan]) >> 15;
                env->rightTarget = (env->volume * powerTable[127-env->pan]) >> 15;
                env->elapsed = env->duration;
                env->left = env->leftTarget;
                env->right = env->rightTarget;
            } else {
                env->left = func_0001ea6c(env->left, env->elapsed, env->leftRate, env->leftFraction);
                env->right = func_0001ea6c(env->right, env->elapsed, env->rightRate, env->rightFraction);
            }
            if (!env->left) env->left = 1;
            if (!env->right) env->right = 1;
            if (env->head->type == 12)
                env->pan = env->head->data.general.value;
            if (env->head->type == 11) {
                env->elapsed = 0;
                env->volume = (env->head->data.general.value * env->head->data.general.value) >> 15;
                env->duration = env->head->data.general.time;
            }
            if (env->head->type == 16) {
                env->dry = powerTable[env->head->data.general.value];
                env->wet = powerTable[127-env->head->data.general.value];
            }
            env->first = 1;
            break;
        case 14: {
            L4Param *param = env->head;
            if (param->field0a)
                env->filter.param(env, 8, 0);
            env->filter.param(env, 5, param->data.pointer);
            env->filter.param(env, 9, 0);
            break;
        }
        case 15:
            commands = func_0001e414(env, &inputBuffer, &outputBuffer, delta, offset, commands);
            env->filter.param(env, 4, 0);
            break;
        case 0: {
            /* Capture both owners before clearing the voice field. */
            void *synth = alGlobals;
            L4Param *param = env->head;
            ((L4Voice *)param->data.pointer)->fieldd8 = 0;
            _freePVoice(synth, param->data.pointer);
            break;
        }
        default:
            commands = func_0001e414(env, &inputBuffer, &outputBuffer, delta, offset, commands);
            env->filter.param(env, env->head->type, env->head->data.pointer);
            break;
        }
        count -= delta;
        outputBuffer += delta << 1;
        {
            L4Param *param = env->head;
            env->head = param->next;
            if (!env->head)
                env->tail = 0;
            __freeParam(param);
        }
    }
    commands = func_0001e414(env, &inputBuffer, &outputBuffer, count, offset, commands);
    if (env->elapsed > env->duration)
        env->elapsed = env->duration;
    return commands;
}

int func_0001e340(void *filter, int selector, void *value)
{
    L4Envelope *env = filter;
    L4Filter *source;
    switch (selector) {
    case 1: env->filter.input = value; break;
    case 3:
        if (env->tail) env->tail->next = value;
        else env->head = value;
        env->tail = value;
        break;
    case 4:
        source = env->filter.input;
        env->first = 1;
        env->playing = 0;
        env->volume = 1;
        if (source)
            source->param(source, 4, value);
        break;
    case 9:
        source = env->filter.input;
        env->playing = 1;
        if (source)
            source->param(source, 9, value);
        break;
    default:
        filter = ((L4Filter *)filter)->input;
        if (filter)
            ((L4Filter *)filter)->param(filter, selector, value);
        break;
    }
    return 0;
}

L4Command *func_0001e414(void *filter, short *input, short *output,
                         int count, int offset, L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    L4Envelope *env = filter;
    L4Filter *source = env->filter.input;
    if (env->playing != 1 || count == 0)
        return commands;
    commands = source->pull(source, input, count, offset, commands);
    SET_BUFFER(commands++, 0, *input, *output+0x440, count << 1);
    SET_BUFFER(commands++, 8, *output+0x580, *output+0x6c0, *output+0x800);
    if (env->first) {
        env->first = 0;
        env->leftTarget = (env->volume * powerTable[env->pan]) >> 15;
        env->leftRate = func_0001e7ec(env->left, env->leftTarget, env->duration, &env->leftFraction);
        env->rightTarget = (env->volume * powerTable[127-env->pan]) >> 15;
        env->rightRate = func_0001e7ec(env->right, env->rightTarget, env->duration, &env->rightFraction);
        SET_VOLUME(commands++, 6, env->left, 0, 0);
        SET_VOLUME(commands++, 4, env->right, 0, 0);
        SET_VOLUME(commands++, 2, env->leftTarget, env->leftRate, env->leftFraction);
        SET_VOLUME(commands++, 0, env->rightTarget, env->rightRate, env->rightFraction);
        SET_VOLUME(commands++, 8, env->dry, 0, env->wet);
        {
            L4Command *command = commands++;
            command->word0 = 0x03090000;
            command->word1 = osVirtualToPhysical(env->state);
        }
    } else {
        {
            L4Command *command = commands++;
            command->word0 = 0x03080000;
            command->word1 = osVirtualToPhysical(env->state);
        }
    }

    *input += count << 1;
    env->elapsed += count;
    return commands;
}

inline double func_0001e6e8(double value, int *exponent)
{
    double magnitude;
    *exponent = 0;
    if (value == 0.0)
        return value;
    magnitude = value > 0.0 ? value : -value;
    while (magnitude >= 1.0) {
        magnitude *= 0.5;
        (*exponent)++;
    }
    while (magnitude < 0.5) {
        magnitude *= 2.0;
        (*exponent)--;
    }
    return value > 0.0 ? magnitude : -magnitude;
}

double float_ldexp_d(double value, int exponent)
{
    if (exponent)
        value *= 1 << exponent;
    return value;
}

short func_0001e7ec(double initial, double target, int count, u16 *fraction)
{
    double reciprocal = 1.0 / count;
    double ratio, result, logarithm, base;
    int exponent, power;
    short integer;
    if (count == 0) {
        if (initial <= target) {
            *fraction = 0xffff;
            return 0x7fff;
        } else {
            *fraction = 0;
            return 0;
        }
    }
    if (target < 1.0) target = 1.0;
    if (initial <= 0.0) initial = 1.0;
    {
        /* Keep initialization after the early returns and clamps. */
        double table[8] = {-0.912537, -0.752072, -0.607683, -0.476438,
                           -0.356144, -0.245112, -0.142019, -0.045804};
        power = reciprocal * 1073741824.0;
        /* Direct argument and a separate power base preserve KMC lifetimes. */
        ratio = func_0001e6e8(target / initial, &exponent);
        logarithm = table[(int)(ratio * 16.0)-8] + exponent;
        logarithm *= 0.6931471805599453;
        logarithm /= 1073741824.0;
        base = 1.0 + logarithm;
        result = 1.0;
        while (power) {
            if (power & 1) result *= base;
            power >>= 1;
            base *= base;
        }
        result *= result;
        result *= result;
        result *= result;
        integer = result;
        *fraction = (int)((result - (float)integer) * 65535.0);
        return integer;
    }
}

float func_0001ea6c(float initial, int count, short integer, u16 fraction)
{
    float ratio, result;
    int i;
    count >>= 3;
    if (!count) return initial;
    ratio = ((float)(integer << 16) + fraction) / 65536.0f;
    result = 1.0f;
    for (i = 0; i < 32; i++) {
        if (count & 1) result *= ratio;
        count >>= 1;
        if (!count) return initial * result;
        ratio *= ratio;
    }
    return initial * result;
}
