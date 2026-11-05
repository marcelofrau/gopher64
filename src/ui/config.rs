use crate::ui;

#[derive(Clone, serde::Serialize, serde::Deserialize)]
pub struct InputKeyButton {
    pub id: i32,
}

#[derive(PartialEq, Clone, serde::Serialize, serde::Deserialize)]
pub struct InputControllerAxis {
    pub id: i32,
    pub axis: i16,
    pub initial_state: i16,
}

#[derive(Clone, serde::Serialize, serde::Deserialize)]
pub struct InputJoystickHat {
    pub id: i32,
    pub direction: u8,
}

#[derive(Clone, serde::Serialize, serde::Deserialize)]
pub enum InputItem {
    Key(InputKeyButton),
    ControllerButton(InputKeyButton),
    ControllerAxis(InputControllerAxis),
    JoystickButton(InputKeyButton),
    JoystickHat(InputJoystickHat),
    JoystickAxis(InputControllerAxis),
}

#[derive(Clone, serde::Serialize, serde::Deserialize)]
pub struct InputProfile {
    pub inputs: [[Option<InputItem>; 2]; ui::input_profile::PROFILE_SIZE],
    pub dinput: bool,
    pub deadzone: i32,
}

#[derive(Default, serde::Serialize, serde::Deserialize)]
pub struct Input {
    pub input_profiles: std::collections::BTreeMap<String, InputProfile>,
    pub input_profile_binding: [String; 4],
    pub controller_assignment: [Option<String>; 4],
    pub controller_enabled: [bool; 4],
    pub transfer_pak: [bool; 4],
    pub gb_rom_path: [String; 4],
    pub gb_ram_path: [String; 4],
    pub emulate_vru: bool,
}

#[derive(Default, serde::Serialize, serde::Deserialize)]
pub struct Video {
    pub upscale: u32,
    pub ssaa: bool,
    pub integer_scaling: bool,
    pub fullscreen: bool,
    pub widescreen: bool,
    pub vsync: bool,
    pub crt: bool,
}

#[derive(Default, serde::Serialize, serde::Deserialize)]
pub struct Emulation {
    pub overclock: bool,
    pub disable_expansion_pak: bool,
    pub usb: bool,
    pub rewind: bool,
}

#[derive(serde::Serialize, serde::Deserialize)]
pub struct Cheats {
    pub cheats: rustc_hash::FxHashMap<String, rustc_hash::FxHashMap<String, Option<String>>>,
}

#[derive(Default, serde::Serialize, serde::Deserialize)]
pub struct Ui {
    pub theme: i32,
    pub rom_dir: std::path::PathBuf,
    pub recent_roms: Vec<String>,
}

#[derive(Default, serde::Serialize, serde::Deserialize)]
pub struct Config {
    pub input: Input,
    pub video: Video,
    pub emulation: Emulation,
    pub ui: Ui,
    #[serde(skip)]
    write_to_disk: bool,
}

impl Drop for Cheats {
    fn drop(&mut self) {
        write_cheats(self);
    }
}

fn write_cheats(cheats: &Cheats) {
    let dirs = ui::get_dirs();
    let file_path = dirs.config_dir.join("cheats.json");
    if let Err(error) = write_json(&file_path, cheats) {
        report_write_error("cheats", &error);
    }
}

impl Cheats {
    pub fn new() -> Cheats {
        let dirs = ui::get_dirs();
        let file_path = dirs.config_dir.join("cheats.json");
        let cheats_file = std::fs::read(file_path);
        if let Ok(cheats_file) = cheats_file {
            let result = serde_json::from_slice(cheats_file.as_ref());
            if let Ok(result) = result {
                return result;
            }
        }
        Cheats {
            cheats: rustc_hash::FxHashMap::default(),
        }
    }
}

impl Drop for Config {
    fn drop(&mut self) {
        if self.write_to_disk {
            write_config(self);
        }
    }
}

fn write_config(config: &Config) {
    let dirs = ui::get_dirs();
    let file_path = dirs.config_dir.join("config.json");
    if let Err(error) = write_json(&file_path, config) {
        report_write_error("config", &error);
    }
}

fn write_json(path: &std::path::Path, value: &impl serde::Serialize) -> std::io::Result<()> {
    let file = std::fs::File::create(path)?;
    serde_json::to_writer_pretty(file, value).map_err(std::io::Error::other)
}

// UWP-PORT: on UWP stderr is captured differently, so route the write-error report to the host
// log instead of eprintln!. Desktop/Android keep eprintln!. (D-10)
fn report_write_error(kind: &str, error: &std::io::Error) {
    #[cfg(target_vendor = "uwp")]
    crate::ui::uwp::entry::log(&format!("Error writing {kind}: {error}"));
    #[cfg(not(target_vendor = "uwp"))]
    eprintln!("Error writing {kind}: {error}");
}

#[cfg(test)]
mod write_tests {
    #[test]
    fn reports_file_creation_errors() {
        let parent = std::env::temp_dir().join(format!("gopher64-missing-{}", std::process::id()));
        let error =
            super::write_json(&parent.join("missing").join("config.json"), &42).unwrap_err();
        assert_eq!(error.kind(), std::io::ErrorKind::NotFound);
    }
}

impl Config {
    pub fn new() -> Config {
        let dirs = ui::get_dirs();
        let file_path = dirs.config_dir.join("config.json");
        let config_file = std::fs::read(file_path);
        let mut input_data: Option<Input> = None;
        if let Ok(config_file) = config_file {
            let result = serde_json::from_slice::<Config>(config_file.as_ref());
            if let Ok(mut result) = result {
                result.write_to_disk = true;
                return result;
            }

            // try to restore input data from old config file
            if let Ok(value) = serde_json::from_slice::<serde_json::Value>(&config_file)
                && let Some(data) = value.get("input")
                && let Ok(data) = serde_json::from_value::<Input>(data.clone())
            {
                input_data = Some(data);
            }
        }

        let mut input_profiles = std::collections::BTreeMap::new();
        input_profiles.insert(
            "default".to_string(),
            ui::input_profile::get_default_profile(),
        );
        Config {
            input: input_data.unwrap_or(Input {
                input_profile_binding: [
                    "default".to_string(),
                    "default".to_string(),
                    "default".to_string(),
                    "default".to_string(),
                ],
                controller_assignment: [None, None, None, None],
                input_profiles,
                controller_enabled: [true, false, false, false],
                transfer_pak: [false, false, false, false],
                gb_rom_path: [String::new(), String::new(), String::new(), String::new()],
                gb_ram_path: [String::new(), String::new(), String::new(), String::new()],
                emulate_vru: false,
            }),
            video: Video {
                upscale: 1,
                ssaa: false,
                integer_scaling: false,
                fullscreen: false,
                widescreen: false,
                vsync: true,
                crt: false,
            },
            emulation: Emulation {
                overclock: false,
                disable_expansion_pak: false,
                usb: false,
                rewind: false,
            },
            ui: Ui {
                theme: 0,
                rom_dir: std::path::PathBuf::new(),
                recent_roms: Vec::new(),
            },
            write_to_disk: true,
        }
    }
}
