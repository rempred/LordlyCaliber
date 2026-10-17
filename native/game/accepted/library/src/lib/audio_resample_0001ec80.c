/* Complete two-body producer independently reconstructed from ROM1EC80..1EF30. */
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
typedef struct L4Resample {
    L4Filter filter;
    void *state;
    union { float value; int bits; } ratio;
    int field1c;
    float fraction;
    int first, field28, field2c, field30;
} L4Resample;

extern u32 osVirtualToPhysical(void *);

L4Command *func_0001ec80(void *filter, short *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Resample *resample = (L4Resample *)filter;
    L4Filter *input = resample->filter.input;
    L4Command *commands = inputCommands;
    short inputBuffer = 0x140;
    int inputCount, pitch;
    float ratio, samples, outputRatio;

    if (count == 0)
        return commands;
    if (resample->field1c) {
        commands = input->pull(input, &inputBuffer, count, offset, commands);
        {
            L4Command *command = commands++;
            command->word0 = 0x0A000000 | (inputBuffer & 0x00FFFFFF);
            command->word1 = ((u16)*output << 16) | ((count << 1) & 0xFFFF);
        }
    } else {
        if (resample->ratio.value > 1.99996)
            resample->ratio.value = 1.99996;
        pitch = resample->ratio.value * 32768.0f;
        ratio = (float)pitch / 32768.0f;
        samples = resample->fraction + ratio * count;
        inputCount = samples;
        resample->ratio.value = ratio;
        resample->fraction = samples - inputCount;
        commands = input->pull(input, &inputBuffer, inputCount, offset, commands);
        outputRatio = resample->ratio.value * 32768.0f;
        {
            L4Command *command = commands++;
            command->word0 = 0x08000000 | (u16)inputBuffer;
            command->word1 = ((u16)*output << 16) | ((count << 1) & 0xFFFF);
        }
        {
            L4Command *command = commands++;
            command->word0 = 0x05000000 | ((u8)resample->first << 16)
                          | (u16)(int)outputRatio;
            command->word1 = osVirtualToPhysical(resample->state);
        }
        resample->first = 0;
    }
    return commands;
}

int func_0001ee6c(L4Filter *filter, int selector, void *value)
{
    L4Resample *resample = (L4Resample *)filter;

    switch (selector) {
    case 1:
        filter->input = value;
        break;
    case 4:
        resample->fraction = 0;
        resample->first = 1;
        resample->field30 = 0;
        resample->field1c = 0;
        filter = filter->input;
        if (filter)
            filter->param(filter, 4, 0);
        break;
    case 9:
        resample->field30 = 1;
        filter = filter->input;
        if (filter)
            filter->param(filter, 9, 0);
        break;
    case 7:
        resample->ratio.bits = (int)value;
        break;
    case 8:
        resample->field1c = 1;
        break;
    default:
        filter = filter->input;
        if (filter)
            filter->param(filter, selector, value);
        break;
    }
    return 0;
}
