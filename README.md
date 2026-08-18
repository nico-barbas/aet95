# Project Aet95

**"The fleet must grow, but you have no direct control"**

This project's goal is to make a programming game. The player is responsible of a fleet of machines in an alien planet.

They need to harvest resources, similarly to factory games. Raw materials are refined into other resources used to produce more advanced hardware and software. _Progression still need to be designed of course_

Due to the high engineering skill ceiling, another comparison would be Kerbal Space program, with this game targeting embedded, low-level, and programming

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

Devices are register only. They do not own part of the address space for their own memory. This means that for high I/O devices, it will probably be important to implement DMA (Direct Memory Access) to allow those devices to write directly to the main RAM.

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

`loadui` is the one I-format instruction that ignores `rs1`. The field is reserved, the assembler always encodes it as zero, and the VM does not read it.

Immediates come in two kinds, fixed per instruction:

- **Signed.** Two's complement, sign-extended when decoded. `imm16` covers -32768..32767, `imm24` covers -8388608..8388607. This is every immediate except the two below.
- **Zero-extended.** The field holds a raw bit pattern rather than a number, so it is never sign-extended and its range is 0..65535. Only `ori` and `loadui` work this way.

The assembler rejects anything outside the range for the instruction being assembled, so `addi r0, rx0, 40000` and `ori r0, r0, -1` are both errors.

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
| 8   | `ori rd, rs1, imm`    | I      | `rd = rs1 \| zext(imm)`                                   |
| 9   | `xor rd, rs1, rs2`    | R      | `rd = rs1 ^ rs2`                                          |
| 10  | `shiftl rd, rs1, rs2` | R      | `rd = rs1 << (rs2 & 31)`                                  |
| 11  | `shiftr rd, rs1, rs2` | R      | `rd = rs1 >> (rs2 & 31)`, **logical** — zeroes shifted in |
| 12  | `loadui rd, imm`      | I      | `rd = imm << 16`, low 16 bits cleared                     |

Wrapping is defined for `add`, `sub` and `mul`: results are truncated to 32 bits, no overflow fault. Shift amounts are masked to 5 bits, so a shift by 32 or more is a shift by `amount % 32` rather than undefined.

Division is the one place that faults instead of producing a value. Both `div` and `divu` raise `Divide_By_Zero` when `rs2` is zero. `div` additionally raises `Divide_Overflow` on `-2147483648 / -1`, the single input pair whose quotient has no signed 32-bit representation. `divu` cannot overflow.

Despite the name, `loadui` touches no memory — it is listed here rather than under Memory because it only materialises a constant. The `u` is for **upper**, not unsigned; it is the one place in the ISA where a trailing `u` does not mean an unsigned operand or result.

### Materialising constants

`ori` and `loadui` are the pair that reaches the full 32-bit space. `loadui` places a bit pattern in the upper half and clears the lower; `ori` merges a pattern into the lower half without disturbing the upper:

```
loadui r0, 0xdead       ; r0 = 0xdead0000
ori    r0, r0, 0xbeef   ; r0 = 0xdeadbeef
```

Both immediates are bit patterns, so the two halves compose directly and no correction is needed between them.

Using `addi` for the lower half instead does **not** work in general, because `addi` sign-extends. For any constant whose bit 15 is set, the sign extension borrows from the upper half: `loadui r0, 0xdead` followed by `addi r0, r0, -16657` — the only way to spell `0xbeef` in a signed field — yields `0xdeacbeef`, one short in the upper half. Reaching such a constant with `addi` means pre-biasing the upper half by `0x8000` to cancel the borrow. `ori` exists so that you do not have to.

This is also what makes the whole address space reachable. Without it, only the 64 KiB window addressable as `rx0 + sext(imm16)` — the bottom and top 32 KiB — could be named without a multi-instruction shift sequence.

### Memory

Effective address is always `rs1 + sext(imm16)`, so offsets can be negative.

| Op  | Mnemonic              | Format | Effect                          |
| --- | --------------------- | ------ | ------------------------------- |
| 13  | `loadb rd, rs1, imm`  | I      | `rd = sext(RAM[addr])`, 1 byte  |
| 14  | `loadbu rd, rs1, imm` | I      | `rd = zext(RAM[addr])`, 1 byte  |
| 15  | `loadh rd, rs1, imm`  | I      | `rd = sext(RAM[addr])`, 2 bytes |
| 16  | `loadhu rd, rs1, imm` | I      | `rd = zext(RAM[addr])`, 2 bytes |
| 17  | `loadw rd, rs1, imm`  | I      | `rd = RAM[addr]`, 4 bytes       |
| 18  | `storeb rd, rs1, imm` | I      | `RAM[addr] = low byte of rd`    |
| 19  | `storeh rd, rs1, imm` | I      | `RAM[addr] = low 2 bytes of rd` |
| 20  | `storew rd, rs1, imm` | I      | `RAM[addr] = rd`, 4 bytes       |

`loadw` needs no signed variant — it already fills the register. Stores do not either, since they only ever truncate.

Note the store operand order: `rd` is the source of the data and `rs1` is the base address, so `storew r0, r1, -4` writes `r0` to `r1 - 4`.

### Branches

All branch offsets are signed and measured **in instructions, relative to the branch itself**. `beq rd, rs1, 0` is an infinite loop; offset `1` is the following instruction, which is the same as not branching.

| Op  | Mnemonic             | Format | Taken when            |
| --- | -------------------- | ------ | --------------------- |
| 21  | `beq rd, rs1, off`   | I      | `rd == rs1`           |
| 22  | `bneq rd, rs1, off`  | I      | `rd != rs1`           |
| 23  | `blt rd, rs1, off`   | I      | `rd < rs1`, signed    |
| 24  | `bgeq rd, rs1, off`  | I      | `rd >= rs1`, signed   |
| 25  | `bltu rd, rs1, off`  | I      | `rd < rs1`, unsigned  |
| 26  | `bgequ rd, rs1, off` | I      | `rd >= rs1`, unsigned |

The `u` suffix selects an unsigned **comparison**. The branch offset itself is a signed displacement in every case.

### Control flow

| Op  | Mnemonic   | Format | Effect                           |
| --- | ---------- | ------ | -------------------------------- |
| 27  | `jump off` | J      | `pc += off`                      |
| 28  | `call off` | J      | `rx1 = pc + 1`, then `pc += off` |
| 29  | `ret`      | N      | `pc = rx1`                       |

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
- Integer literals are decimal by default, or hexadecimal with a `0x` prefix (`0X` is accepted too). Either form takes an optional leading `-`, so `-0x10` is -16. No `+` prefix and no binary literals.
- **Hex digits must be lowercase.** `0xbeef` assembles, `0xBEEF` is rejected. The prefix is the only part where case is free.
- A literal with two `0x` prefixes, like `0x0x1`, is a `Malformed_Number`; a bare `0x` with no digits is an invalid immediate.
- Mnemonics are lowercase. Register names accept an uppercase `R` (`R0` works) but the `x` in the extended registers must be lowercase (`rx0`, not `RX0`).
- There are **no labels**. Branch, jump and call take a raw signed instruction offset, which you compute by hand.

The disassembler is the exact inverse: its output re-assembles to a byte-identical program.

## Known gaps

Deliberately deferred. Recorded so the current shape isn't mistaken for the intended one.

- **No labels**, which is by far the biggest obstacle to writing anything real by hand.
- **No uppercase hex digits.** `0xBEEF` is rejected; only `a`-`f` are accepted. A lexer limitation, not a deliberate choice.
- **No immediate forms** of `sub`, `and`, `xor` or the shifts. `or` has one — `ori` — because constant materialisation needs it; the others have no such forcing reason yet.
- **No remainder or modulo.** Both division forms discard it, so it has to be recovered with a multiply and a subtract.
- **No arithmetic shift right**, so sign-preserving division by powers of two is not expressible.
- **No halt instruction.** Falling off the end is the only clean stop, which makes a top-level `ret` an accidental restart rather than an exit. There is also no way to distinguish "finished" from "never started" beyond inspecting `pc`.
- **No MMIO region**, which is what the whole device story depends on.
- **No misalignment fault.** Whether unaligned access should be free, slow, or trapping is still open.

Two mnemonics still bend the "unsigned variants are suffixed with `u`" rule stated above. `shiftr` shifts logically but carries no `u`; splitting it into `shiftr`/`shiftru` — arithmetic and logical — would settle the rule and close the missing arithmetic shift above in the same move. `loadui` carries a `u` that means **upper** rather than unsigned, which is the only overloaded use of the suffix in the ISA.
