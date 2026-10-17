/* Complete 28-byte call wrapper and native tail at ROM28430..28450. */
extern int __osSpSetPc(unsigned int pc);

int func_00028430(void)
{
    return __osSpSetPc(0);
}
