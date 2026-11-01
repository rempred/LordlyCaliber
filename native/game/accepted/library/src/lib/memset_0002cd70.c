asm(".set noreorder\n"
    ".macro move dst,src\n"
    "or \\dst,\\src,$0\n"
    ".endm\n");

void *func_0002CD70(void *destination, int value, unsigned int count)
{
    register unsigned char *cursor asm("$2");

    asm("or $3,%0,$0" : : "r"(count));
    cursor = destination;
    asm("" : : "r"(cursor));
    if (count != 0) {
        asm(
            "addiu $6,$6,-1\n"
            "1:\n"
            "or $3,$6,$0\n"
            "sb $5,0($2)\n"
            "addiu $2,$2,1\n"
            "bne $6,$0,1b\n"
            "addiu $6,$6,-1\n");
    }
    return destination;
}

asm(
    ".align 4\n"
    ".size func_0002CD70,48\n");
