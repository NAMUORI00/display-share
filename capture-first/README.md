# capture-first

Rust workspace for the Windows-first SmartScreenCapture rewrite.

Current scope:

- Zero-copy oriented capture contracts
- Windows capture backend scaffold using `windows-capture`
- DirectML-first inference backend scaffold using `ort`
- `eframe/egui` desktop shell
- Existing JSON config contract compatibility

The C++ `ScreenMonitor/` project remains intact and serves as the current behavior reference.

Remaining migration work is tracked in [MIGRATION_TASKS.md](./MIGRATION_TASKS.md).
