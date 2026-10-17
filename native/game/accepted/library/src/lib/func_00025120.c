/* Two-word store at ROM25120, with existing runtime storage retained. */
extern unsigned long long __osCurrentTime;

void func_00025120(unsigned long long time)
{
    __osCurrentTime = time;
}
