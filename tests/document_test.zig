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

            for (0..2000) |step| {
                errdefer std.debug.print("\n  failing step: {d}\n", .{step});

                switch (random.uintLessThan(u8, 100)) {
                    0...44 => {
                        const byte = random.intRangeAtMost(u8, 'a', 'z');
                        try doc.writeChar(&document, byte);
                        try model.insert(allocator, byte);
                    },
                    45...59 => {
                        var buf: [16]u8 = undefined;
                        const n = random.intRangeAtMost(usize, 1, buf.len);
                        for (buf[0..n]) |*byte| {
                            byte.* = random.intRangeAtMost(u8, 'A', 'Z');
                        }
                        try doc.writeString(&document, buf[0..n]);
                        for (buf[0..n]) |byte| try model.insert(allocator, byte);
                    },
                    60...77 => {
                        const n = random.uintLessThan(usize, 6);
                        doc.deleteChars(&document, n);
                        model.delete(n);
                    },
                    78...96 => {
                        // Deliberately overshoots the end sometimes.
                        const pos = random.uintLessThan(usize, model.text.items.len + 3);
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
                        doc.clearContent(&document);
                        model.text.clearRetainingCapacity();
                        model.cursor = 0;
                    },
                }

                try doc.expectText(&document, model.text.items);
                try std.testing.expectEqual(model.cursor, document.gap_start);
            }
        }
    }
}
