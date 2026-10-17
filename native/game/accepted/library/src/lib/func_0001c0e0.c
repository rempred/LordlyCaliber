typedef struct L5Link L5Link;
struct L5Link {
    L5Link *next;
};
extern L5Link *D_800AA41C;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

int func_0001c0e0(L5Link *link)
{
    unsigned int mask = __osDisableInt();
    link->next = D_800AA41C;
    D_800AA41C = link;
    __osRestoreInt(mask);
    return 0;
}
