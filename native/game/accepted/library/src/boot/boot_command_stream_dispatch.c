typedef unsigned int u32;

typedef struct ResourceNode28 {
    u32 field_00;
    u32 *field_04;
    u32 field_08;
    void *field_0C;
    struct ResourceNode28 *field_10;
    struct ResourceNode28 *field_14;
    struct ResourceNode28 *field_18;
} ResourceNode28;

typedef struct ResourceContext {
    u32 field_00;
    void *field_04;
    u32 field_08;
    u32 field_0C;
} ResourceContext;

extern ResourceNode28 *g_command_resource_root;
extern ResourceNode28 *g_node28_current;
extern ResourceContext *g_boot_resource_context;
extern void *g_resource_context_output;
extern ResourceNode28 *boot_resource_node_insert_find(ResourceNode28 *node, u32 key);
extern ResourceNode28 *boot_resource_node_payload_materialize(ResourceNode28 *node);
extern void boot_resource_node_context_materialize(ResourceNode28 *node, u32 mode);
extern void func_00009EFC(ResourceNode28 *node);
extern void boot_resource_node_overlay_context_materialize(ResourceNode28 *node);
extern void func_000016C4(void *pointer);

#define NEXT_WORD(cursor) \
    ((cursor) = (char *)(((u32)(cursor) + 3) & ~3U) + 4, *(u32 *)((cursor) - 4))

/* KMC's legacy ellipsis parameter spills all four argument words. The original
 * first word is passed directly to the initial lookup: an early local copy
 * extends its register lifetime and moves the retail pre-frame load/stores. */
void *boot_command_stream_dispatch(__builtin_va_alist)
u32 __builtin_va_alist; ...
{
    u32 key;
    char *cursor;
    ResourceNode28 *node;
    u32 result;
    u32 index;

    g_command_resource_root = boot_resource_node_insert_find(g_command_resource_root, __builtin_va_alist);
    cursor = (char *)__builtin_next_arg();
    key = NEXT_WORD(cursor);
    while (key < (u32)-23) {
        ResourceNode28 *current = g_node28_current;
        boot_resource_node_payload_materialize(current);
        current->field_10 = boot_resource_node_insert_find(current->field_10, current->field_04[key]);
        key = NEXT_WORD(cursor);
    }

    node = g_node28_current;
    result = 0;
    switch (key) {
    case (u32)-4:
    case (u32)-3:
        boot_resource_node_context_materialize(node, *(u32 *)cursor);
        break;
    case (u32)-6:
    case (u32)-5:
        boot_resource_node_context_materialize(node, (u32)-22);
        break;
    case (u32)-10:
    case (u32)-9:
        func_00009EFC(node);
        break;
    case (u32)-14:
    case (u32)-13:
        boot_resource_node_overlay_context_materialize(node);
        break;
    case (u32)-1:
        boot_resource_node_payload_materialize(node);
        result = (u32)node->field_04;
        g_resource_context_output = (void *)node->field_08;
        break;
    case (u32)-12:
    case (u32)-11:
    case (u32)-8:
    case (u32)-7:
    case (u32)-2:
        result = node->field_08 >> 2;
        for (index = 0; index < result; index++) {
            node->field_10 = boot_resource_node_insert_find(node->field_10, node->field_04[index]);
            boot_resource_node_payload_materialize(g_node28_current);
            switch (key) {
            case (u32)-8:
            case (u32)-7:
                boot_resource_node_context_materialize(g_node28_current, (u32)-22);
                break;
            case (u32)-12:
            case (u32)-11:
                func_00009EFC(node);
                break;
            case (u32)-16:
            case (u32)-15:
                boot_resource_node_overlay_context_materialize(node);
                break;
            }
            if ((key == (u32)-8) | (key == (u32)-12)) {
                func_000016C4(node->field_04);
                node->field_04 = 0;
            }
        }
        break;
    }
    if (result == 0) {
        result = (u32)g_boot_resource_context->field_04;
    }
    switch (key) {
    case (u32)-14:
    case (u32)-10:
    case (u32)-9:
    case (u32)-6:
    case (u32)-4:
    case (u32)-3:
        func_000016C4(node->field_04);
        node->field_04 = 0;
        break;
    }
    return (void *)result;
}
