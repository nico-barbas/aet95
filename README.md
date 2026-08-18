# Project Aet95

**"The fleet must grow, but you have no direct control"**

This project's goal is to make a programming game. The player is responsible of a fleet of machines in an alien planet.

They need to harvest resources, similarly to factory games. Raw materials are refined into other resources used to produce more advanced hardware and software. _Progression still need to be designed of course_

Each machine in the fleet will embed a aet-95 cpu following a simple but powerful ISA, limited resources, selected devices and a running program.

The player controls the design (combination of hardware and software). Examples (not all might make it, especially the duplication machine which might break a lot of things if not done carefully):

- A duplication machine -> more storage (physical and memory) and simple duplication program
- A networking machine -> high radio range, no motors, store data from other machines and broadcast information
- An explorer in early game -> low sensors, high ram, record map and useful information found

Player can poke devices via a MMIO. The fun of the game is the open-endedness of the design. Create specialized machines, find clever tricks. One interesting angle would be to bake intentional cpu bugs for the player to exploit. There would then be a chip revision system. This is seductive but very hard to design well.

The meta-progression will be unlocking better tech options. A compiler, an optimizer, a debugger, etc.. One apprehension I have is that the game would be too hard for beginners without a high-level programming language from the get-go. Historically, games like Turing complete and Zachtronics games have a really hard time attracting non-programmers. A problem that The Farmer was replaced does not have. It is fun for really skileld player to write assembly for a big chunk of the game. The better direction is to make it a hardmode (start with no compiler, first few hours are only aet95 assembly) and even honormode (reach a goal with never unlocking the compiler).

Early game machines might have a few Kib of ram and very low clock-speed.

## Physical environment

3D terrain. Mars-like for now, more exo-planet kind expansion angle if useful.

Based on a heightmap. There is no need for overhang so the source of truth can stay 2d. Lowering and raising is supported by this choice

## Hardware architecture

RISC-V like chip architecture

Default for operations with immediate is signed-extended. Separate operations for unsigned are suffixed with `u`

# The aet-95 ISA

32-bit, fixed 32-bit instruction width, 16 registers. This documents the target behavior and act as authority over the implementation.

## Registers

Registers are 32 bits. Registers are encoded as 4 bits in the instructions

| Encoding | Name       | Role                                                        |
| -------- | ---------- | ----------------------------------------------------------- |
| 0        | `rx0`      | Hardwired zero. Reads as 0, writes are discarded.           |
| 1        | `rx1`      | Return address. Written by `call`, read by `ret`.           |
| 2        | `rx2`      | Stack pointer. Convention only — no instruction touches it. |
| 3-15     | `r0`-`r12` | General purpose.                                            |

There is no dedicated flags register. Comparison happens inside the branch instructions.

## Instruction encoding

Four formats. The opcode always occupies the low 8 bits.

| Format | 31:20                | 19:16 | 15:12 | 11:8 | 7:0    |
| ------ | -------------------- | ----- | ----- | ---- | ------ |
| R      | _unused_             | `rs2` | `rs1` | `rd` | opcode |
| I      | `imm16` (bits 31:16) |       | `rs1` | `rd` | opcode |
| J      | `imm24` (bits 31:8)  |       |       |      | opcode |
| N      | _unused_             |       |       |      | opcode |

The `rd` field is not always a destination. It is the written register for arithmetic and loads, the **left comparison operand** for branches, and the **value being stored** for stores. Nothing is written to a register by branches, stores, or `jump`.

Both immediates are two's complement and sign-extended when decoded: `imm16` gives a range of -32768..32767, `imm24` gives -8388608..8388607. The assembler rejects anything outside those ranges.

`addi r0, rx0, 100` assembles to `0x00640300`:

```
0x0064 = 100     imm16   bits 31:16
0x0    = rx0     rs1     bits 15:12
0x3    = r0      rd      bits 11:8
0x00   = addi    opcode  bits 7:0
```

## Instruction reference

`rd`, `rs1`, `rs2` below are register _values_, except where noted. `sext` is sign extension, `zext` is zero extension.

### Arithmetic and logic

| Op  | Mnemonic              | Format | Effect                                                    |
| --- | --------------------- | ------ | --------------------------------------------------------- |
| 0   | `addi rd, rs1, imm`   | I      | `rd = rs1 + sext(imm)`                                    |
| 1   | `add rd, rs1, rs2`    | R      | `rd = rs1 + rs2`                                          |
| 2   | `sub rd, rs1, rs2`    | R      | `rd = rs1 - rs2`                                          |
| 3   | `mul rd, rs1, rs2`    | R      | `rd = low 32 bits of rs1 * rs2`                           |
| 4   | `div rd, rs1, rs2`    | R      | `rd = rs1 / rs2`, **signed**, truncated toward zero       |
| 5   | `divu rd, rs1, rs2`   | R      | `rd = rs1 / rs2`, **unsigned**                            |
| 6   | `and rd, rs1, rs2`    | R      | `rd = rs1 & rs2`                                          |
| 7   | `or rd, rs1, rs2`     | R      | `rd = rs1 \| rs2`                                         |
| 8   | `xor rd, rs1, rs2`    | R      | `rd = rs1 ^ rs2`                                          |
| 9   | `shiftl rd, rs1, rs2` | R      | `rd = rs1 << (rs2 & 31)`                                  |
| 10  | `shiftr rd, rs1, rs2` | R      | `rd = rs1 >> (rs2 & 31)`, **logical** — zeroes shifted in |

Wrapping is defined for `add`, `sub` and `mul`: results are truncated to 32 bits, no overflow fault. Shift amounts are masked to 5 bits, so a shift by 32 or more is a shift by `amount % 32` rather than undefined.

Division is the one place that faults instead of producing a value. Both `div` and `divu` raise `Divide_By_Zero` when `rs2` is zero. `div` additionally raises `Divide_Overflow` on `-2147483648 / -1`, the single input pair whose quotient has no signed 32-bit representation. `divu` cannot overflow.

### Memory

Effective address is always `rs1 + sext(imm16)`, so offsets can be negative.

| Op  | Mnemonic               | Format | Effect                          |
| --- | ---------------------- | ------ | ------------------------------- |
| 11  | `loadb rd, rs1, imm`   | I      | `rd = sext(RAM[addr])`, 1 byte  |
| 12  | `loadbu rd, rs1, imm`  | I      | `rd = zext(RAM[addr])`, 1 byte  |
| 13  | `loadh rd, rs1, imm`   | I      | `rd = sext(RAM[addr])`, 2 bytes |
| 14  | `loadhu rd, rs1, imm`  | I      | `rd = zext(RAM[addr])`, 2 bytes |
| 15  | `loadw rd, rs1, imm`   | I      | `rd = RAM[addr]`, 4 bytes       |
| 16  | `storeb rd, rs1, imm`  | I      | `RAM[addr] = low byte of rd`    |
| 17  | `storeh rd, rs1, imm`  | I      | `RAM[addr] = low 2 bytes of rd` |
| 18  | `storew rd, rs1, imm`  | I      | `RAM[addr] = rd`, 4 bytes       |

`loadw` needs no signed variant — it already fills the register. Stores do not either, since they only ever truncate.

Note the store operand order: `rd` is the source of the data and `rs1` is the base address, so `storew r0, r1, -4` writes `r0` to `r1 - 4`.

### Branches

All branch offsets are signed and measured **in instructions, relative to the branch itself**. `beq rd, rs1, 0` is an infinite loop; offset `1` is the following instruction, which is the same as not branching.

| Op  | Mnemonic             | Format | Taken when            |
| --- | -------------------- | ------ | --------------------- |
| 19  | `beq rd, rs1, off`   | I      | `rd == rs1`           |
| 20  | `bneq rd, rs1, off`  | I      | `rd != rs1`           |
| 21  | `blt rd, rs1, off`   | I      | `rd < rs1`, signed    |
| 22  | `bgeq rd, rs1, off`  | I      | `rd >= rs1`, signed   |
| 23  | `bltu rd, rs1, off`  | I      | `rd < rs1`, unsigned  |
| 24  | `bgequ rd, rs1, off` | I      | `rd >= rs1`, unsigned |

### Control flow

| Op  | Mnemonic   | Format | Effect                           |
| --- | ---------- | ------ | -------------------------------- |
| 25  | `jump off` | J      | `pc += off`                      |
| 26  | `call off` | J      | `rx1 = pc + 1`, then `pc += off` |
| 27  | `ret`      | N      | `pc = rx1`                       |

`call` computes the return address before branching and only commits it if the target is valid, so a faulting `call` leaves `rx1` untouched.

`ret` is unconditional and reads whatever `rx1` currently holds — there is no call stack and no check that a matching `call` ever happened. Since `rx1` starts at 0, a `ret` reached outside a call jumps to instruction 0 and restarts the program. Nesting calls requires saving `rx1` yourself.

## Memory model

**Program memory** is an array of 32-bit words, indexed directly by the program counter — `pc` counts instructions, not bytes. It is not addressable by loads and stores. There is currently no way for a program to read or modify its own code.

**Data RAM** is a flat, byte-addressed, zero-initialised buffer whose size is set per machine when the RAM is created. It is little-endian: `storew 0x11223344` at address 0 puts `0x44` at byte 0. There is no alignment requirement — a word access at an odd address is legal and costs the same.

Every access is bounds-checked against the machine's RAM size, including accesses that would straddle the end of the buffer. Anything out of range raises `Invalid_Memory_Access` and leaves RAM untouched.

## Execution model

`aet_cpu_execute` runs at most `budget` instructions and returns one of:

- **None** — the budget was not exhausted and the program ran to completion, meaning `pc` reached `program.len`.
- **Budget_Spent** — the budget ran out with the program still live. This is the normal path for a long-running program; call again to continue.
- **Fault** — execution stopped on a fault, with the reason in `cpu.fault`.

Every instruction costs exactly one cycle. There are no multi-cycle timings yet.

| Fault                          | Raised by                                                      |
| ------------------------------ | -------------------------------------------------------------- |
| `Invalid_Opcode`               | An opcode byte above the defined range                         |
| `Invalid_Next_Program_Counter` | A branch, jump, call or ret target past the end of the program |
| `Invalid_Memory_Access`        | A load or store outside the machine's RAM                      |
| `Divide_By_Zero`               | `div` or `divu` with a zero divisor                            |
| `Divide_Overflow`              | `div` computing `-2147483648 / -1`                             |

Termination is by falling off the end of the program: `pc` may legally reach `program.len` — by running past the last instruction, or by branching or returning to exactly that address — and that is a clean stop. Going beyond it faults. A finished CPU is idempotent: executing it again does nothing and returns None.

There is no halt instruction, and the `Running`/`Halted` flags in the CPU struct are not yet written or read.

## Assembly syntax

One instruction per line, operands separated by commas. Whitespace around operands is free-form. Blank lines and comment-only lines are ignored.

```
; count r0 down to zero, accumulating into r2
addi r0, rx0, 10        ; counter
addi r1, rx0, 1         ; step
add r2, r2, r1          ; body
sub r0, r0, r1
bneq r0, rx0, -2        ; back to the body
```

That program leaves `r2 = 10` and stops by falling off the end. Note that it does **not** end in `ret`: `ret` only ever jumps to whatever `rx1` holds, and outside a `call` that is 0, so a top-level `ret` restarts the program from instruction 0 instead of halting. Until there is a halt instruction, running off the end is the only way to stop.

- `;` starts a comment that runs to the end of the line, either on its own line or after an instruction.
- Integer literals are decimal, with an optional leading `-`. No `+` prefix, no hex or binary literals.
- Mnemonics are lowercase. Register names accept an uppercase `R` (`R0` works) but the `x` in the extended registers must be lowercase (`rx0`, not `RX0`).
- There are **no labels**. Branch, jump and call take a raw signed instruction offset, which you compute by hand.

The disassembler is the exact inverse: its output re-assembles to a byte-identical program.

## Known gaps

Deliberately deferred. Recorded so the current shape isn't mistaken for the intended one.

- **No labels**, which is by far the biggest obstacle to writing anything real by hand.
- **No way to materialise a full 32-bit constant.** `addi` caps at ±32767 and there is no load-upper-immediate, so large constants have to be built with shifts.
- **No immediate forms** of `sub`, `and`, `or`, `xor` or the shifts.
- **No remainder or modulo.** Both division forms discard it, so it has to be recovered with a multiply and a subtract.
- **No arithmetic shift right**, so sign-preserving division by powers of two is not expressible.
- **No halt instruction.** Falling off the end is the only clean stop, which makes a top-level `ret` an accidental restart rather than an exit. There is also no way to distinguish "finished" from "never started" beyond inspecting `pc`.
- **No MMIO region**, which is what the whole device story depends on.
- **No misalignment fault.** Whether unaligned access should be free, slow, or trapping is still open.

`shiftr` is now the last operation breaking the "unsigned variants are suffixed with `u`" rule stated above: it shifts logically but carries no `u`. Splitting it into `shiftr`/`shiftru` — arithmetic and logical — would settle the rule and close the missing arithmetic shift above in the same move.
