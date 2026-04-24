# nanasm / nanvm

A custom bytecode assembler and virtual machine written in C++.  
Write assembly in `.ns` files, compile to `.nb` bytecode, run with the VM.

---
## Platform support

| Platform | Status |
|---|---|
| Windows | ✅ Tested |
| Linux | ⚠️ Not tested |
| macOS | ⚠️ Not tested |
---

## Getting started

### Compile a source file
```sh
nanasm <path/to/file.ns>
```

### Compile and run immediately
```sh
nanasm <path/to/file.ns> -rn
```

### Run a pre-compiled bytecode file
```sh
nanvm <path/to/file.nb>
```


## Assembly syntax

Source files use the `.ns` extension. Comments start with `//`.

### Data section

Declare named data in the data segment:

```asm
data arr     [1, 2, 3, 4, 5]   // byte array
data hellow  "hellow, world!"  // string
data file    "file.txt"        // string (e.g. file path)
```

Reference data by name using `&name`.

---

### Labels

Labels mark positions in code. The assembler resolves label addresses at compile time.

```asm
entry:          // program entry point
my_function:
    ret
```

---

### Push instructions

| Instruction | Description |
|---|---|
| `pushn <value>` | Push a number, register, or memory address |
| `pushm &name, <offset>` | Push memory block by named address and offset |
| `pushb <byte>` | Push a single byte |
| `pushr <reg>` | Push a register value |
| `push <arg>, <offset>` | Push with explicit stack offset (offset is negative) |

```asm
pushn 123
pushn rx0
pushm &arr, 0x20
pushb 0x01
pushr r1
push  fx0, 0x20
```

---

### Pop instructions

| Instruction | Description |
|---|---|
| `pop` | Pop top value off the stack |
| `popr <reg>` | Pop top value into a register |

```asm
pop
popr r1
```

---

### Call instructions

| Instruction | Description |
|---|---|
| `call <label>` | Call a function (jump to label, push return address) |
| `calle <name>` | Call an external/imported function |
| `ret` | Return from function |

```asm
call  my_function
calle ExternalFunc
ret
```

---

### Registers

The VM has five register banks:

| Bank | Registers | Description |
|---|---|---|
| `r` | r1–r5 | General purpose (64-bit int) |
| `rx` | rx1–rx5 | Extended general purpose |
| `dx` | dx1–dx5 | Double / 64-bit |
| `fx` | fx1–fx5 | Float / special purpose |
| `rdi` | rdi, rdi0, rdi4, rdi8… | Stack frame pointer + offsets |

---

### Math instructions

All math instructions operate on two arguments: destination and source.

| Instruction | Description |
|---|---|
| `add <dst>, <src>` | Addition |
| `sub <dst>, <src>` | Subtraction |
| `mul <dst>, <src>` | Multiplication |
| `div <dst>, <src>` | Division |
| `inc <dst>` | Increment by 1 |
| `dec <dst>` | Decrement by 1 |
| `not <dst>` | Bitwise NOT |
| `xor <dst>, <src>` | Bitwise XOR |
| `or  <dst>, <src>` | Bitwise OR |
| `and <dst>, <src>` | Bitwise AND |
| `ls  <dst>, <n>` | Left shift by n |
| `rs  <dst>, <n>` | Right shift by n |

```asm
add rx0, rdi
sub rx1, rdi4
mul rx2, 5
div rx3, fx1
inc dx0
dec dx2
not r5
xor r5, 0xff
or  r2, r3
and r4, r1
ls  r1, 2
rs  r2, 3
```

---

### Logic and control flow

| Instruction | Description |
|---|---|
| `jmp <label>` | Unconditional jump |
| `exit` | Exit the program |
| `test` | Compare top two stack values, set flags |
| `je  <label>` | Jump if equal |
| `jel <label>` | Jump if equal or less |
| `jem <label>` | Jump if equal or more |
| `jne <label>` | Jump if not equal |
| `jl  <label>` | Jump if less |
| `jm  <label>` | Jump if more |
| `mov <dst>, <src>` | Move value |
| `swap <a>, <b>` | Swap two values |
| `mset <addr>, <value>, <size>` | Fill memory region with a byte value |

```asm
jmp entry
test
je  equal_label
jne different_label
mov rx0, rdi0
swap rx1, rdi8
mset 0x100, 0xff, 64
mset &hellow, 0x41, 2
```

---

### I/O instructions

| Instruction | Description |
|---|---|
| `putc <char>` | Print a character (ASCII code) |
| `puti <value>` | Print an integer value or register |
| `puts &name` | Print a string from the data segment |
| `getch <reg>` | Read a character into a register (non-blocking) |

```asm
putc 0x41       // prints 'A'
puti 41
puti rx1
puts &hellow
getch rx2
```

---

### File I/O instructions

| Instruction | Description |
|---|---|
| `wine &name` | Create file if it does not exist |
| `open &name` | Open a file, push file descriptor onto stack |
| `write <fd>, &name, <size>` | Write bytes to open file |
| `read  <fd>, <reg>, <size>` | Read bytes from open file into register/memory |
| `close <fd>` | Close an open file descriptor |

```asm
wine &file
open &file
popr fx0              // save file descriptor
write fx0, &hellow, 0x32
read  fx0, rx3, 0x32
close fx0
```

---

### Imports

Import another `.ns` file at compile time:

```asm
import "utils"        // loads utils.ns from the same folder
```

---

## Full example

```asm
data hellow "hellow, world!"
data file   "output.txt"

entry:
    puts &hellow

    wine &file
    open &file
    popr fx0
    write fx0, &hellow, 14
    close fx0

    exit
```

---

## Binary file format (.nb)

| Field | Type | Description |
|---|---|---|
| version | u64 | VM version number |
| flags | VM_MANIFEST_FLAGS | Feature flags |
| code size | u64 | Number of instructions |
| code | Instruction[] | Bytecode |
| data size | u64 | Data segment size in bytes |
| data | byte[] | Data segment |

---

## Building

Requires CMake and Clang (ucrt64) or GCC (mingw64).

```sh
cmake -S . -B build -G "MinGW Makefiles" \
  -DCMAKE_CXX_COMPILER=GCC
cmake --build build
```