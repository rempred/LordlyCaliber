typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;

extern u8 D_801969B8[], D_801971F0[], D_8018F481[], D_80197AE6[];
extern u16 D_801951CC[][18], D_801951F0;
extern u32 *D_801F0CB0[];
extern s32 func_801DD244(void);
extern void func_80093380(void *, s32);
extern void func_8016FE78(s32, s32);
extern void func_0012EA80_row(s32, f32 *);
extern void func_801CC480(void *, f32, f32, f32, s32);

void func_001307D4(void)
{
    /* Retail reserves a 16-byte stack slot for each coordinate buffer. */
    f32 position0[4];
    f32 position1[4];
    f32 position2[4];
    s32 special = 0;
    s32 selector;
    /* Reuse this counter across resets/search for the KMC register allocation. */
    s32 recordIndex;
    s32 i;
    u8 *cursor;
    u8 *cursor0;
    u8 *cursor1;
    u8 *record;
    u32 **units;
    u32 *unit;
    u32 *firstUnit;
    u8 stage0, stage1, stage2;
    s32 isStage0, isStage1, isStage2;

    if (func_801DD244()) {
        u32 clearMask0;
        u32 clearMask1;
        recordIndex = 0;
        cursor0 = D_801969B8 + 1;
        do {
            if (!(*cursor0 & 0x80)) {
                *cursor0 &= 0xFE;
            }
            recordIndex++;
            cursor0 += 0xB;
        } while (recordIndex < 10);
        func_80093380(D_801969B8, 0x6E);
        recordIndex = 0;
        cursor1 = D_801971F0 + 1;
        do {
            if ((*cursor1 & 1) && (*cursor1 & 2)) {
                *cursor1 &= 0xFD;
            }
            recordIndex++;
            cursor1 += 0x19;
        } while (recordIndex < 30);
        recordIndex = 0;
        /* Explicit masks preserve their setup before the table address. */
        clearMask0 = ~0x40U;
        clearMask1 = ~0x80U;
        units = D_801F0CB0;
        do {
            u32 flags;
            /* Share the later call argument's real pointer lifetime. */
            firstUnit = *units;
            flags = firstUnit[0];
            flags &= clearMask0;
            flags &= clearMask1;
            firstUnit[0] = flags;
            recordIndex++;
            units++;
        } while (recordIndex < 30);
    }

    stage0 = D_8018F481[0];
    isStage0 = (stage0 == 0x3F) | (stage0 == 0x41);
    if ((stage0 == 0x40) | (stage0 == 0x42)) {
        isStage0 = 1;
    }
    if (isStage0) {
        if (stage0 == 0x3F) {
            if (D_80197AE6[0] == 1) {
                selector = -1;
                func_8016FE78(0xB0, 0);
            } else if (D_80197AE6[0] == 2) {
                selector = 1;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 3) {
                selector = 1;
                special = 0;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 4) {
                selector = 3;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 5) {
                selector = 3;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 6) {
                selector = 3;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 7) {
                selector = 3;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else if (D_80197AE6[0] == 8) {
                selector = 3;
                special = 1;
                func_8016FE78(0xB0, 1);
            } else {
                selector = -1;
                func_8016FE78(0xB0, 0);
            }
            func_0012EA80_row(selector, position0);
            if (selector != -1) {
                D_801951CC[selector][0] = (D_801951CC[selector][0] & 0xFFFB) | 2;
            }
        } else {
            func_0012EA80_row(-1, position0);
          }
    } else {
        func_0012EA80_row(-1, position0);
    }

    firstUnit = D_801F0CB0[0];
    recordIndex = 0;
    cursor = D_801969B8;
    do {
        record = cursor;
        if (record[1] == 0) {
            recordIndex++;
        } else {
            if (record[2] == 0) {
                break;
            }
            recordIndex++;
        }
        cursor = record + 0xB;
    } while (recordIndex < 10);

    if (recordIndex != 10) {
        for (i = 0; i < 5; i++) {
            /* Group base + index to retain the native ADDU operand order. */
            u8 index = (record + i)[2];
            u32 *selectedUnit;
            if (index == 0xFF) {
                continue;
            }
            selectedUnit = D_801F0CB0[index];
            func_801CC480(selectedUnit,
                position0[0], position0[1], position0[2], i == 0);
        }
        record[1] |= 4;
    } else {
        func_801CC480(firstUnit, position0[0], position0[1], position0[2], 1);
    }

    stage1 = D_8018F481[0];
    isStage1 = (stage1 == 0x3F) | (stage1 == 0x41);
    if ((stage1 == 0x40) | (stage1 == 0x42)) {
        isStage1 = 1;
    }
    if (isStage1) {
        if (special) {
            unit = D_801F0CB0[1];
            func_0012EA80_row(1, position1);
            func_801CC480(unit, position1[0], position1[1], position1[2], 1);
            D_801951F0 = (D_801951F0 & 0xFFFB) | 2;
        }
    }

    stage2 = D_8018F481[0];
    isStage2 = (stage2 == 0x3F) | (stage2 == 0x41);
    if ((stage2 == 0x40) | (stage2 == 0x42)) {
        isStage2 = 1;
    }
    if (isStage2) {
        if (stage2 == 0x40) {
            unit = D_801F0CB0[7];
            func_0012EA80_row(1, position2);
            func_801CC480(unit, position2[0], position2[1], position2[2], 1);
            D_801951F0 = (D_801951F0 & 0xFFFB) | 2;
        }
    }
}

