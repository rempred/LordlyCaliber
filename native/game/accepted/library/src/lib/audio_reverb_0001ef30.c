/* Complete eight-body producer ROM1EF30..1FAD0, derived from ROM.
 * Unreferenced original writable storage remains with its original owner. */
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
    int field1c;
    float fraction;
    int first, field28, field2c, field30;
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
typedef struct L4Synth { char fields00[0x44]; int outputRate; } L4Synth;
extern L4Synth *alGlobals;
extern u32 osVirtualToPhysical(void *);
extern void _init_lpfilter(L4Lowpass *);

#define L4_SET_BUFFER(packet, in, out, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x08000000 | ((in) & 0xFFFF); \
    command->word1 = ((u16)(out) << 16) | ((length) & 0xFFFF); \
}
#define L4_LOAD_BUFFER(packet, address) { \
    L4Command *command = (packet); \
    command->word0 = 0x04000000; \
    command->word1 = osVirtualToPhysical(address); \
}
#define L4_SAVE_BUFFER(packet, address) { \
    L4Command *command = (packet); \
    command->word0 = 0x06000000; \
    command->word1 = osVirtualToPhysical(address); \
}
#define L4_MIX_BUFFER(packet, gain, in, out) { \
    L4Command *command = (packet); \
    command->word0 = 0x0C000000 | ((gain) & 0xFFFF); \
    command->word1 = ((u16)(in) << 16) | ((out) & 0xFFFF); \
}
#define L4_CLEAR_BUFFER(packet, address, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x02000000 | ((address) & 0xFFFFFF); \
    command->word1 = (length); \
}
#define L4_MOVE_BUFFER(packet, in, out, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x0A000000 | ((in) & 0xFFFFFF); \
    command->word1 = ((u16)(out) << 16) | ((length) & 0xFFFF); \
}

L4Command *func_0001f4a8(L4Effect *, L4Delay *, int, int, L4Command *);
L4Command *func_0001f6c0(L4Effect *, short *, int, int, L4Command *);
L4Command *func_0001f830(L4Effect *, short *, int, int, L4Command *);
L4Command *func_0001f9a0(L4Lowpass *, int, int, L4Command *);
float func_0001fa3c(L4Delay *, int);

L4Command *func_0001ef30(void *filter, short *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Effect *effect = filter;
    L4Command *commands = inputCommands;
    short i;
    short inputBuffer, outputBuffer;
    u16 mainBuffer = 0x800;
    u16 gain;
    short *lastOutput = 0;
    L4Filter *input = effect->filter.input;
    commands = input->pull(input, output, count, offset, commands);
    inputBuffer = 0;
    outputBuffer = 0x140;
    L4_SET_BUFFER(commands++, 0, 0, count << 1);
    L4_MIX_BUFFER(commands++, (short)0xDA83, 0x6C0, 0x6C0);
    L4_MIX_BUFFER(commands++, 0x5A82, mainBuffer, 0x6C0);
    commands = func_0001f830(effect, effect->cursor, 0x6C0, count, commands);
    L4_CLEAR_BUFFER(commands++, mainBuffer, count << 1);
    for (i = 0; i < effect->count; i++) {
        L4Delay *delay = &effect->delays[i];
        short *inputAddress = effect->cursor - delay->input;
        short *outputAddress = effect->cursor - delay->output;
        if (inputAddress == lastOutput) {
            short swap = outputBuffer;
            outputBuffer = inputBuffer;
            inputBuffer = swap;
        } else {
            commands = func_0001f6c0(effect, inputAddress, inputBuffer, count, commands);
        }
        commands = func_0001f4a8(effect, delay, outputBuffer, count, commands);
        if (delay->field08) {
            gain = delay->field08;
            L4_MIX_BUFFER(commands++, gain, inputBuffer, outputBuffer);
            if (!delay->resample && !delay->lowpass)
                commands = func_0001f830(effect, outputAddress, outputBuffer, count, commands);
        }
        if (delay->field0a) {
            gain = delay->field0a;
            L4_MIX_BUFFER(commands++, gain, outputBuffer, inputBuffer);
            commands = func_0001f830(effect, inputAddress, inputBuffer, count, commands);
        }
        if (delay->lowpass)
            commands = func_0001f9a0(delay->lowpass, outputBuffer, count, commands);
        if (!delay->resample)
            commands = func_0001f830(effect, outputAddress, outputBuffer, count, commands);
        if (delay->field0c) {
            gain = delay->field0c;
            L4_MIX_BUFFER(commands++, gain, outputBuffer, mainBuffer);
        }
        lastOutput = effect->cursor + delay->output;
    }
    effect->cursor += count;
    if (effect->cursor > effect->buffer + effect->length)
        effect->cursor -= effect->length;
    /* Reuse the signed bank: KMC retains the final halfword conversion. */
    inputBuffer = mainBuffer;
    L4_MOVE_BUFFER(commands++, inputBuffer, 0x6C0, count << 1);
    return commands;
}

int func_0001f2a4(L4Filter *filter, int selector, void *value)
{
    if (selector == 1)
        filter->input = value;
    return 0;
}

int func_0001f2b8(L4Effect *effect, int selector, int *value)
{
    int parameter = (selector - 2) % 8;
    int n;
    selector = (selector - 2) / 8;
    n = *value;
    switch (parameter) {
    case 0: effect->delays[selector].input = n & ~7; break;
    case 1: effect->delays[selector].output = n & ~7; break;
    case 3: effect->delays[selector].field08 = n; break;
    case 2: effect->delays[selector].field0a = n; break;
    case 4: effect->delays[selector].field0c = n; break;
    case 5:
        effect->delays[selector].rate = 2.0 * ((float)n / 1000.0f) / alGlobals->outputRate;
        break;
    case 6:
        effect->delays[selector].depth = ((float)n / 173123.404906676) *
            (effect->delays[selector].output - effect->delays[selector].input);
        break;
    case 7:
        if (effect->delays[selector].lowpass) {
            effect->delays[selector].lowpass->cutoff = n;
            _init_lpfilter(effect->delays[selector].lowpass);
        }
        break;
    }
    return 0;
}

L4Command *func_0001f4a8(L4Effect *effect, L4Delay *delay, int outputBuffer,
                       int count, L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    int output = outputBuffer;
    if (delay->resample) {
        int length = delay->output - delay->input;
        float modulation = func_0001fa3c(delay, count) / length;
        int pitch = modulation * 32768.0f;
        float ratio, outputRatio;
        float samples;
        int inputCount, delta;
        short *address;
        ratio = (float)pitch / 32768.0f;
        outputRatio = 1.0 - ratio;
        samples = delay->resample->fraction + outputRatio * count;
        inputCount = samples;
        delay->resample->fraction = samples - inputCount;
        address = effect->cursor + (delay->field18 - delay->output);
        delta = ((int)address & 7) >> 1;
        commands = func_0001f6c0(effect, address - delta, 0x280, inputCount + delta, commands);
        L4_SET_BUFFER(commands++, 0x280 + (delta << 1), output, count << 1);
        {
            L4Command *command = commands++;
            command->word0 = 0x05000000 | ((u8)delay->resample->first << 16)
                | (u16)(int)(outputRatio * 32768.0f);
            command->word1 = osVirtualToPhysical(delay->resample->state);
        }
        delay->resample->first = 0;
        delay->field18 += inputCount - count;
    } else {
        commands = func_0001f6c0(effect, effect->cursor - delay->output, output, count, commands);
    }
    return commands;
}

L4Command *func_0001f6c0(L4Effect *effect, short *address, int input,
                       int count, L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    short *end = effect->buffer + effect->length;
    short *last;
    int firstCount, secondCount;
    if (address < effect->buffer)
        address += effect->length;
    last = address + count;
    if (last > end) {
        firstCount = end - address;
        secondCount = last - end;
        L4_SET_BUFFER(commands++, input, 0, firstCount << 1);
        L4_LOAD_BUFFER(commands++, address);
        L4_SET_BUFFER(commands++, input + (firstCount << 1), 0, secondCount << 1);
        L4_LOAD_BUFFER(commands++, effect->buffer);
    } else {
        L4_SET_BUFFER(commands++, input, 0, count << 1);
        L4_LOAD_BUFFER(commands++, address);
    }
    L4_SET_BUFFER(commands++, 0, 0, count << 1);
    return commands;
}

L4Command *func_0001f830(L4Effect *effect, short *address, int input,
                       int count, L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    short *end = effect->buffer + effect->length;
    short *last;
    int firstCount, secondCount;
    if (address < effect->buffer)
        address += effect->length;
    last = address + count;
    if (last > end) {
        firstCount = end - address;
        secondCount = last - end;
        L4_SET_BUFFER(commands++, 0, input, firstCount << 1);
        L4_SAVE_BUFFER(commands++, address);
        L4_SET_BUFFER(commands++, 0, input + (firstCount << 1), secondCount << 1);
        L4_SAVE_BUFFER(commands++, effect->buffer);
        L4_SET_BUFFER(commands++, 0, 0, count << 1);
    } else {
        L4_SET_BUFFER(commands++, 0, input, count << 1);
        L4_SAVE_BUFFER(commands++, address);
    }
    return commands;
}

L4Command *func_0001f9a0(L4Lowpass *filter, int inputOffset, int count,
                         L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    u16 input = inputOffset;
    L4_SET_BUFFER(commands++, input, input, count << 1);
    {
        L4Command *command = commands++;
        command->word0 = 0x0B000020;
        command->word1 = osVirtualToPhysical(filter->coefficients);
    }
    {
        L4Command *command = commands++;
        command->word0 = 0x0E000000 | ((u8)filter->first << 16) | (u16)filter->gain;
        command->word1 = osVirtualToPhysical(filter->state);
    }
    filter->first = 0;
    return commands;
}

float func_0001fa3c(L4Delay *delay, int count)
{
    float value, magnitude;
    value = delay->ratio + delay->rate * count;
    delay->ratio = value;
    if (value > 2.0)
        value -= 4.0;
    delay->ratio = value;
    magnitude = value;
    if (value < 0)
        magnitude = -value;
    /* Reuse value for the shape; a separate result changes FP allocation. */
    value = magnitude - 1.0;
    return delay->depth * value;
}
