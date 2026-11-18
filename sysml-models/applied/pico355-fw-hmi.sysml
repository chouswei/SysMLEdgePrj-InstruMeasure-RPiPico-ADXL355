// Firmware + HMI + USB link delta for the Pico355 desk (proposal id pico355-fw-hmi).
// Structure decision: EXTEND BY NEW MEMBERS ONLY. The existing file applied/pico355-initial.sysml
// stays untouched (no replaceFile). Every package below is new: Pico355Firmware, Pico355Hmi,
// Pico355UsbLink, Pico355FwHmiRequirements and Pico355FwHmiRoot (private imports + satisfy only).
// Existing elements are only specialized or redefined inside new definitions.
// Only facts from the model, AGENTS.md and the owner ask; everything else is TBD.

package Pico355FwHmiRequirements {
    private import ISQ::*;
    private import SI::*;

    doc /* Firmware, HMI and USB link requirements, prefix PICO355-R, continuing after PICO355-R3. */

    requirement <'PICO355-R4'> fwDrdyAcquisition {
        doc /* Derived from PICO355-R1. The Pico (RP2040) firmware shall read ADXL355B samples over
             * SPI0 (SPI proposed, pending owner approval) only when the DRDY input on GP22 signals
             * new data; the sensor's interrupt pins stay unconnected. SPI clock: TBD.
             * FIFO burst reads (proposed, pending owner approval); FIFO watermark: TBD. */
    }

    requirement <'PICO355-R5'> fwUsbStreaming {
        doc /* Derived from PICO355-R1. The firmware shall buffer the raw X, Y and Z samples and
             * stream them to the PC on the sampleStream channel of the USB CDC serial link
             * (USB CDC proposed, pending owner approval). USB frame format: TBD.
             * Sample buffer depth: TBD. */
    }

    requirement <'PICO355-R6'> fwCommandHandling {
        doc /* Derived from PICO355-R2. The firmware shall accept commands from the PC on the
             * commandChannel of the USB link to set output data rate, measurement range and
             * filters at runtime, and shall answer each command on the same channel.
             * Command set details: TBD. */
    }

    requirement <'PICO355-R7'> fwOperatingStates {
        doc /* Derived from PICO355-R1 and PICO355-R2. The firmware shall run as a state machine
             * with the states idle, configuring, streaming and fault. Initial state, transitions,
             * triggers and fault handling: TBD. */
    }

    requirement <'PICO355-R8'> hmiConfigUi {
        doc /* Derived from PICO355-R2. The PC HMI shall open the USB CDC serial connection to the
             * Pico and offer a configuration UI for output data rate, measurement range and
             * filters. HMI tech stack: TBD. Filter settings exposed: TBD. */
    }

    requirement <'PICO355-R9'> hmiLiveViewAndLogging {
        doc /* Derived from PICO355-R1 and PICO355-R2. The PC HMI shall show the received samples
             * in a live data view and log them. Log format and storage: TBD. HMI tech stack: TBD. */
    }
}

package Pico355UsbLink {
    private import ISQ::*;
    private import SI::*;

    doc /* USB link between pico.usbDevice and pc.usbHost: USB CDC serial (proposed, pending owner
         * approval). Two channels: sampleStream (Pico to PC) and commandChannel (command and
         * response, both directions). USB frame format: TBD. Command set details: TBD. */

    item def AccelSample {
        doc /* One raw X, Y, Z sample from the ADXL355B. Encoding on the link: TBD
             * (USB frame format TBD). */
        attribute xRaw;
        attribute yRaw;
        attribute zRaw;
    }

    item def SampleBatch {
        doc /* Group of raw samples sent on sampleStream. Batch size and framing: TBD
             * (USB frame format TBD; FIFO watermark TBD). */
        item accelSamples : AccelSample [*];
    }

    item def CommandMessage {
        doc /* Command from the PC HMI to the Pico firmware. Command set details: TBD. */
    }

    item def ConfigCommand :> CommandMessage {
        doc /* Runtime sensor configuration (PICO355-R2): output data rate, measurement range,
             * filters. Encoding, allowed values and defaults beyond PICO355-R1: TBD. */
        attribute outputDataRate : FrequencyValue;
        attribute measurementRange : Pico355::AccelRange;
        attribute filterSetting;
    }

    item def ResponseMessage {
        doc /* Answer from the Pico firmware to a command (acknowledge, error or status).
             * Content and encoding: TBD (command set details TBD). */
    }

    port def SampleStreamPort {
        doc /* Device-side sampleStream channel: samples leave the Pico. */
        out item samples : SampleBatch;
    }

    port def CommandChannelPort {
        doc /* Device-side commandChannel: commands in, responses out. Command set details: TBD. */
        in item command : CommandMessage;
        out item response : ResponseMessage;
    }

    port def UsbCdcChannelPort :> Pico355::UsbCdcPort {
        doc /* Device-side USB CDC serial port with its two channels (USB CDC proposed, pending
             * owner approval). USB frame format: TBD. */
        port sampleStream : SampleStreamPort;
        port commandChannel : CommandChannelPort;
    }

    interface def Pico355UsbLink {
        doc /* USB link interface between the Pico usbDevice and the PC usbHost (USB CDC serial,
             * proposed, pending owner approval). USB frame format: TBD. Command set details: TBD.
             * USB cable and connector: TBD. */
        end usbDevice : UsbCdcChannelPort;
        end usbHost : ~UsbCdcChannelPort;
        flow sampleFlow of SampleBatch
            from usbDevice.sampleStream.samples to usbHost.sampleStream.samples;
        flow commandFlow of CommandMessage
            from usbHost.commandChannel.command to usbDevice.commandChannel.command;
        flow responseFlow of ResponseMessage
            from usbDevice.commandChannel.response to usbHost.commandChannel.response;
    }

    part pico355FwHmiSystem :> Pico355::pico355System {
        doc /* The Pico355 system with the firmware on the Pico, the HMI on the PC and the USB link
             * interface between pico.usbDevice and pc.usbHost. Open hardware items kept from the
             * existing model: LDO output check: TBD (PICO355-R3); X5 cable pinout: TBD
             * (assumed straight, unverified). */
        part :>> breakout : Pico355Firmware::Pico355FwBreakout;
        part :>> pc : Pico355Hmi::Pico355HmiHost;
        interface pico355UsbLink : Pico355UsbLink connect breakout.usbDevice to pc.usbHost;
    }
}

package Pico355Firmware {
    private import ISQ::*;
    private import SI::*;

    doc /* Pico (RP2040) firmware: SPI driver to the ADXL355B on SPI0, DRDY-driven acquisition on
         * GP22, sample buffer, USB CDC streaming and a command handler for runtime configuration.
         * Language, toolchain and scheduling: TBD. */

    part def Pico355FwBreakout :> Pico355::PicoBreakoutGpV1 {
        doc /* Breakout board carrying the Pico that runs the firmware. Its usbDevice port is the
             * USB CDC channel port, bound to pico.usbDevice as in the existing breakout. */
        part :>> pico : Pico355FwPico;
        port :>> usbDevice : Pico355UsbLink::UsbCdcChannelPort;
    }

    part def Pico355FwPico :> Pico355::RaspberryPiPico {
        doc /* Raspberry Pi Pico running the firmware. GP22 is read as the DRDY input, never driven. */
        port :>> usbDevice : Pico355UsbLink::UsbCdcChannelPort;
        part firmware : PicoFirmware;
    }

    part def PicoFirmware {
        doc /* Firmware image on the RP2040. Build, memory use and timing budget: TBD. */
        part spiDriver : SpiDriver;
        part drdyAcquisition : DrdyAcquisition;
        part sampleBuffer : SampleBuffer;
        part usbCdcStreamer : UsbCdcStreamer;
        part commandHandler : CommandHandler;
        exhibit state fwStates : FirmwareStates;
        perform action configureSensor : ConfigureSensor;
        perform action acquireOnDrdy : AcquireOnDrdy;
        perform action bufferSamples : BufferSamples;
        perform action streamSamples : StreamSamples;
        perform action handleCommand : HandleCommand;
    }

    part def SpiDriver {
        doc /* SPI master driver to the ADXL355B on SPI0 (SPI proposed, pending owner approval).
             * SPI clock: TBD. SPI mode: TBD. Register access details: TBD. */
    }

    part def DrdyAcquisition {
        doc /* Waits for DRDY on GP22, then reads samples through the SPI driver. FIFO burst reads
             * (proposed, pending owner approval). FIFO watermark: TBD. DRDY edge and polling or
             * interrupt use: TBD. */
    }

    part def SampleBuffer {
        doc /* Holds samples between acquisition and USB streaming. Depth and overflow policy: TBD. */
    }

    part def UsbCdcStreamer {
        doc /* Sends sample batches on the sampleStream channel over USB CDC serial (proposed,
             * pending owner approval). USB frame format: TBD. */
    }

    part def CommandHandler {
        doc /* Parses commands from commandChannel, applies output data rate, range and filter
             * settings through the SPI driver, and sends responses. Command set details: TBD. */
    }

    state def FirmwareStates {
        doc /* Firmware operating states (PICO355-R7). Initial state, transitions, triggers,
             * entry and exit behaviour and fault recovery: TBD. */
        state idleState;
        state configuringState;
        state streamingState;
        state faultState;
    }

    action def ConfigureSensor {
        doc /* Write output data rate, range and filter settings to the ADXL355B over SPI0.
             * Register sequence: TBD. */
        in item config : Pico355UsbLink::ConfigCommand;
    }

    action def AcquireOnDrdy {
        doc /* On DRDY (GP22), read the new samples, by FIFO burst reads (proposed, pending owner
             * approval). FIFO watermark: TBD. SPI clock: TBD. */
        out item batch : Pico355UsbLink::SampleBatch;
    }

    action def BufferSamples {
        doc /* Queue acquired samples for streaming. Buffer depth: TBD. */
        in item batchIn : Pico355UsbLink::SampleBatch;
        out item batchOut : Pico355UsbLink::SampleBatch;
    }

    action def StreamSamples {
        doc /* Send buffered samples to the PC on sampleStream. USB frame format: TBD. */
        in item batch : Pico355UsbLink::SampleBatch;
    }

    action def HandleCommand {
        doc /* Handle one command from commandChannel and answer it. Command set details: TBD. */
        in item command : Pico355UsbLink::CommandMessage;
        out item response : Pico355UsbLink::ResponseMessage;
    }
}

package Pico355Hmi {
    private import ISQ::*;
    private import SI::*;

    doc /* PC HMI: USB CDC serial connection, configuration UI for output data rate, range and
         * filters, live data view and logging. HMI tech stack: TBD (GUI or CLI: TBD). */

    part def Pico355HmiHost :> Pico355::HostPc {
        doc /* Host PC running the HMI. OS: TBD. */
        port :>> usbHost : ~Pico355UsbLink::UsbCdcChannelPort;
        part :>> hmi : Pico355HmiApp;
    }

    part def Pico355HmiApp :> Pico355::PcHmi {
        doc /* HMI application. HMI tech stack: TBD. */
        part serialConnection : SerialConnection;
        part configUi : ConfigUi;
        part liveDataView : LiveDataView;
        part dataLogger : DataLogger;
        perform action openSerialConnection : OpenSerialConnection;
        perform action editSensorConfig : EditSensorConfig;
        perform action showLiveData : ShowLiveData;
        perform action logSamples : LogSamples;
    }

    part def SerialConnection {
        doc /* USB CDC serial connection to the Pico (proposed, pending owner approval). Port
             * discovery and serial settings: TBD. USB frame format: TBD. */
    }

    part def ConfigUi {
        doc /* Lets the user set output data rate, range and filters at runtime (PICO355-R2).
             * Filter settings exposed: TBD. HMI tech stack: TBD. */
    }

    part def LiveDataView {
        doc /* Shows received X, Y, Z samples live. Display form and refresh rate: TBD. */
    }

    part def DataLogger {
        doc /* Logs received samples. Log format and storage: TBD. */
    }

    action def OpenSerialConnection {
        doc /* Open the USB CDC serial connection to the Pico. Details: TBD. */
    }

    action def EditSensorConfig {
        doc /* Build a configuration command from the user's settings. Command set details: TBD. */
        out item config : Pico355UsbLink::ConfigCommand;
    }

    action def ShowLiveData {
        doc /* Display incoming sample batches. Details: TBD. */
        in item batch : Pico355UsbLink::SampleBatch;
    }

    action def LogSamples {
        doc /* Write incoming sample batches to the log. Log format: TBD. */
        in item batch : Pico355UsbLink::SampleBatch;
    }
}

package Pico355FwHmiRoot {
    private import Pico355FwHmiRequirements::*;

    doc /* Root for the firmware + HMI delta: private imports and satisfy only. */

    satisfy Pico355FwHmiRequirements::fwDrdyAcquisition by Pico355UsbLink::pico355FwHmiSystem;
    satisfy Pico355FwHmiRequirements::fwUsbStreaming by Pico355UsbLink::pico355FwHmiSystem;
    satisfy Pico355FwHmiRequirements::fwCommandHandling by Pico355UsbLink::pico355FwHmiSystem;
    satisfy Pico355FwHmiRequirements::fwOperatingStates by Pico355UsbLink::pico355FwHmiSystem;
    satisfy Pico355FwHmiRequirements::hmiConfigUi by Pico355UsbLink::pico355FwHmiSystem;
    satisfy Pico355FwHmiRequirements::hmiLiveViewAndLogging by Pico355UsbLink::pico355FwHmiSystem;
}
