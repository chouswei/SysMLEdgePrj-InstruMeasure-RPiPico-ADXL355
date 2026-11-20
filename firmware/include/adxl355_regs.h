/*
 * adxl355_regs.h - ADXL355B register addresses and bit fields (PICO355-R4).
 *
 * Source rule: a value may only be filled in with a citation, on the same line or
 * the line above, to the Analog Devices no-OS driver (owner-named source, t36u):
 *   no-OS = https://github.com/analogdevicesinc/no-OS, commit
 *   4557b1f58f8a233bd116dbb02d8d5c447d3699b6 (all line numbers below are in the
 *   files at that commit, never the moving main tip):
 *     H = drivers/accel/adxl355/adxl355.h, C = drivers/accel/adxl355/adxl355.c
 *   cited as "no-OS H:<line>" / "no-OS C:<line>" (SHA above applies to every one).
 * Do not copy values from memory or from other code; an entry the no-OS driver
 * does not define stays ADXL355_TBD. No datasheet table number is cited here.
 *
 * TBD rule (same as config.h, see docs/protocol.md "TBD rule"): every uncited
 * entry is ADXL355_TBD, a 64-bit all-ones value that no 8-bit register address,
 * mask or code can equal. Test entries with ADXL355_IS_TBD(x) (C or #if); the
 * driver returns ADXL355_ERR_TBD for any operation that needs a TBD entry,
 * before any bus transfer (contract in adxl355.h). ADXL355_REGS_ARE_TBD (end of
 * this file) is 1 while at least one entry the driver uses is TBD, 0 once every
 * entry the driver uses is cited.
 *
 * Only this header holds register knowledge; adxl355.c is its only user.
 * Header deps: none.
 */
#pragma once /* include guard (no #define: every define in this file is a register entry) */

#define ADXL355_TBD 0xFFFFFFFFFFFFFFFFull /* TODO(TBD) sentinel "not configured": outside every 8-bit address, mask and code */
#define ADXL355_IS_TBD(x) ((x) == ADXL355_TBD) /* TODO(TBD) sentinel test, usable in C and #if */
#define ADXL355_REG_TBD ADXL355_TBD /* TODO(TBD) sentinel: register address not defined by the no-OS driver */
#define ADXL355_FIELD_TBD ADXL355_TBD /* TODO(TBD) sentinel: bit field or code not defined by the no-OS driver */

/* ---- Identification ---- */
#define ADXL355_REG_DEVID_AD 0x00 /* no-OS H:29 ADXL355_DEVID_AD address */
#define ADXL355_REG_DEVID_MST 0x01 /* no-OS H:30 ADXL355_DEVID_MST address */
#define ADXL355_REG_PARTID 0x02 /* no-OS H:31 ADXL355_PARTID address */
#define ADXL355_DEVID_AD_EXPECTED 0xAD /* no-OS H:29 DEVID_AD reset value, checked at C:137-141 */
#define ADXL355_DEVID_MST_EXPECTED 0x1D /* no-OS H:30 DEVID_MST reset value, checked at C:146-150 (not part of the earlier skeleton) */
#define ADXL355_PARTID_EXPECTED 0xED /* no-OS H:31 PARTID reset value for ID_ADXL355 (H:32 gives 0xE9 for the other part, not used) */

/* ---- Status and data ---- */
#define ADXL355_REG_STATUS 0x04 /* no-OS H:34 ADXL355_STATUS address */
#define ADXL355_STATUS_DATA_RDY 0x01 /* no-OS H:220 DATA_RDY is bit-field member 0 of the status byte (LSB first) */
#define ADXL355_STATUS_FIFO_FULL 0x02 /* no-OS H:221 FIFO_FULL is bit-field member 1 */
#define ADXL355_STATUS_FIFO_OVR 0x04 /* no-OS H:222 FIFO_OVR is bit-field member 2 */
#define ADXL355_STATUS_NVM_BUSY 0x10 /* no-OS H:224 NVM_BUSY is bit-field member 4, polled at C:288 */
#define ADXL355_REG_FIFO_ENTRIES 0x05 /* no-OS H:35 ADXL355_FIFO_ENTRIES address */
#define ADXL355_REG_XDATA3 0x08 /* no-OS H:37 ADXL355_XDATA address, 3 bytes (H:37 length 3), bits [19:12],[11:4],[3:0]<<4 per C:896-902 */
#define ADXL355_REG_YDATA3 0x0B /* no-OS H:38 ADXL355_YDATA address, 3 bytes */
#define ADXL355_REG_ZDATA3 0x0E /* no-OS H:39 ADXL355_ZDATA address, 3 bytes */
#define ADXL355_REG_FIFO_DATA 0x11 /* no-OS H:40 ADXL355_FIFO_DATA address, read in one burst at C:718-719 */
#define ADXL355_FIFO_X_MARKER 0x01 /* no-OS C:724 third byte of an X entry has bit 0 set */
#define ADXL355_FIFO_EMPTY 0x02 /* no-OS C:725 third byte bit 1 set means empty entry, skipped */
#define ADXL355_FIFO_SAMPLES_MAX 0x60 /* no-OS H:57 ADXL355_MAX_FIFO_SAMPLES_VAL, the largest FIFO_SAMPLES watermark value (C:682); bounds CFG_FIFO_WATERMARK */
#define ADXL355_FIFO_CAPACITY 96 /* FIFO size in entries (3 entries = 1 XYZ set, so 32 sets): no-OS pin 4557b1f58f8a233bd116dbb02d8d5c447d3699b6 drivers/accel/adxl355/README.rst:92 "There are 96 locations in the FIFO" (README.rst:96 default and maximum FIFO samples value 96, :127 overflow past this limit); equals ADXL355_FIFO_SAMPLES_MAX */

/* ---- Configuration ---- */
#define ADXL355_REG_FILTER 0x28 /* no-OS H:47 ADXL355_FILTER address (ODR/LPF and high-pass corner) */
#define ADXL355_FILTER_ODR_LPF_MASK 0x0F /* no-OS H:119 ODR_LPF_FIELD_MSK = GENMASK(3,0) */
#define ADXL355_FILTER_HPF_MASK 0x70 /* no-OS H:120 HPF_FIELD_MSK = GENMASK(6,4); code is shifted left 4 (C:449) */
#define ADXL355_FILTER_HPF_SHIFT 4 /* no-OS C:449 (hpf_corner_val << 4) */
#define ADXL355_HPF_CODE_MAX 6 /* no-OS H:145-153 enum adxl355_hpf_corner: OFF=0 .. 0_0238=6 */
#define ADXL355_ODR_CODE_4000HZ 0 /* no-OS H:156 ADXL355_ODR_4000HZ */
#define ADXL355_ODR_CODE_2000HZ 1 /* no-OS H:157 ADXL355_ODR_2000HZ */
#define ADXL355_ODR_CODE_1000HZ 2 /* no-OS H:158 ADXL355_ODR_1000HZ */
#define ADXL355_ODR_CODE_500HZ 3 /* no-OS H:159 ADXL355_ODR_500HZ */
#define ADXL355_ODR_CODE_250HZ 4 /* no-OS H:160 ADXL355_ODR_250HZ */
#define ADXL355_ODR_CODE_125HZ 5 /* no-OS H:161 ADXL355_ODR_125HZ (lower ODRs H:162-166 are fractional Hz, not used) */
#define ADXL355_REG_FIFO_SAMPLES 0x29 /* no-OS H:48 ADXL355_FIFO_SAMPLES address (FIFO watermark register), written by adxl355_set_fifo_watermark (no-OS C:678-692, value above ADXL355_FIFO_SAMPLES_MAX refused at C:682) */
#define ADXL355_REG_RANGE 0x2C /* no-OS H:51 ADXL355_RANGE address */
#define ADXL355_RANGE_MASK 0x03 /* no-OS H:118 RANGE_FIELD_MSK = GENMASK(1,0) */
#define ADXL355_RANGE_CODE_2G 1 /* no-OS H:170 ADXL355_RANGE_2G */
#define ADXL355_RANGE_CODE_4G 2 /* no-OS H:172 ADXL355_RANGE_4G */
#define ADXL355_RANGE_CODE_8G 3 /* no-OS H:174 ADXL355_RANGE_8G */
#define ADXL355_REG_POWER_CTL 0x2D /* no-OS H:52 ADXL355_POWER_CTL address */
#define ADXL355_POWER_CTL_STANDBY 0x01 /* no-OS H:131 ADXL355_STDBY_TEMP_ON_DRDY_ON = 1 (odd op modes are standby, H:130-137); reset value H:52 */
#define ADXL355_POWER_CTL_MEASURE_VAL 0x00 /* no-OS H:130 ADXL355_MEAS_TEMP_ON_DRDY_ON = 0, written whole to POWER_CTL (C:232-233) */
#define ADXL355_REG_RESET 0x2F /* no-OS H:54 ADXL355_RESET address */
#define ADXL355_RESET_CODE 0x52 /* no-OS H:59 ADXL355_RESET_CODE, written at C:275-280 */

/* ---- SPI framing ---- */
#define ADXL355_SPI_ADDR_SHIFT 1 /* no-OS C:46 and C:81 command byte = (address << 1) | read/write bit */
#define ADXL355_SPI_READ_BIT 0x01 /* no-OS H:20 ADXL355_SPI_READ, used at C:46 */
#define ADXL355_SPI_WRITE_BIT 0x00 /* no-OS H:21 ADXL355_SPI_WRITE, used at C:81 */
#define ADXL355_SAMPLE_BITS 20 /* no-OS C:896-902 sample is bits [19:0] of a 24-bit word (>>4) and bit 19 is the sign bit (C:921); also model Adxl355B.resolutionBits */
#define ADXL355_SAMPLE_SHIFT 4 /* no-OS C:902 raw 24-bit word >> 4 */

/* ADXL355_REGS_ARE_TBD: 1 while at least one entry the driver uses is TBD, 0 once
 * every entry the driver uses is cited. Computed from the entries so it cannot
 * drift. ADXL355_FIFO_CAPACITY (a cited size, not a register) is used by the drain as a bound only.
 * TODO(TBD) flag: stays 1 while any used entry is TBD. */
#define ADXL355_REGS_ARE_TBD ( \
    ADXL355_IS_TBD(ADXL355_REG_DEVID_AD) || ADXL355_IS_TBD(ADXL355_REG_DEVID_MST) || \
    ADXL355_IS_TBD(ADXL355_REG_PARTID) || ADXL355_IS_TBD(ADXL355_DEVID_AD_EXPECTED) || \
    ADXL355_IS_TBD(ADXL355_DEVID_MST_EXPECTED) || ADXL355_IS_TBD(ADXL355_PARTID_EXPECTED) || \
    ADXL355_IS_TBD(ADXL355_REG_STATUS) || ADXL355_IS_TBD(ADXL355_STATUS_NVM_BUSY) || \
    ADXL355_IS_TBD(ADXL355_REG_FIFO_ENTRIES) || ADXL355_IS_TBD(ADXL355_REG_XDATA3) || \
    ADXL355_IS_TBD(ADXL355_REG_YDATA3) || ADXL355_IS_TBD(ADXL355_REG_ZDATA3) || \
    ADXL355_IS_TBD(ADXL355_REG_FIFO_DATA) || ADXL355_IS_TBD(ADXL355_FIFO_X_MARKER) || \
    ADXL355_IS_TBD(ADXL355_FIFO_EMPTY) || ADXL355_IS_TBD(ADXL355_REG_FILTER) || \
    ADXL355_IS_TBD(ADXL355_FILTER_ODR_LPF_MASK) || ADXL355_IS_TBD(ADXL355_FILTER_HPF_MASK) || \
    ADXL355_IS_TBD(ADXL355_FILTER_HPF_SHIFT) || ADXL355_IS_TBD(ADXL355_HPF_CODE_MAX) || \
    ADXL355_IS_TBD(ADXL355_REG_RANGE) || ADXL355_IS_TBD(ADXL355_RANGE_MASK) || \
    ADXL355_IS_TBD(ADXL355_RANGE_CODE_2G) || ADXL355_IS_TBD(ADXL355_RANGE_CODE_4G) || \
    ADXL355_IS_TBD(ADXL355_RANGE_CODE_8G) || ADXL355_IS_TBD(ADXL355_REG_POWER_CTL) || \
    ADXL355_IS_TBD(ADXL355_POWER_CTL_STANDBY) || ADXL355_IS_TBD(ADXL355_POWER_CTL_MEASURE_VAL) || \
    ADXL355_IS_TBD(ADXL355_REG_RESET) || ADXL355_IS_TBD(ADXL355_RESET_CODE) || \
    ADXL355_IS_TBD(ADXL355_SPI_ADDR_SHIFT) || ADXL355_IS_TBD(ADXL355_SPI_READ_BIT) || \
    ADXL355_IS_TBD(ADXL355_SPI_WRITE_BIT) || ADXL355_IS_TBD(ADXL355_SAMPLE_SHIFT) || \
    ADXL355_IS_TBD(ADXL355_ODR_CODE_4000HZ) || ADXL355_IS_TBD(ADXL355_ODR_CODE_2000HZ) || \
    ADXL355_IS_TBD(ADXL355_ODR_CODE_1000HZ) || ADXL355_IS_TBD(ADXL355_ODR_CODE_500HZ) || \
    ADXL355_IS_TBD(ADXL355_ODR_CODE_250HZ) || ADXL355_IS_TBD(ADXL355_ODR_CODE_125HZ))
