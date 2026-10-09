typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef union ClassWord {
    s32 word;
    struct { u8 high[3]; u8 low; } bytes;
} ClassWord;

typedef struct RosterRow {
    void *links[3];
    void *partners[3];
    u8 unknown18[8];
    u16 field20;
    u8 unknown22[0x1E];
    u32 flags40;
    s32 unknown44;
    ClassWord class48;
    ClassWord variant4C;
    s32 handle50;
    s32 column54;
    s32 row58;
    u8 unknown5C[0x9A];
    u8 fieldF6;
    u8 unknownF7;
} RosterRow;

typedef struct RosterTable {
    u8 unknown00[0x1C4];
    RosterRow rows[20];
} RosterTable;

extern RosterTable *D_801CE8BC;
extern u8 D_801971F0[];
extern u16 D_8019532C[];
extern u8 D_80195480[];
extern s32 func_0020C0E8(RosterRow *);
extern u8 func_00043dc4(u8, u8);
extern void func_0020C908(RosterRow *, u8);
extern void func_0020CBDC(RosterRow *, s32);
extern void func_001F0F6C(void *);
extern void func_001F102C(void *);
extern RosterRow *func_0020C478(u32);
extern s32 func_0020C014(RosterRow *);
extern u16 func_0020C448(RosterRow *);
extern s32 func_0020BFF8(RosterRow *);
extern u32 func_002015C8(s32, s32, s32, s32);
extern void func_00207658(s32 *, s32 *, s32 *, s32 *, s32 *, u32);
extern void func_001F114C(RosterRow *);

/* The retail frame holds six parallel arrays of 18 words. They collect
 * distinct row descriptors, sort by the returned key, and submit equal-key
 * runs. Names retain offsets where the field meaning is not established. */
void func_002ACA3C(s32 selector)
{
    s32 classes[18];
    u32 keys[18];
    s32 tags[18];
    s32 flagsA[18];
    s32 flagsB[18];
    s32 variants[18];
    s32 member;
    s32 freeIndex;
    s32 finalIndex;
    s32 link;
    s32 special = 0;
    u8 *preset = D_801971F0 + selector * 25;
    /* Separate pointers keep the three phases' register lifetimes distinct. */
    RosterRow *unit;
    RosterRow *candidate;
    RosterRow *current;
    u32 count;
    u32 rowIndex;
    u32 index;
    u32 shift;
    u32 key;
    s32 tag;
    s32 flagA;
    s32 flagB;
    u32 run;

    for (member = 0; member < 5; member++) {
        if (preset[member + 2] == 0) continue;
        for (freeIndex = 0; freeIndex < 20; freeIndex++) {
            if (D_801CE8BC->rows[freeIndex].class48.word == 0) break;
        }
        unit = &D_801CE8BC->rows[freeIndex];
        if (func_0020C0E8(unit) &&
            func_00043dc4(unit->class48.bytes.low, unit->variant4C.bytes.low) == 2) {
            special = 1;
        }
        if (preset[member + 2] < 100U) {
            func_0020C908(unit, preset[member + 2]);
            unit->column54 = (u8)(preset[member + 7] % 3U);
            unit->row58 = (u8)(preset[member + 7] / 3U);
            func_0020CBDC(unit, 0);
        } else {
            func_0020C908(unit, 0);
            unit->column54 = (u8)(preset[member + 7] % 3U);
            unit->row58 = (u8)(preset[member + 7] / 3U);
            unit->fieldF6 = preset[member + 2];
            unit->field20 = D_8019532C[unit->fieldF6];
            if (D_80195480[unit->fieldF6] & 4) unit->flags40 |= 2;
            func_0020CBDC(unit, special);
        }
        for (link = 0; link < 3; link++) {
            if (unit->links[link]) func_001F0F6C(unit->links[link]);
            if (unit->partners[link]) func_001F102C(unit->partners[link]);
        }
    }

    count = 0;
    for (rowIndex = 0; rowIndex < 20; rowIndex++) {
        candidate = func_0020C478(rowIndex);
        if (!candidate || func_0020C014(candidate)) continue;
        tag = func_0020C448(candidate);
        flagB = func_0020BFF8(candidate);
        flagA = flagB;
        for (index = 0; index < count; index++) {
            if (candidate->class48.word == classes[index] && tag == tags[index] &&
                flagA == flagsA[index] && flagB == flagsB[index]) break;
        }
        if (index < count) continue;
        index = 0;
        key = func_002015C8(candidate->class48.word, candidate->variant4C.word, flagA, flagB);
        for (; index < count; index++) {
            if (key < keys[index]) break;
        }
        for (shift = count; shift > index; shift--) {
            classes[shift] = classes[shift - 1];
            variants[shift] = variants[shift - 1];
            keys[shift] = keys[shift - 1];
            tags[shift] = tags[shift - 1];
            flagsA[shift] = flagsA[shift - 1];
            flagsB[shift] = flagsB[shift - 1];
        }
        classes[shift] = candidate->class48.word;
        variants[shift] = candidate->variant4C.word;
        keys[shift] = key;
        tags[shift] = tag;
        flagsA[shift] = flagA;
        flagsB[shift] = flagB;
        count++;
    }

    if (count) {
        for (rowIndex = 0; rowIndex < count; rowIndex += run) {
            for (run = 1; run < count; run++) {
                if (keys[rowIndex + run] != keys[rowIndex]) break;
            }
            func_00207658(&classes[rowIndex], &variants[rowIndex],
                &flagsB[rowIndex], &flagsA[rowIndex], &tags[rowIndex], run);
        }
        for (finalIndex = 0; finalIndex < 20; finalIndex++) {
            current = func_0020C478(finalIndex);
            if (current && !func_0020C014(current)) func_001F114C(current);
        }
    }
}
