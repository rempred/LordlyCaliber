typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

void func_000bc684(void);
void func_000bf5dc(void);
void func_000c54c0(s32);
extern u8 * volatile D_80196AF8;

s32 func_000c91a0(void)
{
    register s32 result asm("$16");
    register s32 selection asm("$4");
    register s32 original_selection asm("$17");
    u16 buttons;
    u16 repeat_buttons;
    register u8 *data asm("$6");
    u8 *current_data;

    data = D_80196AF8;
    buttons = *(u16 *)0x800E8100;
    selection = *(u16 *)(data + 0x5EA);
    original_selection = selection;
    result = 0;
    if (buttons & 0x1000) {
        result = 3;
    } else if (buttons & 0x8000) {
        result = 1;
    } else if (buttons & 0x4000) {
        func_000c54c0(1);
        result = 2;
    } else {
        repeat_buttons = *(u16 *)0x800E8700;
        if (repeat_buttons & 0x800) {
            if (selection & 3) {
                register s32 adjusted_selection asm("$2");

                adjusted_selection = selection - 1;
                *(u16 *)(data + 0x5EA) = adjusted_selection;
            }
        } else if (repeat_buttons & 0x400) {
            if ((selection & 3) != 3) {
                register s32 adjusted_selection asm("$2");

                adjusted_selection = selection + 1;
                *(u16 *)(data + 0x5EA) = adjusted_selection;
            }
        } else if (repeat_buttons & 0x200) {
            register s32 row_candidate asm("$2");

            row_candidate = selection - 4;
            if (row_candidate < 0) {
                goto input_done;
            }
            asm volatile("# Hybrid scope: zero-byte barrier keeps the valid row-above store out of a branch-likely delay slot.\n");
            *(u16 *)(data + 0x5EA) = row_candidate;
            goto input_done;
        } else if (repeat_buttons & 0x100) {
            register s32 candidate asm("$2");
            register s32 current_row asm("$5");
            register s32 row_count asm("$3");

            asm volatile("# Hybrid scope: zero-byte barrier keeps the following count load out of the input-test delay slot.\n");
            row_count = *(u16 *)(data + 0x1BFC);
            candidate = row_count - 1;
            asm volatile("# Hybrid scope: bgez keeps a nonnegative last-row index; its delay slot computes the current row.\n"
                         "# Only a negative index is replaced with count + 2, then both paths divide the candidate by four.\n"
                         "# Preserve the signed branch and its current-row delay slot.\n"
                         ".set noreorder\n"
                         "bgez %0,1f # keep row_count - 1 when the last-row index is nonnegative\n"
                         "srl %1,%2,2 # compute selection / 4 as the current row in the branch delay slot\n"
                         "addiu %0,%3,2 # replace a negative last-row index with row_count + 2\n"
                         "1: # join after selecting the nonnegative row candidate\n"
                         "sra %0,%0,2 # divide the chosen candidate by four with signed rounding\n"
                         "# Restore normal assembler scheduling after the row calculation.\n"
                         ".set reorder\n"
                         : "=r" (candidate), "=r" (current_row)
                         : "r" (selection), "r" (row_count), "0" (candidate));
            if (current_row < candidate) {
                register s32 adjusted_selection asm("$2");

                adjusted_selection = selection + 4;
                *(u16 *)(data + 0x5EA) = adjusted_selection;
            }
        }
    }
input_done:
    {
        register s32 current_selection asm("$5");
        register s32 comparison asm("$2");
        register s32 count asm("$4");

        current_data = D_80196AF8;
        current_selection = *(u16 *)(current_data + 0x5EA);
        asm volatile("# Hybrid scope: zero-byte binding keeps the current selection in retail's $a1.\n"
                     : "=r" (current_selection) : "0" (current_selection));
        comparison = original_selection & 0xFF;
        asm volatile("# Hybrid scope: zero-byte binding keeps the masked prior selection in retail's $v0.\n"
                     : "=r" (comparison) : "0" (comparison));
        if (comparison == current_selection) {
            goto return_result;
        }
        asm volatile("# Hybrid scope: zero-byte barrier preserves the retail equality-branch layout.\n");
        count = *(u16 *)(current_data + 0x1BFC);
        comparison = (u32)current_selection < (u32)count;
        if (comparison != 0) {
            goto selection_valid;
        }
        comparison = count - 1;
        *(u16 *)(current_data + 0x5EA) = comparison;
        current_data = D_80196AF8;
selection_valid:
        {
            register s32 lookup asm("$2");

            current_selection = *(u16 *)(current_data + 0x5EA);
            lookup = current_selection * 2;
            asm("# Hybrid scope: this one addu forms retail's base-plus-row offset with $v1 before $v0.\n"
                "# The selection load, doubling shift, table load, mask, and destination store remain C.\n"
                "addu %0,%1,%0 # add the doubled selection offset to the shared-state base pointer\n"
                : "=r" (lookup) : "r" (current_data), "0" (lookup));
            *(s16 *)(current_data + 0x5E8) = *(u16 *)(lookup + 0x1C0E) & 0x7F;
        }
        func_000bc684();
        {
            register u8 *flag_data asm("$2");

            flag_data = D_80196AF8;
            if (*(u16 *)(flag_data + 0x130) & 0x200) {
                func_000bf5dc();
                asm volatile("# Hybrid scope: zero-byte barrier keeps the C return-value move after the helper call.\n");
            }
        }
    }
return_result:
    return result;
    asm("# Hybrid register bindings above emit no instructions.\n"
        "# $s0 retains the return code, $s1 retains the original selection, $a0 holds selection arithmetic,\n"
        "# and $a2 holds the initial data pointer across the input-dispatch C blocks.\n"
        "# Scoped $v0/$v1/$a0/$a1 bindings reproduce retail temporaries without injecting instructions.\n");
}
