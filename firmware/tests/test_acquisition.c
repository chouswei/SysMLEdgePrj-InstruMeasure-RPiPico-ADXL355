/*
 * test_acquisition.c - host test for firmware/src/acquisition.c (PICO355-R4).
 * Mock DRDY hal, real adxl355 on a counting mock bus, real sample_buffer.
 * acquisition.h: DRDY low -> ACQ_OK, *pushed = 0; an ADXL355_ERR_TBD from the driver
 * comes back as ACQ_ERR_TBD with *pushed = 0 and the buffer untouched.
 * No register-sequence expectations while adxl355_regs.h is TODO(TBD).
 */
#include <stdio.h>
#include <string.h>

#include "acquisition.h"
#include "adxl355_regs.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_calls;
static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)ctx; (void)tx; (void)rx; (void)len;
    g_calls++;
    return 0;
}
static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }
static const adxl355_bus_t g_bus = { mock_transfer, NULL, mock_delay };

static bool g_drdy;
static int g_drdy_reads;
static bool mock_drdy(void *ctx) { (void)ctx; g_drdy_reads++; return g_drdy; }
static const acq_hal_t g_hal = { mock_drdy, NULL };

static void arg_cases(void)
{
    acq_t acq;
    adxl355_t dev;
    sample_buffer_t buf;
    acq_hal_t nohal = { NULL, NULL };
    size_t pushed = 5;

    memset(&dev, 0, sizeof dev);
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    CHECK(acq_init(NULL, &g_hal, &dev, &buf) == ACQ_ERR_ARG);
    CHECK(acq_init(&acq, NULL, &dev, &buf) == ACQ_ERR_ARG);
    CHECK(acq_init(&acq, &nohal, &dev, &buf) == ACQ_ERR_ARG);
    CHECK(acq_init(&acq, &g_hal, NULL, &buf) == ACQ_ERR_ARG);
    CHECK(acq_init(&acq, &g_hal, &dev, NULL) == ACQ_ERR_ARG);
    CHECK(acq_init(&acq, &g_hal, &dev, &buf) == ACQ_OK);
    CHECK(acq_step(NULL, &pushed) == ACQ_ERR_ARG);
    CHECK(acq_step(&acq, NULL) == ACQ_ERR_ARG);
}

static void drdy_low(void)
{
    acq_t acq;
    adxl355_t dev;
    sample_buffer_t buf;
    size_t pushed = 5;

    g_calls = 0; g_drdy_reads = 0; g_drdy = false;
    memset(&dev, 0, sizeof dev);
    dev.bus = &g_bus;
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    CHECK(acq_init(&acq, &g_hal, &dev, &buf) == ACQ_OK);
    CHECK(acq_step(&acq, &pushed) == ACQ_OK);
    CHECK(pushed == 0);
    CHECK(sample_buffer_count(&buf) == 0);
    CHECK(g_drdy_reads >= 1);   /* DRDY is what gates the step */
    CHECK(g_calls == 0);        /* no SPI traffic without DRDY */
}

#if ADXL355_REGS_ARE_TBD
static void drdy_high_tbd(void)
{
    acq_t acq;
    adxl355_t dev;
    sample_buffer_t buf;
    size_t pushed = 5;

    g_calls = 0; g_drdy = true;
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_TBD);
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    CHECK(acq_init(&acq, &g_hal, &dev, &buf) == ACQ_OK);
    CHECK(acq_step(&acq, &pushed) == ACQ_ERR_TBD);
    CHECK(pushed == 0);
    CHECK(sample_buffer_count(&buf) == 0 && sample_buffer_overflows(&buf) == 0);
    CHECK(g_calls == 0);
}
#endif

int main(void)
{
    arg_cases();
    drdy_low();
#if ADXL355_REGS_ARE_TBD
    drdy_high_tbd();
#endif
    if (g_fail) { printf("test_acquisition: %d check(s) failed\n", g_fail); return 1; }
    printf("test_acquisition: ok\n");
    return 0;
}
