extern unsigned short func_00045934(unsigned short, unsigned short,
                                  unsigned short, unsigned short);

unsigned short func_0020C448(void *object)
{
    unsigned short *fields = (unsigned short *)object;

    return func_00045934(fields[0x1B], fields[0x1C], fields[0x1D], fields[0x1E]);
}
