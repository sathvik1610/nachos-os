# Task 3: Implementing the `Sleep` System Call in NachOS

## Overview

This task implements a `Sleep(int seconds)` system call in NachOS. When called by a user program, the current thread is **truly blocked** (not busy-waiting) for the specified number of simulated seconds. Since **1 second = 100 ticks** in NachOS, `Sleep(2)` blocks the thread for 200 ticks.

This is much more interesting than Task 1's `Absolute` syscall because `Sleep` requires:
1. The thread to **surrender the CPU** and go dormant
2. The operating system's **timer interrupt** to wake it up later
3. Interaction with the **scheduler** to put it back in the ready queue

---

## The Big Picture: How Sleep Works

The key challenge is: **how does a thread wait for time to pass without wasting CPU?**

The answer is the **sleeping list** inside the `Alarm` class. Here's the flow:

```
User calls Sleep(2)
    ↓
exception.cc: handle_SC_Sleep() reads 2 from register $4
    ↓
ksyscall.h: SysSleep(2) → alarm->WaitUntil(2 × 100 = 200 ticks)
    ↓
alarm.cc: WaitUntil(200):
    - Calculates wakeUpTick = currentTick + 200
    - Adds thread to sleepingList
    - Calls currentThread->Sleep(false) → thread is BLOCKED, CPU goes elsewhere
    ↓
... time passes, timer fires every TimerTicks ...
    ↓
alarm.cc: CallBack() fires on each timer interrupt:
    - Checks sleepingList
    - If any thread's wakeUpTick <= currentTick → moves it to ready queue
    ↓
Scheduler picks the thread from ready queue
    ↓
Thread resumes execution after Sleep() returns
```

---

## Part 1: User-Space Modifications

These files tell user programs that the `Sleep` function exists and how to call it.

### 1. `code/userprog/syscall.h`

**What we added:**
```c
#define SC_Sleep 56

void Sleep(int seconds);
```

**Why:**
- `#define SC_Sleep 56`: Every system call needs a unique numeric ID. `55` was already used by `Absolute`, so we used `56`. The user program puts this number in register `$2` before triggering the `syscall` instruction, so the kernel knows which system call is being requested.
- `void Sleep(int seconds)`: The C function prototype. Without this, any user program that calls `Sleep()` would fail to compile.

---

### 2. `code/test/start.S`

**What we added:**
```assembly
	.globl Sleep
	.ent	Sleep
Sleep:
	addiu $2,$0,SC_Sleep
	syscall
	j	$31
	.end Sleep
```

**Why:**
This is the MIPS assembly "stub" — the actual machine code that runs when a C program calls `Sleep()`.

- `.globl Sleep` / `.ent Sleep` / `.end Sleep`: Marks this as a globally-visible function for the linker.
- `addiu $2,$0,SC_Sleep`: Loads the syscall ID (`56`) into register `$2`. This tells the kernel which syscall is being invoked.
- `syscall`: Fires the hardware trap — CPU immediately switches from user mode to kernel mode and jumps to `ExceptionHandler`.
- `j $31`: After the OS returns, jump back to the return address (i.e., resume the C program where it called `Sleep()`).

**Note:** The argument (`seconds`) is automatically placed in register `$4` by the C compiler per MIPS calling conventions. We don't need to move it manually.

---

## Part 2: Kernel-Space — The Alarm Clock (Core Mechanism)

This is where the real work happens. The `Alarm` class manages timed wakeups.

### 3. `code/threads/alarm.h`

**What we changed:**

Added a struct to hold sleeping thread information:
```cpp
struct SleepingThread {
    Thread *thread;    // The thread that is sleeping
    int wakeUpTick;    // The tick at which this thread should wake up
};
```

Added a sleeping list to the `Alarm` class:
```cpp
List<SleepingThread *> *sleepingList;
```

**Why:**
- `WaitUntil()` was already declared in the original file but was noted as "not yet implemented."
- We need somewhere to remember: *"Thread X should wake up at tick Y."* The `sleepingList` stores these pairs.
- When a thread calls `Sleep()`, it gets added to this list and then blocks. Without the list, we'd have no way to track sleeping threads.

---

### 4. `code/threads/alarm.cc`

This is the most important file. We implemented two functions:

#### `Alarm::WaitUntil(int x)` — Put a thread to sleep

```cpp
void Alarm::WaitUntil(int x) {
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);

    int wakeUpTick = kernel->stats->totalTicks + x;

    SleepingThread *entry = new SleepingThread();
    entry->thread = kernel->currentThread;
    entry->wakeUpTick = wakeUpTick;

    sleepingList->Append(entry);
    kernel->currentThread->Sleep(false);

    kernel->interrupt->SetLevel(oldLevel);
}
```

Step by step:
1. **Disable interrupts** (`SetLevel(IntOff)`): We're modifying the shared `sleepingList`. If a timer interrupt fired in the middle of this, `CallBack()` could read a half-modified list and crash. Disabling interrupts makes this operation atomic.
2. **Calculate wake-up tick**: `totalTicks + x` tells us when to wake up. For `Sleep(2)`, `x = 200` ticks.
3. **Create and add a record**: We store the current thread pointer and its wake-up tick in the list.
4. **Block the thread**: `currentThread->Sleep(false)` puts the thread into `BLOCKED` state and calls `kernel->scheduler->Run()` to switch to another thread. This function **does not return** until another thread calls `ReadyToRun()` on us.
5. **Restore interrupts** (runs after wakeup): When `Sleep(false)` eventually returns, this restores the interrupt level.

#### `Alarm::CallBack()` — Wake up sleeping threads on each timer tick

```cpp
void Alarm::CallBack() {
    // ... check sleepingList on every timer interrupt ...
    int currentTick = kernel->stats->totalTicks;
    // For each entry where wakeUpTick <= currentTick:
    //   → move thread to ready queue via ReadyToRun()
    
    // Wake up in priority order (highest priority first)
    // ... (priority sorting logic) ...

    if (status != IdleMode) {
        interrupt->YieldOnReturn();
    }
}
```

**Why `CallBack()` is the right place:**
- `CallBack()` is invoked by the hardware timer on every clock tick (with interrupts already disabled).
- It's the only piece of OS code that runs periodically — perfect for checking "is it time to wake anyone up yet?"
- We iterate through `sleepingList`, pluck out any thread whose `wakeUpTick` has arrived, and call `kernel->scheduler->ReadyToRun(thread)` on it, which moves it from `BLOCKED` → `READY`.

**Priority-ordered wakeup:**
When multiple threads wake up at the same tick, we wake them in **highest priority first** order. This way they're added to the ready queue in priority order, ensuring that the highest-priority woken thread gets to run next (since our Task 2 scheduler picks the highest-priority thread from the ready queue).

---

## Part 3: Kernel Syscall Handler

### 5. `code/userprog/ksyscall.h`

**What we added:**
```cpp
void SysSleep(int seconds) {
    if (seconds <= 0) return;
    kernel->alarm->WaitUntil(seconds * 100);
}
```

**Why:**
- The **seconds → ticks** conversion lives here: `seconds × 100` (since 1 second = 100 ticks).
- We guard against non-positive values — `Sleep(0)` or `Sleep(-1)` should do nothing.
- By isolating the logic here, `exception.cc` stays clean (only register I/O).

---

### 6. `code/userprog/exception.cc`

**What we added — the handler function:**
```cpp
void handle_SC_Sleep() {
    int seconds = (int)kernel->machine->ReadRegister(4);
    DEBUG(dbgSys, "Sleep " << seconds << " second(s)\n");
    SysSleep(seconds);
    DEBUG(dbgSys, "Sleep: thread woke up\n");
    return move_program_counter();
}
```

**And in the switch statement:**
```cpp
case SC_Sleep:
    return handle_SC_Sleep();
```

**Why:**
- `ReadRegister(4)`: The `seconds` argument was placed in register `$4` by the C calling convention.
- `SysSleep(seconds)`: Does the actual work (converts to ticks and blocks the thread).
- `move_program_counter()`: Advances the PC past the `syscall` instruction. Critical — without this, when the thread wakes up, it would re-execute the `syscall` instruction and sleep again forever.
- The `case SC_Sleep:` routes syscall code `56` to our handler.

---

## Part 4: Testing

### 7. `code/test/sleep_test.c`

```c
#include "syscall.h"

int main() {
    PrintString("Going to sleep for 2 seconds...\n");
    Sleep(2);
    PrintString("Woke up after 2 seconds!\n");

    PrintString("Going to sleep for 1 more second...\n");
    Sleep(1);
    PrintString("Done! Total sleep: 3 seconds.\n");

    Halt();
    return 0;
}
```

### 8. `code/test/Makefile`

Added `sleep_test` to `PROGRAMS` and its build rules:
```makefile
sleep_test.o: sleep_test.c
	$(CC) $(CFLAGS) -c sleep_test.c
sleep_test: sleep_test.o start.o
	$(LD) $(LDFLAGS) start.o sleep_test.o -o sleep_test.coff
	$(COFF2NOFF) sleep_test.coff sleep_test
```

---

## How to Build and Run

### Step 1 — Rebuild the NachOS kernel:
```bash
cd code/build.linux
make
```

### Step 2 — Build the test program:
```bash
cd code/test
make sleep_test
```

### Step 3 — Run:
```bash
cd code/build.linux
./nachos -x ../test/sleep_test
```

### Expected Output:
```
Going to sleep for 2 seconds...
Woke up after 2 seconds!
Going to sleep for 1 more second...
Done! Total sleep: 3 seconds.
Machine halting!

Ticks: total 16940, idle 12728, system 4160, user 52
```

✅ The output confirms the thread truly sleeps (the large number of idle ticks shows the CPU was free to do other things during the sleep).

---

## Key Design Points

### Why not busy-wait?

We could have implemented `Sleep` by spinning in a loop until enough ticks pass:
```c
// BAD approach - wastes CPU
while (kernel->stats->totalTicks < wakeUpTick) { /* spin */ }
```
Instead, we **block** the thread with `currentThread->Sleep(false)`. This frees the CPU to run other threads (or go idle) while our thread waits. The timer interrupt wakes it up when ready. This is the correct OS approach.

### Thread Safety

`WaitUntil()` disables interrupts before modifying `sleepingList`. This is necessary because:
- `CallBack()` also reads/modifies `sleepingList` during timer interrupts
- Without this protection, a race condition could corrupt the list

### Priority & Sleep Interaction

This directly integrates with **Task 2's priority scheduler**. When two threads wake up at the same tick, `CallBack()` wakes them in **priority order** (highest first). The ready queue (a `SortedList` from Task 2) then ensures the highest-priority thread runs next. Sleeping does not affect a thread's priority.
