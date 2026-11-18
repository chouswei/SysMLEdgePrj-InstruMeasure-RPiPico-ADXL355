// Initial model: "RPi Pico - ADXL2358" (title typo kept; the part is ADXL355B, not ADXL358).
// Only owner-confirmed facts (chat 2026-11-18 TPE: t1u, t1s1, t1s3, t1s4, t2u, t4u, t7u, t7s4, t8u)
// and Fusion reads (fusion/summary.md, fusion/gsensor/summary.md). Every other number is TBD.

package Pico355Requirements {
    private import ISQ::*;
    private import SI::*;

    doc /* Requirements, prefix PICO355-R. */

    requirement <'PICO355-R1'> streamRawXyz {
        doc /* The Pico shall stream raw X, Y and Z acceleration samples from the ADXL355B
             * to a PC over USB at the full output data rate, 4000 Hz, by default.
             * Sample framing on the PC link: TBD. */
        attribute defaultStreamRate : FrequencyValue = 4000 [Hz];
    }

    requirement <'PICO355-R2'> hmiRuntimeConfig {
        doc /* A PC-side HMI shall let the user configure ADXL355B settings at runtime over the
             * USB link: output data rate (ODR), measurement range, and filters.
             * 4000 Hz stays the default rate (PICO355-R1).
             * Command protocol: TBD. HMI form (GUI or CLI): TBD. Filter settings exposed: TBD. */
    }

    requirement <'PICO355-R3'> sensorSupply {
        doc /* The ADXL355B supply shall stay within its datasheet range, 2.25 V to 3.6 V
             * (ADXL354/ADXL355 datasheet Rev. D). The sensor board makes its own VDDIO and
             * VSUPPLY from the +5V on X5 with two TPS7A2033 LDOs (3.3 V implied by the part
             * number; output voltage not yet checked against the LDO datasheet). */
        attribute supplyMin : ElectricPotentialValue = 2.25 [V];
        attribute supplyMax : ElectricPotentialValue = 3.6 [V];
    }
}

package Pico355 {
    private import ISQ::*;
    private import SI::*;

    doc /* Structure: Pico on the "Raspberry Pi Pico Breakout GP V1" carrier, cabled through
         * JST-XH header X5 (SPI0) to the separate "3-axis G sensor" board carrying the ADXL355B,
         * plus a host PC with HMI over USB CDC (proposed). Only X5 is connected; the sensor
         * board's X1 (INT1/INT2) is unconnected (owner t8u). Cable assumed straight 1:1 (unverified).
         * SPI clock, FIFO watermark and USB framing: TBD. */

    enum def AccelRange {
        doc /* ADXL355 measurement ranges: +/-2 g, +/-4 g, +/-8 g. Default range: TBD. */
        enum range2g;
        enum range4g;
        enum range8g;
    }

    port def SpiBusPort {
        doc /* SPI0 bus. Clock rate and mode: TBD. */
    }

    port def X5Port {
        doc /* 7-pin JST-XH X5, same pin order on both boards (Fusion reads 2026-11-18):
             * 1 SPI0_CSN (GP17 -> CS, pad 1), 2 SPI0_TX (GP19 -> MOSI, pad 3),
             * 3 SPI0_RX (GP16 <- MISO, pad 4), 4 SPI0_SCK (GP18 -> SCLK, pad 2),
             * 5 GP22 <- DRDY (pad 14; the breakout names it "SPI0_REST", but it is an input),
             * 6 0V, 7 +5V. Sensor side has 22 ohm series resistors on pins 1-5. */
        port spi : SpiBusPort;
    }

    port def UsbCdcPort {
        doc /* USB CDC serial (proposed). Framing and command protocol: TBD. */
    }

    part def RaspberryPiPico {
        doc /* Raspberry Pi Pico. Firmware: TBD. GP22 is read as the DRDY input, never driven. */
        port spiMaster : SpiBusPort;
        port usbDevice : UsbCdcPort;
    }

    part def PicoBreakoutGpV1 {
        doc /* "Raspberry Pi Pico Breakout GP V1" (Fusion hub AM, applied electronics,
             * fsch v61 / fbrd v109; fabricated revision unknown). MC34063 buck makes +5V,
             * which feeds Pico VSYS through a diode. Headers carry +5V only; Pico 3V3 is not
             * broken out. No accelerometer on this board. */
        part pico : RaspberryPiPico;
        port x5 : X5Port;
        port usbDevice : UsbCdcPort;
        bind usbDevice = pico.usbDevice;
    }

    part def Adxl355B {
        doc /* Analog Devices ADXL355B (ADXL355BEZ), 3-axis accelerometer, digital output
             * (SPI or I2C), 20-bit. Datasheet:
             * https://www.analog.com/media/en/technical-documentation/data-sheets/adxl354_adxl355.pdf (Rev. D).
             * Not the ADXL358. Wired for SPI on the sensor board. */
        attribute orderingCode = "ADXL355BEZ";
        attribute resolutionBits = 20;
        attribute supplyMin : ElectricPotentialValue = 2.25 [V];
        attribute supplyMax : ElectricPotentialValue = 3.6 [V];
        attribute maxOutputDataRate : FrequencyValue = 4000 [Hz];
        attribute measurementRange : AccelRange;
        port spiSlave : ~SpiBusPort;
    }

    part def Tps7a2033 {
        doc /* TI TPS7A2033 LDO. Output voltage not yet checked against its datasheet. */
    }

    part def GSensorBoard {
        doc /* "3-axis G sensor" board (Fusion hub AM, applied electronics, fsch v6 / fbrd v17;
             * fabricated revision unknown). U1 ADXL355BEZ-RL. IC2 TPS7A2033 feeds VDDIO (pad 5)
             * from +5V; IC1 TPS7A2033 feeds VSUPPLY (pad 11) from +5V through a Q1 MMBT3904
             * filter stage. Pad 7 (RESERVED) open. X1 (INT1/INT2) exists but is unconnected. */
        part accel : Adxl355B;
        part ldoVddio : Tps7a2033;
        part ldoVsupply : Tps7a2033;
        port x5 : ~X5Port;
    }

    part def PcHmi {
        doc /* PC-side HMI for runtime configuration (PICO355-R2). Form (GUI or CLI): TBD. */
    }

    part def HostPc {
        doc /* PC receiving the stream. OS and receiving software: TBD. */
        port usbHost : ~UsbCdcPort;
        part hmi : PcHmi;
    }

    part pico355System {
        part breakout : PicoBreakoutGpV1;
        part sensorBoard : GSensorBoard;
        part pc : HostPc;
        connect breakout.x5 to sensorBoard.x5;
        connect breakout.pico.spiMaster to sensorBoard.accel.spiSlave;
        connect breakout.usbDevice to pc.usbHost;
    }
}

package Pico355Root {
    private import Pico355::*;
    private import Pico355Requirements::*;

    doc /* Root: private imports only. */

    satisfy Pico355Requirements::streamRawXyz by Pico355::pico355System;
    satisfy Pico355Requirements::hmiRuntimeConfig by Pico355::pico355System;
    satisfy Pico355Requirements::sensorSupply by Pico355::pico355System;
}
