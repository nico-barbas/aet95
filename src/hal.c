#include "hal.h"

#include "core/allocator.h"

#include <assert.h>

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu) {
  *cpu = (Aet_CPU){0};
  return Aet_CPU_Error_None;
}

Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program) {
  cpu->program = program;
  cpu->pc = 0;
  cpu->sp = 0;
  cpu->fp = 0;

  return Aet_CPU_Error_None;
}

Aet_CPU_Error aet_cpu_execute(Aet_CPU *cpu, Aet_RAM *ram, usize budget) {
  usize cycle_spent = 0;

  while (cycle_spent < budget) {
    if (cpu->pc >= cpu->program.len) {
      break;
    }

    u32 instr = array_get(cpu->program, cpu->pc);

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
    case Aet_CPU_Opcode_Beq: {
      rs1 = rd;
      Aet_Register rs2 = rs1;
      i32 offset = (i32)((instr >> 16) & 0xffff);

      cpu->pc = rs1 == rs2 ? (u32)((i32)cpu->pc + offset) : 0;
    } break;
    case Aet_CPU_Opcode_MAX:
      assert(false);
      break;
    }

    cpu->pc += 1;
  }

  return Aet_CPU_Error_None;
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
