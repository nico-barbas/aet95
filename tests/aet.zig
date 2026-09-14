const std = @import("std");

pub const c = @cImport({
    // translate-c has no mapping for `_BitInt` (Translator.zig `transType`
    // falls through to UnsupportedType), so the `bool8`/`bool32`/`utf8_char`
    // typedefs in core/types.h fail to import. Rewrite the keyword to the
    // plain integer of the same ABI container for the header translation only;
    // the C sources still compile with the real `_BitInt`, and the
    // static_asserts in types.h check the sizes still agree.
    @cDefine("_BitInt(N)", "AET_ZIG_BITINT_##N");
    @cDefine("AET_ZIG_BITINT_1", "char");
    @cDefine("AET_ZIG_BITINT_32", "int");
    @cInclude("asm.h");
    @cInclude("hal.h");
    @cInclude("document.h");
});

pub fn Value(comptime Result: type) type {
    return @TypeOf(@as(Result, undefined).unnamed_0.value);
}

pub fn value(result: anytype) ?Value(@TypeOf(result)) {
    return if (result.ok != 0) result.unnamed_0.value else null;
}

pub fn errorCode(result: anytype) ?c_int {
    return if (result.ok != 0) null else @intCast(result.unnamed_0.@"error");
}
