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
