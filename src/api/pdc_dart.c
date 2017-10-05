#include "pdc_dart.h"

static uint32_t pdc_dart_hash(const char *pc, int len)
{
    uint32_t hash = 1;
    char c;
    int loop_count = 0;

    while ((c = *pc++) && (loop_count < len)){
        if (c == '\0') break;
        uint32_t charcode = ((uint32_t)c) % DART_CHAR_SET_SIZE;
        hash = hash * charcode;
        loop_count++;
    }

    if (hash < 0)
        hash *= -1;

    return hash;
}
