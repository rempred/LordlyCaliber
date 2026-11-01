typedef unsigned char u8;

typedef struct AssignmentEntry {
    u8 bytes[54];
} AssignmentEntry;

typedef struct Data {
    u8 unk_0000[0x117C];
    AssignmentEntry entries[1];
} Data;

typedef struct CharacterEntry {
    u8 unk_00[0x11];
    u8 class_11;
    u8 class_12;
    u8 unk_13[0x25];
} CharacterEntry;

extern u8 func_0016DBD8(u8, u8);
extern int func_0016B088(u8, u8, u8, u8, int);
extern CharacterEntry D_80193BC0[];

int func_000bd154(int arg0)
{
    u8 values[5];
    register int index asm("$16");
    register int should_process asm("$2");
    u8 *clear_output;
    u8 *output;
    u8 *end;
    u8 *entry;
    u8 item;
    CharacterEntry *characters;
    CharacterEntry *character;
    u8 result;

    index = 4;
    clear_output = &values[4];
    arg0 &= 0xFFFF;
    entry = (*(Data **)0x80196AF8)->entries[arg0].bytes;
    do {
        *clear_output = 0;
        index--;
        clear_output--;
    } while (index >= 0);

    index = 0;
    characters = D_80193BC0;
    output = values;
    end = values + 5;
    do {
        item = (entry + index)[4];
        should_process = (item != 0xFF) & ((int)output < (int)end);
        asm volatile("# Hybrid scope: retail uses a branch-likely so only the rejected path increments here.\n"
                     "# If should_process in $v0 is zero, beql skips the C success block and its annulled\n"
                     "# delay slot increments the $s0 entry index; the accepted path increments at the call.\n"
                     "# Preserve the branch-likely and its rejected-path-only increment.\n"
                     ".set noreorder\n"
                     "beql %1,$0,1f # skip character processing when the item/output predicate is false\n"
                     "addiu %0,%2,1 # increment the entry index only in the taken likely-branch delay slot\n"
                     "# Restore normal assembler scheduling after the rejected-path branch.\n"
                     ".set reorder\n"
                     : "=r" (index) : "r" (should_process), "0" (index));
        if (item >= 100) {
            character = D_80193BC0;
        } else {
            character = (CharacterEntry *)((item * sizeof(CharacterEntry))
                                            + (int)characters);
        }
        result = func_0016DBD8(character->class_11,
                               character->class_12);
        index++;
        *output = result;
        output++;
        asm volatile("1: # join after rejecting an entry or storing its class-combination result\n"
                     "# The local label is the rejected-path join; it emits no instruction.\n");
    } while (index < 9);

    return func_0016B088(values[0], values[1], values[2], values[3],
                         values[4]) & 0xFF;
}
