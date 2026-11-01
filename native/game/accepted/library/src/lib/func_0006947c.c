void func_00089A10(void);
int func_80199D10(void);

void func_0006947c(void)
{
    do {
        func_00089A10();
    } while (func_80199D10() == 0);
}

__asm__(".space 8\n.size func_0006947c, .-func_0006947c");
