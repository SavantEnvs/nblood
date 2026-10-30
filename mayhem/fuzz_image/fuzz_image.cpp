// kplib image decoders (PNG/JPEG/GIF/BMP/PCX/TGA/DDS/CEL): kpgetdim + kprender on in-memory data.
#include <stdint.h>
#include <stdlib.h>
#include "compat.h"
#include "../nblood_heap.h"
#include "kplib.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nblood_heap_init();
    if (size < 32 || size > (1u << 22))
        return -1;

    int32_t xsiz = 0, ysiz = 0;
    kpgetdim((const char *)data, (int32_t)size, &xsiz, &ysiz);
    if (xsiz <= 0 || ysiz <= 0 || xsiz > 2048 || ysiz > 2048)
        return -1;

    void *pic = calloc((size_t)ysiz * xsiz, 4);
    if (!pic)
        return -1;
    int r = kprender((const char *)data, (int32_t)size, (intptr_t)pic, xsiz * 4, xsiz, ysiz);
    free(pic);
    return r < 0 ? -1 : 0;
}
