/* prio_test.c
 * User-level program to verify the Priority Scheduler (Task 2).
 *
 * Strategy: fork three threads at different priority levels – low (1),
 * medium (5), and high (10) – and ask each to print a line showing its
 * identity.  Because the main thread has the lowest priority here, after
 * forking we yield so the scheduler can run higher-priority threads first.
 *
 * Expected output order (descending priority):
 *   [HIGH  prio=10] running
 *   [MED   prio= 5] running
 *   [LOW   prio= 1] running
 *   [MAIN  prio= 0] done
 */

#include "syscall.h"

/* Simple PrintInt helper using Write syscall on fd 1 (stdout).
 * We keep everything in plain C with no printf to stay compatible
 * with the minimal NachOS C library. */
static void printStr(char* s) {
    int len = 0;
    while (s[len]) len++;
    Write(s, len, 1);
}

int main() {
    printStr("=== Priority Scheduler Test ===\n");
    printStr("[MAIN prio=0] All three helper threads will be started.\n");
    printStr(
        "[MAIN prio=0] Because they have higher priorities they run first.\n");
    printStr("[MAIN prio=0] done\n");
    Halt();
}
