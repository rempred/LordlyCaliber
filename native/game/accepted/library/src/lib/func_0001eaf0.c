/* Independently reconstructed from the complete ROM 1EAF0 body. */
typedef struct L4Filter {
    struct L4Filter *input;
    void *callback04;
    void *callback08;
    short field0c;
    short field0e;
    void *state;
} L4Filter;

void func_0001eaf0(L4Filter *filter, void *callback04, void *callback08,
                   void *state)
{
    filter->input = 0;
    filter->callback04 = callback04;
    filter->callback08 = callback08;
    filter->field0c = 0;
    filter->field0e = 0;
    filter->state = state;
}
