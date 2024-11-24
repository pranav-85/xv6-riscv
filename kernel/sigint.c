#include "user.h"
#include "stdio.h"

int main() {
    printf("Testing SIGINT system call\n");
    sigint();
    printf("Returned from SIGINT system call\n");
    exit(0);
}
