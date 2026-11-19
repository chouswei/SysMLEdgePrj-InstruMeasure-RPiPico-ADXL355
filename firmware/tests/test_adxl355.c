/*
 * test_adxl355.c - host test for firmware/src/adxl355.c (PICO355-R4).
 * Contract: adxl355.h order ARG, TBD, NOT_INIT, BUS/ID, OK. While adxl355_regs.h is
 * TODO(TBD) (ADXL355_REGS_ARE_TBD) every valid call returns ADXL355_ERR_TBD with zero
 * transfer() calls. No register-sequence test exists until entries are cited; then a
 * sequence test must be added using names from adxl355_regs.h.
 */
#include <stdio.h>
#include <string.h>

#include "adxl355.h"
#include "adxl355_regs.h"
#include "config.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_calls;
static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)ctx; (void)tx; (void)rx; (void)len;
    g_calls++;
    return 0;
}
static const adxl355_bus_t g_bus = { mock_transfer, NULL };

static adxl355_config_t good_cfg(void)
{
    adxl355_config_t c;
    c.odr_hz = 1000u;           /* test data, not a design value */
    c.range = ADXL355_RANGE_2G;
    c.filter = 0;
    return c;
}

static void arg_cases(void)
{
    adxl355_t dev;
    adxl355_bus_t nobus = { NULL, NULL };
    adxl355_config_t c = good_cfg();
    adxl355_sample_t smp = {0, 0, 0};
    adxl355_sample_t arr[2];
    size_t n = 5;
    uint8_t st = 0;

    memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(NULL, &g_bus) == ADXL355_ERR_ARG);
    CHECK(adxl355_init(&dev, NULL) == ADXL355_ERR_ARG);
    CHECK(adxl355_init(&dev, &nobus) == ADXL355_ERR_ARG);

    CHECK(adxl355_configure(NULL, &c) == ADXL355_ERR_ARG);
    CHECK(adxl355_configure(&dev, NULL) == ADXL355_ERR_ARG);
    c.range = ADXL355_RANGE_UNSET;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_ARG);
    c = good_cfg(); c.range = (adxl355_range_t)99;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_ARG);
    c = good_cfg(); c.odr_hz = 0;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_ARG);

    CHECK(adxl355_set_measuring(NULL, true) == ADXL355_ERR_ARG);
    CHECK(adxl355_read_sample(NULL, &smp) == ADXL355_ERR_ARG);
    CHECK(adxl355_read_sample(&dev, NULL) == ADXL355_ERR_ARG);
    CHECK(adxl355_read_fifo(NULL, arr, 2, &n) == ADXL355_ERR_ARG);
    CHECK(adxl355_read_fifo(&dev, NULL, 2, &n) == ADXL355_ERR_ARG);
    CHECK(adxl355_read_fifo(&dev, arr, 2, NULL) == ADXL355_ERR_ARG);
    CHECK(adxl355_status(NULL, &st) == ADXL355_ERR_ARG);
    CHECK(adxl355_status(&dev, NULL) == ADXL355_ERR_ARG);
    CHECK(g_calls == 0); /* ARG errors never touch the bus */
}

#if ADXL355_REGS_ARE_TBD
/* Documented TBD-state returns: ADXL355_ERR_TBD, zero transfer() calls, outputs unchanged. */
static void tbd_cases(void)
{
    adxl355_t dev;
    adxl355_config_t c = good_cfg();
    adxl355_sample_t smp = {11, 22, 33};
    adxl355_sample_t arr[2] = {{1, 2, 3}, {4, 5, 6}};
    size_t n = 7;
    uint8_t st = 0x5A;

    g_calls = 0;
    memset(&dev, 0xAB, sizeof dev);
    dev.initialised = true;           /* garbage state must not matter: TBD is checked before NOT_INIT */
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_TBD);
    CHECK(dev.bus == &g_bus);
    CHECK(dev.config.odr_hz == CFG_DEFAULT_ODR_HZ);
    CHECK(dev.config.range == CFG_DEFAULT_RANGE);
#if CFG_IS_TBD(CFG_DEFAULT_FILTER)
    CHECK(dev.config.filter == ADXL355_FILTER_UNSET);
#endif
    CHECK(dev.initialised == false);
    CHECK(g_calls == 0);

    memset(&dev, 0, sizeof dev);      /* never initialised: still TBD, not NOT_INIT */
    dev.bus = &g_bus;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_TBD);
    CHECK(dev.config.odr_hz == 0);    /* dev->config only changes on OK */
    CHECK(adxl355_set_measuring(&dev, true) == ADXL355_ERR_TBD);
    CHECK(adxl355_set_measuring(&dev, false) == ADXL355_ERR_TBD);
    CHECK(adxl355_read_sample(&dev, &smp) == ADXL355_ERR_TBD);
    CHECK(smp.x == 11 && smp.y == 22 && smp.z == 33);
    CHECK(adxl355_read_fifo(&dev, arr, 2, &n) == ADXL355_ERR_TBD);
    CHECK(n == 0);
    CHECK(arr[0].x == 1 && arr[1].z == 6);
    CHECK(adxl355_status(&dev, &st) == ADXL355_ERR_TBD);
    CHECK(st == 0x5A);
    CHECK(g_calls == 0);
}
#endif

int main(void)
{
    arg_cases();
#if ADXL355_REGS_ARE_TBD
    tbd_cases();
#endif
    if (g_fail) { printf("test_adxl355: %d check(s) failed\n", g_fail); return 1; }
    printf("test_adxl355: ok\n");
    return 0;
}
