typedef struct ResourceNode {
    unsigned int field_00;
    void *field_04;
    unsigned int field_08;
    void *field_0C;
    struct ResourceNode *field_10;
    struct ResourceNode *field_14;
    struct ResourceNode *field_18;
} ResourceNode;

extern void func_000016C4(void *value);

void func_0000A1F8(ResourceNode *node)
{
    if (node != 0) {
        func_0000A1F8(node->field_10);
        func_0000A1F8(node->field_14);
        func_0000A1F8(node->field_18);
        if (node->field_0C != 0) {
            func_000016C4(node->field_04);
            node->field_04 = 0;
        }
    }
}
