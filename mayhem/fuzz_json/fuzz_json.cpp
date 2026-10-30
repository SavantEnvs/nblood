// sjson (JSON decoder used for settings/config): validate, decode, read typed values.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "../nblood_heap.h"
#include "sjson.h"

// The typed getters document (sjson_assert) that the member has the requested type; callers check first.
static void walk(sjson_node *n, int depth)
{
    if (!n || depth > 64)
        return;
    if (n->tag != SJSON_ARRAY && n->tag != SJSON_OBJECT)
        return;
    for (sjson_node *c = n->children.head; c; c = c->next)
        walk(c, depth + 1);
}

static bool all_numbers(sjson_node *arr)
{
    for (sjson_node *c = arr->children.head; c; c = c->next)
        if (c->tag != SJSON_NUMBER)
            return false;
    return true;
}

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

    sjson_context *ctx = sjson_create_context(0, 0, NULL);
    int rc = -1;
    if (sjson_validate(ctx, text))
    {
        sjson_node *root = sjson_decode(ctx, text);
        if (root)
        {
            walk(root, 0);
            if (root->tag == SJSON_OBJECT)
            {
                int ints[4];
                float floats[4];
                sjson_node *m;
                if ((m = sjson_find_member(root, "a")) && m->tag == SJSON_NUMBER)
                    sjson_get_int(root, "a", 0);
                if ((m = sjson_find_member(root, "b")) && m->tag == SJSON_NUMBER)
                    sjson_get_double(root, "b", 0.0);
                if ((m = sjson_find_member(root, "c")) && m->tag == SJSON_STRING)
                    sjson_get_string(root, "c", "");
                if ((m = sjson_find_member(root, "d")) && m->tag == SJSON_BOOL)
                    sjson_get_bool(root, "d", false);
                if ((m = sjson_find_member(root, "e")) && m->tag == SJSON_ARRAY && all_numbers(m))
                    sjson_get_ints(ints, 4, root, "e");
                if ((m = sjson_find_member(root, "f")) && m->tag == SJSON_ARRAY && all_numbers(m))
                    sjson_get_floats(floats, 4, root, "f");
            }
            rc = 0;
        }
    }
    sjson_destroy_context(ctx);
    free(text);
    return rc;
}
