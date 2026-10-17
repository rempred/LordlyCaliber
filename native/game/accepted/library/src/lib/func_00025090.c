/* Counter delta and 64-bit time addition reconstructed from ROM25090. */
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);
extern unsigned int osGetCount(void);
extern unsigned int __osBaseCounter;
extern unsigned long long __osCurrentTime;

unsigned long long func_00025090(void)
{
    unsigned int status = __osDisableInt();
    unsigned int delta = osGetCount() - __osBaseCounter;
    unsigned long long time = __osCurrentTime;
    __osRestoreInt(status);
    return time + delta;
}
