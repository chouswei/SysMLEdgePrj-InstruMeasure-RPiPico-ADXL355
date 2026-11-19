/*
 * adxl355.h - SPI driver for the ADXL355B (model part spiDriver : SpiDriver).
 *
 * Requirements: PICO355-R4 (SPI0 reads gated by DRDY), PICO355-R2/PICO355-R6
 * (apply ODR, range, filter). Register knowledge lives only in adxl355_regs.h.
 * The bus is injected as adxl355_bus_t (main.c binds it to SDK SPI0 + CSN GP17),
 * so this module builds and tests on the host.
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
    ADXL355_ERR_TBD,             /**< needed constant is still TODO(TBD) in config.h / adxl355_regs.h */
    ADXL355_ERR_NOT_IMPLEMENTED  /**< stub body */
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
    uint8_t filter;          /**< filter setting; encoding TODO(TBD) (filter settings exposed) */
} adxl355_config_t;

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

/** Bind the bus, reset the part and check its IDs. */
adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus);

/** Write ODR, range and filter (model action configureSensor). Register sequence TODO(TBD). */
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
