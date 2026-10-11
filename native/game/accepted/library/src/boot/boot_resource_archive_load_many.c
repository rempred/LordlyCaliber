typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

typedef struct ResourceArchiveInput {
    u32 field_00, field_04, field_08, field_0C;
} ResourceArchiveInput;

typedef struct ResourceArchiveHandle {
    u8 field_00;
    u8 pad_01[15];
    u32 field_10;
} ResourceArchiveHandle;

/* Sixteen-byte request entries; only these accesses are established here. */
typedef struct ResourceArchiveRequest {
    const u8 *name;
    /* Local qualifiers preserve the observed result-store order. They do not
     * identify hardware or a complete original request API. */
    volatile s32 record_index;
    volatile u32 field_08;
    u32 field_0C;
} ResourceArchiveRequest;

extern void func_0000B33C(u32 acquire);
extern void func_0000C204(void); /* RAM8007C204, not symbol-text arithmetic. */
extern ResourceArchiveHandle *func_0000F4E4(u32 key, const u8 *table, u32 offset, u32 size);
extern void func_0000BBC0(u32 key); /* RAM8007BBC0. */
extern s32 func_0000B3E4(ResourceArchiveHandle *handle, u8 *scratch);
extern s32 func_0000F47C(const u8 *name, const u8 *record_name);
extern void func_0000BC8C(ResourceArchiveHandle *handle, u8 *scratch);
extern void func_0000BF90(const u8 *name, const u8 *message);
extern void func_0000BFF4(u32 key, const u8 *message); /* RAM8007BBF4. */
extern const u8 g_boot_resource_lzss_error_anchor[];
extern u32 g_resource_archive_result[]; /* Observed pair800AF360/800AF364. */

void boot_resource_archive_load_many(ResourceArchiveInput *input,
                                    ResourceArchiveRequest *requests, s32 count)
{
    u8 scratch[0x130];
    ResourceArchiveHandle *handle;
    u32 position;
    s32 index;
    s32 record_index;
    s32 total;

    func_0000B33C(1);
    index = 0;
    func_0000C204();
    total = count;
    for (; index < total; index++) {
        requests[index].field_08 = 0;
        requests[index].field_0C = 0;
    }

    handle = func_0000F4E4(input->field_00,
                           g_boot_resource_lzss_error_anchor - 0x1F58,
                           input->field_08, input->field_0C);
    if (handle == 0) {
        func_0000BBC0(input->field_00);
    }
    record_index = 0;

    while (func_0000B3E4(handle, scratch) != 0) {
        position = handle->field_10;
        for (index = 0; index < total; index++) {
            if (requests[index].name == 0
                    ? requests[index].record_index == record_index
                    : func_0000F47C(requests[index].name, scratch + 0x16) == 0) {
                if (*(u32 *)(scratch + 0x0C) != 0) {
                    u32 second_result;
                    func_0000BC8C(handle, scratch);
                    requests[index].field_08 = g_resource_archive_result[0];
                    /* Preserve the second result read before the index store. */
                    second_result = *(volatile u32 *)(g_resource_archive_result + 1);
                    requests[index].record_index = record_index;
                    requests[index].field_0C = second_result;
                }
                break;
            }
        }
        if (index < total && --count == 0) {
            break;
        }
        record_index++;
        handle->field_10 = position + *(u32 *)(scratch + 8);
    }
    /* The assignment result is the release argument; KMC schedules the real
     * close store into the two exit delay slots in this expression form. */
    func_0000B33C(handle->field_00 = 0);

    index = 0;
    count = 0;
    for (; index < total; index++) {
        if (requests[index].field_08 == 0) {
            count++;
            func_0000BF90(requests[index].name, g_boot_resource_lzss_error_anchor - 0x1F54);
        }
    }
    if (count != 0) {
        func_0000BFF4(input->field_00, g_boot_resource_lzss_error_anchor - 0x1F38);
    }
}
