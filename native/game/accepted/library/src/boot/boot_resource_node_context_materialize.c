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

typedef struct ResourceDescriptor {
    u32 field_00;
    u32 field_04;
    void *field_08;
    u32 field_0C;
} ResourceDescriptor;

extern void *func_00009CB4(void *node, u32 index);
extern u32 func_0002DEF4(u32 key);
extern void *func_00001330(u32 bytes);
extern void func_0002DFB8(void *destination, u32 key);
extern u32 func_0000B29C(ResourceDescriptor *input);
extern void func_8007ACB0(ResourceDescriptor *input, ResourceDescriptor *output, u32 mode);
extern ResourceContext *g_boot_resource_context;
extern void *g_resource_context_output;

void boot_resource_node_context_materialize(ResourceNode *node, u32 mode)
{
    ResourceDescriptor input;
    ResourceDescriptor output;
    u32 index;
    u32 limit;

    input.field_00 = 0;
    input.field_08 = node->field_04;
    input.field_0C = node->field_08;
    if (mode == (u32)-22) {
        if (node->field_04 == 0) {
            node->field_08 = func_0002DEF4(node->field_00);
            if (node->field_08 != 0) {
                func_0002DFB8(
                    (node->field_04 = func_00001330(node->field_08)),
                    node->field_00);
            }
        }
        input.field_08 = node->field_04;
        input.field_0C = node->field_08;
        index = 0;
        limit = func_0000B29C(&input);
    } else {
        index = mode;
        limit = index + 1;
    }

    for (; index < limit; index++) {
        node->field_0C = func_00009CB4(node->field_0C, index);
        if (g_boot_resource_context->field_04 == 0) {
            if (mode != (u32)-22) {
                if (node->field_04 == 0) {
                    node->field_08 = func_0002DEF4(node->field_00);
                    if (node->field_08 != 0) {
                        func_0002DFB8(
                            (node->field_04 = func_00001330(node->field_08)),
                            node->field_00);
                    }
                }
                input.field_08 = node->field_04;
                input.field_0C = node->field_08;
            }
            output.field_00 = 0;
            output.field_04 = index;
            func_8007ACB0(&input, &output, 1);
            g_boot_resource_context->field_04 = output.field_08;
            g_boot_resource_context->field_08 = output.field_0C;
            g_boot_resource_context->field_0C = 1;
        }
    }
    {
        ResourceContext *context = g_boot_resource_context;
        u32 value = context->field_08;
        /* The accepted sibling's late-merged branch keeps the context live
         * through allocation of this output value in v1. */
        if (context) {
            g_resource_context_output = (void *)value;
        } else {
            g_resource_context_output = (void *)value;
        }
    }
}


