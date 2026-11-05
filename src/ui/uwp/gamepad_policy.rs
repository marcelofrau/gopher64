pub(crate) fn can_auto_assign(
    enabled: bool,
    has_assignments: bool,
    gamepad_profile: bool,
    already_open: bool,
) -> bool {
    enabled && !has_assignments && gamepad_profile && !already_open
}

#[cfg(test)]
mod tests {
    use super::can_auto_assign;

    #[test]
    fn auto_assign_only_unconfigured_enabled_gamepad_port() {
        assert!(can_auto_assign(true, false, true, false));
        assert!(!can_auto_assign(false, false, true, false));
        assert!(!can_auto_assign(true, true, true, false));
        assert!(!can_auto_assign(true, false, false, false));
        assert!(!can_auto_assign(true, false, true, true));
    }
}
