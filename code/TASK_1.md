# Task 1: Implementing the `Absolute` System Call in NachOS

This guide explains how to implement a new system call, specifically `Absolute`, in NachOS. System calls act as the bridge between user-level programs (written in C) and the operating system kernel (written in C++). 

When a user program needs the OS to do something (like calculating an absolute value, reading a file, or printing to the console), it cannot just call an OS function directly due to memory protection. Instead, it triggers a "software interrupt" or "trap" using a specific assembly instruction (`syscall`). The OS then catches this trap, determines what the user program wants, performs the action, and returns control to the user program.

To implement a new system call, we need to modify both **User Space** (so the user program knows the function exists and how to ask for it) and **Kernel Space** (so the OS knows how to handle the request).

---

## Part 1: User-Space Modifications

These files are used when compiling the user programs (like our test programs).

### 1. `code/userprog/syscall.h`
**What it does:** This header file defines the interface between user programs and the NachOS kernel. It contains the system call codes (unique IDs for each system call) and the C function prototypes that user programs can call.

**What we added:**
```c
#define SC_Absolute 55

// ... (further down in the file)

int Absolute(int op);
```
**Why we added it:**
* `#define SC_Absolute 55`: We need to assign a unique integer ID to our new system call. When the user program traps to the kernel, it puts this ID in a specific CPU register so the kernel knows *which* system call was requested. We chose `55` because `54` was the last used ID.
* `int Absolute(int op);`: This is the C function prototype. It tells the C compiler (when compiling user programs) that a function named `Absolute` exists, takes one `int` argument, and returns an `int`. Without this, user programs wouldn't compile when trying to call `Absolute()`.

### 2. `code/test/start.S`
**What it does:** This file contains MIPS assembly language code. It provides the actual "stubs" for the system calls. When a C program calls `Absolute()`, it actually jumps to the assembly code written here.

**What we added:**
```assembly
	.globl Absolute
	.ent	Absolute
Absolute:
	addiu $2,$0,SC_Absolute
	syscall
	j 	$31
	.end Absolute
```
**Why we added it:**
This translates the C function call into a hardware-level trap. Let's break down the MIPS assembly syntax:
* `.globl Absolute`: Makes the label `Absolute` globally visible so the linker can find it when the C code calls `Absolute()`.
* `.ent Absolute` / `.end Absolute`: Directives that mark the beginning and end of the function for the assembler/debugger.
* `addiu $2, $0, SC_Absolute`: This is the crucial part. 
  * `$0` is a special MIPS register that always holds the value `0`.
  * `SC_Absolute` is the macro we defined in `syscall.h` (which gets replaced with `55`).
  * `addiu` adds `$0` and `55`, and stores the result in register `$2` (also known as `$v0`).
  * **Convention:** In NachOS (and MIPS generally), register `$2` is used to tell the kernel *which* system call is being invoked.
* `syscall`: This single instruction causes the CPU to switch from user mode to kernel mode and jump to the OS exception handler.
* `j $31`: After the kernel finishes and returns control to user space, this instruction executes. It means "jump to the address stored in register `$31` (the return address)". This returns control back to the exact point in the C program where `Absolute()` was originally called.

*(Note: In MIPS calling conventions, the first argument passed to a function is automatically placed in register `$4` (or `$a0`). The kernel will read from `$4` to get our input number).*

---

## Part 2: Kernel-Space Modifications

These files are part of the NachOS operating system itself.

### 3. `code/userprog/ksyscall.h`
**What it does:** This file contains kernel-level helper functions. While you *could* put all the logic directly into the exception handler, it keeps the code much cleaner to separate the actual logic of the system call (doing the math) from the mechanics of the system call (reading/writing registers).

**What we added:**
```cpp
int SysAbsolute(int op) {
    if (op < 0) return -op;
    return op;
}
```
**Why we added it:**
This is the pure logic of our system call. It simply calculates the mathematical absolute value. By isolating it here, `exception.cc` can stay focused purely on register management.

### 4. `code/userprog/exception.cc`
**What it does:** This is the entry point into the kernel. Whenever a user program executes the `syscall` assembly instruction, the hardware jumps to the `ExceptionHandler()` function in this file.

**What we added:**
First, we added a handler function:
```cpp
void handle_SC_Absolute() {
    DEBUG(dbgSys, "Absolute " << kernel->machine->ReadRegister(4) << "\n");

    int result = SysAbsolute((int)kernel->machine->ReadRegister(4));

    DEBUG(dbgSys, "Absolute returning with " << result << "\n");
    kernel->machine->WriteRegister(2, (int)result);

    return move_program_counter();
}
```
Second, we added a case to the `switch` statement inside `ExceptionHandler`:
```cpp
// inside ExceptionHandler(ExceptionType which), under case SyscallException:
                case SC_Absolute:
                    return handle_SC_Absolute();
```

**Why we added it:**
* `ExceptionHandler` checks the `type` of the exception. If it's a `SyscallException`, it looks at register `$2` to find the system call code. Our addition to the `switch` statement routes `SC_Absolute` (55) to our new `handle_SC_Absolute()` function.
* `handle_SC_Absolute()` performs the necessary context switching steps:
  1. **Read arguments:** It reads MIPS register `$4` using `kernel->machine->ReadRegister(4)`. Remember, `start.S` didn't explicitly move the argument; the C compiler automatically put our input parameter there before calling the `start.S` stub.
  2. **Do the work:** It calls our helper `SysAbsolute()` with the argument it just read.
  3. **Return result:** It writes the result into MIPS register `$2` using `kernel->machine->WriteRegister(2, result)`. MIPS calling conventions dictate that return values are passed back in register `$2`.
  4. **Advance PC:** It calls `move_program_counter()`. If we don't do this, the processor will return exactly to the `syscall` instruction that triggered the trap, causing an infinite loop. This function updates the Program Counter to the *next* assembly instruction.

---

## Part 3: Testing

### 5. `code/test/abs_test.c`
**What we did:** Created a completely new C program to test our syscall.
```c
#include "syscall.h"

int main() {
    int result1 = Absolute(-42);
    // ... print result
}
```
**Why:** To verify our code actually works when running as a simulated user process.

### 6. `code/test/Makefile`
**What we did:** Added `abs_test` to the `PROGRAMS` variable, and added build rules for it.
```makefile
abs_test.o: abs_test.c
	$(CC) $(CFLAGS) -c abs_test.c
abs_test: abs_test.o start.o
	$(LD) $(LDFLAGS) start.o abs_test.o -o abs_test.coff
	$(COFF2NOFF) abs_test.coff abs_test
```
**Why:** The `Makefile` tells the compiler how to build executable binaries. We need to compile `abs_test.c` into an object file (`.o`), link it with `start.o` (which contains our assembly stub so `Absolute()` resolves correctly), and finally convert it into the `.coff` executable format that NachOS understands.

---

## How to Run and Test

To test the system call, you need to compile both the NachOS kernel and the user test program, and then execute the program using NachOS.

1. **Build the NachOS Kernel:**
   Navigate into your build directory (e.g., `build.linux` if you're on Linux/WSL/Cygwin) and compile the kernel.
   ```bash
   cd code/build.linux
   make clean
   make
   ```

3. **Build the coff2noff Translator (Crucial):**
   NachOS requires MIPS executables to be translated into the specialized `.coff` file format to run properly. This translation tool typically needs to be compiled natively on your host OS.
   Navigate to the base `nachos-project` directory and compile it using the provided script:
   ```bash
   cd ~/nachos-project-linux
   bash coff2noff.sh
   ```
   *Note: If this fails, ensure the MIPS cross-compiler archive was downloaded correctly!*

4. **Build the User Programs:**
   Navigate into the `test` directory and compile the MIPS executables using the cross-compiler.
   
   **Important Note:** Do *not* simply run `make` here if the base repository contains broken files (like `matmult.c` which will fail to compile and halt the process). Instead, specifically target your test program:
   ```bash
   cd code/test
   make distclean
   make abs_test
   ```

5. **Run your Test Program:**
   Go back to the build directory and run NachOS, passing the compiled user program `abs_test` to it via the `-x` flag.
   ```bash
   cd code/build.linux
   ./nachos -x ../test/abs_test
   ```

If everything was implemented correctly, NachOS will boot, load the test program into memory, execute the `Absolute(-42)` and `Absolute(100)` system calls, print `42` and `100` to the console, and then halt!



Flow:
C code calls Absolute(-42)
    ↓
start.S puts 55 in register $2, fires syscall instruction
    ↓
CPU switches to kernel mode → ExceptionHandler() in exception.cc
    ↓
Sees SC_Absolute (55) → calls handle_SC_Absolute()
    ↓
Reads -42 from register $4 → calls SysAbsolute(-42) → returns 42
    ↓
Writes 42 to register $2, advances Program Counter
    ↓
CPU returns to user mode → C program gets 42 back
