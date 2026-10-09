typedef unsigned int u32;

typedef struct ContextNode24 {
    u32 field_00;
    void *field_04;
    u32 field_08;
    u32 field_0C;
    struct ContextNode24 *field_10;
    struct ContextNode24 *field_14;
} ContextNode24;

/* The separate search body accesses +0x14/+0x18. Its pointed storage view
 * differs from the primary helper's 24-byte allocation and +0x10/+0x14 links. */
typedef struct SlotSearchNode {
    u32 field_00;
    u32 field_04;
    u32 field_08;
    u32 field_0C;
    u32 field_10;
    struct SlotSearchNode *field_14;
    struct SlotSearchNode *field_18;
} SlotSearchNode;

extern void *func_80071288(u32 bytes);
extern ContextNode24 *g_boot_resource_context;

ContextNode24 *boot_resource_node_recursive_insert_slot_search(ContextNode24 *node, u32 key)
{
    if (node != 0) {
        if (key == node->field_00) {
            g_boot_resource_context = node;
            return node;
        }
        if (key < node->field_00) {
            node->field_10 = boot_resource_node_recursive_insert_slot_search(node->field_10, key);
        } else {
            node->field_14 = boot_resource_node_recursive_insert_slot_search(node->field_14, key);
        }
    } else {
        node = func_80071288(0x18);
        g_boot_resource_context = node;
        node->field_00 = key;
        node->field_0C = 0;
        node->field_04 = 0;
        node->field_08 = 0;
        node->field_14 = 0;
        node->field_10 = 0;
    }
    return node;
}

static SlotSearchNode **func_0000A160(SlotSearchNode **slot, u32 key)
{
    while (*slot != 0) {
        SlotSearchNode *node = *slot;
        if (key == node->field_00) {
            break;
        }
        if (node->field_00 < key) {
            slot = &node->field_18;
        } else {
            slot = &node->field_14;
        }
    }
    return slot;
}
