typedef unsigned int u32;

typedef struct ResourceNode {
    u32 field_00;
    void *field_04;
    u32 field_08;
    void *field_0C;
} ResourceNode;

typedef struct ResourceContext {
    u32 field_00;
    void *field_04;
    u32 field_08;
    u32 field_0C;
} ResourceContext;

extern void *func_00009CB4(void *node, u32 index);
extern u32 func_0002DEF4(u32 key);
extern void *func_00001330(u32 size);
extern void func_0002DFB8(void *buffer, u32 key);
extern u32 func_0000ABE0(void *buffer);
extern void func_0000A510(void *destination, void *source);

extern ResourceContext *g_boot_resource_context;
extern void *g_resource_context_output;

void func_00009EFC(ResourceNode *node)
{
    node->field_0C = func_00009CB4(node->field_0C, 0);
    if (g_boot_resource_context->field_04 == 0) {
        if (node->field_04 == 0) {
            node->field_08 = func_0002DEF4(node->field_00);
            if (node->field_08 != 0) {
                func_0002DFB8(
                    (node->field_04 = func_00001330(node->field_08)),
                    node->field_00);
            }
        }
        if (node->field_04 != 0) {
            u32 size;

            size = func_0000ABE0(node->field_04);
            g_boot_resource_context->field_08 = size;
            g_boot_resource_context->field_04 = func_00001330(size);
            func_0000A510(g_boot_resource_context->field_04, node->field_04);
            g_boot_resource_context->field_0C = 2;
        }
    }
    {
        ResourceContext *context = g_boot_resource_context;
        u32 output = context->field_08;

        /* KMC 2.7.2: keep context live through output's register allocation.
         * The late jump pass merges these identical stores and removes the
         * condition, leaving one unconditional load/store with output in v1.
         * A direct assignment reuses v0 for output. This is no null guard:
         * the field read above still requires a valid context.
         */
        if (context) {
            g_resource_context_output = (void *)output;
        } else {
            g_resource_context_output = (void *)output;
        }
    }
}
