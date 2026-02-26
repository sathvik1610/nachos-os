/* add_child.c
 * A version of the add program designed to be run as a child process.
 * Prints messages so we can see it executing.
 */

#include "syscall.h"

int main() {
    int result;

    PrintString("[Child] add_child process started!\n");
    result = Add(42, 23);
    PrintString("[Child] 42 + 23 = ");
    PrintNum(result);
    PrintString("\n");
    PrintString("[Child] add_child process finishing.\n");

    Exit(result); /* return the result as exit code so parent can see it */
}
