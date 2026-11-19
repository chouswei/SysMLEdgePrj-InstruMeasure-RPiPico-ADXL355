//! HMI settings (model ConfigUi / EditSensorConfig, PICO355-R8; defaults per PICO355-R1, PICO355-R2).
//! Filter settings exposed: TODO(TBD). Default range: TODO(TBD) (model AccelRange).
//!
//! `link` holds the settled wire-protocol and serial-open constants, mirrored 1:1 from firmware
//! `config.h` (`CFG_PROTO_*`, same name without the prefix; hmi/tests/link_config.rs parses
//! config.h and compares). Every value is a design decision (2026-11-19, owner go t37u): our own
//! link choices, NOT part specs. See docs/protocol.md.

/// Default output data rate (PICO355-R1 defaultStreamRate 4000 Hz), from `tbd.rs`.
pub const DEFAULT_ODR_HZ: u32 = crate::tbd::DEFAULT_ODR_HZ;

/// Measurement range, mirrors model enum AccelRange.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Range {
    G2,
    G4,
    G8,
}

/// HMI settings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HmiConfig {
    /// Serial port path given by the user (port discovery TODO(TBD)).
    pub port: Option<String>,
    /// Output data rate in Hz.
    pub odr_hz: u32,
    /// Range; `None` = TODO(TBD) default range.
    pub range: Option<Range>,
    /// Filter setting; `None` = TODO(TBD) filter settings exposed.
    pub filter: Option<u8>,
}

impl Default for HmiConfig {
    fn default() -> Self {
        Self { port: None, odr_hz: DEFAULT_ODR_HZ, range: None, filter: None }
    }
}

/// Settled link constants. Design decision (2026-11-19, owner go t37u), not part specs.
/// Frame = header (sync, type, length u16 LE) + payload + CRC-16/CCITT-FALSE (u16 LE) over header + payload.
pub mod link {
    // ---- frame format (mirrors CFG_PROTO_*) ----
    pub const SYNC: u8 = 0xA5;
    pub const HEADER_LEN: usize = 4;
    pub const LENGTH_FIELD_BYTES: usize = 2;
    pub const CHECKSUM_KIND: u8 = CHECKSUM_CRC16;
    pub const CHECKSUM_NONE: u8 = 0;
    pub const CHECKSUM_CRC16: u8 = 1;
    pub const CHECKSUM_LEN_BYTES: usize = 2;
    pub const MAX_PAYLOAD: usize = 576;
    pub const TYPE_SAMPLE_BATCH: u8 = 0x01;
    pub const TYPE_COMMAND: u8 = 0x02;
    pub const TYPE_RESPONSE: u8 = 0x03;
    // ---- frame layout ----
    pub const SYNC_OFFSET: usize = 0;
    pub const TYPE_OFFSET: usize = 1;
    pub const LENGTH_OFFSET: usize = 2;
    pub const LENGTH_MSB_FIRST: u8 = 0;
    pub const PAD_BYTE: u8 = 0x00;
    pub const CMD_PAYLOAD_LEN: usize = 5;
    pub const CMD_CODE_OFFSET: usize = 0;
    pub const CMD_VALUE_OFFSET: usize = 1;
    pub const CMD_VALUE_BYTES: usize = 4;
    pub const RESP_PAYLOAD_LEN: usize = 6;
    pub const RESP_CODE_OFFSET: usize = 0;
    pub const RESP_CMD_CODE_OFFSET: usize = 1;
    pub const RESP_VALUE_OFFSET: usize = 2;
    pub const RESP_VALUE_BYTES: usize = 4;
    pub const VALUE_MSB_FIRST: u8 = 0;
    pub const SAMPLE_AXIS_BYTES: usize = 3;
    pub const SAMPLE_MSB_FIRST: u8 = 0;
    // ---- command set ----
    /// value = Hz
    pub const CMD_SET_ODR: u8 = 0x01;
    /// value = firmware `adxl355_range_t` (1 = 2 g, 2 = 4 g, 3 = 8 g)
    pub const CMD_SET_RANGE: u8 = 0x02;
    /// value = high-pass corner code 0..6
    pub const CMD_SET_FILTER: u8 = 0x03;
    pub const RESP_ACK: u8 = 0x01;
    pub const RESP_ERROR: u8 = 0x02;
    pub const RESP_STATUS: u8 = 0x03;

    // ---- serial open path (HMI only; USB CDC ignores the baud value) ----
    /// Baud passed to the serial port; a CDC device does not use it.
    pub const SERIAL_BAUD: u32 = 115_200;
    /// Read timeout in ms, so the read loop never busy-spins on 0-byte reads.
    pub const READ_TIMEOUT_MS: u64 = 100;
    /// 8 data bits, no parity, 1 stop bit, no flow control (serialport defaults); DTR asserted after open
    /// because the Pico USB CDC stdio only transmits while a host has the port open with DTR set.
    pub const ASSERT_DTR: bool = true;
}
