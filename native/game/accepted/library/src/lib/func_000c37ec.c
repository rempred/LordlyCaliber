typedef unsigned char u8;
typedef signed int s32;

s32 func_000bbd50(s32);
s32 func_000bbd80(s32);

s32 func_000c37ec(s32 arg0, s32 arg1)
{
    register s32 result asm("$19");
    u8 class_a;
    u8 class_b;
    s32 saved_arg1;
    s32 mask_a;
    s32 mask_b;
    s32 kind;
    s32 intersection;
    s32 class_group;

    saved_arg1 = arg1;
    result = 0;
    class_a = arg0;
    mask_a = func_000bbd80(class_a);
    class_b = saved_arg1;
    mask_b = func_000bbd80(class_b);
    asm volatile("# Hybrid scope: this zero-byte barrier tells KMC that the result register remains live.\n"
                 "# It blocks constant folding of the retail increment and byte-masked return paths;\n"
                 "# it emits no instruction and leaves all calls, tests, branches, and expressions in C.\n"
                 : "=r" (result) : "0" (result));
    kind = mask_a & 0xFFFF;

    if (kind < 5) {
        goto small_kind;
    }
    if (kind == 0x20) {
        goto done;
    }
    if (kind >= 0x21) {
        goto done;
    }
    if (kind == 8) {
        goto kind_eight;
    }
    return result & 0xFF;

small_kind:
    if (kind >= 3) {
        goto done;
    }
    if (kind == 0) {
        goto done;
    }
    if (kind < 0) {
        goto done;
    }
    if (class_a != class_b) {
        if ((func_000bbd50(class_a) & 0xFFFF) == 0) {
            goto done;
        }
    }
    intersection = mask_a & mask_b;
    asm("# Hybrid scope: retail deliberately repeats mask_a & intersection after the conditional call.\n"
        "# KMC algebraically removes this redundant operation in pure C; this one `and` preserves it.\n"
        "# The earlier intersection, calls, tests, branch-likely, increment, and returns remain C.\n"
        "and %0,%1,%0 # intersect mask_a with the existing class-mask intersection a second time\n"
        : "=r" (intersection) : "r" (mask_a), "0" (intersection));
    if ((intersection & 0xFFFF) != 0) {
        result++;
        goto done;
    }
    return result & 0xFF;

kind_eight:
    class_group = func_000bbd50(class_a) & 0xFFFF;
    if (class_group != 5) {
        return result & 0xFF;
    }
    if ((func_000bbd50(class_b) & 0xFFFF) == class_group) {
        result = 1;
    }

done:
    return result & 0xFF;
}
