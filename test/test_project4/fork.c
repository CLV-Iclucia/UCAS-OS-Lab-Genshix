#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int N = 10;
// copied and modified from the cow lab of xv6
void forktest(char *s) {
  // the original N is 1000
  // but my OS cannot support that many processes
  int n, pid;
  for (n = 0; n < N; n++) { 
  //  sys_move_cursor(0, N + n);
    pid = sys_fork();
    if (pid == 0) {
      sys_move_cursor(0, n);
      printf("child proc %d has pid %d, n is at %lx", n, sys_getpid(), &n);
      // for (int i = 0; i < 1000; i++) {
      //   sys_move_cursor(0, n);
      //   printf("child process %d says %d\n", n, i);
      // }
      sys_exit();
    }
    if (pid < 0) break;
  }

  if (n == 0) {
    printf("%s: no fork at all!\n", s);
    sys_exit();
  }

  if (n > N) {
    printf("%s: fork claimed to work 10 times!\n", s);
    sys_exit();
  }
}

int main() {
  sys_move_cursor(0, N);
//  printf("fork %d child processes\n", N);
  forktest("forktest");
//  sys_move_cursor(0, N + 2);
 // printf("I am quiting.");
}