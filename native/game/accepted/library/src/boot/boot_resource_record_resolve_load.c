typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern void *func_00001330(u32 size);
extern void *func_0000F4E4(void *scratch, const u8 *template_data, void *buffer, u32 size);
extern void func_0000F450(void *scratch, const u8 *path);
extern u32 func_0000BE98(void *context, void *buffer, u32 size, u32 record_value,
                       void *scratch_or_zero, u32 match_count_or_flag);
extern int func_00092F50(const u8 *record_suffix, const void *directory_entry, u32 limit);
extern void func_0000BF90(const u8 *message, void *scratch);
extern void func_0000BFC0(const u8 *message);

extern const u8 g_resource_template[];
extern const u8 g_resource_default_path[];
extern const u8 g_resource_result_equal[];
extern const u8 g_resource_type_4000[];
extern const u8 g_resource_missing_entry[];
extern const u8 g_resource_other_type[];
extern void *g_resource_directory_table[];

void func_0000BC8C(void *context, u8 *record)
{
    /* This function observes only the offsets used below; the record's complete
       layout and the reporting fields' meanings remain unresolved. */
    u8 scratch[0x108];
    void *buffer;
    void **directory;
    u32 match_count;
    const u8 *path;
    const u8 *message;
    u32 status;
    u32 type;

    path = record + 0x16;
    if (record[0] & 0x80) {
        buffer = func_0000F4E4(scratch, g_resource_template,
                             func_00001330(*(u32 *)(record + 0x0C)),
                             *(u32 *)(record + 0x0C));
        func_0000BE98(context, buffer, *(u32 *)(record + 0x0C),
                     *(u32 *)(record + 0x08), 0, record[0] & 0x7F);
        *(u8 *)buffer = 0;
        return;
    }

    if (record[0x16] == '/') {
        u8 flags = record[0x11C];
        path = record + 0x17;
        if ((flags == 'K') | (flags == 'X')) {
            u8 character;
            do {
                character = *path++;
            } while ((character != 0) & (character != '/'));
            if (character == 0 || *path == 0) {
                path = g_resource_default_path;
            }
        }
    }
    func_0000F450(scratch, path);
    type = *(u16 *)(record + 0x124) & 0xF000;
    if (type == 0x8000) {
        match_count = 0;
        directory = g_resource_directory_table;
        for (;;) {
            void *entry = *directory;
            if (entry == 0) {
                message = g_resource_missing_entry;
                goto report;
            }
            status = func_00092F50(record + 1, entry, 5);
            directory++;
            if (!status) {
                break;
            }
            match_count++;
        }
        buffer = func_0000F4E4(scratch, g_resource_template,
                             func_00001330(*(u32 *)(record + 0x0C)),
                             *(u32 *)(record + 0x0C));
        {
            /* Keeping the first guard as a Boolean-result phase prevents KMC's
               first CSE pass from deleting the later retail null check. */
            u32 available = buffer != 0;
            if (available != 1) {
                return;
            }
        }
        status = func_0000BE98(context, buffer, *(u32 *)(record + 0x0C),
                              *(u32 *)(record + 0x08), scratch, match_count);
        /* Clear before the repeated guard so KMC places this real store in the
           branch delay slot, after the dispatch-result copy. */
        *(u8 *)buffer = 0;
        if ((u32)buffer == 0) {
            return;
        }
        if (*(u32 *)(record + 0x118) == 0 ||
            status == *(u16 *)(record + 0x116)) {
            return;
        }
        message = g_resource_result_equal;
    } else if (type == 0x4000) {
        func_0000BFC0(g_resource_type_4000);
        return;
    } else {
        message = g_resource_other_type;
    }
report:
    func_0000BF90(message, scratch);
}
