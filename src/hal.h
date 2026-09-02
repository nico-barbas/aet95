#ifndef HAL_H
#define HAL_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/types.h"

#define AET_MMIO_BASE_ADDR 0xffff8000
#define AET_MMIO_REGION (0xffffffff - AET_MMIO_BASE_ADDR)
#define AET_DEVICE_PAGE_SIZE (64u)
#define AET_DEVICE_PAGE_SHIFT (6u)

static_assert(
    1 << AET_DEVICE_PAGE_SHIFT == AET_DEVICE_PAGE_SIZE,
    "Device Page size and page are out of sync"
);

typedef Array(u32) Aet_Program;

typedef enum Aet_Fault {
  Aet_Fault_None,
  Aet_Fault_Invalid_Opcode,
  Aet_Fault_Invalid_Next_Program_Counter,
  Aet_Fault_Invalid_Address,
  Aet_Fault_Invalid_MMIO_Operation,
  Aet_Fault_Misaligned_Address,
  Aet_Fault_Divide_By_Zero,
  Aet_Fault_Divide_Overflow,
  Aet_Fault_Float_NaN,
  Aet_Fault_Internal_Device_Error,
} Aet_Fault;

typedef enum Aet_Machine_Error {
  Aet_Machine_Error_None,
  Aet_Machine_Error_Failed_To_Initialize_CPU,
  Aet_Machine_Error_Failed_To_Initialize_RAM,
  Aet_Machine_Error_Failed_To_Initialize_Devices,
} Aet_Machine_Error;

typedef enum Aet_CPU_Error {
  Aet_CPU_Error_None,
  Aet_CPU_Error_Failed_To_Initialize,
  Aet_CPU_Error_Budget_Spent,
  Aet_CPU_Error_Fault,
} Aet_CPU_Error;

typedef enum Aet_RAM_Error {
  Aet_RAM_Error_None,
  Aet_RAM_Error_Failed_To_Initialize,
} Aet_RAM_Error;

typedef enum Aet_Bit_Extension {
  Aet_Bit_Extension_None,
  Aet_Bit_Extension_Zero,
  Aet_Bit_Extension_Signed,
} Aet_Bit_Extension;

typedef enum Aet_Instruction_Form {
  Aet_Instruction_Form_None,
  Aet_Instruction_Form_RRR,
  Aet_Instruction_Form_RRI,
  Aet_Instruction_Form_RI,
  Aet_Instruction_Form_I,
} Aet_Instruction_Form;

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
// A row accepts a label only where its immediate is a pc-relative displacement
// the assembler can resolve on its own: aet_cpu_branch_to adds the field to the
// pc, so a label there means `target - pc` counted in instructions. Every other
// immediate is an address or a plain value, and this machine has no data
// section for a label to name.
//
// internal value name | mnemonic | opcode | form | immediate extension |
// instruction count | label allowed
#define AET_INSTRUCTIONS(X)                                                    \
  X(Addi, "addi", 0, RRI, Signed, 1, false)                                    \
  X(Add, "add", 1, RRR, None, 1, false)                                        \
  X(Sub, "sub", 2, RRR, None, 1, false)                                        \
  X(Mul, "mul", 3, RRR, None, 1, false)                                        \
  X(Div, "div", 4, RRR, None, 1, false)                                        \
  X(Divu, "divu", 5, RRR, None, 1, false)                                      \
  X(And, "and", 6, RRR, None, 1, false)                                        \
  X(Or, "or", 7, RRR, None, 1, false)                                          \
  /* ori and loadui carry bit patterns, not numbers, so they zero-extend */    \
  X(Ori, "ori", 8, RRI, Zero, 1, false)                                        \
  X(Xor, "xor", 9, RRR, None, 1, false)                                        \
  X(Shl, "shiftl", 10, RRR, None, 1, false)                                    \
  X(Shr, "shiftr", 11, RRR, None, 1, false)                                    \
  X(Lui, "loadui", 12, RI, Zero, 1, false)                                     \
  X(Lb, "loadb", 13, RRI, Signed, 1, false)                                    \
  X(Lbu, "loadbu", 14, RRI, Signed, 1, false)                                  \
  X(Lh, "loadh", 15, RRI, Signed, 1, false)                                    \
  X(Lhu, "loadhu", 16, RRI, Signed, 1, false)                                  \
  X(Lw, "loadw", 17, RRI, Signed, 1, false)                                    \
  X(Sb, "storeb", 18, RRI, Signed, 1, false)                                   \
  X(Sh, "storeh", 19, RRI, Signed, 1, false)                                   \
  X(Sw, "storew", 20, RRI, Signed, 1, false)                                   \
  /* the u suffix selects an unsigned comparison; the branch offset is still   \
     a signed displacement */                                                  \
  X(Beq, "beq", 21, RRI, Signed, 1, true)                                      \
  X(Bneq, "bneq", 22, RRI, Signed, 1, true)                                    \
  X(Blt, "blt", 23, RRI, Signed, 1, true)                                      \
  X(Bgeq, "bgeq", 24, RRI, Signed, 1, true)                                    \
  X(Bltu, "bltu", 25, RRI, Signed, 1, true)                                    \
  X(Bgequ, "bgequ", 26, RRI, Signed, 1, true)                                  \
  X(Jmp, "jump", 27, I, Signed, 1, true)                                       \
  X(Call, "call", 28, I, Signed, 1, true)                                      \
  X(Ret, "ret", 29, None, None, 1, false)                                      \
  /* The float extension */                                                    \
  X(Addf, "addf", 30, RRR, None, 1, false)                                     \
  X(Subf, "subf", 31, RRR, None, 1, false)                                     \
  X(Mulf, "mulf", 32, RRR, None, 1, false)                                     \
  X(Divf, "divf", 33, RRR, None, 1, false)

typedef enum Aet_CPU_Opcode : byte {
#define X(name, text, opcode, form, ext, instr_count, label_allowed)           \
  Aet_CPU_Opcode_##name = (opcode),
  AET_INSTRUCTIONS(X)
#undef X
      Aet_CPU_Opcode_MAX,
} Aet_CPU_Opcode;

// NOTE(nico): aet_instruction_lookup is indexed by opcode, so a gap in the
// numbering above would leave a zeroed hole in it.
#define X(name, text, opcode, form, ext, instr_count, label_allowed) +1
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

typedef enum Aet_CPU_Trap {
  Aet_CPU_Trap_None,
} Aet_CPU_Trap;

typedef struct Aet_CPU {
  usize clock_hz;
  u32 registers[Aet_Register_MAX];
  u32 pc; // Program counter
  Aet_CPU_Flags flags;
  Aet_Fault fault;
  Aet_CPU_Trap trap;
  Aet_Program program;
} Aet_CPU;

typedef struct Aet_RAM {
  byte *raw;
  usize cap;
} Aet_RAM;

#define AET_DEVICE_CLASS(X)                                                    \
  X(Identity, 0)                                                               \
  X(Navigation, 1)                                                             \
  X(Motor, 2)                                                                  \
  X(Sensor, 3)

typedef enum Aet_Device_Class {
#define X(name, kind) Aet_Device_Class_##name = (kind),
  AET_DEVICE_CLASS(X)
#undef X
      Aet_Device_Class_MAX
} Aet_Device_Class;

typedef u32 Aet_Device_Available_Set;

typedef struct Aet_Device {
  rawptr data;
  u64 extra;
  Aet_Fault (*read_u32_fn)(rawptr data, u64 extra, u32 reg, u32 *out);
  Aet_Fault (*write_u32_fn)(rawptr data, u64 extra, u32 reg, u32 value);
} Aet_Device;

typedef struct Aet_Memory_Bus {
  Aet_RAM *ram;
  Aet_Device_Available_Set available_devices;
  Aet_Device *devices;
} Aet_Memory_Bus;

typedef struct Aet_Machine {
  Allocator allocator;
  Aet_CPU cpu;
  Aet_RAM ram;
  Aet_Device_Available_Set available_devices;
  Aet_Device devices[Aet_Device_Class_MAX];
} Aet_Machine;

// NOTE(nico): ultimately the "spec" of a machine.
// It could be interesting to have the devices changed at runtime, but not
// really important for now
typedef struct Aet_Machine_Create_Info {
  usize clock_hz;
  usize ram_byte_cap;
  Aet_Device_Available_Set available_devices;
  Aet_Device devices[Aet_Device_Class_MAX];
} Aet_Machine_Create_Info;

Aet_Machine_Error aet_machine_init(
    Aet_Machine *machine, Aet_Machine_Create_Info *info, Allocator allocator
);
void aet_machine_destroy(Aet_Machine *machine);
// NOTE(nico): The budget should be internal to the machine or the cpu
// ultimately
Aet_CPU_Error aet_machine_run(Aet_Machine *machine, usize budget);

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu, usize clock_hz);
Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program);
Aet_CPU_Error
aet_cpu_execute_program(Aet_CPU *cpu, Aet_Memory_Bus *bus, usize budget);

Aet_RAM_Error aet_ram_init(Aet_RAM *ram, usize byte_cap, Allocator allocator);
void aet_ram_destroy(Aet_RAM *ram, Allocator allocator);

Aet_Fault aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out);
Aet_Fault aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out);
Aet_Fault aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out);

Aet_Fault aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value);
Aet_Fault aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value);
Aet_Fault aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value);

Aet_Fault
aet_identity_device_read_u32(rawptr data, u64 extra, u32 reg, u32 *out);
Aet_Fault
aet_identity_device_write_u32(rawptr data, u64 extra, u32 reg, u32 value);

// Small helpers
f32 aet_u32_to_f32(u32 bits);
u32 aet_f32_to_u32(f32 value);
bool32 aet_is_nan_f32(f32 f);

#endif
