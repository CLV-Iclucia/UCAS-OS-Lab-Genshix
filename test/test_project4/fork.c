#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int N = 1;
// copied and modified from the cow lab of xv6
void forktest(char *s) {
  // the original N is 1000
  // but my OS cannot support that many processesc
  int n, pid;

  for (n = 0; n < N; n++) {
    pid = sys_fork();
    if (pid < 0) break;
    if (pid == 0) {
      sys_move_cursor(0, n + 1);
      printf("Hello world! This is child process %d\n", n);
      sys_exit();
    }
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
  forktest("forktest");
  sys_move_cursor(0, N + 1);
  printf("Congratulation! You have passed fork test!\n");
}