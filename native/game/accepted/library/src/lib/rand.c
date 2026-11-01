typedef unsigned int u32;

extern volatile u32 g_rand_state;

int func_0002CBCC(void)
{
    register volatile u32 *state_ptr asm("$3") = &g_rand_state;
    register u32 current asm("$14") = *state_ptr;
    register u32 product asm("$15");
    register u32 next asm("$25");
    register u32 result asm("$2");

    asm volatile(
        "addiu $1,$0,0x4E6D\n"
        "multu %1,$1\n"
        "mflo %0"
        : "=r"(product)
        : "r"(current)
        : "$1", "hi", "lo");
    next = product + 0x3039;
    asm volatile("sw %0,0(%1)" : : "r"(product), "r"(state_ptr) : "memory");
    result = next >> 16;
    asm volatile("sw %0,0(%1)" : : "r"(next), "r"(state_ptr) : "memory");
    return (int)(result & 0x7FFF);
}

asm(".word 0");
asm(".size func_0002CBCC,52");
