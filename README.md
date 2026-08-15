# Project Aet95

This project's goal is to make a programming game. The player is responsible of a fleet of machines in an alien planet (Mars-like for now, expansion angle if useful).

They need to harvest resources, similarly to factory games. As a comparison the hook of factorio is "the factory must grow", here it would be "the fleet must grow". The difference is there is no direct control.

Each machine in the fleet will embed a aet-95 cpu following a simple but powerful ISA, limited resources, selected devices and a running program.

The player controls the design (combination of hardware and software). Examples (not all might make it, especially the duplication machine which might break a lot of things if not done carefully):

- A duplication machine -> more storage (physical and memory) and simple duplication program
- A networking machine -> high radio range, no motors, store data from other machines and broadcast information
- An explorer in early game -> low sensors, high ram, record map and useful information found

Player can poke devices via a MMIO. The fun of the game is the open-endedness of the design. Create specialized machines, find clever tricks, exploit cpu bugs (could even be intentional with some sort of software release system)

The meta-progression will be unlocking better tech. A compiler, an optimizer, more registers, etc.. One apprehension I have is that the game would be too hard without a high-level programming language from the get-go. This could be a hardmode (start with no compiler, first few hours are only aet95 assembly) and even honormode (reach goal with never unlocking the compiler).

There will be a debugger for the player to use.

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
Fleet/Factory game
```

The low-level environment will include:

- A 32-bit RISC-inspired CPU emulator
- A custom machine-code instruction format
- An assembler
- A disassembler
- byte-addressable RAM
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
