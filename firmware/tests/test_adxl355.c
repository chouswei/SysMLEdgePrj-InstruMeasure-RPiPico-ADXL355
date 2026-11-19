/*
 * test_adxl355.c - host test for firmware/src/adxl355.c with the real, cited
 * adxl355_regs.h (PICO355-R4). Contract: adxl355.h order ARG, TBD, NOT_INIT,
 * BUS/ID, OK. A mock register file answers the SPI transfers; register names
 * come from adxl355_regs.h, the expected IDs are the cited reset values. The
 * TBD paths are tested in test_adxl355_tbd.c and test_adxl355_partial.c.
 */
#include <stdio.h>
#include <string.h>

#include "adxl355.h"
#include "adxl355_regs.h"
#include "config.h"

#if ADXL355_REGS_ARE_TBD
#error "test_adxl355 needs every driver register entry cited"
#endif

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define CMD_R(reg) ((uint8_t)(((reg) << ADXL355_SPI_ADDR_SHIFT) | ADXL355_SPI_READ_BIT))
#define CMD_W(reg) ((uint8_t)(((reg) << ADXL355_SPI_ADDR_SHIFT) | ADXL355_SPI_WRITE_BIT))

/* ---- mock sensor ---- */
static uint8_t g_reg[64];
static uint8_t g_fifo[300];
static size_t g_fifo_len;
static int g_calls;
static int g_fail_at;         /* 1-based transfer index that fails; 0 = never */
static int g_nvm_busy_polls;  /* STATUS reads after reset that still report NVM_BUSY */
static int g_status_reads;
static int g_writes;          /* write transfers seen */
static uint8_t g_wlog[16][2]; /* (reg, value) of each write */

static void mock_reset_state(void)
{
    memset(g_reg, 0, sizeof g_reg);
    memset(g_fifo, 0, sizeof g_fifo);
    g_fifo_len = 0; g_calls = 0; g_fail_at = 0; g_nvm_busy_polls = 0; g_status_reads = 0; g_writes = 0;
    g_reg[ADXL355_REG_DEVID_AD] = ADXL355_DEVID_AD_EXPECTED;
    g_reg[ADXL355_REG_DEVID_MST] = ADXL355_DEVID_MST_EXPECTED;
    g_reg[ADXL355_REG_PARTID] = ADXL355_PARTID_EXPECTED;
    g_reg[ADXL355_REG_POWER_CTL] = ADXL355_POWER_CTL_STANDBY;
    g_reg[ADXL355_REG_RANGE] = 0x81; /* no-OS H:51 reset value of RANGE */
}

static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    uint8_t reg = (uint8_t)(tx[0] >> ADXL355_SPI_ADDR_SHIFT);
    size_t i;
    (void)ctx;
    g_calls++;
    if (g_fail_at != 0 && g_calls == g_fail_at) return 1;
    if ((tx[0] & ADXL355_SPI_READ_BIT) != 0) {
        for (i = 1; i < len; i++) {
            if (rx == NULL) break;
            if (reg == ADXL355_REG_FIFO_DATA) rx[i] = (i - 1 < g_fifo_len) ? g_fifo[i - 1] : 0;
            else if (reg == ADXL355_REG_STATUS) {
                g_status_reads++;
                rx[i] = (g_nvm_busy_polls > 0 && g_status_reads <= g_nvm_busy_polls)
                        ? (uint8_t)(g_reg[reg] | ADXL355_STATUS_NVM_BUSY) : g_reg[reg];
            } else rx[i] = g_reg[reg + i - 1];
        }
    } else {
        if (g_writes < 16) { g_wlog[g_writes][0] = reg; g_wlog[g_writes][1] = tx[1]; }
        g_writes++;
        if (reg == ADXL355_REG_RESET && tx[1] == ADXL355_RESET_CODE) g_status_reads = 0;
        else g_reg[reg] = tx[1];
    }
    return 0;
}
/* post-reset delay hook: records the milliseconds and where it fell in the transfer sequence */
static int g_delay_calls;
static uint32_t g_delay_ms;
static int g_calls_at_delay;
static int g_status_reads_at_delay;
static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    g_delay_calls++;
    g_delay_ms = ms;
    g_calls_at_delay = g_calls;
    g_status_reads_at_delay = g_status_reads;
}
static const adxl355_bus_t g_bus = { mock_transfer, NULL, mock_delay };

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
    adxl355_bus_t nobus = { NULL, NULL, mock_delay };
    adxl355_config_t c = good_cfg();
    adxl355_sample_t smp = {0, 0, 0};
    adxl355_sample_t arr[2];
    size_t n = 5;
    uint8_t st = 0;

    mock_reset_state();
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
    c = good_cfg(); c.odr_hz = 1234u;                      /* not one of 4000 2000 1000 500 250 125 */
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_ARG);
    c = good_cfg(); c.filter = ADXL355_HPF_CODE_MAX + 1;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_ARG);
    c = good_cfg(); c.filter = -2;
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

static void not_init_cases(void)
{
    adxl355_t dev;
    adxl355_config_t c = good_cfg();
    adxl355_sample_t smp = {11, 22, 33};
    adxl355_sample_t arr[2] = {{1, 2, 3}, {4, 5, 6}};
    size_t n = 7;
    uint8_t st = 0x5A;

    mock_reset_state();
    memset(&dev, 0, sizeof dev);
    dev.bus = &g_bus;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_NOT_INIT);
    CHECK(dev.config.odr_hz == 0);
    CHECK(adxl355_set_measuring(&dev, true) == ADXL355_ERR_NOT_INIT);
    CHECK(adxl355_read_sample(&dev, &smp) == ADXL355_ERR_NOT_INIT);
    CHECK(smp.x == 11 && smp.y == 22 && smp.z == 33);
    CHECK(adxl355_read_fifo(&dev, arr, 2, &n) == ADXL355_ERR_NOT_INIT);
    CHECK(n == 0 && arr[0].x == 1 && arr[1].z == 6);
    CHECK(adxl355_status(&dev, &st) == ADXL355_ERR_NOT_INIT);
    CHECK(st == 0x5A);
    CHECK(g_calls == 0);
}

static void init_cases(void)
{
    adxl355_t dev;

    /* success: reset code written to RESET, STATUS polled until NVM_BUSY clears, then the three IDs */
    mock_reset_state();
    g_delay_calls = 0; g_delay_ms = 0;
    g_nvm_busy_polls = 3;
    memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_OK);
    /* post-reset delay (cited: no-OS adxl355.c C:297-298 no_os_mdelay(1)): once, CFG_ADXL355_RESET_DELAY_MS,
     * after NVM_BUSY cleared (4th STATUS read) and before the first ID read */
    CHECK(!CFG_IS_TBD(CFG_ADXL355_RESET_DELAY_MS));
    CHECK(g_delay_calls == 1 && g_delay_ms == (uint32_t)CFG_ADXL355_RESET_DELAY_MS);
    CHECK(g_status_reads_at_delay == 4 && g_calls_at_delay == 1 + 4);
    CHECK(dev.initialised);
    CHECK(dev.bus == &g_bus);
    CHECK(dev.config.odr_hz == CFG_DEFAULT_ODR_HZ && dev.config.range == CFG_DEFAULT_RANGE);
    CHECK(g_writes == 1 && g_wlog[0][0] == ADXL355_REG_RESET && g_wlog[0][1] == ADXL355_RESET_CODE);
    CHECK(g_status_reads == 4);          /* 3 busy polls + 1 clear */
    CHECK(g_calls == 1 + 4 + 3);         /* reset, polls, DEVID_AD, DEVID_MST, PARTID */

    /* each ID mismatch is ADXL355_ERR_ID and leaves the driver not initialised */
    mock_reset_state(); g_reg[ADXL355_REG_DEVID_AD] ^= 1; memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_ID && !dev.initialised);
    mock_reset_state(); g_reg[ADXL355_REG_DEVID_MST] ^= 1; memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_ID && !dev.initialised);
    mock_reset_state(); g_reg[ADXL355_REG_PARTID] ^= 1; memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_ID && !dev.initialised);

    /* bus failure at each of the first transfers (reset write, STATUS, DEVID_AD) */
    for (g_fail_at = 1; g_fail_at <= 3; g_fail_at++) {
        int at = g_fail_at;
        mock_reset_state(); g_fail_at = at; memset(&dev, 0, sizeof dev);
        CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_BUS && !dev.initialised);
        g_fail_at = at;
    }

    /* no delay hook: refused up front, the sensor is not touched */
    {
        adxl355_bus_t nodelay = { mock_transfer, NULL, NULL };
        mock_reset_state(); memset(&dev, 0, sizeof dev);
        CHECK(adxl355_init(&dev, &nodelay) == ADXL355_ERR_ARG && g_calls == 0);
    }

    /* NVM_BUSY never clears: gives up after 255 polls (no-OS C:274, C:288-295) */
    mock_reset_state(); g_nvm_busy_polls = 100000; memset(&dev, 0, sizeof dev);
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_ERR_BUS && !dev.initialised);
    CHECK(g_status_reads == 255);
    CHECK(g_delay_calls == 0);           /* the delay follows a cleared NVM_BUSY only */
}

static void run_init(adxl355_t *dev)
{
    memset(dev, 0, sizeof *dev);
    CHECK(adxl355_init(dev, &g_bus) == ADXL355_OK);
}

static void configure_cases(void)
{
    adxl355_t dev;
    adxl355_config_t c;

    /* measuring part: standby, RANGE, FILTER (ODR then HPF), restore POWER_CTL */
    mock_reset_state();
    run_init(&dev);
    g_reg[ADXL355_REG_POWER_CTL] = ADXL355_POWER_CTL_MEASURE_VAL;
    g_writes = 0;
    c.odr_hz = 1000u; c.range = ADXL355_RANGE_4G; c.filter = 2;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_OK);
    CHECK(dev.config.odr_hz == 1000u && dev.config.range == ADXL355_RANGE_4G && dev.config.filter == 2);
    CHECK(g_writes == 5);
    CHECK(g_wlog[0][0] == ADXL355_REG_POWER_CTL && g_wlog[0][1] == ADXL355_POWER_CTL_STANDBY);
    CHECK(g_wlog[1][0] == ADXL355_REG_RANGE && g_wlog[1][1] == ((0x81 & ~ADXL355_RANGE_MASK) | ADXL355_RANGE_CODE_4G));
    CHECK(g_wlog[2][0] == ADXL355_REG_FILTER && g_wlog[2][1] == ADXL355_ODR_CODE_1000HZ);
    CHECK(g_wlog[3][0] == ADXL355_REG_FILTER && g_wlog[3][1] == (ADXL355_ODR_CODE_1000HZ | (2 << ADXL355_FILTER_HPF_SHIFT)));
    CHECK(g_wlog[4][0] == ADXL355_REG_POWER_CTL && g_wlog[4][1] == ADXL355_POWER_CTL_MEASURE_VAL);

    /* standby part, filter unset: no standby write, HPF untouched, POWER_CTL not rewritten */
    mock_reset_state();
    run_init(&dev);
    g_reg[ADXL355_REG_FILTER] = 0x50;  /* HPF bits set beforehand */
    g_writes = 0;
    c.odr_hz = 4000u; c.range = ADXL355_RANGE_8G; c.filter = ADXL355_FILTER_UNSET;
    CHECK(adxl355_configure(&dev, &c) == ADXL355_OK);
    CHECK(g_writes == 2);
    CHECK((g_reg[ADXL355_REG_RANGE] & ADXL355_RANGE_MASK) == ADXL355_RANGE_CODE_8G);
    CHECK(g_reg[ADXL355_REG_FILTER] == (0x50 | ADXL355_ODR_CODE_4000HZ));
    CHECK(dev.config.filter == ADXL355_FILTER_UNSET);

    /* every supported ODR lands in the ODR/LPF field */
    {
        static const uint32_t hz[6] = { 4000u, 2000u, 1000u, 500u, 250u, 125u };
        static const uint8_t code[6] = { ADXL355_ODR_CODE_4000HZ, ADXL355_ODR_CODE_2000HZ, ADXL355_ODR_CODE_1000HZ,
                                         ADXL355_ODR_CODE_500HZ, ADXL355_ODR_CODE_250HZ, ADXL355_ODR_CODE_125HZ };
        size_t i;
        for (i = 0; i < 6; i++) {
            c = good_cfg(); c.odr_hz = hz[i]; c.filter = ADXL355_FILTER_UNSET;
            CHECK(adxl355_configure(&dev, &c) == ADXL355_OK);
            CHECK((g_reg[ADXL355_REG_FILTER] & ADXL355_FILTER_ODR_LPF_MASK) == code[i]);
        }
    }

    /* bus failure: ERR_BUS and dev->config unchanged */
    mock_reset_state();
    run_init(&dev);
    c = good_cfg();
    g_fail_at = g_calls + 2;  /* second transfer of configure */
    CHECK(adxl355_configure(&dev, &c) == ADXL355_ERR_BUS);
    CHECK(dev.config.odr_hz == CFG_DEFAULT_ODR_HZ);
}

static void measuring_cases(void)
{
    adxl355_t dev;

    mock_reset_state();
    run_init(&dev);
    g_writes = 0;
    CHECK(adxl355_set_measuring(&dev, true) == ADXL355_OK);
    CHECK(g_writes == 1 && g_wlog[0][0] == ADXL355_REG_POWER_CTL && g_wlog[0][1] == ADXL355_POWER_CTL_MEASURE_VAL);
    CHECK(adxl355_set_measuring(&dev, false) == ADXL355_OK);
    CHECK(g_writes == 2 && g_wlog[1][0] == ADXL355_REG_POWER_CTL && g_wlog[1][1] == ADXL355_POWER_CTL_STANDBY);
    g_fail_at = g_calls + 1;
    CHECK(adxl355_set_measuring(&dev, true) == ADXL355_ERR_BUS);
}

static void sample_cases(void)
{
    adxl355_t dev;
    adxl355_sample_t s = {1, 2, 3};

    mock_reset_state();
    run_init(&dev);
    /* 24-bit word, 20-bit two's complement sample in bits [23:4] (no-OS C:896-902, C:921) */
    g_reg[ADXL355_REG_XDATA3] = 0x7F; g_reg[ADXL355_REG_XDATA3 + 1] = 0xFF; g_reg[ADXL355_REG_XDATA3 + 2] = 0xF0;
    g_reg[ADXL355_REG_YDATA3] = 0x80; g_reg[ADXL355_REG_YDATA3 + 1] = 0x00; g_reg[ADXL355_REG_YDATA3 + 2] = 0x00;
    g_reg[ADXL355_REG_ZDATA3] = 0xFF; g_reg[ADXL355_REG_ZDATA3 + 1] = 0xFF; g_reg[ADXL355_REG_ZDATA3 + 2] = 0xF5; /* low nibble reserved */
    g_calls = 0;
    CHECK(adxl355_read_sample(&dev, &s) == ADXL355_OK);
    CHECK(s.x == 524287 && s.y == -524288 && s.z == -1);
    CHECK(g_calls == 3);
    s.x = 9; s.y = 9; s.z = 9;
    g_fail_at = g_calls + 2;
    CHECK(adxl355_read_sample(&dev, &s) == ADXL355_ERR_BUS);
    CHECK(s.x == 9 && s.y == 9 && s.z == 9);   /* outputs unchanged on failure */
}

static void put_entry(size_t k, int32_t v, bool x_marker, bool empty)
{
    uint32_t raw = ((uint32_t)v & 0xFFFFFu) << ADXL355_SAMPLE_SHIFT;
    g_fifo[3 * k + 0] = (uint8_t)(raw >> 16);
    g_fifo[3 * k + 1] = (uint8_t)(raw >> 8);
    g_fifo[3 * k + 2] = (uint8_t)((raw & 0xF0u) | (x_marker ? ADXL355_FIFO_X_MARKER : 0) | (empty ? ADXL355_FIFO_EMPTY : 0));
}

static void fifo_cases(void)
{
    adxl355_t dev;
    adxl355_sample_t out[4];
    size_t n = 99;

    mock_reset_state();
    run_init(&dev);
    /* 7 entries -> 2 whole samples (the 7th entry is left in the FIFO) */
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 7;
    put_entry(0, 100, true, false);  put_entry(1, -200, false, false); put_entry(2, 300, false, false);
    put_entry(3, 1, true, false);    put_entry(4, 2, false, false);    put_entry(5, -3, false, false);
    g_fifo_len = 18;
    g_calls = 0;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_OK);
    CHECK(n == 2);
    CHECK(out[0].x == 100 && out[0].y == -200 && out[0].z == 300);
    CHECK(out[1].x == 1 && out[1].y == 2 && out[1].z == -3);
    CHECK(g_calls == 2);                     /* FIFO_ENTRIES, one FIFO_DATA burst */

    /* max limits the burst */
    n = 99; g_calls = 0;
    CHECK(adxl355_read_fifo(&dev, out, 1, &n) == ADXL355_OK);
    CHECK(n == 1 && out[0].x == 100);

    /* an empty entry group is skipped, not returned */
    put_entry(0, 5, true, true);
    n = 99;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_OK);
    CHECK(n == 1 && out[0].x == 1);

    /* nothing buffered: one transfer, n 0 */
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 2; n = 99; g_calls = 0;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_OK && n == 0 && g_calls == 1);

    /* bus failure on the entry count: n stays 0 */
    n = 99; g_fail_at = g_calls + 1;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_ERR_BUS && n == 0);

    /* full FIFO (0x60 entries) fits the internal buffer */
    mock_reset_state();
    run_init(&dev);
    g_reg[ADXL355_REG_FIFO_ENTRIES] = ADXL355_FIFO_SAMPLES_MAX;
    { size_t k; for (k = 0; k < ADXL355_FIFO_SAMPLES_MAX; k++) put_entry(k, (int32_t)k, (k % 3) == 0, false); }
    g_fifo_len = 3u * ADXL355_FIFO_SAMPLES_MAX;
    {
        adxl355_sample_t big[40];
        n = 0;
        CHECK(adxl355_read_fifo(&dev, big, 40, &n) == ADXL355_OK);
        CHECK(n == ADXL355_FIFO_SAMPLES_MAX / 3 && big[31].x == 93 && big[31].z == 95);
    }
}

static void status_cases(void)
{
    adxl355_t dev;
    uint8_t st = 0;

    mock_reset_state();
    run_init(&dev);
    g_reg[ADXL355_REG_STATUS] = ADXL355_STATUS_DATA_RDY | ADXL355_STATUS_FIFO_OVR;
    g_calls = 0;
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK);
    CHECK(st == (ADXL355_STATUS_DATA_RDY | ADXL355_STATUS_FIFO_OVR) && g_calls == 1);
    g_fail_at = g_calls + 1; st = 0x5A;
    CHECK(adxl355_status(&dev, &st) == ADXL355_ERR_BUS && st == 0x5A);
}

static void spi_framing_cases(void)
{
    /* command byte = (reg << shift) | read/write bit (no-OS C:46, C:81): DEVID_AD read, POWER_CTL write */
    CHECK(CMD_R(ADXL355_REG_DEVID_AD) == 0x01);
    CHECK(CMD_W(ADXL355_REG_POWER_CTL) == 0x5A);
}

int main(void)
{
    arg_cases();
    not_init_cases();
    init_cases();
    configure_cases();
    measuring_cases();
    sample_cases();
    fifo_cases();
    status_cases();
    spi_framing_cases();
    if (g_fail) { printf("test_adxl355: %d check(s) failed\n", g_fail); return 1; }
    printf("test_adxl355: ok\n");
    return 0;
}
