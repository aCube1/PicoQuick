# PQP(Pico Quick Processor) Interpreter

## Project building

- Requirements:
    - OS: Linux or Windows
    - Compiler: GCC or Clang (DO NOT USE MSVC!)
    - Toolchains: CMake and Make/Ninja

> To compile project, use these commands:

```sh
$ cmake -B build
$ cmake --build build
$ ./build/bin/pico data/input.txt
```

## Architecture Specification

- Arch: 32-bits, little-endian
- Registers:
    - 16 32-bits general purpose registers, named r0 to r15
    - 32-bits program counter
    - 3-bit flag register: Greater, Less, Equal; Only set/cleared by `cmp`
- Memory:
    - Size: 256-bytes, Von Neumann architecture (Instruction are stored in RAM)
    - If PC reaches address `0xf0f0`, the interpreter immediatelly interrupts,
      and no trace is registered on the output aside from text: `"EXIT"`

## Instruction Encoding

| Instruction    | Opcode |  Data0   |  Data1   |   Data2    | Operation                           |
| -------------- | :----: | :------: | :------: | :--------: | :---------------------------------- |
| `mov rx, i16`  | `0x00` | rx (3-0) |    --    | imm (15-0) | `rx = i16`                          |
| `mov rx, ry`   | `0x01` | rx (3-0) | ry (3-0) |     --     | `rx = ry`                           |
| `mov rx, [ry]` | `0x02` | rx (3-0) | ry (3-0) |     --     | `rx = MEM[ry]`                      |
| `mov [rx], ry` | `0x03` | rx (3-0) | ry (3-0) |     --     | `MEM[rx] = ry`                      |
| `cmp rx, ry`   | `0x04` | rx (3-0) | ry (3-0) |     --     | `rx <-> ry (Compare and set flags)` |
| `jmp i16`      | `0x05` |    --    |    --    | imm (15-0) | `pc += 4 + i16`                     |
| `jg i16`       | `0x06` |    --    |    --    | imm (15-0) | `if g=1; pc += 4 + i16`             |
| `jl i16`       | `0x07` |    --    |    --    | imm (15-0) | `if l=1; pc += 4 + i16`             |
| `je i16`       | `0x08` |    --    |    --    | imm (15-0) | `if e=1; pc += 4 + i16`             |
| `add rx, ry`   | `0x09` | rx (3-0) | ry (3-0) |     --     | `rx += ry`                          |
| `sub rx, ry`   | `0x0a` | rx (3-0) | ry (3-0) |     --     | `rx -= ry`                          |
| `and rx, ry`   | `0x0b` | rx (3-0) | ry (3-0) |     --     | `rx &= ry`                          |
| `or rx, ry`    | `0x0c` | rx (3-0) | ry (3-0) |     --     | `rx \|= ry`                         |
| `xor rx, ry`   | `0x0d` | rx (3-0) | ry (3-0) |     --     | `rx ^= ry`                          |
| `sal rx, i5`   | `0x0e` | rx (3-0) |    --    | imm (4-0)  | `rx <<= i5`                         |
| `sar rx, i5`   | `0x0f` | rx (3-0) |    --    | imm (4-0)  | `rx >>= i5`                         |

> Symbols:
> `rx,ry` -> General Purpose Register `r0-r15` <br>
> `[rx],[ry]` -> Indirect memory access `[r0-r15]` <br>
> `i16` -> Immediate signed 16-bit value <br>
> `i5` -> Immediate unsinged 5-bit value <br>
