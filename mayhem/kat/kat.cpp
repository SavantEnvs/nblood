// Known-answer probe over the real NBlood parsers (kplib, scriptfile, IniFile, sjson), built with the
// project's normal flags. Prints "KAT ok|FAIL <name>" per check and a final "KAT_SUMMARY" line.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "kplib.h"
#include "scriptfile.h"
#include "sjson.h"
#include "inifile.h"

static int g_pass, g_fail;

static void check(const char *name, bool ok)
{
    printf("KAT %s %s\n", ok ? "ok" : "FAIL", name);
    ok ? g_pass++ : g_fail++;
}

static unsigned char *slurp(const char *path, int32_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (int32_t)n;
    return b;
}

static void le32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static void kat_png(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/kat.png", dir);
    int32_t len = 0;
    unsigned char *png = slurp(path, &len);
    int32_t w = 0, h = 0;
    uint32_t px[6] = {0};
    if (png)
    {
        kpgetdim((const char *)png, len, &w, &h);
        if (w == 3 && h == 2)
            kprender((const char *)png, len, (intptr_t)px, 3 * 4, 3, 2);
    }
    check("png-dimensions-3x2", w == 3 && h == 2);
    check("png-pixels", px[0] == 0xffff0000u && px[1] == 0xff00ff00u && px[2] == 0xff0000ffu &&
                        px[3] == 0xff0a141eu && px[4] == 0xffffffffu && px[5] == 0xff000000u);
    free(png);
}

static void kat_bmp()
{
    // 2x2, 24-bit, bottom-up: row0(bottom)=blue,white  row1(top)=red,green
    unsigned char bmp[14 + 40 + 16] = {0};
    bmp[0] = 'B'; bmp[1] = 'M';
    le32(bmp + 2, sizeof bmp);
    le32(bmp + 10, 54);
    le32(bmp + 14, 40);
    le32(bmp + 18, 2);
    le32(bmp + 22, 2);
    bmp[26] = 1; bmp[28] = 24;
    le32(bmp + 34, 16);
    const unsigned char rows[16] = { 255,0,0, 255,255,255, 0,0, /* bottom: B,W + pad */
                                     0,0,255, 0,255,0, 0,0 };   /* top: R,G + pad */
    memcpy(bmp + 54, rows, 16);
    int32_t w = 0, h = 0;
    uint32_t px[4] = {0};
    kpgetdim((const char *)bmp, sizeof bmp, &w, &h);
    if (w == 2 && h == 2)
        kprender((const char *)bmp, sizeof bmp, (intptr_t)px, 2 * 4, 2, 2);
    check("bmp-dimensions-2x2", w == 2 && h == 2);
    check("bmp-pixels", px[0] == 0xffff0000u && px[1] == 0xff00ff00u && px[2] == 0xff0000ffu && px[3] == 0xffffffffu);
}

static void kat_zip(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/kat.pk3", dir);
    bool mounted = kzaddstack(path) >= 0;
    bool hello = false, raw = false;
    if (mounted)
    {
        if (kzopen("kat/hello.txt"))
        {
            char buf[1024] = {0};
            int32_t n = kzread(buf, sizeof buf - 1);
            hello = kzfilelength() == 30 * 20 && n == 30 * 20 &&
                    !strncmp(buf, "NBlood known answer test line\nNBlood known", 42);
            kzclose();
        }
        if (kzopen("kat/raw.bin"))
        {
            unsigned char buf[64];
            int32_t n = kzread(buf, sizeof buf);
            raw = n == 64 && buf[0] == 0 && buf[17] == 17 && buf[63] == 63;
            kzclose();
        }
    }
    check("zip-mounted", mounted);
    check("zip-deflate-entry", hello);
    check("zip-stored-entry", raw);
    kzuninit();
}

static void kat_script()
{
    scriptfile *sf = scriptfile_fromstring("model \"a/b.md3\" { scale 2.5 frame 42 } // trailing\nnext 0x10");
    char *t1 = scriptfile_gettoken(sf);
    char *s = NULL;
    bool ok = t1 && !strcmp(t1, "model") && !scriptfile_getstring(sf, &s) && s && !strcmp(s, "a/b.md3");
    char *end = NULL;
    bool braces = ok && !scriptfile_getbraces(sf, &end) && end != NULL;
    int toks = 0;
    int32_t frame = -1, n = -1;
    double scale = 0;
    if (braces)
    {
        char *t;
        while (sf->textptr < end && (t = scriptfile_gettoken(sf)) != NULL && sf->ltextptr < end)
        {
            toks++;
            if (!strcmp(t, "frame")) scriptfile_getnumber(sf, &frame);
            else if (!strcmp(t, "scale")) scriptfile_getdouble(sf, &scale);
        }
        sf->textptr = end + 1;
        char *t2 = scriptfile_gettoken(sf);
        if (t2 && !strcmp(t2, "next")) scriptfile_getnumber(sf, &n);
    }
    check("script-string-and-braces", braces);
    check("script-numbers", frame == 42 && n == 16 && scale == 2.5 && toks == 2);
    scriptfile_close(sf);
}

static void kat_ini()
{
    static unsigned char text[] =
        "; comment\n[Options]\nWidth = 640\nName = Caleb\n\n[Other]\nHex = 0x20\n";
    IniFile ini(text, (int)(sizeof text - 1));
    check("ini-sections", ini.SectionExists("Options") && ini.SectionExists("Other") && !ini.SectionExists("Nope"));
    const char *name = ini.GetKeyString("Options", "Name");
    check("ini-values", ini.GetKeyInt("Options", "Width", -1) == 640 && name && !strcmp(name, "Caleb") &&
                        ini.GetKeyInt("Other", "Hex", -1) == 0x20);
}

static void kat_json()
{
    const char *js = "{\"a\": 7, \"b\": 2.5, \"c\": \"hi\", \"d\": true, \"e\": [1,2,3,4]}";
    sjson_context *ctx = sjson_create_context(0, 0, NULL);
    sjson_node *root = sjson_decode(ctx, js);
    int e[4] = {0};
    bool got = root && sjson_get_ints(e, 4, root, "e");
    check("json-values", root && sjson_get_int(root, "a", -1) == 7 && sjson_get_double(root, "b", 0) == 2.5 &&
                         !strcmp(sjson_get_string(root, "c", ""), "hi") && sjson_get_bool(root, "d", false));
    check("json-array", got && e[0] == 1 && e[3] == 4);
    check("json-rejects-garbage", !sjson_validate(ctx, "{\"a\": [1, 2,}"));
    sjson_destroy_context(ctx);
}

int main(int argc, char **argv)
{
    engineCreateAllocator();
    const char *dir = argc > 1 ? argv[1] : "mayhem/kat/fixtures";
    kat_png(dir);
    kat_bmp();
    kat_zip(dir);
    kat_script();
    kat_ini();
    kat_json();
    printf("KAT_SUMMARY passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
