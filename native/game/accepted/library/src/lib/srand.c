void srand(unsigned int seed)
{
    *(unsigned int *)0x800C47D0 = seed;
}
