// UWP-PORT: port-owned helpers used only on the UWP (Xbox) target. (D-14)
pub mod entry;
pub mod time;

use std::path::PathBuf;
use std::sync::RwLock;

// Storage root injected by the C++ host before run() (D-10). Until then, fall back to the
// executable's directory so a PC sideload still works.
static ROOT: RwLock<Option<PathBuf>> = RwLock::new(None);

/// Set the storage root. Called by the host shim via gopher64_uwp_set_root (entry.rs).
pub fn set_root(root: PathBuf) {
    *ROOT.write().unwrap() = Some(root);
}

fn root() -> PathBuf {
    if let Some(r) = ROOT.read().unwrap().clone() {
        return r;
    }
    std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|p| p.to_path_buf()))
        .unwrap_or_else(|| PathBuf::from("."))
}

/// UWP storage layout under the injected root. No `dirs` crate (shell32) on UWP.
pub fn get_dirs() -> super::Dirs {
    let base = root();
    super::Dirs {
        config_dir: base.join("config"),
        data_dir: base.join("data"),
        cache_dir: base.join("cache"),
    }
}
