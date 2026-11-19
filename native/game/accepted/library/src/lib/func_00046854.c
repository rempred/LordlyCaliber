#include "game/story_event.h"

typedef s32 (*EventReadFn)(s32, s32);
typedef void (*EventWriteFn)(s32, s32, s32);
typedef void (*EventIndexedWriteFn)(u16, s16, u16);

struct EventInstruction {
    s8 opcode;
    s8 operand;
};

struct EventTimedRecord {
    u8 id;
    u8 field01;
    u16 field02;
    u8 field04;
    u8 field05;
};

/* The interpreter's eight opcode words follow the saved cursors in this frame. */
struct EventFrame {
    u16 savedCursor[32];
    u16 words[8];
    struct EventInstruction instruction;
};

#define FIELD(base, type, offset) (*(type)((u8 *)(base) + (offset)))

extern volatile u16 D_80197B04;
extern u16 D_8018F1A0;
extern s32 D_80197B14;
extern u8 D_801936AA;
extern u8 D_801969B8[];
extern u8 D_80196AE9;
extern u8 D_80197B20[];
extern u32 D_80196A6C;
extern struct EventTimedRecord D_80197810[];

extern void func_00023780(void *, u32);
extern s32 rand(void);
extern void memmove_0002cc00(void *, const void *, u32);
extern void func_8016FE78(s32, s32);
extern s32 func_0004890C(u8);
extern void func_00048928(s32, s32, u16, s32);
extern void func_00048fbc(u8, s32, u32);
extern void func_000490ec(void);
extern void func_000491e0(s16);
extern s32 func_000492a0(u16);
extern void func_000493d4(u16 *, u16 *, u8);
extern void func_0028a7b0(u8, s16, s32);
extern void func_0028a84c(u8, u8);
extern void func_0028a8b8(u8);

s32 func_00046854(s32 program)
{
    struct EventFrame frame;
    u16 *savedCursor;
    volatile u16 *programCursor;
    volatile u8 *requestPending;
    u16 *sceneRequest;
    u32 emptyRecordId;
    EventReadFn readCallback;
    u16 requestCode;
    s16 sceneFlags;
    s32 readArgument;
    s32 readIndexOrSelector;
    s32 immediateWordIndex;
    s32 readSelectorKind;
    s32 writeSelectorKind;
    s32 arithmeticWordIndex;
    s32 arithmeticImmediate;
    s32 arithmeticSourceIndex;
    s32 writeWordIndex;
    s32 recordIndex;
    s32 sceneStatus;
    s32 requestOperand;
    s32 rangeIndex;
    s32 readWordIndex;
    u16 stackDepth;
    u16 *quotientDestination;
    u16 *remainderDestination;
    u16 *immediateRemainderDestination;
    u16 *andDestination;
    u16 *orDestination;
    u16 *addDestination;
    u16 *subtractDestination;
    u16 *multiplyDestination;
    u16 *wordForHighByte;
    u16 *immediateAndDestination;
    u16 *immediateOrDestination;
    u16 *immediateAddDestination;
    u16 *immediateSubtractDestination;
    u16 *immediateMultiplyDestination;
    u16 *immediateQuotientDestination;
    u16 *readObjectWord;
    u16 *writeObjectWord;
    u32 immediateOperation;
    u32 readSelector;
    s16 callbackSelector;
    u32 arithmeticOperation;
    u8 *nextEventByte;
    struct EventTimedRecord *recordSlot;

    stackDepth = 0;
    savedCursor = frame.savedCursor;
    /* KMC allocates the cursor and depth before merging the advance paths.
     * Keep the explicit advances in the five handlers that use continue. */
    for (;;) {
        sceneRequest = &D_8018F1A0;
        programCursor = &D_80197B04;
        requestPending = (volatile u8 *)programCursor - 2;
        emptyRecordId = 0xFF;
        frame.instruction = ((struct EventInstruction *)program)[*(s16 *)0x80197B04];
        switch ((u8) ((volatile struct EventInstruction *)&frame.instruction)->opcode) {
        case 0x3:
            savedCursor[stackDepth++] = *programCursor;
            /* fallthrough */
        case 0x2:
            *programCursor = (s16) ((u16) *programCursor + (((u8 *)&frame.words[0])[1] + ((u8) frame.instruction.operand << 8)));
            *programCursor = (u16)(*programCursor + 1);
            continue;
        case 0x4:
            *(u16 *)0x80197B04 = savedCursor[--stackDepth];
            goto next_instruction;
        case 0x5:
            savedCursor[stackDepth++] = FIELD(&savedCursor[(u8)frame.instruction.operand & 7], u16 *, 0x40);
            goto next_instruction;
        case 0x6: {
            u8 operand = (u8)frame.instruction.operand;
            FIELD((u8 *)(((operand & 7) * 2) + (u32)savedCursor), u16 *, 0x40) = savedCursor[--stackDepth];
            goto next_instruction;
        }
        case 0x10: {
            u16 scene = frame.words[0];
            sceneRequest[1] = 0x8002;
            *requestPending = 1;
            sceneRequest[0] = scene;
            sceneFlags = FIELD(((FIELD(requestPending, u8 *, -2) * 2) + 0x80190000), s16 *, -0xD08);
            FIELD(requestPending, s32 *, 0x12) = (s32) sceneFlags;
            if (sceneFlags == 0x10) {
                sceneRequest[0] = scene | 0x4000;
                sceneStatus = func_000492a0(scene);
                if (sceneStatus == 2) {
                    goto check_scene_flags;
                }
                if (sceneStatus != 7) {
                    FIELD(requestPending, s32 *, 0x12) |= 0x800;
                }
    check_scene_flags:
                if ((u8) frame.instruction.operand & 0x80) {
                    s32 *flags = &D_80197B14;
                    *flags |= 0x400;
                }
            }
            {
                s32 *flags = &D_80197B14;
                *flags |= ((s32 *)0x8018F304)[(u8)frame.instruction.operand & 3];
            }
            if ((u8) frame.instruction.operand & 0x40) {
                sceneRequest[0] = (u16) (sceneRequest[0] & 0xBFFF);
            }
            return 1;
        }
        case 0x11:
            requestOperand = ((u8 *)&frame.words[0])[1] + ((u8) frame.instruction.operand << 8);
            if (requestOperand == 1) {
                *(u8 *)0x80197B01 = emptyRecordId;
            case 0x2F:
                return 0;
            }
            sceneRequest[0] = 0U;
            sceneRequest[1] = func_00046738(requestOperand & 0xFF);
            *(s32 *)0x80197B14 = 0;
            return 1;
        case 0x12: {
            u16 range = frame.words[0];
            rangeIndex = range & 0xFF;
            goto test_range;
    process_range:
            func_8016FE78(rangeIndex, 0);
            range = frame.words[0];
            rangeIndex++;
    test_range:
            if (rangeIndex < (s32)(range >> 8)) {
                goto process_range;
            }
            *(u8 *)0x801936A8 = (u8)frame.instruction.operand;
            return 2;
        }
        case 0x13:
            requestCode = func_00046738((u8) frame.instruction.operand);
            *(u16 *)0x8018F1A2 = requestCode;
            if (requestCode & 0xFFFF) {
                *requestPending = 1;
                FIELD(requestPending, s32 *, 0x12) = 0;
                return 1;
            }
            goto next_instruction;
        case 0x15: {
            s16 index = 0;
            u8 id = (u8)frame.instruction.operand;
            u8 found;
            s32 value;

    scan_records:
            found = FIELD(((s16)index * 6 + 0x80190000), u8 *, 0x7810);
            if (found == id) {
                goto after_scan;
            }
            index++;
            if (index < 121) {
                goto scan_records;
            }
    after_scan:
            if (index < 121) {
                goto next_instruction;
            }

            index = 0;
            value = frame.words[2] + frame.words[1] * 40 + frame.words[0] * 600;
            do {
                recordIndex = index;
                recordSlot = &D_80197810[recordIndex];
                if (recordSlot->id == emptyRecordId) {
                    goto insert_record;
                }
                if (value < recordSlot->field05 + recordSlot->field04 * 40 + recordSlot->field02 * 600) {
                    goto shift_records;
                }
                index++;
            } while (index < 121);
            goto next_instruction;
        }
        case 0x16: {
            u8 operand = (u8)frame.instruction.operand;
            u32 argument = operand & 0xFF;
            if (argument == 5) {
                func_00048928(0, 0, ((u8)func_0004890C(0) * 4) + 0x1AE, 1);
            } else if (argument == 6) {
                func_00048928(0, 0, ((u8)func_0004890C(1) * 4) + 0x1AF, 1);
            } else if (argument == 7) {
                func_00048928(0, 0, ((u8)func_0004890C(2) * 4) + 0x1B0, 1);
            } else if (argument == 8) {
                func_00048928(0, 0, ((u8)func_0004890C(3) * 4) + 0x1B1, 1);
            } else {
                func_00048928(argument >> 7, operand & 0x3F, frame.words[0], (argument >> 6) & 1);
            }
            goto next_instruction;
        }
        case 0x17:
            func_000491e0((s16) (((u8 *)&frame.words[0])[1] + ((u8) frame.instruction.operand << 8)));
            *programCursor = (u16)(*programCursor + 1);
            continue;
        case 0x18:
            *(u8 *)0x801939D7 = (u8) frame.instruction.operand;
            goto next_instruction;
        case 0x19:
            *(u8 *)0x80196AEA = (u8) frame.instruction.operand;
            *(u8 *)0x80196AEB = (u8) frame.words[0];
            goto next_instruction;
        case 0x1A:
            *(u16 *)0x80196A9C = frame.words[0];
            *(s16 *)0x80196A9E = (s16) (u8) frame.instruction.operand;
            goto next_instruction;
        case 0x1B:
            *(s32 *)0x80193A8C = (s32)D_80197B20;
            nextEventByte = D_80197B20 + frame.words[7];
            frame.words[7] += 1;
            *nextEventByte = (u8) frame.instruction.operand;
            goto next_instruction;
        case 0x1C:
        case 0x1D:
            func_0028a7b0(((u8 *)&frame.words[0])[1], (s16) (((u8 *)&frame.words[1])[1] + ((u8) frame.instruction.operand << 8)), (u8) frame.instruction.opcode == 0x1C);
            goto next_instruction;
        case 0x1E:
            func_0028a84c(((u8 *)&frame.words[0])[1], (u8) frame.instruction.operand);
            *programCursor = (u16)(*programCursor + 1);
            continue;
        case 0x1F:
            func_0028a8b8((u8) frame.instruction.operand);
            *programCursor = (u16)(*programCursor + 1);
            continue;
        case 0x20:
            func_000490ec();
            *programCursor = (u16)(*programCursor + 1);
            continue;
        case 0x21:
            if ((D_80196AE9 = (u8)frame.instruction.operand) == 0) {
                func_00023780(D_801969B8, 0x6E);
            }
            goto next_instruction;
        case 0x22:
            func_00048fbc((u8) frame.instruction.operand, frame.words[0] & 0xFF, frame.words[0] >> 8);
            goto next_instruction;
        case 0x28:
            func_000493d4(&frame.words[6], &frame.words[7], ((u8 *)&frame.words[5])[1]);
            goto next_instruction;
        case 0x29: {
            u8 *target = &D_801936AA;
            u32 masked = *(u8 *)0x80196A9A & 0x3F;
            u8 value = masked;
            if (*(u8 *)0x80196AEA == masked) {
                value = *(u8 *)0x80196AEB;
                *target = value;
            } else {
                *target = value;
            }
            goto next_instruction;
        }
        case 0x2D:
            if (*(u32 *)0x80196A6C < frame.words[0]) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0x2E: {
            u32 *value = &D_80196A6C;
            s16 decrement = ((u8)frame.instruction.operand << 8) + ((u8 *)&frame.words[0])[1];
            *value -= decrement;
            goto next_instruction;
        }
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67: {
            u16 *words;
            u8 opcode = ((volatile struct EventInstruction *)&frame.instruction)->opcode;
            u8 operand = (u8)frame.instruction.operand;
            readWordIndex = opcode & 7;
            readSelector = operand & 0xFF;
            if (readSelector >= 0xE5U) {
                switch (readSelector) {
                case 0xE4:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80197B62;
                    goto next_instruction;
                case 0xE5:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x8022A999;
                    goto next_instruction;
                case 0xE6:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80197B60;
                    goto next_instruction;
                case 0xE7:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x8018F481;
                    goto next_instruction;
                case 0xE8:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801936A8;
                    goto next_instruction;
                case 0xE9:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801936A7;
                    goto next_instruction;
                case 0xEA:
                    FIELD(&savedCursor[readWordIndex], u16 *, 0x40) = rand();
                    goto next_instruction;
                case 0xEB:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x8023DF10;
                    goto next_instruction;
                case 0xEC:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x801936B0;
                    goto next_instruction;
                case 0xED:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801936AE;
                    goto next_instruction;
                case 0xEE:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80193678;
                    goto next_instruction;
                case 0xEF:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x8019367A;
                    goto next_instruction;
                case 0xF0:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x8019367B;
                    goto next_instruction;
                case 0xF1:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80196A72;
                    goto next_instruction;
                case 0xF2:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A74;
                    goto next_instruction;
                case 0xF3:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A75;
                    goto next_instruction;
                case 0xF4:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80196A30;
                    goto next_instruction;
                case 0xF5:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A32;
                    goto next_instruction;
                case 0xF6:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A33;
                    goto next_instruction;
                case 0xF8:
                    FIELD(&savedCursor[readWordIndex], s16 *, 0x40) = (s16) (*(u8 *)0x801976E8 + 1);
                    break;
                case 0xF9:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A9A;
                    goto next_instruction;
                case 0xFA:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80197B08;
                    goto next_instruction;
                case 0xFB:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801976D8;
                    goto next_instruction;
                case 0xFC:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801976D9;
                    goto next_instruction;
                case 0xFD:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801976E8;
                    goto next_instruction;
                case 0xFE:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801976DC;
                    goto next_instruction;
                case 0xFF:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x801936A9;
                    goto next_instruction;
                default:
                    break;
                }
                goto next_instruction;
            }
            words = frame.words;
            readObjectWord = &words[operand & 7];
            readSelectorKind = operand & 0xF8;
            switch (readSelectorKind) {
            case 0:
                FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) FIELD(readObjectWord, u8 *, 1);
                goto next_instruction;
            case 8:
                readArgument = frame.words[7];
                readIndexOrSelector = (s16) FIELD(readObjectWord, u16 *, 0);
                readCallback = *(EventReadFn *)0x8018F1B4;
                goto invoke_read_callback;
            default:
                readArgument = (s16) FIELD(readObjectWord, u16 *, 0);
                readCallback = FIELD(((((u8) frame.instruction.operand >> 3) * 4) + 0x80190000), EventReadFn *, -0xE50);
                readIndexOrSelector = 0;
                goto invoke_read_callback;
            }
            goto next_instruction;
        }
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F: {
            u16 result = (u8) frame.instruction.operand;
            FIELD(&savedCursor[(u8)frame.instruction.opcode & 7], u16 *, 0x40) = result;
            goto next_instruction;
        }
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77: {
            u32 selector;
            u16 *words;
            u8 opcode = ((volatile struct EventInstruction *)&frame.instruction)->opcode;
            selector = (u8)((volatile struct EventInstruction *)&frame.instruction)->operand;
            writeWordIndex = opcode & 7;
            /* Keep both byte conversions so KMC places the base setup in the
             * taken delay slot and still reloads the packed operand below. */
            if ((u8)selector >= 0xE4U) {
                switch ((u8)selector) {
                case 0xE4:
                    *(u8 *)0x80197B62 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xE5:
                    *(u8 *)0x8022A999 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xE6:
                    *(u16 *)0x80197B60 = (u16) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xE7:
                    *(u8 *)0x8018F481 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xE8:
                    *(u8 *)0x801936A8 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xE9:
                    *(u8 *)0x801936A7 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xEB:
                    *(u8 *)0x8023DF10 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xEC:
                    *(u16 *)0x801936B0 = (u16) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xED:
                    *(u8 *)0x801936AE = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xEE:
                    *(u16 *)0x80193678 = (u16) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xEF:
                    *(u8 *)0x8019367A = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xF0:
                    *(u8 *)0x8019367B = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xF9:
                    *(u8 *)0x80196A9A = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xFA:
                    *(u16 *)0x80197B08 = (u16) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xFB:
                    *(u8 *)0x801976D8 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xFD:
                    *(u8 *)0x801976E8 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xFE:
                    *(u8 *)0x801976DC = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xFF:
                    *(u8 *)0x801936A9 = (u8) FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                    goto next_instruction;
                case 0xEA:
                case 0xF1:
                case 0xF2:
                case 0xF3:
                case 0xF4:
                case 0xF5:
                case 0xF6:
                case 0xF7:
                case 0xF8:
                case 0xFC:
                default:
    invalid_write_selector:
                    goto invalid_write_selector;
                }
            }
            words = frame.words;
            {
                u8 operand = frame.instruction.operand;
                writeObjectWord = &words[operand & 7];
                writeSelectorKind = operand & 0xF8;
            }
            switch (writeSelectorKind) {
            case 0:
    invalid_packed_write:
                goto invalid_packed_write;
            case 8:
                (*(EventIndexedWriteFn *)0x8018F224)(frame.words[7], (s16) *writeObjectWord, FIELD(&savedCursor[writeWordIndex], u16 *, 0x40));
                goto next_instruction;
            default: {
                s32 argument = *(s16 *)writeObjectWord;
                u32 selector = (u8)((volatile struct EventInstruction *)&frame.instruction)->operand;
                u32 value = FIELD(&savedCursor[writeWordIndex], u16 *, 0x40);
                EventWriteFn write = ((EventWriteFn *)0x8018F220)[selector >> 3];
                write(argument, 0, value);
                goto next_instruction;
            }
            }
            goto next_instruction;
        }
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            readWordIndex = (u8) frame.instruction.opcode & 7;
            readIndexOrSelector = (u8)frame.instruction.operand;
            callbackSelector = readIndexOrSelector & 0xFF;
            if (callbackSelector >= 0xE5U) {
                if (callbackSelector == 0xFE) {
                    goto read_byte_6A2E;
                }
                if ((s32)callbackSelector >= 0xFF) {
                    goto read_ff;
                }
                if (callbackSelector == 0xFD) {
                    goto read_word_6A2C;
                }
                goto next_instruction;
    read_ff:
                if (callbackSelector == emptyRecordId) {
                    goto read_byte_6A2F;
                }
                goto next_instruction;
    read_word_6A2C:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u16 *)0x80196A2C;
                    goto next_instruction;
    read_byte_6A2E:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = (u16) *(u8 *)0x80196A2E;
                    goto next_instruction;
    read_byte_6A2F:
                    FIELD((u8 *)((readWordIndex * 2) + (u32)savedCursor), u16 *, 0x40) = *(u8 *)0x80196A2F;
                    goto next_instruction;
            } else {
                readArgument = ((s16 *)frame.words)[readIndexOrSelector & 7];
                readCallback = FIELD(((((u32)readIndexOrSelector >> 3) * 4) + 0x80190000), EventReadFn *, -0xD70);
                readIndexOrSelector = 0;
    invoke_read_callback:
                FIELD(&savedCursor[readWordIndex], u16 *, 0x40) = readCallback(readArgument, readIndexOrSelector);
            }
            goto next_instruction;
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87: {
            u8 opcode = ((volatile struct EventInstruction *)&frame.instruction)->opcode;
            u32 selector = (u8)frame.instruction.operand;
            s32 index = (u32)opcode & 7;
            s32 argument;
            s32 value;
            EventWriteFn writer;
            if (selector < 0xE5U) {
                argument = ((s16 *)frame.words)[selector & 7];
                value = FIELD(&savedCursor[index], u16 *, 0x40);
                writer = ((EventWriteFn *)0x8018F2C0)[selector >> 3];
                writer(argument, 0, value);
                goto next_instruction;
            }
            goto next_instruction;
        }
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
            wordForHighByte = (u16 *)((((u8)frame.instruction.opcode & 7) * 2) + (u32)savedCursor);
            FIELD(wordForHighByte, s16 *, 0x40) = (s16) (FIELD(wordForHighByte, u8 *, 0x41) + ((u8) frame.instruction.operand << 8));
            goto next_instruction;
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
            arithmeticSourceIndex = (u8) frame.instruction.operand & 7;
            arithmeticWordIndex = (u8) frame.instruction.opcode & 7;
            arithmeticOperation = (u8) frame.instruction.operand >> 3;
            switch (arithmeticOperation) {
            case 0:
                andDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                FIELD(andDestination, u16 *, 0x40) = (u16) (FIELD(andDestination, u16 *, 0x40) & FIELD((u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor), u16 *, 0x40));
                break;
            case 1:
                orDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                FIELD(orDestination, u16 *, 0x40) = (u16) (FIELD(orDestination, u16 *, 0x40) | FIELD((u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor), u16 *, 0x40));
                break;
            case 2:
                addDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                FIELD(addDestination, u16 *, 0x40) = (u16) (FIELD(addDestination, u16 *, 0x40) + FIELD((u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor), u16 *, 0x40));
                break;
            case 3:
                subtractDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                FIELD(subtractDestination, u16 *, 0x40) = (u16) (FIELD(subtractDestination, u16 *, 0x40) - FIELD((u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor), u16 *, 0x40));
                break;
            case 4:
                multiplyDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                FIELD(multiplyDestination, u16 *, 0x40) = (u16) (FIELD(multiplyDestination, u16 *, 0x40) * FIELD((u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor), u16 *, 0x40));
                break;
            case 5: {
                u32 dividend;
                u32 divisor;
                u16 *right;
                quotientDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                right = (u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor);
                dividend = FIELD(quotientDestination, volatile u16 *, 0x40);
                divisor = FIELD(right, volatile u16 *, 0x40);
                dividend = dividend / divisor;
                FIELD(quotientDestination, u16 *, 0x40) = dividend;
                break;
            }
            case 6: {
                u32 dividend;
                u32 divisor;
                u16 *right;
                remainderDestination = (u16 *)((arithmeticWordIndex * 2) + (u32)savedCursor);
                right = (u16 *)((arithmeticSourceIndex * 2) + (u32)savedCursor);
                dividend = FIELD(remainderDestination, volatile u16 *, 0x40);
                divisor = FIELD(right, volatile u16 *, 0x40);
                divisor = dividend % divisor;
                FIELD(remainderDestination, u16 *, 0x40) = divisor;
                break;
            }
            }
            goto next_instruction;
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
            arithmeticImmediate = (u8) frame.instruction.operand & 0x1F;
            immediateWordIndex = (u8) frame.instruction.opcode & 7;
            immediateOperation = (u8) frame.instruction.operand >> 5;
            switch (immediateOperation) {
            case 0:
                immediateAndDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateAndDestination, u16 *, 0x40) = (u16) (FIELD(immediateAndDestination, u16 *, 0x40) & arithmeticImmediate);
                break;
            case 1:
                immediateOrDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateOrDestination, u16 *, 0x40) = (u16) (FIELD(immediateOrDestination, u16 *, 0x40) | arithmeticImmediate);
                break;
            case 2:
                immediateAddDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateAddDestination, u16 *, 0x40) = (u16) (FIELD(immediateAddDestination, u16 *, 0x40) + arithmeticImmediate);
                break;
            case 3:
                immediateSubtractDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateSubtractDestination, u16 *, 0x40) = (u16) (FIELD(immediateSubtractDestination, u16 *, 0x40) - arithmeticImmediate);
                break;
            case 4:
                immediateMultiplyDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateMultiplyDestination, u16 *, 0x40) = (u16) (FIELD(immediateMultiplyDestination, u16 *, 0x40) * arithmeticImmediate);
                break;
            case 5:
                immediateQuotientDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateQuotientDestination, u16 *, 0x40) = (u16) ((s32) FIELD(immediateQuotientDestination, u16 *, 0x40) / arithmeticImmediate);
                break;
            case 6:
                immediateRemainderDestination = (u16 *)((immediateWordIndex * 2) + (u32)savedCursor);
                FIELD(immediateRemainderDestination, u16 *, 0x40) = (u16) ((s32) FIELD(immediateRemainderDestination, u16 *, 0x40) % arithmeticImmediate);
                break;
            }
            goto next_instruction;
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA6:
        case 0xA7:
        case 0xAB:
            if (FIELD(((u8 *)(((u32) ((u8) frame.instruction.opcode & 0xC) >> 1) + (u32)savedCursor)), u16 *, 0x40) != FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], u16 *, 0x40)) {

            } else {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], u16 *, 0x40) != 0) {

            } else {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xB1:
        case 0xB2:
        case 0xB3:
        case 0xB6:
        case 0xB7:
        case 0xBB:
            if (FIELD(((u8 *)(((u32) ((u8) frame.instruction.opcode & 0xC) >> 1) + (u32)savedCursor)), u16 *, 0x40) != FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], u16 *, 0x40)) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], u16 *, 0x40) != 0) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC6:
        case 0xC7:
        case 0xCB: {
            u8 opcode = frame.instruction.opcode;
            u32 firstOffset = (opcode & 0xC) >> 1;
            const s16 *first = (const s16 *)(firstOffset + (u32)savedCursor);
            u32 secondOffset = (opcode & 3) * 2;
            const s16 *second = (const s16 *)(secondOffset + (u32)savedCursor);
            s32 left = first[32];
            s32 right = second[32];
            if (right < left) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        }
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], s16 *, 0x40) > 0) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD6:
        case 0xD7:
        case 0xDB: {
            u8 opcode = frame.instruction.opcode;
            u32 firstOffset = (opcode & 0xC) >> 1;
            const s16 *first = (const s16 *)(firstOffset + (u32)savedCursor);
            u32 secondOffset = (opcode & 3) * 2;
            const s16 *second = (const s16 *)(secondOffset + (u32)savedCursor);
            s32 left = first[32];
            s32 right = second[32];
            if (left < right) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        }
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], s16 *, 0x40) >= 0) {

            } else {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE6:
        case 0xE7:
        case 0xEB: {
            u8 opcode = frame.instruction.opcode;
            u32 firstOffset = (opcode & 0xC) >> 1;
            const s16 *first = (const s16 *)(firstOffset + (u32)savedCursor);
            u32 secondOffset = (opcode & 3) * 2;
            const s16 *second = (const s16 *)(secondOffset + (u32)savedCursor);
            s32 left = first[32];
            s32 right = second[32];
            if (left >= right) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        }
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], s16 *, 0x40) >= 0) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF6:
        case 0xF7:
        case 0xFB: {
            u8 opcode = frame.instruction.opcode;
            u32 firstOffset = (opcode & 0xC) >> 1;
            const s16 *first = (const s16 *)(firstOffset + (u32)savedCursor);
            u32 secondOffset = (opcode & 3) * 2;
            const s16 *second = (const s16 *)(secondOffset + (u32)savedCursor);
            s32 left = first[32];
            s32 right = second[32];
            if (right >= left) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        }
    shift_records:
            memmove_0002cc00((u8 *)recordSlot + 6, recordSlot, (0x78 - recordIndex) * 6);
    insert_record:
            recordSlot->id = (u8)frame.instruction.operand;
            recordSlot->field02 = frame.words[0];
            recordSlot->field04 = (u8) frame.words[1];
            recordSlot->field05 = (u8) frame.words[2];
            goto next_instruction;
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            if (FIELD(&savedCursor[(u8) frame.instruction.opcode & 3], s16 *, 0x40) <= 0) {
                goto apply_relative_offset;
            }
            goto next_instruction;
        case 0x01:
    apply_relative_offset:
            *programCursor = (u16)(*programCursor + (s8)frame.instruction.operand);
        default:
            break;
        }
    next_instruction:
        *programCursor = (u16)(*programCursor + 1);
    }
}
