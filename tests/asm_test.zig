const std = @import("std");
const aet = @import("aet.zig");
const _asm = @import("asm.zig");

const Encoding = struct { source: [*:0]const u8, want: []const u32 };
const Rejection = struct { source: [*:0]const u8, want: _asm.AssemblerError };

const encodings = [_]Encoding{
    // R format: rd, rs1, rs2 in bits 11:8, 15:12, 19:16.
    .{ .source = "add r0, r1, r2", .want = &.{0x00054301} },
    .{ .source = "sub r12, rx2, r5", .want = &.{0x00082f02} },
    .{ .source = "shiftl r1, r2, r3", .want = &.{0x0006540a} },
    .{ .source = "divu r4, r5, r6", .want = &.{0x00098705} },

    // I format, signed imm16 at both ends of its range.
    .{ .source = "loadw r0, rx0, -32768", .want = &.{0x80000311} },
    .{ .source = "storeb r3, r4, 32767", .want = &.{0x7fff7612} },
    .{ .source = "beq r1, r2, -4", .want = &.{0xfffc5415} },
    .{ .source = "bgequ r7, r8, 12", .want = &.{0x000cba1a} },

    // I format, zero-extended imm16: the two that carry bit patterns.
    .{ .source = "ori r1, r2, 65535", .want = &.{0xffff5408} },
    .{ .source = "loadui r0, 65535", .want = &.{0xffff030c} },

    // Hex and binary literals, in either prefix case. Hex digits are accepted
    // in either case too, so all three spellings of 0xdead encode alike.
    .{ .source = "loadui r0, 0xdead", .want = &.{0xdead030c} },
    .{ .source = "loadui r0, 0xDEAD", .want = &.{0xdead030c} },
    .{ .source = "loadui r0, 0XdEaD", .want = &.{0xdead030c} },
    .{ .source = "loadui r0, 0b1101111010101101", .want = &.{0xdead030c} },
    .{ .source = "ori r1, r2, 0b1111", .want = &.{0x000f5408} },
    .{ .source = "ori r1, r2, 0B1111", .want = &.{0x000f5408} },

    // The sign binds outside the base prefix, and a leading zero is not an
    // octal prefix: 0123 is one hundred and twenty-three.
    .{ .source = "addi r0, r1, -0x10", .want = &.{0xfff04300} },
    .{ .source = "addi r0, r1, -0b10000", .want = &.{0xfff04300} },
    .{ .source = "addi r0, r1, 0123", .want = &.{0x007b4300} },
    .{ .source = "addi r0, r1, -0123", .want = &.{0xff854300} },

    // A literal ends at the separator, the comment or the line, not only at a
    // space.
    .{ .source = "addi r0, r1, 0x10\nret", .want = &.{ 0x00104300, 0x0000001d } },
    .{ .source = "addi r0, r1, 5 ; note\nret", .want = &.{ 0x00054300, 0x0000001d } },

    // J format, signed imm24 at both ends. N format takes no operand.
    .{ .source = "jump -1", .want = &.{0xffffff1b} },
    .{ .source = "call 8388607", .want = &.{0x7fffff1c} },
    .{ .source = "ret", .want = &.{0x0000001d} },

    // Comments, blank lines and trailing comments are not instructions.
    .{
        .source = "; comment\nadd r0, r1, r2\n\nret ; trailing\n",
        .want = &.{ 0x00054301, 0x0000001d },
    },

    // Labels resolve to `target - pc` counted in instructions, measured from
    // the branch itself. Forward, backward, and zero.
    .{
        .source = "beq r0, r1, done\nret\ndone:\nret",
        .want = &.{ 0x00024315, 0x0000001d, 0x0000001d },
    },
    .{
        .source = "loop:\nadd r0, r1, r2\njump loop",
        .want = &.{ 0x00054301, 0xffffff1b },
    },
    .{ .source = "here:\nbeq r0, r1, here", .want = &.{0x00004315} },

    // A label after the last instruction names the clean-stop address.
    .{ .source = "jump done\nret\ndone:", .want = &.{ 0x0000021b, 0x0000001d } },
};

const rejections = [_]Rejection{
    .{ .source = "add r0, r1", .want = error.InvalidSyntax },
    .{ .source = "add r0, r1, 3", .want = error.InvalidSyntax },
    .{ .source = "add r0, r1, r2 r3", .want = error.InvalidSyntax },
    .{ .source = "add rr, r1, r2", .want = error.InvalidSyntax },
    .{ .source = "nope r0, r1, r2", .want = error.InvalidSyntax },

    // Both immediate widths, just past the field.
    .{ .source = "addi r0, r1, 32768", .want = error.InvalidImmediateValue },
    .{ .source = "loadui r0, 65536", .want = error.InvalidImmediateValue },

    // A malformed literal is reported against the shape it started with, and
    // a base prefix on its own carries no digits.
    .{ .source = "loadui r0, 0xbeefz", .want = error.MalformedHexLiteral },
    .{ .source = "loadui r0, 0x", .want = error.MalformedHexLiteral },
    .{ .source = "loadui r0, 0b0102", .want = error.MalformedBinaryLiteral },
    .{ .source = "loadui r0, 0b", .want = error.MalformedBinaryLiteral },
    .{ .source = "loadui r0, 1.2.3", .want = error.MalformedDecimalLiteral },
    .{ .source = "loadui r0, 13.", .want = error.MalformedDecimalLiteral },

    // Labels: undefined, duplicated, and used where no label is allowed.
    .{ .source = "jump nowhere", .want = error.UnknownSymbol },
    .{ .source = "a:\na:\nret", .want = error.DuplicateSymbol },
    .{ .source = "x:\naddi r0, r1, x", .want = error.InvalidSyntax },
    .{ .source = "x:\nloadui r0, x", .want = error.InvalidSyntax },
    .{ .source = "loop:\njump Loop", .want = error.UnknownSymbol },
};

// A source with no instructions is an empty program, not an error.
const empty_sources = [_][*:0]const u8{ "", "; only a comment\n", "\n\n\n" };

test "golden encodings" {
    for (encodings) |case| {
        errdefer std.debug.print("\n  failing source: \"{s}\"\n", .{case.source});
        try _asm.expectProgram(case.source, case.want);
    }
}

test "golden rejections" {
    for (rejections) |case| {
        errdefer std.debug.print("\n  failing source: \"{s}\"\n", .{case.source});
        try std.testing.expectError(case.want, _asm.assemble(case.source));
    }
}

test "sources with no instructions assemble to an empty program" {
    for (empty_sources) |source| {
        errdefer std.debug.print("\n  failing source: \"{s}\"\n", .{source});
        try _asm.expectProgram(source, &.{});
    }
}

test "disassembly re-assembles to a byte-identical program" {
    const sources = [_][*:0]const u8{
        "add r0, r1, r2\nsub r12, rx2, r5\nloadw r0, rx0, -32768",
        "loop:\naddi r0, r0, -1\nbneq r0, rx0, loop\nret",
        "loadui r0, 57005\nori r0, r0, 48879\njump 2\ncall -3\nret",
        "storeb r3, r4, 32767\nshiftr r1, r2, r3\ndivu r4, r5, r6\nbgequ r7, r8, 12",
    };

    for (sources) |source| {
        errdefer std.debug.print("\n  failing source: \"{s}\"\n", .{source});

        const first = try _asm.assemble(source);
        const text = try _asm.disassemble(first);

        const round_tripped = try std.testing.allocator.allocSentinel(u8, text.len, 0);
        defer std.testing.allocator.free(round_tripped);
        @memcpy(round_tripped, text);

        const second = try _asm.assemble(round_tripped.ptr);
        try std.testing.expectEqualSlices(u32, _asm.words(first), _asm.words(second));
    }
}
