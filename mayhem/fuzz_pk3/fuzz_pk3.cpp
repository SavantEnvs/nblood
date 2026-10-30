// kplib ZIP/PK3/GRP reader: mount the input as an archive, enumerate, open and read every entry.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compat.h"
#include "../nblood_heap.h"
#include "kplib.h"

static char s_archive[512];
static int s_inited;

static void init_once()
{
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp)
        tmp = "/tmp";

    // empty cwd so kzfindfile's directory walk contributes nothing besides the archive mounts
    char dir[512];
    snprintf(dir, sizeof dir, "%s/fuzz_pk3_cwd_XXXXXX", tmp);
    if (!mkdtemp(dir) || chdir(dir) != 0)
        abort();

    snprintf(s_archive, sizeof s_archive, "%s/fuzz_pk3_%d.pk3", tmp, (int)getpid());
    s_inited = 1;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nblood_heap_init();
    if (size < 22 || size > (1u << 22))
        return -1;
    if (!s_inited)
        init_once();

    FILE *f = fopen(s_archive, "wb");
    if (!f)
        return -1;
    size_t w = fwrite(data, 1, size, f);
    fclose(f);
    if (w != size)
        return -1;

    int rc = -1;
    if (kzaddstack(s_archive) >= 0)
    {
        char name[BMAX_PATH];
        char buf[4096];
        int files = 0;
        kzfindfilestart("*");
        while (kzfindfile(name) && files < 64)
        {
            size_t n = strlen(name);
            if (!n || name[n - 1] == '/' || name[n - 1] == '\\')
                continue;
            files++;
            if (!kzopen(name))
                continue;
            int32_t total = 0, got;
            while (total < (1 << 20) && (got = kzread(buf, sizeof buf)) > 0)
                total += got;
            kzclose();
            rc = 0;
        }
    }
    kzuninit();
    return rc;
}
