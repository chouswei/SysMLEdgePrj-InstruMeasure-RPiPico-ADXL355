//! Pico355 PC HMI library (model Pico355HmiApp): USB CDC serial link, config,
//! live view and logging. Requirements: PICO355-R8 (serial connection, config UI),
//! PICO355-R9 (live view, logging), PICO355-R2 (runtime config).
//! Module map: see docs/protocol.md.

pub mod config;
pub mod link;
pub mod live_view;
pub mod logger;
pub mod protocol;
pub mod tbd;
