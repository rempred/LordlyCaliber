typedef struct Phase8PackedFields {
    unsigned char unused_00[3];
    unsigned char shift_03;
    unsigned short value_04;
    unsigned short scale_06;
} Phase8PackedFields;

unsigned int func_000E5938(Phase8PackedFields *fields)
{
    return (((unsigned int)fields->value_04 << fields->shift_03) >> 1) *
        fields->scale_06;
}
