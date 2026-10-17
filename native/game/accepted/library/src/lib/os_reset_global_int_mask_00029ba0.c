/* Complete ROM29BA0 routine; retain the observed 0x401 mask bits. */
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int status);
extern unsigned int __OSGlobalIntMask;

void osResetGlobalIntMask(unsigned int mask)
{
    unsigned int status = __osDisableInt();
    __OSGlobalIntMask &= ~mask | 0x401;
    __osRestoreInt(status);
}
