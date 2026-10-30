// scriptfile tokenizer as used by the .def / .cfg parsers: tokens, numbers, strings, symbols, braces.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "../nblood_heap.h"
#include "scriptfile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nblood_heap_init();
    if (size < 2 || size > (1u << 20) || memchr(data, 0, size))
        return -1;

    char *text = (char *)malloc(size + 1);
    if (!text)
        return -1;
    memcpy(text, data, size);
    text[size] = 0;

    scriptfile *sf = scriptfile_fromstring(text);
    free(text);
    if (!sf)
        return -1;

    scriptfile_addsymbolvalue("fuzzsym", 7);
    int steps = 0;
    while (!scriptfile_eof(sf) && steps++ < 4096)
    {
        char *tok = scriptfile_gettoken(sf);
        if (!tok)
            break;
        scriptfile_getlinum(sf, sf->ltextptr);
        switch (steps & 7)
        {
        case 0: { int32_t n; scriptfile_getnumber(sf, &n); break; }
        case 1: { char *s; scriptfile_getstring(sf, &s); break; }
        case 2: { int32_t n; scriptfile_getsymbol(sf, &n); break; }
        case 3: { char *e; scriptfile_getbraces(sf, &e); break; }
        case 4: { double d; scriptfile_getdouble(sf, &d); break; }
        default: break;
        }
    }
    scriptfile_clearsymbols();
    scriptfile_close(sf);
    return 0;
}
