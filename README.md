# Fantasy CPU / OS

A small fantasy computer designed primarily as a learning project for compiler backends, code generation, JIT compilation, and low-level runtime architecture.

## Motivation and Goals

The project is built around a simple 32-bit RISC-like CPU emulator.

The intended stack is:

```text
Custom language
      ↓
Compiler / JIT
      ↓
Fantasy CPU machine code
      ↓
CPU emulator
      ↓
Fantasy OS
```

The low-level environment will include:

- A 32-bit RISC-inspired CPU emulator
- A custom machine-code instruction format
- An assembler
- A disassembler
- 512 MiB of byte-addressable RAM
- A simple ABI and calling convention
- Instruction-budgeted execution so the virtual CPU can coexist with the host application's main loop

The custom language will eventually target this CPU directly. The compiler side is intended to explore areas such as:

- SSA-based intermediate representations
- Control-flow graphs
- Optimization passes
- Code generation
- Register allocation
- JIT compilation
- GC/runtime integration
- Coroutine lowering

A fantasy OS will sit on top of the CPU and provide the runtime environment for programs.

The machine is intentionally kept simpler than real hardware. In particular, there is currently no plan to model MMIO, ROM mappings, or additional hardware devices. Host interaction can instead be exposed through a small trap/syscall-style interface where necessary.

## CPU Direction

The CPU is intended to resemble a small RISC-V-style architecture without attempting to implement RISC-V itself.

Current design direction:

- 32-bit registers
- 32-bit addresses
- Fixed-width instructions
- Load/store architecture
- Register-register and register-immediate operations
- Flat RAM
- Little-endian memory
- Simple control flow and function-call support

The CPU should remain language-agnostic. Concepts such as objects, GC, closures, coroutines, and language-level functions belong in generated code or the runtime rather than in the instruction set.

## Current Status

Implemented:

- CPU emulator foundation
- Arithmetic instructions
- Arithmetic instruction encoding in the assembler
- Arithmetic instruction decoding in the disassembler
- Arithmetic instruction execution in the CPU
- RAM module with 512 MiB of memory

The RAM implementation is not yet exercised through CPU load/store instructions.

## Near-Term Work

The next major steps are:

1. Add load/store instructions and connect the CPU to RAM.
2. Add integer comparison and branch instructions.
3. Add jumps and basic control flow.
4. Define the register conventions and ABI.
5. Add stack-based function calls.
6. Add traps/syscalls for minimal host interaction.
7. Begin compiling a small custom language directly to the fantasy machine code.

The immediate milestone is to be able to assemble, execute, and disassemble a small program that performs arithmetic, reads/writes RAM, branches, and calls functions.
