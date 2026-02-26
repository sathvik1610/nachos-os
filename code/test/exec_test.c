/* exec_test.c
 * Demonstrates parent-child process relationship in NachOS.
 *
 * This is the PARENT process. It uses Exec() to spawn add_child as a
 * CHILD process, then uses Join() to wait for the child to complete.
 */

#include "syscall.h"

int main() {
    SpaceId childId;
    int exitCode;

    PrintString("[Parent] Parent process started.\n");
    PrintString("[Parent] Spawning child process 'add_child' via Exec()...\n");

    /* Exec() loads and starts add_child as a brand new process.
     * NachOS is run from build.linux, so we use the relative path to test/.
     * It returns a SpaceId (process ID) for the child, or -1 on failure. */
    childId = Exec("../test/add_child");

    if (childId < 0) {
        PrintString("[Parent] ERROR: Exec() failed to start child!\n");
        Halt();
    }

    PrintString(
        "[Parent] Child spawned successfully. Waiting with Join()...\n");

    /* Join() blocks this (parent) thread until the child calls Exit().
     * It returns the exit code that the child passed to Exit(). */
    exitCode = Join(childId);

    PrintString("[Parent] Child finished! Exit code = ");
    PrintNum(exitCode);
    PrintString("\n");
    PrintString("[Parent] Parent process done.\n");

    Halt();
    return 0;
}
