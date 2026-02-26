# Task 3b: Exec & Join — Parent-Child Process Demonstration in NachOS

## Overview

This task demonstrates **process creation** in NachOS using the `Exec()` and `Join()` system calls. A **parent process** spawns a **child process** and waits for it to complete — exactly how a shell works when you run a command.

**No kernel changes were needed** — `Exec` and `Join` were already implemented. We only write user-space programs to demonstrate the relationship.

---

## The Concept: Parent-Child Processes

In an operating system, processes can create other processes:
- The **parent** calls `Exec()` to create and start the child
- The **child** runs independently in its own address space
- The **parent** optionally calls `Join()` to wait for the child to finish
- The child's return value (exit code) is passed back to the parent via `Exit()`

This is the foundation of how every command shell works — when you type `ls`, the shell is the parent and `ls` is the child.

---

## Files Changed / Created

### 1. `code/test/add_child.c` — The Child Process

```c
#include "syscall.h"

int main() {
    int result;

    PrintString("[Child] add_child process started!\n");
    result = Add(42, 23);
    PrintString("[Child] 42 + 23 = ");
    PrintNum(result);
    PrintString("\n");
    PrintString("[Child] add_child process finishing.\n");

    Exit(result);   /* passes 65 back to the parent as exit code */
}
```

**Why:**
- This is the **child process**. It does a simple computation (`Add(42, 23) = 65`), prints its progress, and calls `Exit(result)`.
- `Exit(result)` is important — it sends the result (`65`) back to the parent. The parent receives this when it calls `Join()`.
- Without `Exit()`, the parent's `Join()` would never return.

---

### 2. `code/test/exec_test.c` — The Parent Process

```c
#include "syscall.h"

int main() {
    SpaceId childId;
    int exitCode;

    PrintString("[Parent] Parent process started.\n");
    PrintString("[Parent] Spawning child process 'add_child' via Exec()...\n");

    childId = Exec("../test/add_child");   /* spawn the child */

    if (childId < 0) {
        PrintString("[Parent] ERROR: Exec() failed!\n");
        Halt();
    }

    PrintString("[Parent] Child spawned. Waiting with Join()...\n");

    exitCode = Join(childId);   /* block until child finishes */

    PrintString("[Parent] Child finished! Exit code = ");
    PrintNum(exitCode);
    PrintString("\n[Parent] Parent process done.\n");

    Halt();
}
```

**Why each syscall:**
- `Exec("../test/add_child")`: Tells the kernel to load the `add_child` binary into a **new address space** and start running it as a new process. Returns a `SpaceId` (the child's process ID), or `-1` on failure. The parent continues running immediately after this — both parent and child are now running concurrently.
- `Join(childId)`: Blocks the parent until the child with that ID calls `Exit()`. Returns the child's exit code.
- The path `"../test/add_child"` is relative to `build.linux/` — where `nachos` is run from. NachOS's stub filesystem opens files using Linux paths.

---

### 3. `code/test/Makefile`

Added both programs to `PROGRAMS` and their build rules:
```makefile
add_child.o: add_child.c
    $(CC) $(CFLAGS) -c add_child.c
add_child: add_child.o start.o
    $(LD) $(LDFLAGS) start.o add_child.o -o add_child.coff
    $(COFF2NOFF) add_child.coff add_child

exec_test.o: exec_test.c
    $(CC) $(CFLAGS) -c exec_test.c
exec_test: exec_test.o start.o
    $(LD) $(LDFLAGS) start.o exec_test.o -o exec_test.coff
    $(COFF2NOFF) exec_test.coff exec_test
```

---

## How Exec() & Join() Work Internally (Kernel Side)

These were already implemented in the NachOS codebase. Here's what happens under the hood:

### `Exec("../test/add_child")` → `SysExec()` in `ksyscall.h`
1. Opens the binary file using the filesystem
2. Calls `kernel->pTab->ExecUpdate(name)` which:
   - Creates a new **PCB (Process Control Block)** for the child
   - Creates a new **Thread** for the child
   - Loads the binary into a new **AddrSpace** (memory)
   - Forks the thread to start running
3. Returns the child's process ID (`SpaceId`)

### `Join(childId)` → `SysJoin()` in `ksyscall.h`
1. Calls `kernel->pTab->JoinUpdate(id)` which:
   - Looks up the child PCB by ID
   - **Blocks** the parent thread using a semaphore (waits until child exits)
   - When the child calls `Exit()`, it signals the semaphore and stores its exit code
   - Parent unblocks and receives the exit code
2. Returns the exit code from the child's `Exit()` call

---

## How to Build and Run

### Step 1 — Build the test programs:
```bash
cd code/test
make add_child exec_test
```

### Step 2 — Run:
```bash
cd code/build.linux
./nachos -x ../test/exec_test
```

### Expected Output:
```
[Parent] Parent process started.
[Parent] Spawning child process 'add_child' via Exec()...
[Parent] Child spawned successfully. Waiting with Join()...
[Child] add_child process started!
[Child] 42 + 23 = 65
[Child] add_child process finishing.
[Parent] Child finished! Exit code = 65
[Parent] Parent process done.
Machine halting!
```

> **Notice the interleaving!** The `[Parent] Child spawned...` message appears before the `[Child]` messages because both processes are running concurrently. The parent prints, then the scheduler switches to the child, then switches back to the parent after `Join()` completes.

---

## Key Concepts Demonstrated

| Concept | What it shows |
|---------|--------------|
| **Process creation** | `Exec()` loads a new binary and creates an independent process |
| **Address space isolation** | Child has its own memory — parent's variables are not accessible to child |
| **Concurrency** | Parent and child run simultaneously (interleaved output) |
| **Synchronization** | `Join()` blocks parent until child is done — parent-child sync |
| **Exit codes** | Child passes data back to parent via `Exit(value)` → `Join()` returns it |
| **PCB** | NachOS tracks parent-child relationships via its process table (`pTab`) |

---

## Connection to the Shell

This is exactly what a command shell does when you type a command:
```
shell (parent) → Exec("ls") → ls (child) runs
shell calls Join() → blocks → ls finishes → shell gets exit code → shows prompt again
```

Understanding Exec+Join is the foundation for Task 4 (pipes), where the shell will spawn two child processes and connect them.
