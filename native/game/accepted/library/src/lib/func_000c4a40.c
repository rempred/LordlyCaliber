typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef s32 (*ClassCallback)(u8, u8);

typedef struct Func000C4A40Entry {
    void *field_00;
    s16 field_04;
    s16 field_06;
    u32 field_08;
} Func000C4A40Entry;

typedef struct Func000C4A40ClassRecord {
    u8 unused_00[0x11];
    u8 field_11;
    u8 field_12;
    u8 unused_13[0x25];
} Func000C4A40ClassRecord;

void func_000c54c0(s32);
void func_000c4e5c(void *, void *, s16, s16);
u8 func_000bbad0(void);
u8 func_0016dec4(u8, u8);
extern u8 * volatile D_80196AF8;
extern void *g_func_000c4a40_dispatch_table[];
extern ClassCallback g_func_000c4a40_callbacks[];
extern Func000C4A40ClassRecord g_func_000c4a40_class_records[];

void func_000c4a40(void *arg0, s32 arg1)
{
    register u8 *output asm("$18") = arg0;
    u8 *data;
    u8 *state_entry;
    Func000C4A40Entry *entry;
    ClassCallback *callbacks;
    ClassCallback callback;
    s32 index;
    s32 count;
    s32 accepted;
    register u16 mode asm("$16") = arg1;

    func_000c54c0(0x2E1);
    {
        register u8 *initial_data asm("$2");
        initial_data = D_80196AF8;
        *(s16 *)(initial_data + 0x90) = mode;
    }
    mode &= 0xFFFF;
    entry = &((Func000C4A40Entry *)0x801EF728)[mode];
    func_000c4e5c(output, entry->field_00, entry->field_04, entry->field_06);

    __asm__ volatile ("# Keep the shared jump table's mode-9 C destination live; emits no instruction."
                      : : "X"(&&mode_9));
    __asm__ volatile ("# Keep the shared jump table's modes-0/1/5/6 C destination live; emits no instruction."
                      : : "X"(&&mode_0_1_5_6));
    __asm__ volatile ("# Keep the shared jump table's modes-2/3 C destination live; emits no instruction."
                      : : "X"(&&mode_2_3));
    __asm__ volatile ("# Keep the shared jump table's mode-7 C destination live; emits no instruction."
                      : : "X"(&&mode_7));
    __asm__ volatile ("# Keep the shared jump table's mode-8 C destination live; emits no instruction."
                      : : "X"(&&mode_8));
    __asm__ volatile ("# Keep the shared jump table's mode-10 C destination live; emits no instruction."
                      : : "X"(&&mode_10));
    if (mode < 11) {
        goto *g_func_000c4a40_dispatch_table[mode];
    }
    goto mode_done;

mode_9:
        if ((func_000bbad0() & 0xFF) == 0) {
            output[0x0B]++;
        }
        goto mode_done;

mode_0_1_5_6:
    {
        register Func000C4A40ClassRecord *class_record asm("$6");
        {
            register s32 class_offset asm("$2");
            register s32 state_flags asm("$3");
            register u8 *class_base asm("$4");
            register u8 *class_state_entry asm("$5");

            __asm__ volatile (
                "# Load the shared state pointer into $a0.\n"
                "lui %2,%%hi(D_80196AF8) # load the shared-state pointer address high half into $a0\n"
                "lw %2,%%lo(D_80196AF8)(%2) # dereference the shared-state pointer into $a0\n"
                "# Read the signed current-class index into $v1.\n"
                "lh %1,0x18A(%2) # load the signed current-class index into $v1\n"
                "# Form state + 0x1872 + index*2 in $a1.\n"
                "sll %0,%1,1 # multiply the current-class index by two for the state flag table\n"
                "addiu %0,%0,0x1872 # add the state flag table's byte offset\n"
                "addu %3,%2,%0 # form the address of the current class's state flag byte in $a1\n"
                "# Form the 0x38-byte class-record offset in $v0.\n"
                "sll %0,%1,3 # multiply the current-class index by eight\n"
                "subu %0,%0,%1 # reduce that product to seven times the index\n"
                "sll %0,%0,3 # multiply by eight again to obtain the 0x38-byte class-record offset\n"
                "# Replace $v1 with the state flag byte used by the C tests.\n"
                "lbu %1,0(%3) # load and zero-extend the current class's state flags into $v1\n"
                "# Materialize the class-record table base in $a0.\n"
                "lui %2,%%hi(g_func_000c4a40_class_records) # load the class-record table address high half\n"
                "addiu %2,%2,%%lo(g_func_000c4a40_class_records) # complete the class-record table base in $a0\n"
                : "=&r"(class_offset), "=&r"(state_flags),
                  "=&r"(class_base), "=&r"(class_state_entry));
            class_record = (Func000C4A40ClassRecord *)
                ((u32)class_offset + (u32)class_base);
            if ((state_flags & 0x20) == 0) {
                output[0x0B]++;
            }
            if ((class_state_entry[0] & 8) == 0) {
                output[0x0C]++;
            }
        }

        {
            register s32 current_index asm("$2");

            current_index = *(s16 *)(D_80196AF8 + 0x18A);
            if (current_index == 0) {
                output[8]++;
                for (count = 5; count < 10; count++) {
                    (output + count)[8]++;
                }
                output[0x0A]++;
                output[0x12]++;
                output[0x13]++;
                output[0x14]++;
                output[0x17]++;
            } else if (current_index >= 100) {
                output[8]++;
                for (count = 5; count < 11; count++) {
                    (output + count)[8]++;
                }
                output[9]++;
                output[0x18]++;
            } else {
                if ((func_0016dec4(class_record->field_11, class_record->field_12) & 0xFF) == 0) {
                    output[0x10]++;
                }
                output[9]++;
                output[0x0A]++;
                output[0x17]++;
                output[0x18]++;
            }
        }
    }
    goto mode_done;

mode_2_3:
        data = D_80196AF8;
        index = *(u16 *)(data + 0x18C);
        state_entry = data + 0x117C + (index * 54);
        if (((state_entry[1] & 8) == 0) && (state_entry[2] == 0xFF)) {
            output[9]++;
        }
        for (count = state_entry[0x20]; count < 10; count++) {
            (output + count)[0x0D]++;
        }
        goto mode_done;

mode_7:
        accepted = 0;
        count = 0;
        callbacks = g_func_000c4a40_callbacks;
        for (; count < 4; callbacks++) {
            {
                register u8 *callback_data asm("$2");

                callback_data = D_80196AF8;
                count++;
                index = callback_data[0x5E9];
            }
            callback = *callbacks;
            accepted += ((callback(index, index) & 0xFFFF) != 0);
        }
        for (; accepted < 4; accepted++) {
            (output + accepted)[0x0D]++;
        }
        goto mode_done;

mode_8:
    {
        register u8 *cursor asm("$17");

        count = 0;
        cursor = output;
        __asm__ volatile ("# Keep the mode-8 output cursor based at retail $s1=$s2; emits no instruction."
                          : : "r"(cursor));
        callbacks = g_func_000c4a40_callbacks;
        for (; count < 4; cursor++, count++, callbacks++) {
            register u8 *callback_data asm("$3");

            callback_data = D_80196AF8;
            if (callback_data[0x60C] != 0) {
                callback = *callbacks;
                index = callback_data[0x5E9];
                if ((callback(index, index) & 0xFFFF) != 0) {
                    continue;
                }
            }
            cursor[0x0B]++;
        }
    }
        goto mode_done;

mode_10:
        data = D_80196AF8;
        index = *(u16 *)(data + 0x18C);
        state_entry = (u8 *)((u32)(index * 54) + (u32)data);
        if (state_entry[0x117E] == 0xFF) {
            output[0x0B]++;
        }
        goto mode_done;

mode_done:
    count = 0;
    if (output[8] != 0) {
        do {
            count++;
        } while ((output + count)[8] != 0);
    }
    *(s32 *)(output + 0x3C) = count;
}
