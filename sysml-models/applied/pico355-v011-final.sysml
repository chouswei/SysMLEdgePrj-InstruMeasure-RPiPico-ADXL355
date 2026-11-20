package Pico355Toolchain {
    private import Pico355Firmware::*;
    private import Pico355Hmi::*;

    doc /* Records the implementation-language choice on the existing firmware and HMI elements. */

    part def PicoFirmwareC :> Pico355Firmware::PicoFirmware {
        doc /* Implementation language: C on the Pico SDK (owner decision). */
    }

    part def Pico355HmiAppRust :> Pico355Hmi::Pico355HmiApp {
        doc /* Implementation language: Rust (owner decision). GUI stack: eframe 0.37, egui_plot 0.38, hdf5-pure 0.48. */
    }
}

package Pico355Link {
    private import ScalarValues::*;
    private import ISQ::*;
    private import SI::*;

    doc /* Link settings and frame layout. Pins: SPI0 GP16 MISO, GP17 CSN, GP18 SCK, GP19 MOSI, DRDY GP22 (input only).
         * Cited reference: Analog Devices no-OS commit 4557b1f58f8a233bd116dbb02d8d5c447d3699b6. */

    part def SpiSettings {
        doc /* SPI0 settings cited from no-OS eval-adxl355-pmdz: 1 MHz, mode 0, MSB first. */
        attribute clockHz : Natural = 1000000;
        attribute spiMode : Natural = 0;
        attribute msbFirst : Boolean = true;
        attribute postResetDelayMs : Natural = 1;
    }

    part def SerialSettings {
        doc /* USB CDC open settings, design decision. CDC ignores the baud value. 8N1, no flow control, DTR asserted by the HMI. */
        attribute baudValue : Natural = 115200;
        attribute dataBits : Natural = 8;
        attribute stopBits : Natural = 1;
        attribute readTimeoutMs : Natural = 100;
        attribute dtrAsserted : Boolean = true;
    }

    part def FrameLayout {
        doc /* Frame = header (sync, type, length u16 LE) + payload + CRC-16/CCITT-FALSE (u16 LE) over header and payload. Design decision. */
        attribute sync : Natural = 165;
        attribute headerLen : Natural = 4;
        attribute lengthFieldBytes : Natural = 2;
        attribute lengthLittleEndian : Boolean = true;
        attribute checksumKind : String = "CRC-16/CCITT-FALSE";
        attribute checksumBytes : Natural = 2;
        attribute maxPayload : Natural = 576;
        attribute typeSampleBatch : Natural = 1;
        attribute typeCommand : Natural = 2;
        attribute typeResponse : Natural = 3;
    }

    part def SampleEncoding {
        doc /* Sample-batch payload: N samples, x then y then z, each 3 bytes two's complement little-endian (20-bit value sign-extended). */
        attribute axisBytes : Natural = 3;
        attribute axesPerSample : Natural = 3;
        attribute littleEndian : Boolean = true;
    }

    part def CommandSet {
        doc /* Command payload 5 bytes: code, u32 LE value. Response payload 6 bytes: code, answered command code, u32 LE value.
             * RESP_ACK value from firmware is 0, so the HMI remembers pending values and applies them on ACK.
             * RESP_STATUS value is the FIFO overflow count (u32 LE, saturating). CMD_GET_STATUS ignores its value and changes no state. */
        attribute cmdSetOdr : Natural = 1;
        attribute cmdSetRange : Natural = 2;
        attribute cmdSetFilter : Natural = 3;
        attribute cmdGetStatus : Natural = 4;
        attribute respAck : Natural = 1;
        attribute respError : Natural = 2;
        attribute respStatus : Natural = 3;
        attribute commandPayloadLen : Natural = 5;
        attribute responsePayloadLen : Natural = 6;
    }

    part def LinkDefaults {
        doc /* Needed to stream. Default range and buffer depth are design choices. Default filter stays TBD. */
        attribute defaultOdrHz : Natural = 4000;
        attribute defaultRange : String = "2g";
        attribute sampleBufferDepth : Natural = 64;
    }

    part def FifoDrain {
        doc /* ADXL355 FIFO drain on SPI0 by RP2040 DMA. FIFO capacity 96 entries = 32 XYZ sets (no-OS README.rst:92).
             * Whole sets only. One CS-low window of 1 + entries x 3 bytes: TX repeats the FIFO_DATA read byte 0x23,
             * the first received byte is a dummy. Shipped in v0.1.1 (PR #7), verified on RPi Pico + ADXL355B over COM5 at 4 kHz. */
        attribute fifoCapacityEntries : Natural = 96;
        attribute entriesPerSample : Natural = 3;
        attribute fifoDataReadByte : Natural = 35;
        attribute watermarkEntries : Natural = 48;
        attribute drdyPin : Natural = 22;
        attribute triggerRule : String = "poll FIFO_ENTRIES against the watermark on each DRDY GP22 high";
        attribute overflowCountBits : Natural = 32;
        attribute spiLoadPercentDataPlusDrain : Natural = 30;
    }

    part linkSettings {
        part fifoDrain : FifoDrain;
        part spi : SpiSettings;
        part serial : SerialSettings;
        part frameLayout : FrameLayout;
        part sampleEncoding : SampleEncoding;
        part commandSet : CommandSet;
        part linkDefaults : LinkDefaults;
    }
}

package Pico355HmiGui {
    private import ISQ::*;
    private import SI::*;
    private import ScalarValues::*;
    private import Pico355Hmi::*;

    doc /* PC HMI native Windows window (eframe 0.37, egui_plot 0.38, hdf5-pure 0.48). Headless mode kept as --cli.
         * Firmware protocol is unchanged by this package. */

    attribute def PicoUsbIdentity {
        doc /* USB identity used to flag the Pico CDC port: VID 0x2E8A, PID 0x000A. A non-matching port stays selectable. */
        attribute vendorId : Natural = 11914;
        attribute productId : Natural = 10;
    }

    part def PortDiscovery {
        doc /* Lists serial ports, Pico-identified ports first, refresh on demand, custom path entry allowed. */
        attribute identity : PicoUsbIdentity;
        attribute refreshOnDemand : Boolean = true;
    }

    part def ConnectionControl :> Pico355Hmi::SerialConnection {
        doc /* Connect and disconnect the USB CDC link. Last ACK or ERR is shown next to the control. */
        attribute lastResponseShown : Boolean = true;
    }

    enum def OutputDataRateHz {
        doc /* ODR choices offered by the UI: 4000, 2000, 1000, 500, 250, 125 Hz. */
        enum odr4000;
        enum odr2000;
        enum odr1000;
        enum odr500;
        enum odr250;
        enum odr125;
    }

    enum def MeasurementRange {
        doc /* Range choices: 2 g, 4 g, 8 g. */
        enum range2g;
        enum range4g;
        enum range8g;
    }

    part def HighPassFilter {
        doc /* High-pass filter code 0..6 over CMD_SET_FILTER. */
        attribute hpfCodeMax : Natural = 6;
    }

    part def ControlPanel :> Pico355Hmi::ConfigUi {
        doc /* ODR, range and high-pass controls. Only an ACKed range change moves the displayed g scale. */
        part outputDataRate : OutputDataRateHz;
        part measurementRange : MeasurementRange;
        part highPass : HighPassFilter;
    }

    part def StatusPoll {
        doc /* CMD_GET_STATUS polled about once per second plus manual refresh. Shows measured rate, sample count and FIFO overflow count. */
        attribute pollIntervalMs : Natural = 1000;
        attribute manualRefresh : Boolean = true;
    }

    attribute def GScale {
        doc /* Raw 20-bit counts to g. no-OS 4557b1f58f8a233bd116dbb02d8d5c447d3699b6 scale factors:
             * 3.9 ug/LSB at 2 g, 7.8 ug/LSB at 4 g, 15.6 ug/LSB at 8 g. */
        attribute ugPerLsb2g : Real = 3.9;
        attribute ugPerLsb4g : Real = 7.8;
        attribute ugPerLsb8g : Real = 15.6;
    }

    part def StackedAxisCharts :> Pico355Hmi::LiveDataView {
        doc /* Three stacked g-scaled charts for X, Y and Z. Window choices 2 s, 5 s, 10 s. Vertical axis in g. */
        attribute windowChoicesS : Natural[3] = (2, 5, 10);
        attribute axisUnit : String = "g";
    }

    part def OscilloscopeTrace {
        doc /* PR #9, released as v0.1.1: single-colour min/max oscilloscope trace in plots_ui.
             * Strictly one Line per axis via envelope_trace, replacing the earlier dual min/max lines.
             * Colours: X red, Y green, Z blue. */
        attribute linesPerAxis : Natural = 1;
        attribute xColor : String = "red";
        attribute yColor : String = "green";
        attribute zColor : String = "blue";
        attribute traceBuilder : String = "envelope_trace";
    }

    part def Hdf5SessionLogger :> Pico355Hmi::DataLogger {
        doc /* Background writer thread writing a real .h5 file, verified readable by h5py 3.16 / libhdf5 2.0.0.
             * Groups: accel_g/{x_g,y_g,z_g} f64; raw_counts/{x,y,z} i32; root sample_index u64. Chunk length 4096. */
        attribute accelGroup : String = "accel_g";
        attribute rawGroup : String = "raw_counts";
        attribute chunkLength : Natural = 4096;
        attribute sensorName : String = "ADXL355";
        attribute unitsName : String = "g";
    }

    part def HeadlessStream {
        doc /* --cli --port <name> runs the headless stream without a window. */
        attribute cliFlag : String = "--cli";
        attribute portFlag : String = "--port";
    }

    part def HmiWindow :> Pico355Hmi::Pico355HmiApp {
        doc /* Default native window: port discovery, connection, configuration, status poll, stacked charts, oscilloscope trace and HDF5 logging. */
        part portDiscovery : PortDiscovery;
        part serialLink : ConnectionControl;
        part controls : ControlPanel;
        part statusPoll : StatusPoll;
        part charts : StackedAxisCharts;
        part traceView : OscilloscopeTrace;
        part sessionLogger : Hdf5SessionLogger;
        part headless : HeadlessStream;
        attribute gScale : GScale;
    }
}

package Pico355V011Status {
    private import ScalarValues::*;

    doc /* Release status of v0.1.1. Merged PRs #7, #8, #9, verified on hardware. */

    part def ReleaseStatus {
        attribute version : String;
        attribute hardwareVerified : Boolean;
        attribute verifiedOn : String;
        attribute artifact : String;
        attribute artifactSha256Prefix : String;
    }

    part firmwareRelease : ReleaseStatus {
        doc /* PR #7: DMA FIFO drain, 48-entry watermark, DRDY GP22 poll, CMD_GET_STATUS 0x04 / RESP_STATUS 0x03.
             * Flashed and verified at 4 kHz. */
        attribute :>> version = "0.1.1";
        attribute :>> hardwareVerified = true;
        attribute :>> verifiedOn = "RPi Pico + ADXL355B, COM5, 4 kHz";
        attribute :>> artifact = "pico355_fw.uf2";
        attribute :>> artifactSha256Prefix = "a8efe924";
    }

    part hmiRelease : ReleaseStatus {
        doc /* PR #8 native GUI and HDF5 logging, PR #9 single-colour oscilloscope trace. Verified running live on hardware. */
        attribute :>> version = "0.1.1";
        attribute :>> hardwareVerified = true;
        attribute :>> verifiedOn = "RPi Pico + ADXL355B, COM5, live";
        attribute :>> artifact = "pico355-hmi.exe";
        attribute :>> artifactSha256Prefix = "d565fa36";
    }
}
