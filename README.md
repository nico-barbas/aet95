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

## Fantasy

### Very early game (first 15 minutes)

"I start a new game with 1-2 machines with 2 devices: motor and navigation. I need to gather resources: iron, copper. As the player, I see that there is some copper at coordinate [20, 16]. I open the code editor and starts a new design. I write the begining of a program to load the coordinate as hard-coded values into the navigation device dedicated registers. Then I write the movement routine: poll two other navigation registers to get the machine's absolute coordinate, then write to the motors registers, one for the x velocity, one for the y velocity (signed, the speed of the motor is not controllable). Once arrived at a close enough distance, stop and gather."

## Physical environment

3D terrain. Mars-like for now, more exo-planet kind expansion angle if useful.

Based on a heightmap. There is no need for overhang so the source of truth can stay 2d. Lowering and raising is supported by this choice

## Hardware architecture

RISC-V like chip architecture

Default for operations with immediate is signed-extended. Separate operations for unsigned are suffixed with `u`

Devices are register only. Each one owns a small fixed page of registers at the top of the address space, but no bulk memory of its own. This means that for high I/O devices, it will probably be important to implement DMA (Direct Memory Access) to allow those devices to write directly to the main RAM.

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

The assembler rejects anything outside the range for the instruction being assembled, so `addi r0, rx0, 40000` and `ori r0, r0, -1` are both errors. A displacement computed from a label goes through the same check.

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

Effective address is always `rs1 + sext(imm16)`, so offsets can be negative. `MEM` below is the memory bus, not the RAM buffer — the address decides whether the access lands in RAM or in a device register. See [Address space](#address-space).

| Op  | Mnemonic              | Format | Effect                          |
| --- | --------------------- | ------ | ------------------------------- |
| 13  | `loadb rd, rs1, imm`  | I      | `rd = sext(MEM[addr])`, 1 byte  |
| 14  | `loadbu rd, rs1, imm` | I      | `rd = zext(MEM[addr])`, 1 byte  |
| 15  | `loadh rd, rs1, imm`  | I      | `rd = sext(MEM[addr])`, 2 bytes |
| 16  | `loadhu rd, rs1, imm` | I      | `rd = zext(MEM[addr])`, 2 bytes |
| 17  | `loadw rd, rs1, imm`  | I      | `rd = MEM[addr]`, 4 bytes       |
| 18  | `storeb rd, rs1, imm` | I      | `MEM[addr] = low byte of rd`    |
| 19  | `storeh rd, rs1, imm` | I      | `MEM[addr] = low 2 bytes of rd` |
| 20  | `storew rd, rs1, imm` | I      | `MEM[addr] = rd`, 4 bytes       |

`loadw` needs no signed variant — it already fills the register. Stores do not either, since they only ever truncate.

Note the store operand order: `rd` is the source of the data and `rs1` is the base address, so `storew r0, r1, -4` writes `r0` to `r1 - 4`.

`loadw` and `storew` are also the only two instructions that can reach a device register. The six sub-word forms are RAM-only.

### Branches

All branch offsets are signed and measured **in instructions, relative to the branch itself**. `beq rd, rs1, 0` is an infinite loop; offset `1` is the following instruction, which is the same as not branching. Note that this differs from MIPS and RISC-V, where the displacement is relative to the _following_ instruction. A [label](#labels) written in this position resolves to exactly this displacement.

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

### Address space

Loads and stores go through the **memory bus**, which decodes the address into one of three regions:

| Range                              | Contents                      |
| ---------------------------------- | ----------------------------- |
| `0x00000000` .. `ram_byte_cap - 1` | Data RAM                      |
| `ram_byte_cap` .. `0xffff7fff`     | Unmapped                      |
| `0xffff8000` .. `0xffffffff`       | MMIO — 32 KiB of device pages |

The split is fixed: the MMIO window is always the top 32 KiB, whatever the machine's RAM size. RAM can therefore never grow past `0xffff8000`, and a machine asking for more fails to initialise rather than silently overlapping the device window.

The unmapped hole is not an error condition in itself, just address space no one has claimed. Touching it raises `Invalid_Address` exactly like running off the end of RAM.

That base address is chosen so the whole device window is reachable in one instruction. `rx0 + sext(imm16)` covers `0xffff8000`..`0xffffffff` with a negative immediate, so `loadw r0, rx0, -32768` reads the first device register without materialising an address first.

### Data RAM

Flat, byte-addressed, zero-initialised, its size set per machine at creation. Little-endian: `storew 0x11223344` at address 0 puts `0x44` at byte 0. There is no alignment requirement — a word access at an odd address is legal and costs the same.

Every access is bounds-checked against the machine's RAM size, including accesses that would straddle the end of the buffer. Anything out of range raises `Invalid_Address` and leaves RAM untouched.

### MMIO

The device window is divided into fixed **64-byte pages**, one per device slot. A page holds up to 16 word-sized registers: register _n_ lives at `page_base + n * 4`, and that byte offset within the page — not the index — is what the device is handed.

A slot number _is_ a device class, so a device's address is a property of what it is, not of how the machine was assembled. A program never has to discover where its motor lives; it only has to ask whether one is present.

| Slot | Class        | Page base    | As `imm16` from `rx0` |
| ---- | ------------ | ------------ | --------------------- |
| 0    | `Identity`   | `0xffff8000` | -32768                |
| 1    | `Navigation` | `0xffff8040` | -32704                |
| 2    | `Motor`      | `0xffff8080` | -32640                |
| 3    | `Sensor`     | `0xffff80c0` | -32576                |

Slots past the last defined class are decoded but unclaimed, so most of the 32 KiB window currently faults. The region is sized for growth, not because 512 device classes are planned.

Device access is deliberately narrower than RAM access:

- **Word only.** `loadw` and `storew` are the only instructions that reach a device. The six sub-word forms raise `Invalid_MMIO_Operation` anywhere in the window — a device register is a register, not a byte range to slice.
- **Aligned only.** The address must be a multiple of 4, otherwise `Misaligned_Address`. This is the one place the ISA enforces alignment; RAM stays free-form.
- **Present only.** A slot with no defined class, or one whose device the machine does not carry, raises `Invalid_Address`.

Only once all three hold does the device itself see the access, and it may still refuse — an unimplemented register or a write to a read-only one raises a fault of the device's choosing.

## Devices

A machine carries at most one device per class, fixed at creation. `Navigation` and `Motor` are specified below but not implemented yet; `Sensor` is a reserved slot with nothing behind it.

### Identity — slot 0

The one device every machine has. It cannot be omitted, and the bus wires it in whether or not the machine's spec asked for it, so a program can always count on slot 0 answering.

| Reg | Address      | Access | Meaning                |
| --- | ------------ | ------ | ---------------------- |
| 0   | `0xffff8000` | R      | Device presence bitset |

Bit _n_ of the presence bitset is set when the machine carries a device in slot _n_, so bit 0 always reads as 1. This is the entry point to the whole device story: read it once at startup and branch on what the chassis actually has, rather than hardcoding a machine layout into the program.

```
loadw r0, rx0, -32768   ; r0 = presence bitset, from 0xffff8000
addi  r1, rx0, 4        ; bit 2 = Motor
and   r2, r0, r1
beq   r2, rx0, 3        ; no motor on this machine — skip the drive routine
```

Every other register on the page raises `Invalid_Address`, and the device is read-only — any `storew` to it raises `Invalid_MMIO_Operation`.

### Navigation — slot 1

Where the machine is, and where it has been told to go. Position is a register read in whole cell coordinates, signed and exact — a program never has to derive it from how long it has been driving.

| Reg | Address      | Access | Meaning                   |
| --- | ------------ | ------ | ------------------------- |
| 0   | `0xffff8040` | R      | Status bitset             |
| 1   | `0xffff8044` | R      | Current X, in cells       |
| 2   | `0xffff8048` | R      | Current Y, in cells       |
| 3   | `0xffff804c` | R/W    | Target X, in cells        |
| 4   | `0xffff8050` | R/W    | Target Y, in cells        |
| 5   | `0xffff8054` | R      | Distance to target, cells |

The target lives on the device rather than in RAM so that register 5 can exist: distance needs a square root and the ISA has none, so it is the one spatial quantity a program cannot work out for itself. Everything else is a `sub` away and is left to the program.

| Bit | Name                   | Meaning                                                 |
| --- | ---------------------- | ------------------------------------------------------- |
| 0   | `signal_valid`         | Position registers are trustworthy. Always set for now. |
| 1   | `target_set`           | A target has been written since boot                    |
| 2   | `target_out_of_bounds` | Target names a cell outside the world                   |

Distance reads 0 while `target_set` is clear, which is the same value it reads on arrival — that ambiguity is exactly what the bit is for. A program that polls distance without ever writing a target sits still and looks broken; the status register is how it finds out why.

Registers 0, 1, 2 and 5 are read-only and raise `Invalid_MMIO_Operation` on a `storew`. Every other register on the page raises `Invalid_Address`.

### Motor — slot 2

Actuation and nothing else. The motor is handed a velocity per world axis and applies it; it knows nothing about position or destination.

| Reg | Address      | Access | Meaning              |
| --- | ------------ | ------ | -------------------- |
| 0   | `0xffff8080` | R      | Status bitset        |
| 1   | `0xffff8084` | R/W    | Velocity X, signed   |
| 2   | `0xffff8088` | R/W    | Velocity Y, signed   |
| 3   | `0xffff808c` | R      | Speed, hardware stat |

Velocity is per axis rather than a heading and a throttle: the machine is holonomic and moves in any direction without turning to face it.

Only the **sign** of a velocity register is read. Negative drives one way, positive the other, zero stops, and the magnitude is discarded — the machine always travels at the rated speed in register 3, which a program can read but not change. Nothing therefore has to be scaled: storing `target - current` straight into a velocity register is a complete drive command, and since position is counted in whole cells that difference reaches exactly zero in the destination cell, so the machine stops itself on arrival.

Both registers are latched and persist across ticks until overwritten, including while blocked. Nothing clears them, so a program that wants to stop anywhere other than its target writes zeroes itself.

| Bit | Name      | Meaning                                           |
| --- | --------- | ------------------------------------------------- |
| 0   | `moving`  | Velocity is non-zero and the world is allowing it |
| 1   | `blocked` | Terrain refused the commanded direction           |

`blocked` is the one a drive loop has to check. A machine held against impassable terrain never closes the gap to its target, so a routine that only watches distance spins forever.

Registers 0 and 3 are read-only and raise `Invalid_MMIO_Operation` on a `storew`. Every other register on the page raises `Invalid_Address`.

A worked example of the two devices driving a machine to a named cell is in [docs/examples/drive-to-target.asm](docs/examples/drive-to-target.asm).

## Machine configuration

A machine is specified by its RAM size and its device set. Creation fails, rather than degrading, when the spec does not hold together:

| Error                          | Cause                                                       |
| ------------------------------ | ----------------------------------------------------------- |
| `Failed_To_Initialize_RAM`     | `ram_byte_cap` above `0xffff8000`, or the allocation failed |
| `Failed_To_Initialize_Devices` | A slot marked present supplies no read or no write handler  |
| `Failed_To_Initialize_CPU`     | The CPU could not be brought up                             |

Slot 0 is exempt from the handler check — the bus installs its own `Identity` device and overwrites whatever the spec put there.

Clock speed and ISA revision belong in this spec too, and are not there yet.

## Execution model

`aet_machine_run` runs at most `budget` instructions against the machine's bus and returns one of:

- **None** — the budget was not exhausted and the program ran to completion, meaning `pc` reached `program.len`.
- **Budget_Spent** — the budget ran out with the program still live. This is the normal path for a long-running program; call again to continue.
- **Fault** — execution stopped on a fault, with the reason in `cpu.fault`.

Every instruction costs exactly one cycle. There are no multi-cycle timings yet.

| Fault                          | Raised by                                                         |
| ------------------------------ | ----------------------------------------------------------------- |
| `Invalid_Opcode`               | An opcode byte above the defined range                            |
| `Invalid_Next_Program_Counter` | A branch, jump, call or ret target past the end of the program    |
| `Invalid_Address`              | An access outside RAM, or to an absent or undefined device slot   |
| `Invalid_MMIO_Operation`       | A sub-word access to the device window, or a rejected device poke |
| `Misaligned_Address`           | A device access not on a 4-byte boundary                          |
| `Divide_By_Zero`               | `div` or `divu` with a zero divisor                               |
| `Divide_Overflow`              | `div` computing `-2147483648 / -1`                                |

The three memory faults are checked in that order for a device access: shape first (word-sized, then aligned), then whether anything answers at that address, then the device's own verdict. A sub-word load from an empty slot reports `Invalid_MMIO_Operation`, not `Invalid_Address` — the access was malformed before the question of what lives there arose.

Termination is by falling off the end of the program: `pc` may legally reach `program.len` — by running past the last instruction, or by branching or returning to exactly that address — and that is a clean stop. Going beyond it faults. A finished CPU is idempotent: executing it again does nothing and returns None.

There is no halt instruction, and the `Running`/`Halted` flags in the CPU struct are not yet written or read.

## Assembly syntax

One instruction per line, operands separated by commas. Whitespace around operands is free-form. Blank lines and comment-only lines are ignored.

```
; count r0 down to zero, accumulating into r2
addi r0, rx0, 10        ; counter
addi r1, rx0, 1         ; step
body:
add r2, r2, r1
sub r0, r0, r1
bneq r0, rx0, body      ; back to the body
```

That program leaves `r2 = 10` and stops by falling off the end. Note that it does **not** end in `ret`: `ret` only ever jumps to whatever `rx1` holds, and outside a `call` that is 0, so a top-level `ret` restarts the program from instruction 0 instead of halting. Until there is a halt instruction, running off the end is the only way to stop.

- `;` starts a comment that runs to the end of the line, either on its own line or after an instruction.
- Integer literals are decimal by default, or hexadecimal with a `0x` prefix (`0X` is accepted too). Either form takes an optional leading `-`, so `-0x10` is -16. No `+` prefix and no binary literals.
- **Hex digits must be lowercase.** `0xbeef` assembles, `0xBEEF` is rejected. The prefix is the only part where case is free.
- A literal with two `0x` prefixes, like `0x0x1`, is a `Malformed_Number`; a bare `0x` with no digits is an invalid immediate.
- Mnemonics are lowercase. Register names accept an uppercase `R` (`R0` works) but the `x` in the extended registers must be lowercase (`rx0`, not `RX0`).
- Branch, jump and call take either a [label](#labels) or a raw signed instruction offset.

The disassembler is the exact inverse: its output re-assembles to a byte-identical program. It does not reconstruct labels — every offset comes back as a number, since nothing in the encoded program records that a name was ever there.

### Labels

A label names an instruction index. Define one with an identifier followed by a colon, either on its own line or in front of an instruction:

```
loop:
add r0, r1, r2
```

```
loop: add r0, r1, r2
```

Both spellings mark the same index. A label occupies no space of its own, it marks the position of whatever instruction comes next. Several labels may share a line or an index, and `a: b: jump a` is legal.

Use one wherever a branch, jump or call takes its offset. Nothing else accepts one:

| Accepts a label                          | Displacement must fit |
| ---------------------------------------- | --------------------- |
| `beq` `bneq` `blt` `bgeq` `bltu` `bgequ` | -32768..32767         |
| `jump` `call`                            | -8388608..8388607     |

The displacement is resolved exactly as the [branch](#branches) encoding defines it, `target - pc` counted in instructions from the branch itself, and is then range-checked like any other immediate. A label too far away is an `Invalid_Immediate_Value` — the assembler does not relax the branch into a longer sequence.

Every other immediate rejects a label with `Invalid_Syntax`, including the load and store offsets. This is not an oversight: a label names a **program** index, and program memory is not addressable (see [Memory model](#memory-model)), so there is nothing for a data label to point at and no data section to declare it in.

Naming rules follow the identifier lexer:

- Letters and digits only, and the first character must be a letter. `l0` is a label, `0l` and `my_loop` are not — there are no underscores.
- Case-sensitive. `Loop` and `loop` are different labels, and referring to one by the other's spelling is an `Unknown_Symbol`.
- A label may not be spelled like a mnemonic or a register. `add:` and `r0:` are both `Invalid_Syntax`, because the lexer resolves those spellings to instructions and registers before it ever considers an identifier.

Forward references work: labels are collected in full before any instruction is encoded, so a branch may name a label defined later in the file. Defining the same label twice is a `Duplicate_Symbol`; naming one that was never defined is an `Unknown_Symbol`.

A label placed after the last instruction names the address one past the end, which is precisely the clean-stop address (see [Execution model](#execution-model)). Jumping to it is a structured exit:

```
addi r0, rx0, 7
jump done
addi r0, rx0, 99        ; skipped
done:
```

That program stops with `r0 = 7` and no fault.

## Known gaps

Deliberately deferred. Recorded so the current shape isn't mistaken for the intended one.

- **No branch relaxation.** A label out of reach of its branch is rejected rather than rewritten into an inverted branch over a `jump`, the way GNU assembler would. The ranges are far past anything these machines can hold for now, so the check reads more as an assertion on the assembler's own arithmetic than as a limit a program will meet. It is in place in case it ever needs to grow.
- **No data labels**, and nothing for them to name — no `.data` section, no way to reserve or initialise RAM from source. Constants have to be materialised with `loadui`/`ori` and stored by hand.
- **No uppercase hex digits.** `0xBEEF` is rejected; only `a`-`f` are accepted. A lexer limitation, not a deliberate choice.
- **No immediate forms** of `sub`, `and`, `xor` or the shifts. `or` has one — `ori` — because constant materialisation needs it; the others have no such forcing reason yet.
- **No remainder or modulo.** Both division forms discard it, so it has to be recovered with a multiply and a subtract.
- **No arithmetic shift right**, so sign-preserving division by powers of two is not expressible.
- **No halt instruction.** Falling off the end is the only clean stop. A label after the last instruction at least gives that exit a name, but it is still a jump to the end rather than a stop, and a top-level `ret` remains an accidental restart. There is also no way to distinguish "finished" from "never started" beyond inspecting `pc`.
- **Only one real device.** `Identity` exists to exercise the bus end to end. `Navigation` and `Motor` are specified but unimplemented, and the slot numbering above does not match the `AET_DEVICE_CLASS` list in `hal.h` yet. `Sensor` has neither a spec nor an implementation.
- **No world model.** `Navigation` and `Motor` are written against cells, world bounds and impassable terrain, none of which are specified here or implemented. The heightmap under [Physical environment](#physical-environment) is the intended source of truth, but cell size, the mapping from height to traversability, and what makes a cell refuse a machine are all still open.
- **No DMA.** Devices move data one word at a time through the CPU, which is fine for a motor and hopeless for anything with real I/O volume.
- **No interrupts.** `Aet_CPU_Trap` has a single `None` member and nothing raises it, so devices cannot notify a program — polling is the only option.
- **Devices are fixed at machine creation.** Nothing can be attached or removed while the machine runs.
- **Misalignment traps in MMIO only.** RAM still accepts unaligned access at no cost, so the ISA now answers the alignment question two different ways depending on the address. Whether unaligned RAM access should stay free, become slow, or start trapping is still open.
- **Device pages are uniform.** Every device gets the same 64 bytes whether it needs 1 register or 16, and there is no way for a device to claim a second page.

Two mnemonics still bend the "unsigned variants are suffixed with `u`" rule stated above. `shiftr` shifts logically but carries no `u`; splitting it into `shiftr`/`shiftru` — arithmetic and logical — would settle the rule and close the missing arithmetic shift above in the same move. `loadui` carries a `u` that means **upper** rather than unsigned, which is the only overloaded use of the suffix in the ISA.
