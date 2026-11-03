// UWP-PORT: high-resolution sleep replacing spin_sleep on UWP.
//
// spin_sleep imports timeBeginPeriod/timeEndPeriod/timeGetDevCaps from winmm, which is not
// part of the UWP API partition (docs/xbox-port/05 §4). This mirrors spin_sleep's behaviour
// with a high-resolution waitable timer for the bulk of the wait and a short spin tail for
// accuracy. The Win32 calls used (CreateWaitableTimerExW, SetWaitableTimer,
// WaitForSingleObject, CloseHandle) are all in the UWP partition. (D-14)

use std::ffi::c_void;

type Handle = *mut c_void;

// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION | TIMER_ALL_ACCESS
const CREATE_WAITABLE_TIMER_HIGH_RESOLUTION: u32 = 0x0000_0002;
const TIMER_ALL_ACCESS: u32 = 0x001F_0003;
const INFINITE: u32 = 0xFFFF_FFFF;

unsafe extern "system" {
    fn CreateWaitableTimerExW(
        timer_attributes: *const c_void,
        name: *const u16,
        flags: u32,
        desired_access: u32,
    ) -> Handle;
    fn SetWaitableTimer(
        timer: Handle,
        due_time: *const i64,
        period: i32,
        completion_routine: *const c_void,
        completion_arg: *const c_void,
        resume: i32,
    ) -> i32;
    fn WaitForSingleObject(handle: Handle, milliseconds: u32) -> u32;
    fn CloseHandle(handle: Handle) -> i32;
}

/// Sleep for approximately `dur`, with sub-millisecond accuracy. Spins the last ~1 ms.
pub fn precise_sleep(dur: std::time::Duration) {
    let start = std::time::Instant::now();

    // Leave a short spin tail so wake-up jitter does not overshoot the deadline.
    const SPIN_TAIL: std::time::Duration = std::time::Duration::from_micros(1000);
    if dur > SPIN_TAIL {
        let wait = dur - SPIN_TAIL;
        if !wait.is_zero() {
            wait_on_timer(wait);
        }
    }

    // Spin until the deadline.
    while start.elapsed() < dur {
        std::hint::spin_loop();
    }
}

fn wait_on_timer(dur: std::time::Duration) {
    unsafe {
        let timer = CreateWaitableTimerExW(
            std::ptr::null(),
            std::ptr::null(),
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS,
        );
        if timer.is_null() {
            // Fall back to std sleep if the timer could not be created.
            std::thread::sleep(dur);
            return;
        }
        // Negative due time = relative, in 100-ns units.
        let due: i64 = -((dur.as_nanos() / 100) as i64);
        if SetWaitableTimer(timer, &due, 0, std::ptr::null(), std::ptr::null(), 0) != 0 {
            WaitForSingleObject(timer, INFINITE);
        } else {
            std::thread::sleep(dur);
        }
        CloseHandle(timer);
    }
}
