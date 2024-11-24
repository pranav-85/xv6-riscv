#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_MSG_SIZE 256
#define MAX_QUEUES 16   // Add this definition to match kernel's definition

void test_multiple_messages() {
  printf("Test 2: Multiple messages in queue\n");
  
  int qid = msgget();
  if(qid < 0) {
    printf("FAIL: Could not create message queue\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0) {
    printf("FAIL: Fork failed\n");
    exit(1);
  }

  if(pid == 0) {  // Child process
    sleep(20);  // Wait for parent to send all messages
    
    char buf[MAX_MSG_SIZE];
    for(int i = 0; i < 3; i++) {
      int n = msgrcv(qid, buf, sizeof(buf));
      if(n < 0) {
        printf("FAIL: Child could not receive message %d\n", i);
        exit(1);
      }
      buf[n] = 0;
      printf("Child received message %d: %s\n", i, buf);
    }
    
    msgclose(qid);
    exit(0);
  } else {  // Parent process
    char *messages[] = {
      "Message 1",
      "Message 2",
      "Message 3"
    };

    for(int i = 0; i < 3; i++) {
      if(msgsend(qid, messages[i], strlen(messages[i])) < 0) {
        printf("FAIL: Parent could not send message %d\n", i);
        exit(1);
      }
      printf("Parent sent message %d\n", i);
    }

    wait(0);
    msgclose(qid);
  }
  printf("Test 2: PASS\n\n");
}

void test_queue_limits() {
  printf("Test 3: Testing queue limits\n");
  
  // Test maximum queues
  int queues[MAX_QUEUES + 1];
  int count = 0;
  for(int i = 0; i < MAX_QUEUES + 1; i++) {
    queues[i] = msgget();
    if(queues[i] < 0) {
      printf("Queue limit reached at %d queues (expected)\n", i);
      count = i;
      break;
    }
  }
  
  // Cleanup queues
  for(int j = 0; j < count; j++) {
    msgclose(queues[j]);
  }
  
  // Test message size limit
  int qid = msgget();
  if(qid < 0) {
    printf("FAIL: Could not create message queue\n");
    exit(1);
  }
  
  char large_msg[MAX_MSG_SIZE + 10];
  memset(large_msg, 'A', sizeof(large_msg));
  
  int ret = msgsend(qid, large_msg, sizeof(large_msg));
  if(ret < 0) {
    printf("Large message rejected as expected (size: %lu)\n", sizeof(large_msg));  // Fixed format specifier
  } else {
    printf("FAIL: Large message should have been rejected\n");
    exit(1);
  }
  
  msgclose(qid);
  printf("Test 3: PASS\n");
}

int main(int argc, char *argv[]) {
//   test_basic_messaging();
  test_multiple_messages();
  test_queue_limits();
  printf("All tests completed successfully!\n");
  exit(0);
}