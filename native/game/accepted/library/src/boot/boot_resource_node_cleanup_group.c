typedef struct ChildNode24 {
    unsigned int field_00;
    void *field_04;
    unsigned int field_08;
    void *field_0C;
    struct ChildNode24 *field_10;
    struct ChildNode24 *field_14;
} ChildNode24;

extern void func_000016C4(void *pointer);

typedef struct ResourceNode28 {
    unsigned int field_00;
    void *field_04;
    unsigned int field_08;
    ChildNode24 *field_0C;
    struct ResourceNode28 *field_10;
    struct ResourceNode28 *field_14;
    struct ResourceNode28 *field_18;
} ResourceNode28;

extern ChildNode24 *boot_resource_node_recursive_child_free(ChildNode24 *node);
void boot_resource_node_recursive_field0c_rewrite(ResourceNode28 *node)
{
    if (node != 0) {
        boot_resource_node_recursive_field0c_rewrite(node->field_10);
        boot_resource_node_recursive_field0c_rewrite(node->field_14);
        boot_resource_node_recursive_field0c_rewrite(node->field_18);
        node->field_0C = boot_resource_node_recursive_child_free(node->field_0C);
    }
}

ChildNode24 *boot_resource_node_recursive_child_free(ChildNode24 *node)
{
    if (node != 0) {
        node->field_10 = boot_resource_node_recursive_child_free(node->field_10);
        node->field_14 = boot_resource_node_recursive_child_free(node->field_14);
        func_000016C4(node->field_04);
        func_000016C4(node);
        node = 0;
    }
    return node;
}

void boot_resource_node_recursive_key_field_clear(ChildNode24 *node, unsigned int key)
{
    if (node != 0) {
        if (key < node->field_00) {
            boot_resource_node_recursive_key_field_clear(node->field_10, key);
        } else if (node->field_00 < key) {
            boot_resource_node_recursive_key_field_clear(node->field_14, key);
        } else {
            func_000016C4(node->field_04);
            node->field_04 = 0;
            node->field_08 = 0;
            node->field_0C = 0;
        }
    }
}

