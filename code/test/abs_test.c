#include "syscall.h"

int main() {
    int result1;
    int result2;

    result1 = Absolute(-42);
    result2 = Absolute(100);

    PrintNum(result1);
    PrintChar('\n');
    PrintNum(result2);
    PrintChar('\n');

    Halt();
    /* not reached */
}
