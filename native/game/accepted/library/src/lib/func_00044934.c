typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unk_00[0x15];
    u8 field_15;
    u8 field_16;
    u8 field_17;
    u8 field_18;
    u8 related_class_19;
    u8 unk_1A[0x2E];
} ClassEntry;

int func_00044934(u8 arg0, u8 arg1, int arg2)
{
    int offset;
    u8 threshold;
    u8 selected_class;

    selected_class = arg0;
    if (((ClassEntry *)0x80187C40)[selected_class & 0xFF].related_class_19 != (arg1 & 0xFF)) {
        selected_class = arg1;
    }

    /* Keep the byte offset: direct array indexing makes KMC retain a table
     * base register and changes the retail branch/addressing sequence. */
    offset = (selected_class & 0xFF) * sizeof(ClassEntry);
    threshold = ((ClassEntry *)(offset + 0x80187C40))->field_16;
    if (threshold != 0xFF && (unsigned int)(arg2 & 0xFF) < threshold) {
        selected_class = ((ClassEntry *)(offset + 0x80187C40))->field_15;
    } else {
        offset = (selected_class & 0xFF) * sizeof(ClassEntry);
        threshold = ((ClassEntry *)(offset + 0x80187C40))->field_18;
        if (threshold != 0xFF && (unsigned int)(arg2 & 0xFF) < threshold) {
            selected_class = ((ClassEntry *)(offset + 0x80187C40))->field_17;
        }
    }

    return selected_class & 0xFF;
}
