/*
 * test_sample_buffer.c - host test for firmware/src/sample_buffer.c (PICO355-R5).
 * Uses CFG_SAMPLE_BUFFER_DEPTH from config.h (placeholder 1 while TBD): the
 * assertions hold for any depth >= 1. Overflow behaviour is the one sample_buffer.h
 * states for the not-configured policy; if the policy is decided only the
 * documented return code and counter are checked.
 */
#include <stdio.h>

#include "sample_buffer.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static adxl355_sample_t mk(int32_t v) { adxl355_sample_t s; s.x = v; s.y = v + 1; s.z = v + 2; return s; }
static int same(adxl355_sample_t a, adxl355_sample_t b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

static void args(void)
{
    sample_buffer_t b;
    adxl355_sample_t s = mk(1);
    CHECK(sample_buffer_init(NULL) == SAMPLE_BUFFER_ERR_ARG);
    CHECK(sample_buffer_init(&b) == SAMPLE_BUFFER_OK);
    CHECK(sample_buffer_push(NULL, &s) == SAMPLE_BUFFER_ERR_ARG);
    CHECK(sample_buffer_push(&b, NULL) == SAMPLE_BUFFER_ERR_ARG);
    CHECK(sample_buffer_pop(NULL, &s) == SAMPLE_BUFFER_ERR_ARG);
    CHECK(sample_buffer_pop(&b, NULL) == SAMPLE_BUFFER_ERR_ARG);
    CHECK(sample_buffer_count(NULL) == 0);
    CHECK(sample_buffer_overflows(NULL) == 0);
}

static void empty_and_order(void)
{
    sample_buffer_t b;
    adxl355_sample_t out = mk(-1), s;
    size_t i, n;
    CHECK(sample_buffer_init(&b) == SAMPLE_BUFFER_OK);
    CHECK(sample_buffer_count(&b) == 0 && sample_buffer_overflows(&b) == 0);
    CHECK(sample_buffer_pop(&b, &out) == SAMPLE_BUFFER_EMPTY);
    CHECK(same(out, mk(-1))); /* untouched on EMPTY */

    /* fill to capacity: every push OK, FIFO order out */
    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) {
        s = mk((int32_t)(10 * i));
        CHECK(sample_buffer_push(&b, &s) == SAMPLE_BUFFER_OK);
        CHECK(sample_buffer_count(&b) == i + 1);
    }
    CHECK(sample_buffer_overflows(&b) == 0);
    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) {
        CHECK(sample_buffer_pop(&b, &out) == SAMPLE_BUFFER_OK);
        CHECK(same(out, mk((int32_t)(10 * i))));
    }
    CHECK(sample_buffer_count(&b) == 0);

    /* wrap around the ring several times */
    n = 2 * CFG_SAMPLE_BUFFER_DEPTH + 1;
    for (i = 0; i < n; i++) {
        s = mk((int32_t)(100 + i));
        CHECK(sample_buffer_push(&b, &s) == SAMPLE_BUFFER_OK);
        CHECK(sample_buffer_pop(&b, &out) == SAMPLE_BUFFER_OK);
        CHECK(same(out, s));
    }
    CHECK(sample_buffer_count(&b) == 0 && sample_buffer_overflows(&b) == 0);
}

static void overflow(void)
{
    sample_buffer_t b;
    adxl355_sample_t s, extra = mk(999), out;
    size_t i;
    CHECK(sample_buffer_init(&b) == SAMPLE_BUFFER_OK);
    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) {
        s = mk((int32_t)(10 * i));
        CHECK(sample_buffer_push(&b, &s) == SAMPLE_BUFFER_OK);
    }
    CHECK(sample_buffer_push(&b, &extra) == SAMPLE_BUFFER_OVERFLOW);
    CHECK(sample_buffer_overflows(&b) == 1);
    CHECK(sample_buffer_count(&b) == CFG_SAMPLE_BUFFER_DEPTH);
    CHECK(sample_buffer_push(&b, &extra) == SAMPLE_BUFFER_OVERFLOW);
    CHECK(sample_buffer_overflows(&b) == 2);
#if CFG_IS_TBD(CFG_SAMPLE_BUFFER_OVERFLOW_POLICY)
    /* sample_buffer.h: policy not configured -> stored samples unchanged, new one not stored */
    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) {
        CHECK(sample_buffer_pop(&b, &out) == SAMPLE_BUFFER_OK);
        CHECK(same(out, mk((int32_t)(10 * i))));
    }
    CHECK(sample_buffer_pop(&b, &out) == SAMPLE_BUFFER_EMPTY);
#else
    (void)out;
#endif
    /* init clears the overflow counter and the contents */
    CHECK(sample_buffer_init(&b) == SAMPLE_BUFFER_OK);
    CHECK(sample_buffer_overflows(&b) == 0 && sample_buffer_count(&b) == 0);
}

int main(void)
{
    args();
    empty_and_order();
    overflow();
    if (g_fail) { printf("test_sample_buffer: %d check(s) failed\n", g_fail); return 1; }
    printf("test_sample_buffer: ok\n");
    return 0;
}
