typedef unsigned char u8;
typedef unsigned int u32;

/* Partial views of the observed sixteen-byte input and archive handle. */
typedef struct ResourceArchiveInput {
    u32 field_00;
    u32 field_04;
    u32 field_08;
    u32 field_0C;
} ResourceArchiveInput;

typedef struct ResourceArchiveRecord {
    u8 field_00;
    u8 pad_01[0x0F];
    u32 field_10;
} ResourceArchiveRecord;

typedef union ResourceArchiveInputOrCount {
    ResourceArchiveInput *input;
    u32 count;
} ResourceArchiveInputOrCount;

extern void func_0000B33C(u32 acquire);
extern void func_0000C204(void); /* Existing binding is RAM8007C204. */
extern ResourceArchiveRecord *func_0000F4E4(u32 key, const u8 *table,
                                          u32 offset, u32 size);
extern void func_0000BBC0(u32 key); /* Existing binding is RAM8007BBC0. */
extern int func_0000B3E4(ResourceArchiveRecord *record, u8 *scratch);
extern const u8 g_boot_resource_lzss_error_anchor[];

u32 func_0000B29C(ResourceArchiveInput *input)
{
    u8 scratch[0x130];
    ResourceArchiveRecord *record;
    ResourceArchiveInputOrCount state;

    state.input = input;
    func_0000B33C(1);
    func_0000C204();
    /* Archive table800AE0A8, relative to the existing800B0000 anchor. */
    record = func_0000F4E4(state.input->field_00,
                         g_boot_resource_lzss_error_anchor - 0x1F58,
                         state.input->field_08, state.input->field_0C);
    if (record == 0) {
        func_0000BBC0(state.input->field_00);
    }
    state.count = 0;

    while (func_0000B3E4(record, scratch) != 0) {
        state.count++;
        record->field_10 += *(u32 *)(scratch + 8);
    }
    record->field_00 = 0;
    func_0000B33C(0);
    return state.count;
}
