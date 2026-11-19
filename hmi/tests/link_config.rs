//! Mirror check: the settled link constants in hmi/src/config.rs equal the firmware
//! config.h values (design decision 2026-11-19, owner go t37u), and are no longer TBD.
//! SPI settings and the post-reset delay are firmware-side (cited to the no-OS pin);
//! the HMI side owns the serial-open settings.
use pico355_hmi::config::link;

const CONFIG_H: &str = include_str!("../../firmware/include/config.h");

/// Value of `#define <name> <value>` in config.h, numeric only (u/U suffix and hex allowed).
fn cfg(name: &str) -> u64 {
    for line in CONFIG_H.lines() {
        let mut it = line.split_whitespace();
        if it.next() == Some("#define") && it.next() == Some(name) {
            let v = it.next().unwrap_or_else(|| panic!("{name} has no value"));
            if v.starts_with("CFG_") {
                return cfg(v); // alias of another config.h macro
            }
            let v = v.trim_end_matches(['u', 'U']);
            return match v.strip_prefix("0x") {
                Some(h) => u64::from_str_radix(h, 16).unwrap(),
                None => v.parse().unwrap_or_else(|_| panic!("{name}: {v} is not numeric")),
            };
        }
    }
    panic!("{name} not found in config.h");
}

#[test]
fn frame_constants_mirror_config_h() {
    assert_eq!(link::SYNC as u64, cfg("CFG_PROTO_SYNC"));
    assert_eq!(link::HEADER_LEN as u64, cfg("CFG_PROTO_HEADER_LEN"));
    assert_eq!(link::LENGTH_FIELD_BYTES as u64, cfg("CFG_PROTO_LENGTH_FIELD_BYTES"));
    assert_eq!(link::CHECKSUM_KIND as u64, cfg("CFG_PROTO_CHECKSUM_KIND"));
    assert_eq!(link::CHECKSUM_NONE as u64, cfg("CFG_PROTO_CHECKSUM_NONE"));
    assert_eq!(link::CHECKSUM_CRC16 as u64, cfg("CFG_PROTO_CHECKSUM_CRC16"));
    assert_eq!(link::CHECKSUM_LEN_BYTES as u64, cfg("CFG_PROTO_CHECKSUM_LEN_BYTES"));
    assert_eq!(link::MAX_PAYLOAD as u64, cfg("CFG_PROTO_MAX_PAYLOAD"));
    assert_eq!(link::TYPE_SAMPLE_BATCH as u64, cfg("CFG_PROTO_TYPE_SAMPLE_BATCH"));
    assert_eq!(link::TYPE_COMMAND as u64, cfg("CFG_PROTO_TYPE_COMMAND"));
    assert_eq!(link::TYPE_RESPONSE as u64, cfg("CFG_PROTO_TYPE_RESPONSE"));
    assert_eq!(link::SYNC_OFFSET as u64, cfg("CFG_PROTO_SYNC_OFFSET"));
    assert_eq!(link::TYPE_OFFSET as u64, cfg("CFG_PROTO_TYPE_OFFSET"));
    assert_eq!(link::LENGTH_OFFSET as u64, cfg("CFG_PROTO_LENGTH_OFFSET"));
    assert_eq!(link::LENGTH_MSB_FIRST as u64, cfg("CFG_PROTO_LENGTH_MSB_FIRST"));
    assert_eq!(link::PAD_BYTE as u64, cfg("CFG_PROTO_PAD_BYTE"));
}

#[test]
fn command_response_sample_constants_mirror_config_h() {
    assert_eq!(link::CMD_PAYLOAD_LEN as u64, cfg("CFG_PROTO_CMD_PAYLOAD_LEN"));
    assert_eq!(link::CMD_CODE_OFFSET as u64, cfg("CFG_PROTO_CMD_CODE_OFFSET"));
    assert_eq!(link::CMD_VALUE_OFFSET as u64, cfg("CFG_PROTO_CMD_VALUE_OFFSET"));
    assert_eq!(link::CMD_VALUE_BYTES as u64, cfg("CFG_PROTO_CMD_VALUE_BYTES"));
    assert_eq!(link::RESP_PAYLOAD_LEN as u64, cfg("CFG_PROTO_RESP_PAYLOAD_LEN"));
    assert_eq!(link::RESP_CODE_OFFSET as u64, cfg("CFG_PROTO_RESP_CODE_OFFSET"));
    assert_eq!(link::RESP_CMD_CODE_OFFSET as u64, cfg("CFG_PROTO_RESP_CMD_CODE_OFFSET"));
    assert_eq!(link::RESP_VALUE_OFFSET as u64, cfg("CFG_PROTO_RESP_VALUE_OFFSET"));
    assert_eq!(link::RESP_VALUE_BYTES as u64, cfg("CFG_PROTO_RESP_VALUE_BYTES"));
    assert_eq!(link::VALUE_MSB_FIRST as u64, cfg("CFG_PROTO_VALUE_MSB_FIRST"));
    assert_eq!(link::SAMPLE_AXIS_BYTES as u64, cfg("CFG_PROTO_SAMPLE_AXIS_BYTES"));
    assert_eq!(link::SAMPLE_MSB_FIRST as u64, cfg("CFG_PROTO_SAMPLE_MSB_FIRST"));
    assert_eq!(link::CMD_SET_ODR as u64, cfg("CFG_PROTO_CMD_SET_ODR"));
    assert_eq!(link::CMD_SET_RANGE as u64, cfg("CFG_PROTO_CMD_SET_RANGE"));
    assert_eq!(link::CMD_SET_FILTER as u64, cfg("CFG_PROTO_CMD_SET_FILTER"));
    assert_eq!(link::RESP_ACK as u64, cfg("CFG_PROTO_RESP_ACK"));
    assert_eq!(link::RESP_ERROR as u64, cfg("CFG_PROTO_RESP_ERROR"));
    assert_eq!(link::RESP_STATUS as u64, cfg("CFG_PROTO_RESP_STATUS"));
    assert_eq!(link::CMD_GET_STATUS as u64, cfg("CFG_PROTO_CMD_GET_STATUS"));
}

#[test]
fn serial_open_settings_are_decided() {
    // Not part specs: USB CDC ignores baud; 8N1 no flow control, DTR asserted (design decision).
    assert!(link::SERIAL_BAUD > 0);
    assert!(link::READ_TIMEOUT_MS > 0);
    assert!(link::ASSERT_DTR);
}
