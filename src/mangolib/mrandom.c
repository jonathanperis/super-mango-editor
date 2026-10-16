#include "mrandom.h"

void mrandom_seed(RandomStream *stream, uint32_t seed)
{
    stream->state = seed ? seed : UINT32_C(0x9e3779b9);
}

uint32_t mrandom_next(RandomStream *stream)
{
    uint32_t x = stream->state ? stream->state : 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    stream->state = x;
    return x;
}

float mrandom_unit(RandomStream *stream)
{
    return (float)(mrandom_next(stream) >> 8) / 16777215.0f;
}
