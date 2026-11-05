// UWP-PORT: C ABI entry points called by the C++ host shim (P2). Modelled on the Android
// `gopher64_sdl_main` (src/ui/android.rs), which also drives run() in-process. (D-02)

use crate::{Args, create_runtime, run};
use clap::Parser;
use std::ffi::{CStr, CString, c_char, c_int};
use std::sync::RwLock;

type LogCallback = unsafe extern "C" fn(*const c_char);
static LOG_CALLBACK: RwLock<Option<LogCallback>> = RwLock::new(None);

pub(crate) fn log(message: &str) {
    eprintln!("[rust] {message}");
    let callback = *LOG_CALLBACK
        .read()
        .unwrap_or_else(|error| error.into_inner());
    if let Some(callback) = callback {
        let sanitized = message.replace('\0', "\\0");
        if let Ok(message) = CString::new(sanitized) {
            unsafe { callback(message.as_ptr()) };
        }
    }
}

/// Register a host callback that copies each message before returning.
/// Pass null to unregister it before destroying host logging state.
#[unsafe(no_mangle)]
pub extern "C" fn gopher64_uwp_set_log_callback(callback: Option<LogCallback>) {
    *LOG_CALLBACK
        .write()
        .unwrap_or_else(|error| error.into_inner()) = callback;
}

/// UWP-PORT: Forward native renderer diagnostics to the host's timestamped log.
///
/// # Safety
/// A non-null `message` must be a valid NUL-terminated string for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gopher64_uwp_log_native(message: *const std::ffi::c_char) {
    if !message.is_null() {
        log(&unsafe { std::ffi::CStr::from_ptr(message) }.to_string_lossy());
    }
}

fn argv_to_strings(argc: c_int, argv: *mut *mut c_char) -> Vec<String> {
    if argc <= 0 || argv.is_null() {
        return Vec::new();
    }
    unsafe {
        (0..argc as usize)
            .map(|i| CStr::from_ptr(*argv.add(i)).to_string_lossy().into_owned())
            .collect()
    }
}

/// Install a panic hook that logs the message before the process aborts. With
/// `panic = "abort"` on UWP there is no unwinding, so this is the only chance to record it.
fn install_panic_hook() {
    std::panic::set_hook(Box::new(|info| {
        log(&format!("panic: {info}"));
    }));
}

/// SDL log callback: route SDL's own category logging to the host log. SDL's default handler
/// writes to stderr/OutputDebugString, which is invisible on a retail Xbox, so without this the
/// `[sdl]` diagnostics are lost.
unsafe extern "C" fn sdl_log_output(
    _userdata: *mut std::ffi::c_void,
    _category: c_int,
    _priority: sdl3_sys::log::SDL_LogPriority,
    message: *const c_char,
) {
    if !message.is_null() {
        log(&format!(
            "[sdl] {}",
            unsafe { CStr::from_ptr(message) }.to_string_lossy()
        ));
    }
}

fn install_sdl_log_hook() {
    unsafe {
        sdl3_sys::log::SDL_SetLogOutputFunction(Some(sdl_log_output), std::ptr::null_mut());
    }
}

/// Set the storage root (external drive or LocalState). Call before `gopher64_uwp_main`.
/// `root` is a UTF-8, NUL-terminated path. A null pointer is ignored.
///
/// # Safety
/// `root` must be a valid NUL-terminated C string or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gopher64_uwp_set_root(root: *const c_char) {
    if root.is_null() {
        return;
    }
    let path = unsafe { CStr::from_ptr(root) }
        .to_string_lossy()
        .into_owned();
    super::set_root(std::path::PathBuf::from(path));
}

/// Entry point: parse argv like the CLI, then run the emulator on this thread (D-06).
/// Returns 0 on success, 1 on error.
///
/// # Safety
/// `argv` must point to `argc` valid NUL-terminated C strings, or be null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gopher64_uwp_main(argc: c_int, argv: *mut *mut c_char) -> c_int {
    install_panic_hook();
    install_sdl_log_hook();
    log("entry started");

    let (close_tx, handle) = create_runtime();
    let _guard = handle.enter();

    let raw = argv_to_strings(argc, argv);
    let args = match Args::try_parse_from(raw) {
        Ok(args) => args,
        Err(err) => {
            log(&format!("failed to parse arguments: {err}"));
            let _ = close_tx.send(());
            return 1;
        }
    };

    let result = run(args, argc as usize);
    log("run returned; stopping runtime");
    if let Err(err) = result {
        let _ = close_tx.send(());
        log(&format!("error running game: {err}"));
        return 1;
    }

    let _ = close_tx.send(());
    log("entry returned successfully");
    0
}

// --- Config C ABI for the gamepad shell (P5) ---------------------------------
// The shell reads/writes config.json as opaque JSON text; the schema stays owned by
// src/ui/config.rs, which parses this same file at boot. These are pure file I/O under the
// injected root, so they work before any game is running and never touch the renderer.

static LAST_ERROR: RwLock<Option<CString>> = RwLock::new(None);

fn set_last_error(message: String) {
    if let Ok(cstring) = CString::new(message) {
        *LAST_ERROR
            .write()
            .unwrap_or_else(|error| error.into_inner()) = Some(cstring);
    }
}

/// Return the current config.json as a newly allocated UTF-8 C string (caller frees with
/// `gopher64_uwp_string_free`). Returns null and sets the last error if it cannot be read.
/// An absent file yields "{}" so the shell can start from defaults.
#[unsafe(no_mangle)]
pub extern "C" fn gopher64_uwp_config_get() -> *mut c_char {
    let path = crate::ui::get_dirs().config_dir.join("config.json");
    let text = match std::fs::read_to_string(&path) {
        Ok(text) => text,
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => "{}".to_string(),
        Err(err) => {
            set_last_error(format!("config_get: {err}"));
            return std::ptr::null_mut();
        }
    };
    match CString::new(text) {
        Ok(cstring) => cstring.into_raw(),
        Err(err) => {
            set_last_error(format!("config_get: {err}"));
            std::ptr::null_mut()
        }
    }
}

/// Overwrite config.json with `json` (NUL-terminated UTF-8). Returns true on success; on
/// failure returns false and sets the last error. The caller is expected to pass valid JSON;
/// gopher64 validates the schema when it reloads at the next boot.
///
/// # Safety
/// `json` must be a valid NUL-terminated C string or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gopher64_uwp_config_set(json: *const c_char) -> bool {
    if json.is_null() {
        set_last_error("config_set: null json".to_string());
        return false;
    }
    let text = unsafe { CStr::from_ptr(json) }
        .to_string_lossy()
        .into_owned();
    // Reject obviously invalid JSON before touching the file, so a bad write can't corrupt config.
    if serde_json::from_str::<serde_json::Value>(&text).is_err() {
        set_last_error("config_set: not valid JSON".to_string());
        return false;
    }
    let dir = crate::ui::get_dirs().config_dir;
    if let Err(err) = std::fs::create_dir_all(&dir) {
        set_last_error(format!("config_set: {err}"));
        return false;
    }
    if let Err(err) = std::fs::write(dir.join("config.json"), text) {
        set_last_error(format!("config_set: {err}"));
        return false;
    }
    true
}

/// Return the last error message set by a config call (never null; empty if none). The returned
/// pointer is owned by Rust and valid until the next config call on any thread; copy it.
#[unsafe(no_mangle)]
pub extern "C" fn gopher64_uwp_last_error() -> *const c_char {
    static EMPTY: &[u8] = b"\0";
    let guard = LAST_ERROR.read().unwrap_or_else(|error| error.into_inner());
    match guard.as_ref() {
        Some(cstring) => cstring.as_ptr(),
        None => EMPTY.as_ptr() as *const c_char,
    }
}

/// Free a string returned by `gopher64_uwp_config_get`.
///
/// # Safety
/// `ptr` must be a pointer previously returned by `gopher64_uwp_config_get`, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gopher64_uwp_string_free(ptr: *mut c_char) {
    if !ptr.is_null() {
        drop(unsafe { CString::from_raw(ptr) });
    }
}
