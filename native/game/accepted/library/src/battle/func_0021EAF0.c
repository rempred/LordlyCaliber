typedef struct Func001F0E64Record Func001F0E64Record;

typedef struct TerminalBridgeObject {
    unsigned char prefix[0x44];
    int word44;
    short selector48;
} TerminalBridgeObject;

typedef struct TerminalBridgeState {
    unsigned char prefix[0x18];
    TerminalBridgeObject *first;
    TerminalBridgeObject *second;
} TerminalBridgeState;

void func_001F0E64(Func001F0E64Record *record, unsigned int selector);

void func_0021EAF0(TerminalBridgeState *state)
{
    TerminalBridgeObject *object;
    int selector;

    object = state->first;
    if (object != 0) {
        selector = object->selector48;
        if (selector != 0 && (((unsigned int)(selector - 1) & 3u) < 2u)) {
            func_001F0E64((Func001F0E64Record *)((unsigned char *)object + 0x44),
                         (unsigned int)(selector + 2));
        }
    }
    object = state->second;
    if (object != 0) {
        selector = object->selector48;
        if (selector != 0 && (((unsigned int)(selector - 1) & 3u) < 2u)) {
            func_001F0E64((Func001F0E64Record *)((unsigned char *)object + 0x44),
                         (unsigned int)(selector + 2));
        }
    }
}
