typedef unsigned int u32;

typedef struct Node28 {
    u32 field_00;
    void *field_04;
    u32 field_08;
    void *field_0C;
    u32 field_10;
    struct Node28 *field_14;
    struct Node28 *field_18;
} Node28;

extern void *func_80071288(u32 bytes);
extern void func_00023780(void *destination, u32 bytes);
extern Node28 *g_node28_current;

Node28 *boot_resource_node_insert_find(Node28 *node, u32 key)
{
    if (node != 0) {
        if (key == node->field_00) {
            g_node28_current = node;
            return node;
        }
        if (key < node->field_00) {
            node->field_14 = boot_resource_node_insert_find(node->field_14, key);
        } else {
            node->field_18 = boot_resource_node_insert_find(node->field_18, key);
        }
    } else {
        node = func_80071288(0x1C);
        g_node28_current = node;
        func_00023780(node, 0x1C);
        node->field_00 = key;
    }
    return node;
}

