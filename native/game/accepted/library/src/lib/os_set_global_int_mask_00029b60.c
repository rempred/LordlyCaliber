/* Interrupt-mask update reconstructed from the complete ROM29B60 body. */
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int status);
extern unsigned int __OSGlobalIntMask;

void osSetGlobalIntMask(unsigned int mask)
{
    unsigned int status = __osDisableInt();
    __OSGlobalIntMask |= mask;
    __osRestoreInt(status);
}
