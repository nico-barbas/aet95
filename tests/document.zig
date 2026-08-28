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

pub fn positionAt(document: *c.Document, offset: usize) DocumentError!c.Document_Position {
    const result = c.document_query_position_from_logical_offset(document, offset);
    return aet.value(result) orelse documentError(aet.errorCode(result).?);
}

pub fn offsetAt(document: *c.Document, line: usize, col: usize) DocumentError!usize {
    const result = c.document_query_logical_offset_from_position(
        document,
        .{ .line = line, .col = col },
    );
    return aet.value(result) orelse documentError(aet.errorCode(result).?);
}

/// Writes a sentinel into the slot just past the end of the line table. That
/// slot is inside the allocation but outside `len`, so reading it is a bug the
/// sanitizer cannot see. Zero is chosen because it is below every real offset,
/// so a bounds check that consults it will reject a valid position.
pub fn poisonPastLineTable(document: *c.Document) void {
    if (document.lines.len < document.lines.cap) {
        document.lines.items[document.lines.len] = .{ .logical_offset = 0 };
    }
}

/// Recomputes the line table from scratch and compares it against the one the
/// document maintained incrementally, along with `current_line`. `scratch` is
/// reused across calls so a long fuzz run does not churn the allocator.
pub fn expectLineTable(
    document: *c.Document,
    contents: []const u8,
    cursor: usize,
    scratch: *std.ArrayList(usize),
    allocator: std.mem.Allocator,
) !void {
    scratch.clearRetainingCapacity();
    try scratch.append(allocator, 0);
    for (contents, 0..) |byte, i| {
        if (byte == '\n') try scratch.append(allocator, i + 1);
    }

    {
        errdefer std.debug.print("  line table disagrees\n", .{});
        errdefer dumpLineTable(document, scratch.items);
        try std.testing.expectEqual(scratch.items.len, document.lines.len);
        for (scratch.items, 0..) |want, i| {
            try std.testing.expectEqual(want, document.lines.items[i].logical_offset);
        }
    }

    var line: usize = 0;
    while (line + 1 < scratch.items.len and scratch.items[line + 1] <= cursor) {
        line += 1;
    }
    errdefer std.debug.print(
        "  current_line disagrees: want {d}, got {d} (cursor {d}, {d} line(s))\n",
        .{ line, document.current_line, cursor, scratch.items.len },
    );
    try std.testing.expectEqual(line, document.current_line);
}

fn dumpLineTable(document: *c.Document, want: []const usize) void {
    std.debug.print("    want ({d}):", .{want.len});
    for (want) |start| std.debug.print(" {d}", .{start});
    std.debug.print("\n    got  ({d}):", .{document.lines.len});
    for (0..document.lines.len) |i| {
        std.debug.print(" {d}", .{document.lines.items[i].logical_offset});
    }
    std.debug.print("\n", .{});
}

/// Content length of a line, terminator excluded.
pub fn lineContentLen(starts: []const usize, line: usize, text_len: usize) usize {
    const end = if (line + 1 < starts.len) starts[line + 1] - 1 else text_len;
    return end - starts[line];
}

pub fn lineStarts(document: *c.Document, out: []usize) []usize {
    for (0..document.lines.len) |i| out[i] = document.lines.items[i].logical_offset;
    return out[0..document.lines.len];
}

/// Writes the line table directly, so a query can be tested without depending
/// on the maintenance code that normally produces it. Stays within the
/// capacity `make_document` allocates, so no growth is involved.
pub fn setLineStarts(document: *c.Document, starts: []const usize) void {
    std.debug.assert(starts.len <= document.lines.cap);
    for (starts, 0..) |start, i| document.lines.items[i].logical_offset = start;
    document.lines.len = starts.len;
}

/// The line is the last start less than or equal to the offset.
pub fn referencePosition(starts: []const usize, offset: usize) c.Document_Position {
    var line: usize = 0;
    while (line + 1 < starts.len and starts[line + 1] <= offset) line += 1;
    return .{ .line = line, .col = offset - starts[line] };
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