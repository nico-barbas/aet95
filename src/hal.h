#ifndef HAL_H
#define HAL_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/types.h"

typedef Array(u32) Aet_Program;

typedef enum Aet_Bit_Extension {
  Aet_Bit_Extension_None,
  Aet_Bit_Extension_Zero,
  Aet_Bit_Extension_Signed,
} Aet_Bit_Extension;

// NOTE(nico): single source of truth for the instruction set. Feeds the opcode
// enum below, the assembler's token kinds and keyword table, and the
// disassembler's opcode -> text mapping. Adding an instruction should only mean
// adding a line here.
//
// Opcode numbers are explicit so inserting a row never renumbers the rows below
// it. The static_assert after the enum keeps the numbering dense, which
// aet_instruction_lookup relies on.
//
// Only /* */ comments may appear between rows: line splicing happens before
// comment removal, so a // comment would swallow every row below it.
//
// mnemonic | text | opcode | immediate extension
#define AET_INSTRUCTIONS(X)                                                    \
  X(Addi, "addi", 0, Signed)                                                   \
  X(Add, "add", 1, None)                                                       \
  X(Sub, "sub", 2, None)                                                       \
  X(Mul, "mul", 3, None)                                                       \
  X(Div, "div", 4, None)                                                       \
  X(Divu, "divu", 5, None)                                                     \
  X(And, "and", 6, None)                                                       \
  X(Or, "or", 7, None)                                                         \
  /* ori and loadui carry bit patterns, not numbers, so they zero-extend */    \
  X(Ori, "ori", 8, Zero)                                                       \
  X(Xor, "xor", 9, None)                                                       \
  X(Shl, "shiftl", 10, None)                                                   \
  X(Shr, "shiftr", 11, None)                                                   \
  X(Lui, "loadui", 12, Zero)                                                   \
  X(Lb, "loadb", 13, Signed)                                                   \
  X(Lbu, "loadbu", 14, Signed)                                                 \
  X(Lh, "loadh", 15, Signed)                                                   \
  X(Lhu, "loadhu", 16, Signed)                                                 \
  X(Lw, "loadw", 17, Signed)                                                   \
  X(Sb, "storeb", 18, Signed)                                                  \
  X(Sh, "storeh", 19, Signed)                                                  \
  X(Sw, "storew", 20, Signed)                                                  \
  /* the u suffix selects an unsigned comparison; the branch offset is still   \
     a signed displacement */                                                  \
  X(Beq, "beq", 21, Signed)                                                    \
  X(Bneq, "bneq", 22, Signed)                                                  \
  X(Blt, "blt", 23, Signed)                                                    \
  X(Bgeq, "bgeq", 24, Signed)                                                  \
  X(Bltu, "bltu", 25, Signed)                                                  \
  X(Bgequ, "bgequ", 26, Signed)                                                \
  X(Jmp, "jump", 27, Signed)                                                   \
  X(Call, "call", 28, Signed)                                                  \
  X(Ret, "ret", 29, None)

typedef enum Aet_CPU_Opcode : byte {
#define X(name, text, opcode, ext) Aet_CPU_Opcode_##name = opcode,
  AET_INSTRUCTIONS(X)
#undef X
      Aet_CPU_Opcode_MAX,
} Aet_CPU_Opcode;

// NOTE(nico): aet_instruction_lookup is indexed by opcode, so a gap in the
// numbering above would leave a zeroed hole in it.
#define X(name, text, opcode, ext) +1
static_assert(
    0 AET_INSTRUCTIONS(X) == Aet_CPU_Opcode_MAX,
    "opcode numbers must be dense and MAX must follow the last instruction"
);
#undef X

typedef enum Aet_Register : byte {
  Aet_Register_Rx0, // zero
  Aet_Register_Rx1, // return-address
  Aet_Register_Rx2, // stack pointer
  Aet_Register_R0,  // general
  Aet_Register_R1,
  Aet_Register_R2,
  Aet_Register_R3,
  Aet_Register_R4,
  Aet_Register_R5,
  Aet_Register_R6,
  Aet_Register_R7,
  Aet_Register_R8,
  Aet_Register_R9,
  Aet_Register_R10,
  Aet_Register_R11,
  Aet_Register_R12,
  Aet_Register_MAX,
} Aet_Register;

typedef enum Aet_CPU_Flag {
  Aet_CPU_Flag_Running = 1 << 0,
  Aet_CPU_Flag_Halted = 1 << 1,
} Aet_CPU_Flag;
typedef u32 Aet_CPU_Flags;

typedef enum Aet_CPU_Error {
  Aet_CPU_Error_None,
  Aet_CPU_Error_Failed_To_Initialize,
  Aet_CPU_Error_Budget_Spent,
  Aet_CPU_Error_Fault,
} Aet_CPU_Error;

typedef enum Aet_CPU_Fault {
  Aet_CPU_Fault_None,
  Aet_CPU_Fault_Invalid_Opcode,
  Aet_CPU_Fault_Invalid_Next_Program_Counter,
  Aet_CPU_Fault_Invalid_Memory_Access,
  Aet_CPU_Fault_Divide_By_Zero,
  Aet_CPU_Fault_Divide_Overflow,
  // Maybe misaligned access fault, depending on the strictness of the ISA and
  // VM
} Aet_CPU_Fault;

typedef enum Aet_CPU_Trap {
  Aet_CPU_Trap_None,
} Aet_CPU_Trap;

typedef struct Aet_CPU {
  u32 registers[Aet_Register_MAX];
  u32 pc; // Program counter
  Aet_CPU_Flags flags;
  Aet_CPU_Fault fault;
  Aet_CPU_Trap trap;
  Aet_Program program;
} Aet_CPU;

typedef enum Aet_RAM_Error {
  Aet_RAM_Error_None,
  Aet_RAM_Error_Failed_To_Initialize,
  Aet_RAM_Error_Invalid_Read_Ptr,
  Aet_RAM_Error_Access_Out_Of_Bounds,
} Aet_RAM_Error;

typedef struct Aet_RAM {
  Allocator allocator;
  byte *raw;
  usize cap;
} Aet_RAM;

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu);
Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program);
Aet_CPU_Error aet_cpu_execute(Aet_CPU *cpu, Aet_RAM *ram, usize budget);

Aet_RAM_Error aet_ram_init(Aet_RAM *ram, usize byte_cap, Allocator allocator);
void aet_ram_destroy(Aet_RAM *ram);

Aet_RAM_Error aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out);
Aet_RAM_Error aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out);
Aet_RAM_Error aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out);

Aet_RAM_Error aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value);
Aet_RAM_Error aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value);
Aet_RAM_Error aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value);

#endif
