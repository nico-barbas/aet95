// Test root. `zig build test` only collects tests from the file it is pointed
// at and whatever that file references, so every test file is listed here.
test {
    _ = @import("asm_test.zig");
}
