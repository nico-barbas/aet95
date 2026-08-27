const std = @import("std");
const aet = @import("aet.zig");

pub const c = aet.c;

pub const DocumentError = error{
    FailedToAllocate,
    InvalidPosition,
    UnrecognisedErrorCode,
};

fn documentError(code: c_int) DocumentError {
    return switch (code) {
        c.Document_Error_Failed_To_Allocate => error.FailedToAllocate,
        c.Document_Error_Invalid_Position => error.InvalidPosition,
        else => error.UnrecognisedErrorCode,
    };
}

fn check(code: c.Document_Error) DocumentError!void {
    if (code != c.Document_Error_None) return documentError(@intCast(code));
}

pub fn make(initial_cap: usize, gap_size: usize) DocumentError!c.Document {
    var info = c.Document_Create_Info{
        .initial_cap = initial_cap,
        .gap_size = gap_size,
    };
    const result = c.make_document(&info, c.heap_allocator());
    return aet.value(result) orelse documentError(aet.errorCode(result).?);
}

pub fn destroy(document: c.Document) void {
    c.destroy_document(document);
}

pub fn writeChar(document: *c.Document, byte: u8) DocumentError!void {
    try check(c.document_write_char(document, @bitCast(byte)));
}

pub fn writeString(document: *c.Document, str: []const u8) DocumentError!void {
    const string = c.String{ .data = str.ptr, .len = str.len };
    try check(c.document_write_string(document, string));
}

pub fn deleteChars(document: *c.Document, n: usize) void {
    _ = c.document_delete_chars(document, n);
}

pub fn moveGap(document: *c.Document, pos: usize) DocumentError!void {
    try check(c.document_move_gap(document, pos));
}

pub fn clearContent(document: *c.Document) void {
    c.document_clear_content(document);
}

pub fn textLen(document: *c.Document) usize {
    return c.document_text_len(document);
}

/// The document has no read API, so the visible text is reconstructed from the
/// two live regions: prefix `[0, gap_start)` and suffix `[gap_end, buffer_len)`.
/// Caller owns the returned slice.
pub fn text(document: *c.Document, allocator: std.mem.Allocator) ![]u8 {
    try expectInvariants(document);

    const prefix = document.buffer[0..document.gap_start];
    const suffix = document.buffer[document.gap_end..document.buffer_len];

    const out = try allocator.alloc(u8, prefix.len + suffix.len);
    @memcpy(out[0..prefix.len], prefix);
    @memcpy(out[prefix.len..], suffix);
    return out;
}

pub fn expectText(document: *c.Document, want: []const u8) !void {
    const got = try text(document, std.testing.allocator);
    defer std.testing.allocator.free(got);
    try std.testing.expectEqualStrings(want, got);
}

/// `buffer_len` is the index one past the last used byte, so the used span is
/// `[0, buffer_len)` = prefix ++ gap ++ suffix. Every derived length falls out
/// of that; when it does not hold, the `usize` subtractions underflow.
pub fn expectInvariants(document: *c.Document) !void {
    try std.testing.expect(document.gap_start <= document.gap_end);
    try std.testing.expect(document.gap_end <= document.buffer_len);
    try std.testing.expect(document.buffer_len <= document.buffer_cap);

    const gap = document.gap_end - document.gap_start;
    try std.testing.expectEqual(document.buffer_len - gap, textLen(document));
}