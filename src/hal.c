#include "hal.h"

#include "core/allocator.h"
#include "core/math.h"

#include <assert.h>

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu) {
  *cpu = (Aet_CPU){0};
  return Aet_CPU_Error_None;
}

Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program) {
  cpu->program = program;
  cpu->pc = 0;

  return Aet_CPU_Error_None;
}

static bool32 aet_cpu_branch_to(Aet_CPU *cpu, u32 *next_pc, i32 offset) {
  *next_pc = cpu->pc + (u32)offset;
  return *next_pc < cpu->program.len;
}

static bool32 aet_cpu_compare_registers(
    Aet_CPU *cpu, Aet_Register lhs, Aet_Register rhs, Aet_CPU_Opcode opcode
) {
  u32 a = cpu->registers[lhs];
  u32 b = cpu->registers[rhs];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
  switch (opcode) {
  case Aet_CPU_Opcode_Beq:
    return a == b;
  case Aet_CPU_Opcode_Bneq:
    return a != b;
  case Aet_CPU_Opcode_Blt:
    return (i32)a < (i32)b;
  case Aet_CPU_Opcode_Bgeq:
    return (i32)a >= (i32)b;
  case Aet_CPU_Opcode_Bltu:
    return a == b;
  case Aet_CPU_Opcode_Bgequ:
    return a == b;
  default:
    assert(false);
    return false;
  }
#pragma clang diagnostic pop
}

Aet_CPU_Error aet_cpu_execute(Aet_CPU *cpu, Aet_RAM *ram, usize budget) {
  usize cycle_spent = 0;
  Aet_CPU_Error err = Aet_CPU_Error_None;

  while (cycle_spent < budget) {
    if (cpu->pc >= cpu->program.len) {
      break;
    }

    u32 instr = array_get(cpu->program, cpu->pc);
    u32 next_pc = cpu->pc + 1;

    // TODO(nico): Need to encode the different instructions
    Aet_CPU_Opcode opcode = (Aet_CPU_Opcode)(instr & 0xff);
    Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
    Aet_Register rs1 = (Aet_Register)((instr >> 12) & 0x0f);

    switch (opcode) {
    case Aet_CPU_Opcode_Addi: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      cpu->registers[rd] = cpu->registers[rs1] + immediate;
    } break;
    case Aet_CPU_Opcode_Add: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] + cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Sub: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] - cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Mul: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] * cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Div: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] / cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_And: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] & cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Or: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] | cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Xor: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] ^ cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Shl: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] << cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Shr: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      cpu->registers[rd] = cpu->registers[rs1] >> cpu->registers[rs2];
    } break;
    case Aet_CPU_Opcode_Lb: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      byte value = 0;
      assert(aet_ram_read_byte(ram, addr, &value) == Aet_RAM_Error_None);
      cpu->registers[rd] = (u32)value;
    } break;
    case Aet_CPU_Opcode_Lh: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      u16 value = 0;
      assert(aet_ram_read_u16(ram, addr, &value) == Aet_RAM_Error_None);
      cpu->registers[rd] = (u16)value;
    } break;
    case Aet_CPU_Opcode_Lw: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      u32 value = 0;
      assert(aet_ram_read_u32(ram, addr, &value) == Aet_RAM_Error_None);
      cpu->registers[rd] = value;
    } break;
    case Aet_CPU_Opcode_Sb: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      assert(
          aet_ram_write_u32(ram, addr, (byte)cpu->registers[rd]) ==
          Aet_RAM_Error_None
      );
    } break;
    case Aet_CPU_Opcode_Sh: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      assert(
          aet_ram_write_u32(ram, addr, (u16)cpu->registers[rd]) ==
          Aet_RAM_Error_None
      );
    } break;
    case Aet_CPU_Opcode_Sw: {
      u32 immediate = (u32)((instr >> 16) & 0xffff);
      u32 addr = cpu->registers[rs1] + immediate;

      assert(
          aet_ram_write_u32(ram, addr, cpu->registers[rd]) == Aet_RAM_Error_None
      );
    } break;
    case Aet_CPU_Opcode_Beq:
    case Aet_CPU_Opcode_Bneq:
    case Aet_CPU_Opcode_Blt:
    case Aet_CPU_Opcode_Bgeq:
    case Aet_CPU_Opcode_Bltu:
    case Aet_CPU_Opcode_Bgequ: {
      Aet_Register lhs = rd;
      Aet_Register rhs = rs1;
      i32 offset = sign_extend_i32((instr >> 16) & 0xffff, 16);

      if (aet_cpu_compare_registers(cpu, lhs, rhs, opcode)) {
        if (!aet_cpu_branch_to(cpu, &next_pc, offset)) {
          err = Aet_CPU_Error_Trap;
          goto exit;
        }
      }
    } break;
    case Aet_CPU_Opcode_Jmp: {
      // The total offset possible is held in 24 bits
      i32 offset = sign_extend_i32(instr >> 8, 24);
      if (!aet_cpu_branch_to(cpu, &next_pc, offset)) {
        err = Aet_CPU_Error_Trap;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Call: {
      i32 offset = sign_extend_i32(instr >> 8, 24);
      cpu->registers[Aet_Register_Rx1] = next_pc;
      if (!aet_cpu_branch_to(cpu, &next_pc, offset)) {
        err = Aet_CPU_Error_Trap;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Ret: {
      if (cpu->registers[Aet_Register_Rx1] >= cpu->program.len) {
        err = Aet_CPU_Error_Trap;
        goto exit;
      }
      next_pc = cpu->registers[Aet_Register_Rx1];
    } break;
    case Aet_CPU_Opcode_MAX:
    default:
      err = Aet_CPU_Error_Trap;
      goto exit;
    }

    cpu->pc = next_pc;
    cycle_spent += 1;
  }

exit:
  return err;
}

Aet_RAM_Error aet_ram_init(Aet_RAM *ram, Allocator allocator) {
  ram->allocator = allocator;

  Allocation_Result alloc =
      allocator.alloc(allocator, AET_RAM_CAP * sizeof(byte));
  if (alloc.err != Allocation_Error_None) {
    return Aet_RAM_Error_Failed_To_Initialize;
  }

  ram->raw = (byte *)alloc.allocation;
  return Aet_RAM_Error_None;
}

void aet_ram_destroy(Aet_RAM *ram) {
  ram->allocator.free(ram->allocator, ram->raw);
}

Aet_RAM_Error aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out) {
  if (addr >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  *out = ram->raw[addr];
  return Aet_RAM_Error_None;
}

Aet_RAM_Error aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value) {
  if (addr >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  ram->raw[addr] = value;
  return Aet_RAM_Error_None;
}

Aet_RAM_Error aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out) {
  if (addr + 1 >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  *out = (u16)(ram->raw[addr + 1] << 8) | (u16)(ram->raw[addr]);
  return Aet_RAM_Error_None;
}

Aet_RAM_Error aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value) {
  if (addr + 1 >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  ram->raw[addr] = (byte)(value & 0xff);
  ram->raw[addr + 1] = (byte)((value >> 8) & 0xff);
  return Aet_RAM_Error_None;
}

Aet_RAM_Error aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out) {
  if (addr + 3 >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  *out = ((u32)ram->raw[addr + 3] << 24) | ((u32)ram->raw[addr + 2] << 16) |
         ((u32)ram->raw[addr + 1] << 8) | ((u32)ram->raw[addr]);
  return Aet_RAM_Error_None;
}

Aet_RAM_Error aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value) {
  if (addr + 3 >= AET_RAM_CAP) {
    return Aet_RAM_Error_Access_Out_Of_Bounds;
  }

  ram->raw[addr] = (byte)(value & 0xff);
  ram->raw[addr + 1] = (byte)((value >> 8) & 0xff);
  ram->raw[addr + 2] = (byte)((value >> 16) & 0xff);
  ram->raw[addr + 3] = (byte)((value >> 24) & 0xff);
  return Aet_RAM_Error_None;
}
