/* Complete four-body ROM1D220..1DD30 producer, independently reconstructed. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct L4Command { u32 word0, word1; } L4Command;
typedef struct L4Loop { u32 start, end, count; short state[16]; } L4Loop;
typedef struct L4Book { int field00, field04; short entries[1]; } L4Book;
typedef struct L4Wave {
    u8 *base;
    int length;
    u8 type, field09, field0a, field0b;
    L4Loop *loop;
    L4Book *book;
} L4Wave;
typedef struct L4Load {
    void *input;
    L4Command *(*pull)(void *, short *, int, int, L4Command *);
    void *param;
    short field0c, field0e;
    int field10;
    void *state14, *state18;
    u32 loopStart, loopEnd, loopCount;
    L4Wave *wave;
    int bookBytes;
    u32 (*dma)(u8 *, int, void *);
    void *dmaState;
    u32 position;
    int previousSamples, first, field44;
} L4Load;

#define L4_SET_BUFFER(packet, in, out, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x08000000 | ((in) & 0xFFFF); \
    command->word1 = ((u16)(out) << 16) | ((length) & 0xFFFF); \
}
#define L4_LOAD_BUFFER(packet, address) { \
    L4Command *command = (packet); \
    command->word0 = 0x04000000; \
    command->word1 = (address); \
}
#define L4_MOVE_BUFFER(packet, in, out, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x0A000000 | ((in) & 0xFFFFFF); \
    command->word1 = ((u16)(out) << 16) | ((length) & 0xFFFF); \
}
#define L4_CLEAR_BUFFER(packet, address, length) { \
    L4Command *command = (packet); \
    command->word0 = 0x02000000 | ((address) & 0xFFFFFF); \
    command->word1 = (length); \
}

extern void mem_byte_copy(void *, void *, int);
L4Command *func_0001dbac(L4Command *, L4Load *, int, int, short, int, int);

L4Command *func_0001d220(void *filter, short *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    L4Load *load = filter;
    int partial = 0;
    int looping;
    int n, left, decoded, frames, bytes, overflow, missing;
    int destination, temporary;
    /* Keep the typed input offset in every decoder call. Replacing the early
     * uses with literal zero changes KMC's frame and register allocation even
     * though their final argument stores still fold to zero. */
    short inputBuffer;

    if (count == 0)
        return commands;
    {
        L4Command *command = commands++;
        command->word0 = 0x0B000000 | (load->bookBytes & 0xFFFFFF);
        command->word1 = (u32)load->wave->book->entries & 0x1FFFFFFF;
    }
    looping = 0;
    inputBuffer = 0;
    if (count + load->position > load->loopEnd)
        looping = load->loopCount > 0;
    n = count;
    if (looping)
        n = load->loopEnd - load->position;
    if (load->previousSamples)
        left = 16 - load->previousSamples;
    else
        left = 0;
    decoded = n - left;
    if (decoded < 0)
        decoded = 0;
    frames = (decoded + 15) >> 4;
    bytes = frames * 9;
    if (looping) {
        commands = func_0001dbac(commands, load, decoded, bytes, *output, inputBuffer,
                                 load->first);
        if (load->previousSamples)
            *output += load->previousSamples << 1;
        else
            *output += 32;
        load->previousSamples = load->loopStart & 15;
        load->field44 = (int)(load->wave->base + ((load->loopStart >> 4) + 1) * 9);
        load->position = load->loopStart;
        destination = *output;
        while (n < count) {
            temporary = (destination + ((frames + 1) << 5)) & ~31;
            destination += n << 1;
            count -= n;
            if (load->loopCount != (u32)-1 && load->loopCount != 0)
                load->loopCount--;
            n = (u32)count < load->loopEnd - load->loopStart
              ? count : load->loopEnd - load->loopStart;
            decoded = n - 16 + load->previousSamples;
            if (decoded < 0)
                decoded = 0;
            frames = (decoded + 15) >> 4;
            commands = func_0001dbac(commands, load, decoded, frames * 9,
                                     temporary, inputBuffer, load->first | 2);
            L4_MOVE_BUFFER(commands++, temporary + (load->previousSamples << 1),
                           destination, n << 1);
        }
        load->previousSamples = (count + load->previousSamples) & 15;
        load->position += count;
        load->field44 += frames * 9;
        return commands;
    } else {
        overflow = (u8 *)load->field44 + bytes -
                   (load->wave->base + load->wave->length);
        if (overflow < 0)
            overflow = 0;
        n = frames << 4;
        missing = (overflow / 9) << 4;
        if (missing > n + left)
            missing = n + left;
        bytes -= overflow;
        if (missing - (missing & 15) < count) {
            partial = 1;
            commands = func_0001dbac(commands, load, n - missing, bytes,
                                     *output, inputBuffer, load->first);
            if (load->previousSamples)
                *output += load->previousSamples << 1;
            else
                *output += 32;
            load->previousSamples = (count + load->previousSamples) & 15;
            load->position += count;
            load->field44 += frames * 9;
        } else {
            load->previousSamples = 0;
            load->field44 += frames * 9;
        }
        if (missing) {
            load->previousSamples = 0;
            if (partial)
                decoded = (left + n - missing) << 1;
            else
                decoded = 0;
            L4_CLEAR_BUFFER(commands++, decoded + *output, missing << 1);
        }
    }
    return commands;
}

L4Command *func_0001d664(void *filter, short *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Command *commands = inputCommands;
    L4Load *load = filter;
    int n, bytes, delta, padding, overflow;
    int destination;
    u32 address;

    if (count == 0)
        return commands;
    if (count + load->position > load->loopEnd && load->loopCount) {
        n = load->loopEnd - load->position;
        if (n > 0) {
            bytes = n << 1;
            address = load->dma((u8 *)load->field44, bytes, load->dmaState);
            delta = address & 7;
            bytes += delta;
            L4_SET_BUFFER(commands++, *output, 0, bytes + 8 - (bytes & 7));
            L4_LOAD_BUFFER(commands++, address - delta);
        } else {
            delta = 0;
        }
        *output += delta;
        load->field44 = (int)(load->wave->base + (load->loopStart << 1));
        load->position = load->loopStart;
        destination = *output;
        while (n < count) {
            destination += n << 1;
            count -= n;
            if (load->loopCount != (u32)-1 && load->loopCount != 0)
                load->loopCount--;
            n = (u32)count < load->loopEnd - load->loopStart
              ? count : load->loopEnd - load->loopStart;
            bytes = n << 1;
            address = load->dma((u8 *)load->field44, bytes, load->dmaState);
            delta = address & 7;
            bytes += delta;
            if (destination & 7)
                padding = 8 - (destination & 7);
            else
                padding = 0;
            L4_SET_BUFFER(commands++, destination + padding, 0,
                          bytes + 8 - (bytes & 7));
            L4_LOAD_BUFFER(commands++, address - delta);
            if (delta || padding)
                L4_MOVE_BUFFER(commands++, destination + delta + padding,
                               destination, n << 1);
        }
        load->position += count;
        load->field44 += count << 1;
        return commands;
    } else {
        u32 endAddress;
        bytes = count << 1;
        endAddress = load->field44 + bytes;
        overflow = (u8 *)endAddress - (load->wave->base + load->wave->length);
        if (overflow < 0)
            overflow = 0;
        if (overflow > bytes)
            overflow = bytes;
        if (overflow < bytes) {
            if (count > 0) {
                bytes -= overflow;
                address = load->dma((u8 *)load->field44, bytes, load->dmaState);
                delta = address & 7;
                bytes += delta;
                L4_SET_BUFFER(commands++, *output, 0, bytes + 8 - (bytes & 7));
                L4_LOAD_BUFFER(commands++, address - delta);
            } else {
                delta = 0;
            }
            *output += delta;
            load->position += count;
            load->field44 += count << 1;
        } else {
            load->field44 = endAddress;
        }
        if (overflow) {
            int clearOffset = (count << 1) - overflow;
            if (clearOffset < 0)
                clearOffset = 0;
            L4_CLEAR_BUFFER(commands++, clearOffset + *output, overflow);
        }
    }
    return commands;
}

void func_0001da00(void *filter, int selector, void *value)
{
    L4Load *load = filter;
    switch (selector) {
    case 5:
        load->wave = value;
        load->field44 = (int)load->wave->base;
        load->position = 0;
        switch (load->wave->type) {
        case 0:
            load->pull = func_0001d220;
            load->wave->length = load->wave->length / 9 * 9;
            load->bookBytes = load->wave->book->field00 *
                             (load->wave->book->field04 << 1) << 3;
            if (load->wave->loop) {
                load->loopStart = load->wave->loop->start;
                load->loopEnd = load->wave->loop->end;
                load->loopCount = load->wave->loop->count;
                mem_byte_copy(load->wave->loop->state, load->state18, 32);
            } else {
                load->loopCount = 0;
                load->loopEnd = 0;
                load->loopStart = 0;
            }
            break;
        case 1:
            load->pull = func_0001d664;
            if (load->wave->loop) {
                load->loopStart = load->wave->loop->start;
                load->loopEnd = load->wave->loop->end;
                load->loopCount = load->wave->loop->count;
            } else {
                load->loopCount = 0;
                load->loopEnd = 0;
                load->loopStart = 0;
            }
            break;
        }
        break;
    case 4:
        load->previousSamples = 0;
        load->first = 1;
        load->position = 0;
        if (load->wave) {
            load->field44 = (int)load->wave->base;
            if (load->wave->type == 0) {
                if (load->wave->loop)
                    load->loopCount = load->wave->loop->count;
            } else if (load->wave->type == 1) {
                if (load->wave->loop)
                    load->loopCount = load->wave->loop->count;
            }
        }
        break;
    }
}

L4Command *func_0001dbac(L4Command *commands, L4Load *load, int samples,
                         int bytes, short output, int input, int flags)
{
    u32 address;
    int delta, count;
    short buffer = input;

    if (bytes > 0) {
        address = load->dma((u8 *)load->field44, bytes, load->dmaState);
        delta = address & 7;
        bytes += delta;
        L4_SET_BUFFER(commands++, input, 0, bytes + 8 - (bytes & 7));
        L4_LOAD_BUFFER(commands++, address - delta);
    } else {
        delta = 0;
    }
    if (flags & 2) {
        L4Command *command = commands++;
        command->word0 = 0x0F000000;
        command->word1 = (u32)load->state18 & 0x1FFFFFFF;
    }
    L4_SET_BUFFER(commands++, (short)buffer + delta, output, samples << 1);
    {
        L4Command *command = commands++;
        command->word0 = 0x01000000 | ((flags & 0xFF) << 16);
        command->word1 = (u32)load->state14 & 0x1FFFFFFF;
    }
    load->first = 0;
    return commands;
}
