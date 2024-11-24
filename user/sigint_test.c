#include "user.h"  // Provides user-space definitions for printf, exit, etc.
#include "kernel/types.h"
int main() {
    printf("Testing SIGINT system call\n");
    sigint();  // Call the SIGINT system call to terminate
    printf("Returned from SIGINT system call\n");  // This will not execute if sigint terminates
    exit(0);
}
