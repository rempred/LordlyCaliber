extern float sqrtf(float value);

float func_0002CB80(float x, float y)
{
    unsigned char frame_pad[24];
    register float x_loaded asm("$f4");
    register float x_squared asm("$f6");
    register float y_squared asm("$f8");
    register float result asm("$f0");

    asm volatile (".set noreorder");
    asm volatile ("" : "=m"(frame_pad));
    asm volatile ("" : "=m"(x) : "0"(x));
    x_loaded = x;
    asm volatile ("sw $31,20($sp)");
    x_squared = x_loaded * x_loaded;
    asm volatile ("nop");
    y_squared = y * y;
    asm volatile (
        ".set noreorder\n"
        "jal sqrtf\n"
        "add.s $f12,%1,%2"
        : "=f"(result)
        : "f"(x_squared), "f"(y_squared)
        : "memory");
    asm volatile ("lw $31,20($sp)");
    return result;
}

asm("nop");
asm(".space 12");
asm(".size func_0002CB80, 64");
