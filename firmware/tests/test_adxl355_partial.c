/*
 * test_adxl355_partial.c - per-operation TBD check (PICO355-R4). CMake generates the
 * register header from the real adxl355_regs.h with only ADXL355_REG_XDATA3 replaced
 * by ADXL355_TBD. Then adxl355_read_sample (which needs it) returns ADXL355_ERR_TBD
 * with zero transfer() calls, outputs unchanged, while the operations that do not
 * need it still run. ADXL355_REGS_ARE_TBD is 1 (a used entry is TBD).
 */
#include <stdio.h>
#include <string.h>

#include "adxl355.h"
#include "adxl355_regs.h"

#if !ADXL355_REGS_ARE_TBD
#error "test_adxl355_partial must be built against the generated partial register header"
#endif

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_calls;
static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    size_t i;
    (void)ctx; (void)tx;
    g_calls++;
    if (rx != NULL) for (i = 0; i < len; i++) rx[i] = 0;
    return 0;
}
static const adxl355_bus_t g_bus = { mock_transfer, NULL };

int main(void)
{
    adxl355_t dev;
    adxl355_sample_t s = {11, 22, 33};
    uint8_t st = 0x5A;

    memset(&dev, 0, sizeof dev);
    dev.bus = &g_bus;
    dev.initialised = true;
    CHECK(adxl355_read_sample(&dev, &s) == ADXL355_ERR_TBD);
    CHECK(s.x == 11 && s.y == 22 && s.z == 33);
    CHECK(g_calls == 0);                                   /* TBD before any bus transfer */
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK);        /* STATUS needs no XDATA3 */
    CHECK(g_calls == 1);
    g_calls = 0;
    CHECK(adxl355_set_measuring(&dev, true) == ADXL355_OK);
    CHECK(g_calls == 1);
    if (g_fail) { printf("test_adxl355_partial: %d check(s) failed\n", g_fail); return 1; }
    printf("test_adxl355_partial: ok\n");
    return 0;
}
