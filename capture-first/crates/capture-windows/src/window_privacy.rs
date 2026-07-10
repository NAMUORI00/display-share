use windows::Win32::Foundation::{HWND, LPARAM};
use windows::Win32::System::Threading::GetCurrentProcessId;
use windows::Win32::UI::WindowsAndMessaging::{
    EnumWindows, GetWindowThreadProcessId, SetWindowDisplayAffinity, WDA_EXCLUDEFROMCAPTURE,
};
use windows::core::BOOL;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct WindowExclusionReport {
    pub applied: usize,
    pub failed: usize,
}

struct WindowExclusionState {
    pid: u32,
    report: WindowExclusionReport,
}

/// Best-effort recursion guard for the application's own preview windows.
#[must_use]
pub fn exclude_own_windows_from_capture() -> WindowExclusionReport {
    unsafe extern "system" fn enum_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let state = unsafe { &mut *(lparam.0 as *mut WindowExclusionState) };
        let mut pid = 0u32;
        unsafe {
            GetWindowThreadProcessId(hwnd, Some(&mut pid));
            if pid == state.pid {
                if SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE).is_ok() {
                    state.report.applied += 1;
                } else {
                    state.report.failed += 1;
                }
            }
        }
        BOOL(1)
    }

    let mut state = WindowExclusionState {
        pid: unsafe { GetCurrentProcessId() },
        report: WindowExclusionReport::default(),
    };
    let ptr = &mut state as *mut WindowExclusionState;
    let _ = unsafe { EnumWindows(Some(enum_proc), LPARAM(ptr as isize)) };
    state.report
}
