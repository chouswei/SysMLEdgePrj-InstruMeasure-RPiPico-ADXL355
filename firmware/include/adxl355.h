/*
 * adxl355.h - SPI driver for the ADXL355B (model part spiDriver : SpiDriver).
 *
 * Requirements: PICO355-R4 (SPI0 reads gated by DRDY), PICO355-R2/PICO355-R6
 * (apply ODR, range, filter). Register knowledge lives only in adxl355_regs.h.
 * The bus is injected as adxl355_bus_t (main.c binds it to SDK SPI0 + CSN GP17),
 * so this module builds and tests on the host.
 *
 * Register values come from adxl355_regs.h, cited to the Analog Devices no-OS driver
 * (path + commit SHA in that header). Contract for every public function, checked in
 * this order:
 *   1. ADXL355_ERR_ARG: a NULL pointer, bus->transfer NULL, or a value out of
 *      range (range not one of ADXL355_RANGE_2G/4G/8G, odr_hz not one of
 *      4000 2000 1000 500 250 125, filter not ADXL355_FILTER_UNSET or 0..ADXL355_HPF_CODE_MAX).
 *   2. ADXL355_ERR_TBD: a register entry (ADXL355_IS_TBD) the operation needs is
 *      still a sentinel (per operation, see the *_TBD lists in adxl355.c). No bus
 *      transfer is made; *out / *status are left unchanged; *n_read is set to 0.
 *      adxl355_init still binds dev->bus and loads dev->config with the defaults
 *      (CFG_DEFAULT_ODR_HZ, CFG_DEFAULT_RANGE, filter ADXL355_FILTER_UNSET while
 *      CFG_DEFAULT_FILTER is TBD) and leaves dev->initialised false.
 *   3. ADXL355_ERR_NOT_INIT: dev->initialised is false (any call but init).
 *   4. ADXL355_ERR_BUS: transfer() returned non-zero (or NVM_BUSY never cleared);
 *      ADXL355_ERR_ID: a DEVID_AD, DEVID_MST or PARTID value differs.
 *   5. ADXL355_OK.
 * With the header fully cited ADXL355_REGS_ARE_TBD is 0 and these sequences run
 * (init: reset, wait NVM_BUSY, ID check; configure: standby, RANGE, ODR/LPF, HPF,
 * restore; set_measuring; read_sample: X, Y, Z reads; read_fifo: entry count and
 * one burst; status). The TBD paths are tested against generated all-TBD and
 * partial register headers (test_adxl355_tbd.c, test_adxl355_partial.c).
 * ADXL355_ERR_NOT_IMPLEMENTED is returned only by the A1 stub bodies.
 *
 * Header deps: stdint, stdbool, stddef.
 */
#ifndef PICO355_ADXL355_H
#define PICO355_ADXL355_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Return codes of the adxl355 module. */
typedef enum {
    ADXL355_OK = 0,
    ADXL355_ERR_ARG,             /**< NULL pointer or value out of range */
    ADXL355_ERR_BUS,             /**< bus transfer callback reported failure */
    ADXL355_ERR_ID,              /**< device ID read back did not match */
    ADXL355_ERR_TBD,             /**< needed register entry is still TODO(TBD) in adxl355_regs.h; no transfer made */
    ADXL355_ERR_NOT_INIT,        /**< adxl355_init has not succeeded on this dev */
    ADXL355_ERR_NOT_IMPLEMENTED  /**< stub body only */
} adxl355_rc_t;

/** Measurement range, mirrors model enum AccelRange; UNSET = TBD default. */
typedef enum {
    ADXL355_RANGE_UNSET = 0,
    ADXL355_RANGE_2G,
    ADXL355_RANGE_4G,
    ADXL355_RANGE_8G
} adxl355_range_t;

/** One raw X, Y, Z sample (model item AccelSample: xRaw, yRaw, zRaw), sign-extended 20-bit. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} adxl355_sample_t;

/** Sensor configuration (model item ConfigCommand attributes). */
typedef struct {
    uint32_t odr_hz;         /**< output data rate; default CFG_DEFAULT_ODR_HZ (PICO355-R1) */
    adxl355_range_t range;   /**< measurement range */
    int16_t filter;          /**< high-pass corner code 0..ADXL355_HPF_CODE_MAX (no-OS enum adxl355_hpf_corner, H:145-153); ADXL355_FILTER_UNSET = HPF left unchanged, default TODO(TBD) (filter settings exposed) */
} adxl355_config_t;

/** adxl355_config_t.filter sentinel "not configured" (no legal code is negative). */
#define ADXL355_FILTER_UNSET (-1)

/**
 * Bus operations. transfer() asserts CSN, clocks len bytes out of tx and into rx
 * (rx may be NULL), releases CSN. Returns 0 on success, non-zero on failure.
 */
typedef struct {
    int (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    void *ctx;
} adxl355_bus_t;

/** Driver instance; caller owns storage. */
typedef struct {
    const adxl355_bus_t *bus;
    adxl355_config_t config;
    bool initialised;
} adxl355_t;

/** Bind the bus, load default config, reset the part and check its IDs (contract above). */
adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus);

/** Write ODR, range and filter (model action configureSensor); on OK dev->config = *cfg. */
adxl355_rc_t adxl355_configure(adxl355_t *dev, const adxl355_config_t *cfg);

/** Start (true) or stop (false) measurement. */
adxl355_rc_t adxl355_set_measuring(adxl355_t *dev, bool on);

/** Read one sample from the data registers. */
adxl355_rc_t adxl355_read_sample(adxl355_t *dev, adxl355_sample_t *out);

/** Burst-read up to max samples from the FIFO; *n_read gets the number stored in out. */
adxl355_rc_t adxl355_read_fifo(adxl355_t *dev, adxl355_sample_t *out, size_t max, size_t *n_read);

/** Read the raw STATUS register into *status. */
adxl355_rc_t adxl355_status(adxl355_t *dev, uint8_t *status);

#endif /* PICO355_ADXL355_H */
