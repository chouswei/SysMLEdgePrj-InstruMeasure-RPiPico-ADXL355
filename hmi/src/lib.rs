//! Pico355 PC HMI library (model Pico355HmiApp): USB CDC serial link, config,
//! live view and logging. Requirements: PICO355-R8 (serial connection, config UI),
//! PICO355-R9 (live view, logging), PICO355-R2 (runtime config).
//! Module map: see docs/protocol.md. GUI: `gui` (window), `worker` (serial reader thread),
//! `samples` (chart store in g), `scale` (cited counts to g), `h5log` (HDF5 logging).

pub mod config;
pub mod gui;
pub mod h5log;
pub mod link;
pub mod live_view;
pub mod logger;
pub mod protocol;
pub mod samples;
pub mod scale;
pub mod tbd;
pub mod worker;
