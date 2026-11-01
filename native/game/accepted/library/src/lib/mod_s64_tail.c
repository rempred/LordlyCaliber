asm(
    ".macro move dst,src\n"
    "addu \\dst,\\src,$0\n"
    ".endm\n");

void func_0002DE10(void)
{
    asm volatile(
        ".set noat\n"
        ".set noreorder\n"
        "bne $6,$0,1f\n"
        "nop\n"
        "move $13,$31\n"
        "jal func_0002DBB4\n"
        "nop\n"
        "move $31,$13\n"
        "move $3,$9\n"
        "jr $31\n"
        "xor $2,$2,$2\n"
        "1:\n"
        "move $13,$31\n"
        "jal func_0002DAB8\n"
        "nop\n"
        "move $31,$13\n"
        "move $3,$5\n"
        "jr $31\n"
        "move $2,$4\n"
        ".set reorder\n"
        ".macro j reg\n"
        ".endm\n");
}
