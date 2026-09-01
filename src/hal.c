#include "hal.h"

#include "core/allocator.h"
#include "core/math.h"
#include "core/types.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

typedef struct Aet_Device_Poke_Info {
  u32 slot;
  u32 reg;
} Aet_Device_Poke_Info;

typedef Result(Aet_Device_Poke_Info, Aet_Fault) Aet_Device_Poke_Info_Result;

Aet_Machine_Error aet_machine_init(
    Aet_Machine *machine, Aet_Machine_Create_Info *info, Allocator allocator
) {
  for (usize i = 0; i < Aet_Device_Class_MAX; i += 1) {
    if (i == 0 || !(info->available_devices & (1u << i))) {
      continue;
    }

    if (info->devices[i].read_u32_fn == nullptr ||
        info->devices[i].write_u32_fn == nullptr) {
      return Aet_Machine_Error_Failed_To_Initialize_Devices;
    }
  }

  if (info->ram_byte_cap > AET_MMIO_BASE_ADDR) {
    return Aet_Machine_Error_Failed_To_Initialize_RAM;
  }

  machine->allocator = allocator;
  machine->available_devices = info->available_devices;

  memcpy(
      machine->devices, info->devices, Aet_Device_Class_MAX * sizeof(Aet_Device)
  );
  machine->available_devices |= 1 << Aet_Device_Class_Identity;
  machine->devices[Aet_Device_Class_Identity] = (Aet_Device){
    .data = machine,
    .read_u32_fn = aet_identity_device_read_u32,
    .write_u32_fn = aet_identity_device_write_u32,
  };

  if (aet_cpu_init(&machine->cpu, info->clock_hz) != Aet_CPU_Error_None) {
    return Aet_Machine_Error_Failed_To_Initialize_CPU;
  }

  if (aet_ram_init(&machine->ram, info->ram_byte_cap, allocator) !=
      Aet_RAM_Error_None) {
    return Aet_Machine_Error_Failed_To_Initialize_RAM;
  }

  return Aet_Machine_Error_None;
}

void aet_machine_destroy(Aet_Machine *machine) {
  aet_ram_destroy(&machine->ram, machine->allocator);
}

Aet_CPU_Error aet_machine_run(Aet_Machine *machine, usize budget) {
  Aet_Memory_Bus bus = {
    .ram = &machine->ram,
    .available_devices = machine->available_devices,
    .devices = machine->devices,
  };

  return aet_cpu_execute_program(&machine->cpu, &bus, budget);
}

static Aet_Device_Poke_Info_Result
aet_mmio_get_reg(Aet_Memory_Bus *bus, u32 paddr) {
  if ((paddr & 3) > 0) {
    return err(Aet_Device_Poke_Info_Result, Aet_Fault_Misaligned_Address);
  }

  u32 offset = paddr - AET_MMIO_BASE_ADDR;
  if (offset > AET_MMIO_REGION) {
    return err(Aet_Device_Poke_Info_Result, Aet_Fault_Invalid_Address);
  }

  u32 slot = offset >> AET_DEVICE_PAGE_SHIFT;
  if (slot >= Aet_Device_Class_MAX) {
    return err(Aet_Device_Poke_Info_Result, Aet_Fault_Invalid_Address);
  }

  if (!(bus->available_devices & (1u << slot))) {
    return err(Aet_Device_Poke_Info_Result, Aet_Fault_Invalid_Address);
  }

  Aet_Device_Poke_Info info = {
    .slot = slot,
    .reg = offset & (AET_DEVICE_PAGE_SIZE - 1),
  };
  return ok(Aet_Device_Poke_Info_Result, info);
}

static Aet_Fault aet_bus_read_byte(Aet_Memory_Bus *bus, u32 paddr, byte *out) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_read_byte(bus->ram, paddr, out);
  } else {
    return Aet_Fault_Invalid_MMIO_Operation;
  }
}

static Aet_Fault aet_bus_read_u16(Aet_Memory_Bus *bus, u32 paddr, u16 *out) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_read_u16(bus->ram, paddr, out);
  } else {
    return Aet_Fault_Invalid_MMIO_Operation;
  }
}

static Aet_Fault aet_bus_read_u32(Aet_Memory_Bus *bus, u32 paddr, u32 *out) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_read_u32(bus->ram, paddr, out);
  } else {
    Aet_Device_Poke_Info_Result device_poke_result =
        aet_mmio_get_reg(bus, paddr);
    if (!device_poke_result.ok) {
      return device_poke_result.error;
    }

    Aet_Device *device = &bus->devices[device_poke_result.value.slot];
    return device->read_u32_fn(device->data, device_poke_result.value.reg, out);
  }
}

static Aet_Fault
aet_bus_write_byte(Aet_Memory_Bus *bus, u32 paddr, byte value) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_write_byte(bus->ram, paddr, value);
  } else {
    return Aet_Fault_Invalid_MMIO_Operation;
  }
}

static Aet_Fault aet_bus_write_u16(Aet_Memory_Bus *bus, u32 paddr, u16 value) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_write_u16(bus->ram, paddr, value);
  } else {
    return Aet_Fault_Invalid_MMIO_Operation;
  }
}

static Aet_Fault aet_bus_write_u32(Aet_Memory_Bus *bus, u32 paddr, u32 value) {
  if (paddr < AET_MMIO_BASE_ADDR) {
    return aet_ram_write_u32(bus->ram, paddr, value);
  } else {
    Aet_Device_Poke_Info_Result device_poke_result =
        aet_mmio_get_reg(bus, paddr);
    if (!device_poke_result.ok) {
      return device_poke_result.error;
    }

    Aet_Device *device = &bus->devices[device_poke_result.value.slot];
    return device->write_u32_fn(
        device->data, device_poke_result.value.reg, value
    );
  }
}

/*
  NOTE(nico): No halt for now even if exists in the enum and flags
*/

Aet_CPU_Error aet_cpu_init(Aet_CPU *cpu, usize clock_hz) {
  *cpu = (Aet_CPU){.clock_hz = clock_hz};
  return Aet_CPU_Error_None;
}

Aet_CPU_Error aet_cpu_load_program(Aet_CPU *cpu, Aet_Program program) {
  cpu->program = program;
  cpu->pc = 0;

  return Aet_CPU_Error_None;
}

static bool32 aet_cpu_branch_to(Aet_CPU *cpu, u32 *next_pc, i32 offset) {
  *next_pc = cpu->pc + (u32)offset;
  return *next_pc <= cpu->program.len;
}

static u32 aet_cpu_read_register(Aet_CPU *cpu, Aet_Register reg) {
  return reg == Aet_Register_Rx0 ? 0 : cpu->registers[reg];
}

static void aet_cpu_write_register(Aet_CPU *cpu, Aet_Register reg, u32 value) {
  if (reg != Aet_Register_Rx0) {
    cpu->registers[reg] = value;
  }
}

static Aet_Fault
aet_cpu_write_register_f32(Aet_CPU *cpu, Aet_Register reg, f32 value) {
  if (aet_is_nan_f32(value)) {
    return Aet_Fault_Float_NaN;
  }

  aet_cpu_write_register(cpu, reg, aet_f32_to_u32(value));
  return Aet_Fault_None;
}

static bool32 aet_cpu_compare_registers(
    Aet_CPU *cpu, Aet_Register lhs, Aet_Register rhs, Aet_CPU_Opcode opcode
) {
  u32 a = aet_cpu_read_register(cpu, lhs);
  u32 b = aet_cpu_read_register(cpu, rhs);

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
    return a < b;
  case Aet_CPU_Opcode_Bgequ:
    return a >= b;
  default:
    assert(false);
    return false;
  }
#pragma clang diagnostic pop
}

Aet_CPU_Error
aet_cpu_execute_program(Aet_CPU *cpu, Aet_Memory_Bus *bus, usize budget) {
  (void)bus;

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
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      aet_cpu_write_register(
          cpu, rd, aet_cpu_read_register(cpu, rs1) + (u32)immediate
      );
    } break;
    case Aet_CPU_Opcode_Add: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) + aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Sub: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) - aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Mul: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) * aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Div: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);

      i32 divisor = (i32)(aet_cpu_read_register(cpu, rs2));
      if (divisor == 0) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Divide_By_Zero;
        goto exit;
      }

      i32 dividend = (i32)aet_cpu_read_register(cpu, rs1);
      if (dividend == INT_MIN && divisor == -1) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Divide_Overflow;
        goto exit;
      }

      aet_cpu_write_register(cpu, rd, (u32)(dividend / divisor));
    } break;
    case Aet_CPU_Opcode_Divu: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);

      u32 divisor = aet_cpu_read_register(cpu, rs2);
      if (divisor == 0) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Divide_By_Zero;
        goto exit;
      }

      aet_cpu_write_register(
          cpu, rd, aet_cpu_read_register(cpu, rs1) / divisor
      );
    } break;
    case Aet_CPU_Opcode_And: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) & aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Or: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) | aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Ori: {
      u32 immediate = (instr >> 16) & 0xffff;
      aet_cpu_write_register(
          cpu, rd, aet_cpu_read_register(cpu, rs1) | immediate
      );
    } break;
    case Aet_CPU_Opcode_Xor: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      aet_cpu_write_register(
          cpu,
          rd,
          aet_cpu_read_register(cpu, rs1) ^ aet_cpu_read_register(cpu, rs2)
      );
    } break;
    case Aet_CPU_Opcode_Shl: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      u32 amount = aet_cpu_read_register(cpu, rs2) & 31;
      aet_cpu_write_register(
          cpu, rd, aet_cpu_read_register(cpu, rs1) << amount
      );
    } break;
    case Aet_CPU_Opcode_Shr: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      u32 amount = aet_cpu_read_register(cpu, rs2) & 31;
      aet_cpu_write_register(
          cpu, rd, aet_cpu_read_register(cpu, rs1) >> amount
      );
    } break;
    case Aet_CPU_Opcode_Lui: {
      u32 immediate = instr & 0xffff0000;
      aet_cpu_write_register(cpu, rd, immediate);
    } break;
    case Aet_CPU_Opcode_Lb: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      byte value = 0;
      Aet_Fault bus_fault = aet_bus_read_byte(bus, addr, &value);

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }

      aet_cpu_write_register(cpu, rd, (u32)sign_extend_i32(value, 8));
    } break;
    case Aet_CPU_Opcode_Lbu: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      byte value = 0;
      Aet_Fault bus_fault = aet_bus_read_byte(bus, addr, &value);

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }

      aet_cpu_write_register(cpu, rd, (u32)value);
    } break;
    case Aet_CPU_Opcode_Lh: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      u16 value = 0;
      Aet_Fault bus_fault = aet_bus_read_u16(bus, addr, &value);

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
      aet_cpu_write_register(cpu, rd, (u32)sign_extend_i32(value, 16));
    } break;
    case Aet_CPU_Opcode_Lhu: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      u16 value = 0;
      Aet_Fault bus_fault = aet_bus_read_u16(bus, addr, &value);

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
      aet_cpu_write_register(cpu, rd, (u32)value);
    } break;
    case Aet_CPU_Opcode_Lw: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      u32 value = 0;
      Aet_Fault bus_fault = aet_bus_read_u32(bus, addr, &value);

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
      aet_cpu_write_register(cpu, rd, value);
    } break;
    case Aet_CPU_Opcode_Sb: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      Aet_Fault bus_fault =
          aet_bus_write_byte(bus, addr, (byte)aet_cpu_read_register(cpu, rd));

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Sh: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      Aet_Fault bus_fault =
          aet_bus_write_u16(bus, addr, (u16)aet_cpu_read_register(cpu, rd));

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Sw: {
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);
      u32 addr = aet_cpu_read_register(cpu, rs1) + (u32)immediate;

      Aet_Fault bus_fault =
          aet_bus_write_u32(bus, addr, aet_cpu_read_register(cpu, rd));

      if (bus_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = bus_fault;
        goto exit;
      }
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
          err = Aet_CPU_Error_Fault;
          cpu->fault = Aet_Fault_Invalid_Next_Program_Counter;
          goto exit;
        }
      }
    } break;
    case Aet_CPU_Opcode_Jmp: {
      // The total offset possible is held in 24 bits
      i32 offset = sign_extend_i32(instr >> 8, 24);
      if (!aet_cpu_branch_to(cpu, &next_pc, offset)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Invalid_Next_Program_Counter;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Call: {
      i32 offset = sign_extend_i32(instr >> 8, 24);
      u32 return_pc = next_pc;
      if (!aet_cpu_branch_to(cpu, &next_pc, offset)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Invalid_Next_Program_Counter;
        goto exit;
      }

      aet_cpu_write_register(cpu, Aet_Register_Rx1, return_pc);
    } break;
    case Aet_CPU_Opcode_Ret: {
      u32 return_addr = aet_cpu_read_register(cpu, Aet_Register_Rx1);
      if (return_addr > cpu->program.len) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Invalid_Next_Program_Counter;
        goto exit;
      }
      next_pc = return_addr;
    } break;

    // NOTE(nico): All floats instructions:
    // IEEE-754 implementation
    case Aet_CPU_Opcode_Addf: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      f32 left = aet_u32_to_f32(aet_cpu_read_register(cpu, rs1));
      f32 right = aet_u32_to_f32(aet_cpu_read_register(cpu, rs2));

      if (aet_is_nan_f32(left) || aet_is_nan_f32(right)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Float_NaN;
        goto exit;
      }

      Aet_Fault f32_fault = aet_cpu_write_register_f32(cpu, rd, left + right);
      if (f32_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = f32_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Subf: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      f32 left = aet_u32_to_f32(aet_cpu_read_register(cpu, rs1));
      f32 right = aet_u32_to_f32(aet_cpu_read_register(cpu, rs2));

      if (aet_is_nan_f32(left) || aet_is_nan_f32(right)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Float_NaN;
        goto exit;
      }

      Aet_Fault f32_fault = aet_cpu_write_register_f32(cpu, rd, left - right);
      if (f32_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = f32_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Mulf: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);
      f32 left = aet_u32_to_f32(aet_cpu_read_register(cpu, rs1));
      f32 right = aet_u32_to_f32(aet_cpu_read_register(cpu, rs2));

      if (aet_is_nan_f32(left) || aet_is_nan_f32(right)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Float_NaN;
        goto exit;
      }

      Aet_Fault f32_fault = aet_cpu_write_register_f32(cpu, rd, left * right);
      if (f32_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = f32_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_Divf: {
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);

      f32 dividend = aet_u32_to_f32(aet_cpu_read_register(cpu, rs1));
      f32 divisor = aet_u32_to_f32(aet_cpu_read_register(cpu, rs2));

      if (aet_is_nan_f32(dividend) || aet_is_nan_f32(divisor)) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Float_NaN;
        goto exit;
      }

      if (divisor == 0) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = Aet_Fault_Divide_By_Zero;
        goto exit;
      }

      Aet_Fault f32_fault =
          aet_cpu_write_register_f32(cpu, rd, dividend / divisor);
      if (f32_fault != Aet_Fault_None) {
        err = Aet_CPU_Error_Fault;
        cpu->fault = f32_fault;
        goto exit;
      }
    } break;
    case Aet_CPU_Opcode_MAX:
    default:
      err = Aet_CPU_Error_Fault;
      cpu->fault = Aet_Fault_Invalid_Opcode;
      goto exit;
    }

    cpu->pc = next_pc;
    cycle_spent += 1;
  }

  if (cycle_spent >= budget) {
    err = Aet_CPU_Error_Budget_Spent;
  }

exit:
  return err;
}

Aet_RAM_Error aet_ram_init(Aet_RAM *ram, usize byte_cap, Allocator allocator) {
  Allocation_Result alloc = allocator.alloc(allocator, byte_cap * sizeof(byte));
  if (alloc.err != Allocation_Error_None) {
    return Aet_RAM_Error_Failed_To_Initialize;
  }

  ram->raw = (byte *)alloc.allocation;
  ram->cap = byte_cap;

  memset(ram->raw, 0, ram->cap);
  return Aet_RAM_Error_None;
}

void aet_ram_destroy(Aet_RAM *ram, Allocator allocator) {
  allocator.free(allocator, ram->raw);
}

Aet_Fault aet_ram_read_byte(Aet_RAM *ram, u32 addr, byte *out) {
  if (addr >= ram->cap) {
    return Aet_Fault_Invalid_Address;
  }

  *out = ram->raw[addr];
  return Aet_Fault_None;
}

Aet_Fault aet_ram_read_u16(Aet_RAM *ram, u32 addr, u16 *out) {
  usize size = sizeof(u16);
  if (addr >= ram->cap || ram->cap - addr < size) {
    return Aet_Fault_Invalid_Address;
  }

  *out = (u16)(ram->raw[addr + 1] << 8) | (u16)(ram->raw[addr]);
  return Aet_Fault_None;
}

Aet_Fault aet_ram_read_u32(Aet_RAM *ram, u32 addr, u32 *out) {
  usize size = sizeof(u32);
  if (addr >= ram->cap || ram->cap - addr < size) {
    return Aet_Fault_Invalid_Address;
  }

  *out = ((u32)ram->raw[addr + 3] << 24) | ((u32)ram->raw[addr + 2] << 16) |
         ((u32)ram->raw[addr + 1] << 8) | ((u32)ram->raw[addr]);
  return Aet_Fault_None;
}

Aet_Fault aet_ram_write_byte(Aet_RAM *ram, u32 addr, byte value) {
  if (addr >= ram->cap) {
    return Aet_Fault_Invalid_Address;
  }

  ram->raw[addr] = value;
  return Aet_Fault_None;
}

Aet_Fault aet_ram_write_u16(Aet_RAM *ram, u32 addr, u16 value) {
  usize size = sizeof(u16);
  if (addr >= ram->cap || ram->cap - addr < size) {
    return Aet_Fault_Invalid_Address;
  }

  ram->raw[addr] = (byte)(value & 0xff);
  ram->raw[addr + 1] = (byte)((value >> 8) & 0xff);
  return Aet_Fault_None;
}

Aet_Fault aet_ram_write_u32(Aet_RAM *ram, u32 addr, u32 value) {
  usize size = sizeof(u32);
  if (addr >= ram->cap || ram->cap - addr < size) {
    return Aet_Fault_Invalid_Address;
  }

  ram->raw[addr] = (byte)(value & 0xff);
  ram->raw[addr + 1] = (byte)((value >> 8) & 0xff);
  ram->raw[addr + 2] = (byte)((value >> 16) & 0xff);
  ram->raw[addr + 3] = (byte)((value >> 24) & 0xff);
  return Aet_Fault_None;
}

Aet_Fault aet_identity_device_read_u32(rawptr data, u32 reg, u32 *out) {
  if (reg != 0) {
    return Aet_Fault_Invalid_Address;
  }

  if (data == nullptr) {
    return Aet_Fault_Invalid_MMIO_Operation;
  }

  Aet_Machine *machine = (Aet_Machine *)data;
  *out = (u32)machine->available_devices;
  return Aet_Fault_None;
}

// NOTE(nico): For now this device is read-only
Aet_Fault aet_identity_device_write_u32(rawptr data, u32 reg, u32 value) {
  (void)data;
  (void)reg;
  (void)value;
  return Aet_Fault_Invalid_MMIO_Operation;
}

f32 aet_u32_to_f32(u32 bits) {
  f32 out;
  memcpy(&out, &bits, sizeof(out));
  return out;
}

u32 aet_f32_to_u32(f32 value) {
  u32 out;
  memcpy(&out, &value, sizeof(out));
  return out;
}

bool32 aet_is_nan_f32(f32 f) {
  u32 bits;
  memcpy(&bits, &f, sizeof(bits));

  u32 exp = (bits >> 23) & 0xff;
  u32 frac = bits & 0x7fffff;

  return exp == 0xff && frac != 0;
}

// static bool32 aet_is_inf_f32(f32 f) {
//   u32 bits;
//   memcpy(&bits, &f, sizeof(bits));

//   u32 exp = (bits >> 23) & 0xff;
//   u32 frac = bits & 0x7fffff;

//   return exp == 0xff && frac == 0;
// }