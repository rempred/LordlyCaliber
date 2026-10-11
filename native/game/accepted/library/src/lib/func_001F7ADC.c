#include "game/combat_draw.h"
#include "game/combat_draw_commands.h"
#include "game/combat_pose_record.h"

typedef unsigned char u8;
typedef struct DrawImage {
    u8 field00, field01, field02, format;
    u16 width, height;
} DrawImage;
typedef union DrawMatrix {
    u32 words[16];
    double alignment;
} DrawMatrix;
typedef struct DrawPose {
    u8 unknown00[0x12];
    u8 items[1];
} DrawPose;
extern void func_00022e90(float [4][4], float, float, float);
extern void func_00023018(DrawMatrix *, float, float, float);
extern float func_0002d3b0(float);
extern u8 func_00043d1c(int, int);
extern u32 func_00201E38(int, u32);
extern u32 func_00201E9C(int, u32);
extern int func_00205484(int, int, int, int, int, u32, CombatPoseIndexedRecord *);
extern void *func_00206340(int, int, CombatPoseIndexedRecord *, int, int, int, int);
extern int func_0020BFF8(void *);
extern int func_0020C034(void *);
extern u16 func_0020C448(void *);
extern void guMtxF2L(float [4][4], DrawMatrix *);
extern void guScale(DrawMatrix *, float, float, float);
extern DrawMatrix *D_801969B0;
extern CombatDrawVertex *D_80197178;
extern CombatDrawCommand D_801CE9E0[];

static __inline__ u32 line_field(u32 width) { width += 7; width >>= 3; width &= 0x1FF; width <<= 9; return width; }

#define SIZE_ROW(command, row) do { (row) |= 0xF2000000; (command)->first = (row); } while (0)

#define HALF(p,o) (*(short *)((u8 *)(p) + (o)))
#define WORD(p,o) (*(int *)((u8 *)(p) + (o)))
#define BYTE(p,o) (*((u8 *)(p) + (o)))
#define PAIR COMBAT_DRAW_PAIR
#define FIELD COMBAT_DRAW_FIELD
#define WIDTH() func_00201E9C(header->format, header->width)

int func_001F7ADC(void *object, float referenceX, float scaleInput,
                  float referenceZ, short factorInput)
{
    CombatPoseIndexedRecord decoded;
    float matrix[4][4];
    short factor;
    int index;
    void *actor;
    DrawPose *pose;
    int poseId, firstId, secondId, variant;
    u8 orientation;
    int vertexCount, top, capacity, accumulated;
    int left, right;
    float angle, verticalScale, dx, dz, rotationY;
    float negativeQuarterTurn = -90.0f;
    DrawImage *header;
    int bit10, bit8, stride, remaining, strip, row, count, itemAlpha;

    actor = *(void **)((u8 *)object + 0x40);
    poseId = HALF(object, 0x4E);
    firstId = WORD(actor, 0x48);
    secondId = WORD(actor, 0x4C);
    variant = func_0020C448(actor);
    pose = (DrawPose *)((u8 *)object + 0x44);
    factor = (short)(factorInput * HALF(object, 0x34) / 255);
    factor = factor * BYTE(actor, 0xAC) / 255;
    if (factor == 0) {
        return 0;
    }
    if ((float)HALF(object, 0x20) < referenceZ) {
        angle = (double)func_0002d3b0(
            ((float)(HALF(object, 0x1C) + HALF(object, 0x22)) - referenceX) /
            ((float)(HALF(object, 0x20) + HALF(object, 0x26)) - referenceZ)) *
            *(double *)0x801D0098 / *(double *)0x801D00A0;
    } else if (referenceZ < (float)HALF(object, 0x20)) {
        angle = (double)func_0002d3b0(
            ((float)(HALF(object, 0x1C) + HALF(object, 0x22)) - referenceX) /
            ((float)(HALF(object, 0x20) + HALF(object, 0x26)) - referenceZ)) *
            *(double *)0x801D00A8 / *(double *)0x801D00B0 + *(double *)0x801D00A8;
    } else {
        angle = negativeQuarterTurn;
        if ((float)(HALF(object, 0x1C) + HALF(object, 0x22)) < referenceX) {
            angle = 90.0f;
        }
    }
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    orientation = (angle >= 150.0f) & (angle < 330.0f);
    dx = (float)(HALF(object, 0x1C) + HALF(object, 0x22)) - referenceX;
    dz = (float)(HALF(object, 0x20) + HALF(object, 0x26)) - referenceZ;
    /* The original evaluates this even though the result is not consumed. */
    __builtin_sqrtf(dx * dx + dz * dz);
    if (scaleInput < 0.0f) {
        verticalScale = 1.0f;
    } else if (scaleInput > 100.0f) {
        verticalScale = 0.5f;
    } else {
        verticalScale = (double)(100.0f - scaleInput) * *(double *)0x801D00B8 +
                        *(double *)0x801D00C0;
    }
    index = 0;
    rotationY = 0.0f;
    func_00022e90(matrix, rotationY, rotationY, rotationY);
    matrix[3][0] = HALF(object, 0x1C) + HALF(object, 0x22) + HALF(pose, 0x0C);
    matrix[3][1] = HALF(object, 0x1E) + HALF(object, 0x24);
    matrix[3][2] = HALF(object, 0x20) + HALF(object, 0x26) + HALF(pose, 0x10);
    guMtxF2L(matrix, D_801969B0);
    PAIR(0xDA380000, (u32)D_801969B0++);
    rotationY = angle;
    func_00023018(D_801969B0, negativeQuarterTurn, rotationY, 0.0f);
    PAIR(0xDA380001, (u32)D_801969B0++);
    PAIR(0xDE000000, (u32)D_801CE9E0);
    while (1) {
        bit10 = func_0020C034(actor);
        if (!func_00205484(firstId, secondId, bit10, func_0020BFF8(actor),
                           poseId, index, &decoded)) {
            break;
        }
        if (pose->items[index] != 0 && !(decoded.record.field_14 & 4)) {
            bit8 = func_0020BFF8(actor);
            header = (DrawImage *)func_00206340(firstId, secondId, &decoded, bit8,
                                               func_0020C034(actor), variant, 0);
            stride = func_00201E38(header->format, header->width);
            if (header->format == 2) {
                header = (DrawImage *)((u8 *)header + 8 + stride * header->height);
                stride = func_00201E38(header->format, header->width);
            }
            itemAlpha = factor * pose->items[index] / 255;
            PAIR(0xFA000000, itemAlpha & 255);
            capacity = 4096 / stride;
            remaining = header->height;
            left = decoded.record.field_04;
            top = -decoded.record.field_08;
            right = decoded.record.field_0C + left;
            if (func_00043d1c(BYTE(actor, 0x4B), BYTE(actor, 0x4F)) == 2) {
                decoded.record.field_18 *= *(double *)0x801D00C8;
                decoded.record.field_1C *= *(double *)0x801D00C8;
            }
            if (orientation) {
                guScale(D_801969B0, -decoded.record.field_18,
                        decoded.record.field_1C * verticalScale, 1.0f);
            } else {
                guScale(D_801969B0, decoded.record.field_18,
                        decoded.record.field_1C * verticalScale, 1.0f);
            }
            PAIR(0xDA380000, (u32)D_801969B0++);
            accumulated = 0;
            strip = capacity;
            if (remaining < strip) {
                strip = remaining;
            }
            row = 0;
            func_002103EC(D_80197178, (short)left, (short)top, 0, 0);
            count = 2;
            func_002103EC(D_80197178 + 1, (short)right, (short)top,
                          (short)(decoded.record.field_0C - 1), 0);
            /* Vertex construction and packet emission are separate passes. */
            while (1) {
                int byteOffset = count * 16;
                CombatDrawVertex *firstVertex = (CombatDrawVertex *)((u8 *)D_80197178 + byteOffset);
                int nextRow = row + strip;
                int bottom = (short)(top - accumulated - strip);
                func_002103EC(firstVertex, (short)left, bottom, 0, (short)nextRow);
                count += 2;
                func_002103EC((CombatDrawVertex *)((u8 *)D_80197178 + byteOffset + 16), (short)right, bottom,
                              (short)decoded.record.field_0C, (short)nextRow);
                if (remaining == strip) {
                    break;
                }
                remaining -= strip;
                row = nextRow;
                accumulated += strip;
                if (remaining < strip) {
                    strip = remaining;
                }
            }
            remaining = header->height;
            strip = capacity;
            vertexCount = count;
            if (remaining < strip) {
                strip = remaining;
            }
            row = 0;
            PAIR(0x01000000 | FIELD(count, 12, 8) | FIELD(count, 1, 7),
                 (u32)D_80197178);
            D_80197178 += count;
            for (count = 0; count < vertexCount; count += 2) {
                /* Keep the row shared across formats, but each packed endpoint
                 * local to its two packet consumers. This preserves the pinned
                 * compiler's register allocation and final common store tail. */
                int rowField;
                CombatDrawCommand *sizeCommand;
                CombatDrawCommand *renderCommand;
                u32 renderWord;
                if (header->format == 3) {
                    int endField;
                    CombatDrawCommand *renderCommand;
                    u32 renderWord;
                    rowField = row * 4;
                    PAIR(0xFD180000 | FIELD(WIDTH() - 1, 0, 12), (u32)((u8 *)header + 8));
                    rowField &= 0xFFF;
                    PAIR(0xF5180000 | FIELD((WIDTH() * 2 + 7) >> 3, 9, 9), 0x07000000);
                    PAIR(0xE6000000, 0);
                    {
                        CombatDrawCommand *command = D_800E9BA0++;
                        u32 width;
                        command->first = 0xF4000000 | rowField;
                        width = FIELD((WIDTH() - 1) * 4, 12, 12);
                        endField = (row + strip - 1) * 4;
                        endField &= 0xFFF;
                        {
                            u32 loadEnd = endField | 0x07000000;
                            width |= loadEnd;
                            command->second = width;
                        }
                    }
                    renderCommand = D_800E9BA0++;
                    renderCommand->first = 0xE7000000;
                    renderCommand->second = 0;
                    ++D_800E9BA0;
                    renderWord = WIDTH();
                    renderWord *= 2;
                    renderWord = line_field(renderWord);
                    sizeCommand = D_800E9BA0;
                    renderCommand[1].first = renderWord | 0xF5180000;
                    renderCommand[1].second = 0;
                    D_800E9BA0 = sizeCommand + 1;
                    SIZE_ROW(sizeCommand, rowField);
                    sizeCommand->second = FIELD((WIDTH() - 1) * 4, 12, 12) | endField;
                    goto strip_triangles;
                } else if (header->format == 1) {
                    int endField;
                    CombatDrawCommand *loadCommand;

                CombatDrawCommand *renderCommand;
                u32 renderWord;
                    rowField = row * 4;
                    PAIR(0xFD880000 | FIELD(WIDTH() - 1, 0, 12), (u32)((u8 *)header + 8));
                    rowField &= 0xFFF;
                    PAIR(0xF5880000 | line_field(WIDTH()), 0x07000000);
                    loadCommand = D_800E9BA0;
                    D_800E9BA0 = loadCommand + 1;
                    loadCommand->first = 0xE6000000;
                    loadCommand->second = 0;
                    D_800E9BA0 = loadCommand + 2;
                    {
                        CombatDrawCommand *command = loadCommand + 1;
                        u32 width;
                        command->first = 0xF4000000 | rowField;
                        width = FIELD((WIDTH() - 1) * 4, 12, 12);
                        endField = (row + strip - 1) * 4;
                        endField &= 0xFFF;
                        {
                            u32 loadEnd = endField | 0x07000000;
                            width |= loadEnd;
                            command->second = width;
                        }
                    }
                    renderCommand = D_800E9BA0++;
                    renderCommand->first = 0xE7000000;
                    renderCommand->second = 0;
                    ++D_800E9BA0;
                    renderWord = WIDTH();
                    renderWord = line_field(renderWord);
                    loadCommand = D_800E9BA0;
                    renderCommand[1].first = renderWord | 0xF5880000;
                    renderCommand[1].second = 0;
                    D_800E9BA0 = loadCommand + 1;
                    SIZE_ROW(loadCommand, rowField);
                    loadCommand->second = FIELD((WIDTH() - 1) * 4, 12, 12) | endField;
                    goto strip_triangles;
                } else {
                    int endField;
                    CombatDrawCommand *renderCommand;
                    u32 renderWord;
                    rowField = row * 4;
                    PAIR(0xFD880000 | FIELD((WIDTH() >> 1) - 1, 0, 12), (u32)((u8 *)header + 8));
                    rowField &= 0xFFF;
                    PAIR(0xF5880000 | FIELD(((WIDTH() >> 1) + 7) >> 3, 9, 9), 0x07000000);
                    PAIR(0xE6000000, 0);
                    {
                        CombatDrawCommand *command = D_800E9BA0++;
                        u32 width;
                        command->first = 0xF4000000 | rowField;
                        width = FIELD((WIDTH() - 1) * 2, 12, 12);
                        endField = (row + strip - 1) * 4;
                        endField &= 0xFFF;
                        {
                            u32 loadEnd = endField | 0x07000000;
                            width |= loadEnd;
                            command->second = width;
                        }
                    }
                    renderCommand = D_800E9BA0++;
                    renderCommand->first = 0xE7000000;
                    renderCommand->second = 0;
                    ++D_800E9BA0;
                    renderWord = WIDTH();
                    renderWord >>= 1;
                    renderWord = line_field(renderWord);
                    sizeCommand = D_800E9BA0;
                    renderCommand[1].first = renderWord | 0xF5800000;
                    renderCommand[1].second = 0;
                    D_800E9BA0 = sizeCommand + 1;
                    SIZE_ROW(sizeCommand, rowField);
                    sizeCommand->second = FIELD((WIDTH() - 1) * 4, 12, 12) | endField;
                    goto strip_triangles;
                }
strip_triangles:
                PAIR(FIELD(count * 2, 16, 8) |
                     FIELD((count + 3) * 2, 8, 8) | FIELD((count + 1) * 2, 0, 8) | 0x06000000,
                     FIELD(count * 2, 16, 8) | (((u32)(count + 2) << 9) & 0xFE00) |
                     FIELD((count + 3) * 2, 0, 8));
                if (remaining == strip) {
                    break;
                }
                remaining -= strip;
                row += strip;
                if (remaining < strip) {
                    strip = remaining;
                }
            }
            PAIR(0xD8380002, 0x40);
        }
        ++index;
    }
    PAIR(0xD8380002, 0x40);
    return 0;
}
