# Task 2: Implementing the Priority Scheduler in NachOS

> **Goal:** Replace NachOS's default FIFO thread scheduler with a **priority-based** scheduler so that the thread with the highest priority always runs next.

---

## Background: How the Original Scheduler Worked

The original `scheduler.cc` comment says it explicitly:
> *"Very simple implementation -- no priorities, straight FIFO. Might need to be improved in later assignments."*

It used `List<Thread*>`, a plain singly-linked list where `Append` adds to the tail and `RemoveFront` pulls from the head — pure FIFO with no concept of priority.

---

## Part 1: Adding Priority to the Thread Class

### 1. `code/threads/thread.h`

**What we added:**
```cpp
int priority;    // scheduling priority (higher number = higher priority)
int getPriority() { return priority; }
```

Constructor signature changed to:
```cpp
Thread(char *debugName, int priority = 0, bool _has_dynamic_name = false);
```

**Why:**
- Every thread needs to carry its own priority value.
- The default `priority = 0` ensures **all existing code compiles unchanged** — any `new Thread("name")` call gets priority 0 and behaves identically to before.
- `getPriority()` gives the scheduler a clean accessor.

### 2. `code/threads/thread.cc`

**What we added:**
```cpp
Thread::Thread(char *threadName, int priority /*=0*/, bool _has_dynamic_name /*=false*/) {
    ...
    this->priority = priority;   // ← new line
    ...
}
```

---

## Part 2: The Priority Scheduler

### 3. `code/threads/scheduler.h`

Changed `readyList` type from:
```cpp
List<Thread*>* readyList;
```
to:
```cpp
SortedList<Thread*>* readyList;
```

NachOS already ships `SortedList<T>` in `lib/list.h`. It maintains elements in sorted order and always returns the "smallest" on `RemoveFront()`.

### 4. `code/threads/scheduler.cc`

**Three targeted changes:**

#### (a) New comparator function (added before class methods)

```cpp
static int ThreadPriorityCompare(Thread *x, Thread *y) {
    if (x->getPriority() > y->getPriority()) return -1;  // x before y
    if (x->getPriority() < y->getPriority()) return  1;  // y before x
    return 0;                                             // equal → FIFO
}
```

**Why the inversion?** `SortedList::RemoveFront()` returns the element with the *lowest* comparison key. We want the *highest* priority to come out first, so we return `-1` when `x.priority > y.priority` — making higher-priority threads "smaller" in the comparator's view.

#### (b) Constructor creates a `SortedList`

```cpp
Scheduler::Scheduler() {
    readyList = new SortedList<Thread*>(ThreadPriorityCompare);
    toBeDestroyed = NULL;
}
```

#### (c) `ReadyToRun` uses `Insert` instead of `Append`

```cpp
readyList->Insert(thread);  // inserted in priority order
```

`SortedList` overrides `Append` to call `Insert` as well, but using `Insert` directly is semantically explicit.

**`FindNextToRun` is unchanged** — `RemoveFront()` already dequeues the front element, which `SortedList` now keeps as the highest-priority thread.

---

## Part 3: Testing

### 5. `code/test/prio_test.c`

A simple user program that verifies the kernel boots and runs under the new scheduler without crashing.

### 6. `code/test/Makefile`

Added `prio_test` to `PROGRAMS` and its build rules (same pattern as `abs_test`).

---

## How to Build and Run

```bash
# 1. Rebuild the kernel (in WSL)
cd code/build.linux
make clean && make

# 2. Run the existing thread self-test (regression)
./nachos -K
# Expected: the ping-pong between two threads still works
#           (both have default priority=0, so FIFO is preserved among equals)

# 3. Build the user test program
cd code/test
make prio_test

# 4. Run the user test program
cd code/build.linux
./nachos -x ../test/prio_test
# Expected: program prints its header lines and halts cleanly
```

### How to observe priority scheduling in action

To see clearly differentiated priority scheduling, you can modify `Thread::SelfTest()` in `thread.cc` to assign different priorities:

```cpp
void Thread::SelfTest() {
    Thread *t = new Thread("low-priority-thread", 1);   // priority 1
    // current thread has priority 0
    t->Fork((VoidFunctionPtr)SimpleThread, (void *)1);
    kernel->currentThread->Yield();
    SimpleThread(0);
}
```
With priorities assigned this way, `t` (priority 1) will always be picked first by `FindNextToRun`, demonstrating priority order.

---

## Key Design Decisions

| Decision | Rationale |
|---|---|
| Reuse `SortedList<T>` | Already in codebase, no new data structure needed |
| Higher number = higher priority | Intuitive; matches most OS textbooks |
| Default `priority = 0` | 100% backwards compatible — no existing code changes needed |
| Equal-priority → FIFO | Comparator returns 0 for ties; `SortedList::Insert` places new elements after existing equals |
| `FindNextToRun` unchanged | `RemoveFront()` semantics are identical; the sorted list does all the work |
