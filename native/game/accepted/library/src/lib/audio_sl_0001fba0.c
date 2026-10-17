/* ROM 1FBA0..1FC50; writable pointer storage stays with original ASM.
 * The 800AA710 binding is independently visible in both entry bodies and
 * the already accepted func_000201c8 producer. */
extern void *data_800AA710;
#define L4_GLOBALS data_800AA710
extern void func_0001fd20(void *, void *);
extern void func_00020360(void);

typedef struct L4Link {
    struct L4Link *next;
    struct L4Link *previous;
} L4Link;

void func_0001fba0(void *globals, void *config)
{
    if (!L4_GLOBALS) {
        L4_GLOBALS = globals;
        func_0001fd20(globals, config);
    }
}

void func_0001fbcc(void)
{
    if (L4_GLOBALS) {
        func_00020360();
        L4_GLOBALS = 0;
    }
}

void list_insert_head(L4Link *link, L4Link *head)
{
    link->next = head->next;
    link->previous = head;
    if (head->next)
        head->next->previous = link;
    head->next = link;
}

void list_unlink(L4Link *link)
{
    if (link->next)
        link->next->previous = link->previous;
    if (link->previous)
        link->previous->next = link->next;
}

