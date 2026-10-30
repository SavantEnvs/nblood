// Blood INI parser (in-memory): load, enumerate sections/keys, look values up.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "../nblood_heap.h"
#include "inifile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nblood_heap_init();
    if (size < 2 || size > (1u << 20))
        return -1;

    unsigned char *raw = (unsigned char *)malloc(size);
    if (!raw)
        return -1;
    memcpy(raw, data, size);

    IniFile *ini = new IniFile(raw, (int)size, (char)(data[0] & 3));
    free(raw);

    char *section = NULL;
    int sections = 0;
    while (sections++ < 256 && ini->GetNextSection(&section))
    {
        const char *key, *val;
        int prev = -1, keys = 0;
        while (keys++ < 256 && ini->GetNextString(&key, &val, &prev, section))
        {
            ini->GetKeyString(section, key);
            ini->GetKeyInt(section, key, 0);
        }
    }
    ini->SectionExists("Options");
    ini->GetKeyInt("Options", "Key", 0);
    delete ini;
    return 0;
}
