/* Complete ROM 1FAD0..1FBA0 producer. */
typedef union L4Command { struct { unsigned int word0, word1; } words; double align; } L4Command;
typedef struct L4Filter L4Filter;
struct L4Filter {
    L4Filter *input;
    L4Command *(*pull)(void *, int *, int, int, L4Command *);
    int (*param)(void *, int, void *);
    short field0c, field0e;
    void *state;
};
typedef struct L4Save {
    L4Filter filter;
    void *destination;
} L4Save;

L4Command *func_0001fad0(void *filter, int *output, int count, int offset,
                         L4Command *inputCommands)
{
    L4Save *save = (L4Save *)filter;
    L4Filter *input = save->filter.input;
    L4Command *commands = input->pull(input, output, count,
                                       offset, inputCommands);
    {
        L4Command *command = commands++;
        command->words.word0 = 0x08000000;
        command->words.word1 = (count << 1) & 0xFFFF;
    }
    {
        L4Command *command = commands++;
        command->words.word0 = 0x0D000000;
        command->words.word1 = 0x04400580;
    }
    {
        L4Command *command = commands++;
        command->words.word0 = 0x08000000;
        command->words.word1 = (count << 2) & 0xFFFF;
    }
    {
        L4Command *command = commands++;
        command->words.word0 = 0x06000000;
        command->words.word1 = (unsigned int)save->destination;
    }
    return commands;
}

int func_0001fb78(L4Save *save, int selector, void *value)
{
    switch (selector) {
    case 1:
        save->filter.input = value;
        break;
    case 6:
        save->destination = value;
        break;
    }
    return 0;
}


