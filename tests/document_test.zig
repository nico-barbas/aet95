const std = @import("std");
const doc = @import("document.zig");

const Shape = struct { cap: usize, gap: usize };

const shapes = [_]Shape{
    .{ .cap = 512, .gap = 64 },
    .{ .cap = 32, .gap = 8 },
    .{ .cap = 16, .gap = 4 },
    .{ .cap = 8, .gap = 1 },
    .{ .cap = 8, .gap = 512 },
    .{ .cap = 256, .gap = 0 },
};

test "a new document is empty and consistent" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);

        try std.testing.expectEqual(0, doc.textLen(&document));
        try doc.expectText(&document, "");
        // The gap must fit inside what was actually allocated.
        try std.testing.expect(document.gap_end <= document.buffer_cap);
    }
}

test "appended text reads back" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);

        try doc.writeString(&document, "hello");
        try doc.expectText(&document, "hello");

        try doc.writeChar(&document, '!');
        try doc.expectText(&document, "hello!");
        try std.testing.expectEqual(6, doc.textLen(&document));
    }
}

test "text survives growing the buffer many times over" {
    var document = try doc.make(8, 4);
    defer doc.destroy(document);

    var want: [2048]u8 = undefined;
    for (&want, 0..) |*byte, i| {
        byte.* = @intCast('a' + (i % 26));
        try doc.writeChar(&document, byte.*);
    }

    try doc.expectText(&document, &want);
}

test "insertion happens where the gap is" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);

        try doc.writeString(&document, "hello");
        try doc.moveGap(&document, 2);
        try doc.expectText(&document, "hello");

        try doc.writeString(&document, "XY");
        try doc.expectText(&document, "heXYllo");

        try doc.moveGap(&document, 0);
        try doc.writeChar(&document, '>');
        try doc.expectText(&document, ">heXYllo");
    }
}

test "the gap reaches every position including the end" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);
        try doc.writeString(&document, "hello");

        // The append position is a cursor position like any other: with the
        // gap at the end, the next write goes after the last character.
        for (0..doc.textLen(&document) + 1) |pos| {
            errdefer std.debug.print("\n  failing pos: {d}\n", .{pos});
            try doc.moveGap(&document, pos);
            try doc.expectText(&document, "hello");
            try std.testing.expectEqual(pos, document.gap_start);
        }

        try doc.moveGap(&document, doc.textLen(&document));
        try doc.writeChar(&document, '.');
        try doc.expectText(&document, "hello.");
    }
}

test "positions past the end of the text are rejected" {
    var document = try doc.make(64, 8);
    defer doc.destroy(document);
    try doc.writeString(&document, "hello");

    for (doc.textLen(&document) + 1..doc.textLen(&document) + 8) |pos| {
        errdefer std.debug.print("\n  failing pos: {d}\n", .{pos});
        try std.testing.expectError(
            error.InvalidPosition,
            doc.moveGap(&document, pos),
        );
        // A rejected move leaves the document untouched.
        try doc.expectText(&document, "hello");
    }
}

test "deleting removes the characters before the gap and clamps" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);

        try doc.writeString(&document, "hello");
        doc.deleteChars(&document, 2);
        try doc.expectText(&document, "hel");

        // Deleting only reaches back to the start of the document.
        doc.deleteChars(&document, 99);
        try doc.expectText(&document, "");
        doc.deleteChars(&document, 1);
        try doc.expectText(&document, "");

        // Deleting in the middle takes from before the cursor, not after.
        try doc.writeString(&document, "abcdef");
        try doc.moveGap(&document, 3);
        doc.deleteChars(&document, 2);
        try doc.expectText(&document, "adef");
    }
}

test "clearing resets the document for reuse" {
    for (shapes) |shape| {
        errdefer std.debug.print(
            "\n  failing shape: cap={d} gap={d}\n",
            .{ shape.cap, shape.gap },
        );

        var document = try doc.make(shape.cap, shape.gap);
        defer doc.destroy(document);

        try doc.writeString(&document, "hello world");
        doc.clearContent(&document);
        try std.testing.expectEqual(0, doc.textLen(&document));
        try doc.expectText(&document, "");

        // A cleared document behaves like a fresh one, gap moves included.
        try doc.writeString(&document, "again");
        try doc.moveGap(&document, 1);
        try doc.writeChar(&document, '-');
        try doc.expectText(&document, "a-gain");
    }
}

fn failingAlloc(_: doc.c.Allocator, _: usize) callconv(.c) doc.c.Allocation_Result {
    return .{ .err = doc.c.Allocation_Error_Out_Of_Memory, .allocation = null };
}

fn noopFree(_: doc.c.Allocator, _: ?*anyopaque) callconv(.c) doc.c.Allocation_Result {
    return .{ .err = doc.c.Allocation_Error_None, .allocation = null };
}

test "a write that cannot allocate fails loudly and changes nothing" {
    var document = try doc.make(16, 4);
    defer doc.destroy(document);

    try doc.writeString(&document, "abc");
    try doc.moveGap(&document, 1);

    var mirror: std.ArrayList(u8) = .empty;
    defer mirror.deinit(std.testing.allocator);
    try mirror.appendSlice(std.testing.allocator, "abc");
    var cursor: usize = 1;

    document.allocator.alloc = failingAlloc;
    document.allocator.free = noopFree;

    // Writes succeed until the gap runs out and the buffer cannot grow. The
    // failing write must report the failure rather than corrupt the document.
    var failed = false;
    for (0..256) |_| {
        doc.writeChar(&document, 'x') catch {
            failed = true;
            break;
        };
        try mirror.insert(std.testing.allocator, cursor, 'x');
        cursor += 1;
    }

    try std.testing.expect(failed);
    try doc.expectInvariants(&document);
    try doc.expectText(&document, mirror.items);

    // Restore a working allocator so the document can be destroyed.
    document.allocator = doc.c.heap_allocator();
}

// "abc\ndefg\n\nhi" -- covers a normal line, an empty line, and a last line
// with no terminator.
const fixture_text = "abc\ndefg\n\nhi";
const fixture_starts = [_]usize{ 0, 4, 9, 10 };

test "offset maps to line and column" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);

    for (0..doc.textLen(&document) + 1) |offset| {
        errdefer std.debug.print("\n  failing offset: {d}\n", .{offset});

        const want = doc.referencePosition(&fixture_starts, offset);
        const got = try doc.positionAt(&document, offset);

        try std.testing.expectEqual(want.line, got.line);
        try std.testing.expectEqual(want.col, got.col);
    }
}

test "the offset one past the last character is a valid position" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);

    // The cursor sits after the last character far more often than anywhere
    // else, so this position has to resolve rather than be rejected.
    const end = try doc.positionAt(&document, doc.textLen(&document));
    try std.testing.expectEqual(3, end.line);
    try std.testing.expectEqual(2, end.col);

    try std.testing.expectError(
        error.InvalidPosition,
        doc.positionAt(&document, doc.textLen(&document) + 1),
    );
}

test "offset maps correctly for arbitrary line tables" {
    var prng = std.Random.DefaultPrng.init(1);
    const random = prng.random();

    for (0..200) |trial| {
        errdefer std.debug.print("\n  failing trial: {d}\n", .{trial});

        var document = try doc.make(256, 16);
        defer doc.destroy(document);

        const text_len = random.intRangeAtMost(usize, 1, 200);
        for (0..text_len) |_| try doc.writeChar(&document, 'x');

        // A strictly increasing table starting at 0, within the capacity
        // make_document allocated.
        var starts: [32]usize = undefined;
        var count: usize = 1;
        starts[0] = 0;
        for (1..text_len) |offset| {
            if (count < starts.len and random.uintLessThan(u8, 8) == 0) {
                starts[count] = offset;
                count += 1;
            }
        }
        doc.setLineStarts(&document, starts[0..count]);

        for (0..text_len + 1) |offset| {
            errdefer std.debug.print(
                "\n  lines={d} offset={d}\n",
                .{ count, offset },
            );
            const want = doc.referencePosition(starts[0..count], offset);
            const got = try doc.positionAt(&document, offset);
            try std.testing.expectEqual(want.line, got.line);
            try std.testing.expectEqual(want.col, got.col);
        }
    }
}

test "offset and position are inverses" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);

    for (0..doc.textLen(&document) + 1) |offset| {
        errdefer std.debug.print("\n  failing offset: {d}\n", .{offset});

        const position = try doc.positionAt(&document, offset);
        errdefer std.debug.print(
            "  went through ({d},{d})\n",
            .{ position.line, position.col },
        );

        try std.testing.expectEqual(
            offset,
            try doc.offsetAt(&document, position.line, position.col),
        );
    }
}

test "a column past the end of its line is rejected" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);
    const text_len = doc.textLen(&document);

    for (fixture_starts, 0..) |start, line| {
        errdefer std.debug.print("\n  failing line: {d}\n", .{line});

        // The last valid column sits at the terminator, one past the last
        // character, which is where the cursor rests at end of line.
        const content_len = doc.lineContentLen(&fixture_starts, line, text_len);
        try std.testing.expectEqual(
            start + content_len,
            try doc.offsetAt(&document, line, content_len),
        );

        // One further would name a character belonging to the next line.
        try std.testing.expectError(
            error.InvalidPosition,
            doc.offsetAt(&document, line, content_len + 1),
        );
    }
}

test "a line past the end of the table is rejected" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);

    for (fixture_starts.len..fixture_starts.len + 3) |line| {
        errdefer std.debug.print("\n  failing line: {d}\n", .{line});
        try std.testing.expectError(
            error.InvalidPosition,
            doc.offsetAt(&document, line, 0),
        );
    }
}

test "a position on the last line ignores memory past the line table" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    doc.setLineStarts(&document, &fixture_starts);
    doc.poisonPastLineTable(&document);

    // The final line has no successor. Anything read from the slot after it is
    // out of bounds of the table, whatever the allocation happens to hold.
    const last = fixture_starts.len - 1;
    try std.testing.expectEqual(
        fixture_starts[last],
        try doc.offsetAt(&document, last, 0),
    );
    try std.testing.expectEqual(
        doc.textLen(&document),
        try doc.offsetAt(&document, last, 2),
    );
}

test "typing maintains the line table" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    for (fixture_text) |byte| try doc.writeChar(&document, byte);

    var buf: [32]usize = undefined;
    try std.testing.expectEqualSlices(
        usize,
        &fixture_starts,
        doc.lineStarts(&document, &buf),
    );
}

test "moving the gap tracks the current line" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);

    for (0..doc.textLen(&document) + 1) |offset| {
        errdefer std.debug.print("\n  failing offset: {d}\n", .{offset});

        try doc.moveGap(&document, offset);
        const want = try doc.positionAt(&document, offset);
        try std.testing.expectEqual(want.line, document.current_line);
    }
}

test "inserting mid-document shifts the lines after the cursor" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    try doc.moveGap(&document, 2);
    try doc.writeChar(&document, 'X');

    try doc.expectText(&document, "abXc\ndefg\n\nhi");

    var buf: [32]usize = undefined;
    try std.testing.expectEqualSlices(
        usize,
        &[_]usize{ 0, 5, 10, 11 },
        doc.lineStarts(&document, &buf),
    );
}

test "inserting a newline mid-document splits the line" {
    var document = try doc.make(256, 16);
    defer doc.destroy(document);

    try doc.writeString(&document, fixture_text);
    try doc.moveGap(&document, 2);
    try doc.writeChar(&document, '\n');

    try doc.expectText(&document, "ab\nc\ndefg\n\nhi");
    try std.testing.expectEqual(1, document.current_line);

    var buf: [32]usize = undefined;
    try std.testing.expectEqualSlices(
        usize,
        &[_]usize{ 0, 3, 5, 10, 11 },
        doc.lineStarts(&document, &buf),
    );
}

const DeleteCase = struct {
    text: []const u8,
    gap: ?usize = null,
    delete: usize,
    want_text: []const u8,
    want_line: usize,
    want_starts: []const usize,
};

test "deleting keeps the line table in sync" {
    const cases = [_]DeleteCase{
        // No newline in the removed range: the table is untouched.
        .{
            .text = "abc\ndefg",
            .delete = 1,
            .want_text = "abc\ndef",
            .want_line = 1,
            .want_starts = &.{ 0, 4 },
        },
        // Backspacing over a newline merges the line into the one above.
        .{
            .text = "abc\ndefg",
            .delete = 5,
            .want_text = "abc",
            .want_line = 0,
            .want_starts = &.{0},
        },
        .{
            .text = "abc\ndefg\nhi",
            .delete = 3,
            .want_text = "abc\ndefg",
            .want_line = 1,
            .want_starts = &.{ 0, 4 },
        },
        // Mid-document: the lines still above the cursor slide down.
        .{
            .text = "abc\ndefg\nhi",
            .gap = 6,
            .delete = 3,
            .want_text = "abcfg\nhi",
            .want_line = 0,
            .want_starts = &.{ 0, 6 },
        },
        // Several newlines at once are one contiguous removal.
        .{
            .text = "a\nb\nc\nd\ne",
            .delete = 6,
            .want_text = "a\nb",
            .want_line = 1,
            .want_starts = &.{ 0, 2 },
        },
        // Clamped: the table must not lose its first entry.
        .{
            .text = "ab\ncd",
            .delete = 99,
            .want_text = "",
            .want_line = 0,
            .want_starts = &.{0},
        },
        .{
            .text = "abc\ndefg\nhi",
            .gap = 4,
            .delete = 0,
            .want_text = "abc\ndefg\nhi",
            .want_line = 1,
            .want_starts = &.{ 0, 4, 9 },
        },
    };

    for (cases, 0..) |case, i| {
        errdefer std.debug.print(
            "\n  failing case {d}: \"{f}\" delete {d}\n",
            .{ i, std.zig.fmtString(case.text), case.delete },
        );

        var document = try doc.make(256, 16);
        defer doc.destroy(document);

        try doc.writeString(&document, case.text);
        if (case.gap) |gap| try doc.moveGap(&document, gap);
        doc.deleteChars(&document, case.delete);

        try doc.expectText(&document, case.want_text);
        try std.testing.expectEqual(case.want_line, document.current_line);

        var buf: [32]usize = undefined;
        try std.testing.expectEqualSlices(
            usize,
            case.want_starts,
            doc.lineStarts(&document, &buf),
        );
    }
}

const Model = struct {
    text: std.ArrayList(u8) = .empty,
    cursor: usize = 0,

    fn deinit(self: *Model, allocator: std.mem.Allocator) void {
        self.text.deinit(allocator);
    }

    fn insert(self: *Model, allocator: std.mem.Allocator, byte: u8) !void {
        try self.text.insert(allocator, self.cursor, byte);
        self.cursor += 1;
    }

    fn delete(self: *Model, n: usize) void {
        const count = @min(n, self.cursor);
        self.text.replaceRangeAssumeCapacity(self.cursor - count, count, &.{});
        self.cursor -= count;
    }
};

test "randomised edits agree with a naive model" {
    const allocator = std.testing.allocator;

    for ([_]u64{ 1, 7, 1337 }) |seed| {
        for (shapes) |shape| {
            errdefer std.debug.print(
                "\n  failing seed={d} shape: cap={d} gap={d}\n",
                .{ seed, shape.cap, shape.gap },
            );

            var prng = std.Random.DefaultPrng.init(seed);
            const random = prng.random();

            var document = try doc.make(shape.cap, shape.gap);
            defer doc.destroy(document);

            var model = Model{};
            defer model.deinit(allocator);

            var line_scratch: std.ArrayList(usize) = .empty;
            defer line_scratch.deinit(allocator);

            for (0..2000) |step| {
                // Named so a failure says which operation produced the state,
                // not just which iteration it happened on.
                var op: []const u8 = "?";
                var arg: usize = 0;
                errdefer std.debug.print(
                    "\n  failing step {d}: {s} {d}\n" ++
                        "  model: len={d} cursor={d}\n" ++
                        "  doc:   text_len={d} current_line={d} lines.len={d}\n",
                    .{
                        step,           op,
                        arg,            model.text.items.len,
                        model.cursor,   doc.textLen(&document),
                        document.current_line, document.lines.len,
                    },
                );

                switch (random.uintLessThan(u8, 100)) {
                    0...44 => {
                        // Newlines have to appear often enough to build a
                        // real line table, and to be crossed by deletes.
                        const byte: u8 = if (random.uintLessThan(u8, 8) == 0)
                            '\n'
                        else
                            random.intRangeAtMost(u8, 'a', 'z');
                        op = if (byte == '\n') "write_char newline" else "write_char";
                        arg = byte;
                        try doc.writeChar(&document, byte);
                        try model.insert(allocator, byte);
                    },
                    45...59 => {
                        var buf: [16]u8 = undefined;
                        const n = random.intRangeAtMost(usize, 1, buf.len);
                        for (buf[0..n]) |*byte| {
                            byte.* = if (random.uintLessThan(u8, 10) == 0)
                                '\n'
                            else
                                random.intRangeAtMost(u8, 'A', 'Z');
                        }
                        op = "write_string";
                        arg = n;
                        try doc.writeString(&document, buf[0..n]);
                        for (buf[0..n]) |byte| try model.insert(allocator, byte);
                    },
                    60...77 => {
                        const n = random.uintLessThan(usize, 6);
                        op = "delete_chars";
                        arg = n;
                        doc.deleteChars(&document, n);
                        model.delete(n);
                    },
                    78...96 => {
                        // Deliberately overshoots the end sometimes.
                        const pos = random.uintLessThan(usize, model.text.items.len + 3);
                        op = "move_gap";
                        arg = pos;
                        if (pos <= model.text.items.len) {
                            try doc.moveGap(&document, pos);
                            model.cursor = pos;
                        } else {
                            try std.testing.expectError(
                                error.InvalidPosition,
                                doc.moveGap(&document, pos),
                            );
                        }
                    },
                    else => {
                        op = "clear_content";
                        doc.clearContent(&document);
                        model.text.clearRetainingCapacity();
                        model.cursor = 0;
                    },
                }

                try doc.expectText(&document, model.text.items);
                try std.testing.expectEqual(model.cursor, document.gap_start);
                try doc.expectLineTable(
                    &document,
                    model.text.items,
                    model.cursor,
                    &line_scratch,
                    allocator,
                );
            }
        }
    }
}
