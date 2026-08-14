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
  Aet_CPU_Opcode_MAX,
} Aet_CPU_Opcode;

typedef enum Aet_Register : byte {
  Aet_Register_R0,
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
  Aet_Register_R13,
  Aet_Register_R14,
  Aet_Register_R15,
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
  Aet_CPU_Error_Trap,
} Aet_CPU_Error;

typedef struct Aet_CPU {
  u32 registers[Aet_Register_MAX];
  u32 pc; // Program counter
  u32 sp; // Stack pointer
  u32 fp; // Frame pointer
  Aet_CPU_Flags flags;
  Aet_Program program;
  // TODO(nico): Trap state
} Aet_CPU;

typedef enum Aet_RAM_Error {
  Aet_RAM_Error_None,
  Aet_RAM_Error_Invalid_Read_Ptr,
  Aet_RAM_Error_Access_Out_Of_Bounds,
} Aet_RAM_Error;

typedef struct Aet_RAM {
  byte raw[AET_RAM_CAP];
} Aet_RAM;

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu);
Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program);
Aet_CPU_Error aet_cpu_execute(Aet_CPU *cpu, usize budget);

Aet_RAM_Error aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out);
Aet_RAM_Error aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value);

Aet_RAM_Error aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out);
Aet_RAM_Error aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value);

Aet_RAM_Error aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out);
Aet_RAM_Error aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value);

#endif
