// UWP-PORT: stub for `netplay` on the UWP target (D-09).
//
// ggrs and matchbox_socket (WebRTC) are excluded from the UWP crate graph, so the real
// `netplay` module cannot compile there. This stub mirrors the real module's public API
// (type names, field names, function signatures) exactly so every call site outside the
// module — device.rs, savestates.rs, device/controller.rs, ui/storage.rs, device/vi.rs,
// lib.rs — compiles without any change. Netplay is never initialised on UWP (`init`
// returns `None`), so none of these bodies ever runs; they exist only to satisfy the type
// checker. See docs/xbox-port/05-rust-uwp-toolchain.md §8.
#![allow(dead_code)]

use crate::device;
use crate::ui;

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct RtcIceServerConfig {
    pub urls: Vec<String>,
    pub username: Option<String>,
    pub credential: Option<String>,
}

pub struct NetplayConfig {
    pub server_addr: String,
    pub player_number: usize,
    pub number_of_players: usize,
    pub input_delay: usize,
    pub ice_config_path: std::path::PathBuf,
}

/// Stand-in for `ggrs::InputStatus`. The real tuple element is only ever read as
/// `netplay.inputs[channel].0` (the `InputData`), so the status type just needs to exist.
#[derive(Clone, Copy)]
pub struct InputStatus;

/// Stand-in for `ggrs::P2PSession`. Only `frames_ahead()` is read outside the module.
pub struct Session;

impl Session {
    pub fn frames_ahead(&self) -> i32 {
        0
    }
}

pub struct Netplay {
    pub disconnected: bool,
    pub session: Session,
    pub peers: Vec<()>,
    pub player_number: usize,
    pub connected: [bool; 4],
    pub input_delay: usize,
    pub messages: std::collections::HashMap<String, Vec<u8>>,
    pub received_data: std::collections::VecDeque<Vec<u8>>,
    pub inputs: Vec<(ui::input::InputData, InputStatus)>,
    pub incoming_message: Vec<u8>,
    pub ice_config_path: std::path::PathBuf,
}

pub fn init(
    _device: &mut device::Device,
    _netplay_config: &NetplayConfig,
    _pal: bool,
) -> Option<Netplay> {
    // Netplay is not available on UWP.
    None
}

pub fn close(_netplay: &mut Netplay) {}

pub fn send_rtc(_netplay: &mut Netplay, _rtc: i64) {}

pub fn receive_rtc(_netplay: &mut Netplay) -> i64 {
    0
}

pub fn send_rng(_netplay: &mut Netplay, _seed: u64) {}

pub fn receive_rng(_netplay: &mut Netplay) -> u64 {
    0
}

pub fn send_save(_netplay: &mut Netplay, _save_type: &str, _save_data: &[u8]) {}

pub fn receive_save(_netplay: &mut Netplay, _save_type: &str, _save_data: &mut Vec<u8>) {}

pub fn send_input_delay(_netplay: &mut Netplay, _input_delay: usize) {}

pub fn in_rollback(_netplay: Option<&Netplay>) -> bool {
    false
}

pub fn process_requests(_device: &mut device::Device) -> Vec<(ui::input::InputData, InputStatus)> {
    Vec::new()
}
