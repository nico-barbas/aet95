const std = @import("std");
const aet = @import("aet.zig");

pub const c = aet.c;

pub const AssemblerError = error{
    InternalFailure,
    MalformedDecimalLiteral,
    MalformedHexLiteral,
    MalformedBinaryLiteral,
    InvalidIdentifier,
    InvalidSyntax,
    InvalidImmediateValue,
    DuplicateSymbol,
    UnknownSymbol,
    UnrecognisedErrorCode,
};

fn assemblerError(code: c_int) AssemblerError {
    return switch (code) {
        c.Aet_Assembler_Error_Internal_Failure => error.InternalFailure,
        c.Aet_Assembler_Error_Malformed_Decimal_Literal => error.MalformedDecimalLiteral,
        c.Aet_Assembler_Error_Malformed_Binary_Literal => error.MalformedBinaryLiteral,
        c.Aet_Assembler_Error_Malformed_Hex_Literal => error.MalformedHexLiteral,
        c.Aet_Assembler_Error_Invalid_Identifier => error.InvalidIdentifier,
        c.Aet_Assembler_Error_Invalid_Syntax => error.InvalidSyntax,
        c.Aet_Assembler_Error_Invalid_Immediate_Value => error.InvalidImmediateValue,
        c.Aet_Assembler_Error_Duplicate_Symbol => error.DuplicateSymbol,
        c.Aet_Assembler_Error_Unknown_Symbol => error.UnknownSymbol,
        else => error.UnrecognisedErrorCode,
    };
}

pub fn assemble(source: [*:0]const u8) AssemblerError!c.Aet_Program {
    const result = c.aet_assemble(
        c.from_c_str(source),
        c.heap_allocator(),
    );
    return aet.value(result) orelse assemblerError(aet.errorCode(result).?);
}

pub const DisassemblerError = error{
    InvalidProgram,
    InvalidOpcode,
    UnrecognisedErrorCode,
};

fn disassemblerError(code: c_int) DisassemblerError {
    return switch (code) {
        c.Aet_Disassembler_Error_Invalid_Program => error.InvalidProgram,
        c.Aet_Disassembler_Error_Invalid_Opcode => error.InvalidOpcode,
        else => error.UnrecognisedErrorCode,
    };
}

pub fn disassemble(program: c.Aet_Program) DisassemblerError![]const u8 {
    const result = c.aet_disassemble(program, c.heap_allocator());
    const text = aet.value(
        result,
    ) orelse return disassemblerError(aet.errorCode(result).?);
    return text.data[0..text.len];
}

pub fn words(program: c.Aet_Program) []const u32 {
    return program.items[0..program.len];
}

pub fn expectProgram(source: [*:0]const u8, want: []const u32) !void {
    const program = try assemble(source);
    try std.testing.expectEqualSlices(u32, want, words(program));
}
