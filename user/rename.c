#include "kernel/types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("Usage: rename old new\n");
    exit(0);
  }
  if(rename(argv[1], argv[2]) < 0){
    printf("rename %s %s failed\n", argv[1], argv[2]);
    exit(0);
  }
  exit(0);
}