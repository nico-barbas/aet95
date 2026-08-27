const std = @import("std");

pub const c = @cImport({
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
