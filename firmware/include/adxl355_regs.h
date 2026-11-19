/*
 * adxl355_regs.h - ADXL355B register addresses and bit fields (PICO355-R4).
 *
 * Source rule: a value may only be filled in with a citation to the ADXL355
 * datasheet Rev. D (https://www.analog.com/media/en/technical-documentation/data-sheets/adxl354_adxl355.pdf)
 * on the same line or the line above, in the form
 *   "Datasheet Rev. D, Table <n>" or "Datasheet Rev. D, Section <name>".
 * The datasheet could not be fetched on the build box when this skeleton was
 * written, so every entry below is a TODO(TBD) sentinel. Replace one entry at a
 * time, each with its citation; do not copy values from memory or other code.
 *
 * Only this header holds register knowledge; adxl355.c is its only user.
 * Header deps: none.
 */
#pragma once /* include guard (no #define: every define in this file is a register entry) */

#define ADXL355_REG_TBD 0xFFu /* TODO(TBD) sentinel: register address not yet cited from the datasheet */
#define ADXL355_FIELD_TBD 0x00u /* TODO(TBD) sentinel: bit field not yet cited from the datasheet */

/* ---- Identification ---- */
#define ADXL355_REG_DEVID_AD ADXL355_REG_TBD /* TODO(TBD) analog devices ID register, cite register map table */
#define ADXL355_REG_DEVID_MST ADXL355_REG_TBD /* TODO(TBD) MEMS ID register, cite register map table */
#define ADXL355_REG_PARTID ADXL355_REG_TBD /* TODO(TBD) part ID register, cite register map table */
#define ADXL355_DEVID_AD_EXPECTED ADXL355_FIELD_TBD /* TODO(TBD) expected DEVID_AD reset value */
#define ADXL355_PARTID_EXPECTED ADXL355_FIELD_TBD /* TODO(TBD) expected PARTID reset value */

/* ---- Status and data ---- */
#define ADXL355_REG_STATUS ADXL355_REG_TBD /* TODO(TBD) status register */
#define ADXL355_STATUS_DATA_RDY ADXL355_FIELD_TBD /* TODO(TBD) data-ready bit mask */
#define ADXL355_STATUS_FIFO_FULL ADXL355_FIELD_TBD /* TODO(TBD) FIFO full bit mask */
#define ADXL355_STATUS_FIFO_OVR ADXL355_FIELD_TBD /* TODO(TBD) FIFO overrun bit mask */
#define ADXL355_REG_FIFO_ENTRIES ADXL355_REG_TBD /* TODO(TBD) FIFO entry count register */
#define ADXL355_REG_XDATA3 ADXL355_REG_TBD /* TODO(TBD) first X data byte; X/Y/Z data layout to cite */
#define ADXL355_REG_FIFO_DATA ADXL355_REG_TBD /* TODO(TBD) FIFO read port register */
#define ADXL355_FIFO_CAPACITY ADXL355_FIELD_TBD /* TODO(TBD) FIFO size in entries, bounds CFG_FIFO_WATERMARK */

/* ---- Configuration ---- */
#define ADXL355_REG_FILTER ADXL355_REG_TBD /* TODO(TBD) filter register (ODR and high-pass corner) */
#define ADXL355_FILTER_ODR_LPF_MASK ADXL355_FIELD_TBD /* TODO(TBD) ODR/LPF field mask and code table */
#define ADXL355_FILTER_HPF_MASK ADXL355_FIELD_TBD /* TODO(TBD) high-pass corner field mask */
#define ADXL355_REG_FIFO_SAMPLES ADXL355_REG_TBD /* TODO(TBD) FIFO watermark register */
#define ADXL355_REG_RANGE ADXL355_REG_TBD /* TODO(TBD) range register */
#define ADXL355_RANGE_MASK ADXL355_FIELD_TBD /* TODO(TBD) range field mask and codes for 2 g, 4 g, 8 g */
#define ADXL355_REG_POWER_CTL ADXL355_REG_TBD /* TODO(TBD) power control register */
#define ADXL355_POWER_CTL_STANDBY ADXL355_FIELD_TBD /* TODO(TBD) standby bit mask */
#define ADXL355_REG_RESET ADXL355_REG_TBD /* TODO(TBD) soft reset register */
#define ADXL355_RESET_CODE ADXL355_FIELD_TBD /* TODO(TBD) soft reset code */

/* ---- SPI framing ---- */
#define ADXL355_SPI_ADDR_SHIFT ADXL355_FIELD_TBD /* TODO(TBD) address position in the SPI command byte */
#define ADXL355_SPI_READ_BIT ADXL355_FIELD_TBD /* TODO(TBD) read/write flag in the SPI command byte */
#define ADXL355_SAMPLE_BITS 20 /* TODO(TBD) cite data format section; 20-bit resolution is also in the model (Adxl355B.resolutionBits) */

