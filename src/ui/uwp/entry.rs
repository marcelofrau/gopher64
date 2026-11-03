// UWP-PORT: C ABI entry points called by the C++ host shim (P2). Modelled on the Android
// `gopher64_sdl_main` (src/ui/android.rs), which also drives run() in-process. (D-02)

use crate::{Args, create_runtime, run};
use clap::Parser;
use std::ffi::{CStr, c_char, c_int};

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
        eprintln!("[rust] panic: {info}");
    }));
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

    let (close_tx, handle) = create_runtime();
    let _guard = handle.enter();

    let raw = argv_to_strings(argc, argv);
    let args = match Args::try_parse_from(raw) {
        Ok(args) => args,
        Err(err) => {
            eprintln!("[rust] failed to parse arguments: {err}");
            let _ = close_tx.send(());
            return 1;
        }
    };

    if let Err(err) = run(args, argc as usize) {
        let _ = close_tx.send(());
        eprintln!("[rust] error running game: {err:?}");
        return 1;
    }

    let _ = close_tx.send(());
    0
}
