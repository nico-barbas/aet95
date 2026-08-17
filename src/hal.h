#ifndef HAL_H
#define HAL_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/types.h"

#define AET_RAM_CAP MEGABYTE * 512

typedef Array(u32) Aet_Program;

typedef enum Aet_CPU_Opcode : byte {
  Aet_CPU_Opcode_Addi,
  Aet_CPU_Opcode_Add,
  Aet_CPU_Opcode_Sub,
  Aet_CPU_Opcode_Mul,
  Aet_CPU_Opcode_Div,
  Aet_CPU_Opcode_And,
  Aet_CPU_Opcode_Or,
  Aet_CPU_Opcode_Xor,
  Aet_CPU_Opcode_Shl,
  Aet_CPU_Opcode_Shr,
  Aet_CPU_Opcode_Lb,
  Aet_CPU_Opcode_Lh,
  Aet_CPU_Opcode_Lw,
  Aet_CPU_Opcode_Sb,
  Aet_CPU_Opcode_Sh,
  Aet_CPU_Opcode_Sw,
  Aet_CPU_Opcode_Beq,
  Aet_CPU_Opcode_Bneq,
  // NOTE(nico): comparisons are signed by default, the u suffix selects the
  // unsigned variant
  Aet_CPU_Opcode_Blt,
  Aet_CPU_Opcode_Bgeq,
  Aet_CPU_Opcode_Bltu,
  Aet_CPU_Opcode_Bgequ,
  Aet_CPU_Opcode_Jmp,
  Aet_CPU_Opcode_Call,
  Aet_CPU_Opcode_Ret,
  Aet_CPU_Opcode_MAX,
} Aet_CPU_Opcode;

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
  Aet_CPU_Fault_CPU_Divide_By_Zero,
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
} Aet_RAM;

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu);
Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program);
Aet_CPU_Error aet_cpu_execute(Aet_CPU *cpu, Aet_RAM *ram, usize budget);

Aet_RAM_Error aet_ram_init(Aet_RAM *ram, Allocator allocator);
void aet_ram_destroy(Aet_RAM *ram);

Aet_RAM_Error aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out);
Aet_RAM_Error aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value);

Aet_RAM_Error aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out);
Aet_RAM_Error aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value);

Aet_RAM_Error aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out);
Aet_RAM_Error aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value);

#endif
