/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * The shell acts as a task running in user mode. The main function is
 * to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * */

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#define SHELL_BEGIN 20
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 20
#define BUFFER_SIZE 256

const char prompt[] = "> root@Genshix: ";
char buffer[BUFFER_SIZE];
int cur = 0;

void clear_buffer() {
  memset(buffer, 0, BUFFER_SIZE);
  cur = 0;
}

int input_offset = 0;
void screen_clear() {
  sys_clear_region(0, SHELL_BEGIN + 1, SCREEN_WIDTH, SCREEN_HEIGHT);
}

int getchar()
{
  int c = -1;
  while (1) {
    c = sys_getchar();
    if (c != -1) {
      if (c == '\r') c = '\n';
      break;
    }
  }
  return c;
}

int read_input(char* buf) {
  clear_buffer();

  char c;
  cur = 0;
  do {
    c = getchar();
    if (c == '\b' || c == 127) {
      sys_move_cursor_x(input_offset);
      if (cur > 0) {
        buffer[cur] = '\0';
        buffer[--cur] = ' ';
        printf("%s", buffer);
      }
    } else {
      buffer[cur++] = c;
      buffer[cur] = '\0';
      sys_move_cursor_x(input_offset);
      printf("%s", buffer);
    }
  } while (c != '\n');
  buf[--cur] = '\0';
  return cur;
}

#define MAX_TOKENS 64
#define MAX_ARGS 64
#define CMD_POOL_SIZE 64
char* args[MAX_ARGS];

typedef struct command {
  int argc;
  char** argv;
} cmd_t;

cmd_t commands[CMD_POOL_SIZE];
int cmd_queue[CMD_POOL_SIZE];
int cmd_head = 0, cmd_tail = CMD_POOL_SIZE - 1;

cmd_t* alloc_cmd() {
  int id = cmd_queue[cmd_head];
  cmd_head = (cmd_head + 1) % CMD_POOL_SIZE;
  return commands + id;
}

cmd_t* new_cmd(int argc, char** argv) {
  cmd_t* cmd = alloc_cmd();
  cmd_tail = (cmd_tail + 1) % CMD_POOL_SIZE;
  cmd->argc = argc;
  cmd->argv = argv;
  return cmd;
}

void free_cmd(cmd_t* cmd) {
  int id = cmd - commands;
  // put into tail
  cmd_queue[cmd_tail] = id;
  cmd_tail = (cmd_tail + 1) % CMD_POOL_SIZE;
}

cmd_t* parse_cmd(char* cmd, int len) {
  int argc = 0;
  char* ptr = cmd;
  while (argc < MAX_ARGS) {
    while (*ptr == ' ') ptr++;
    if (*ptr == '\0') break;
    args[argc++] = ptr;
    while (*ptr != ' ' && *ptr != '\0') ptr++;
    if (*ptr == '\0') break;
    *ptr = '\0';
    ptr++;
  }
  return new_cmd(argc, args);
}

void exec(cmd_t* cmd) {
  int run_in_bg = 0;
  cmd->argc--;  // remove "exec"
  cmd->argv++;  // now points to programme
  if (cmd->argc > 0 && strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
    run_in_bg = 1;
    cmd->argc--;  // remove "&"
  }
  if (cmd->argc == 0) {
    printf("Usage: exec <programme> [args...]\n");
    return;
  }
  pid_t pid = sys_exec(cmd->argv[0], cmd->argc, cmd->argv);
  if (pid == -1) {
    printf("exec failed\n");
    return;
  }
  if (!run_in_bg) sys_waitpid(pid);
}

void kill(cmd_t* cmd) {
  if (cmd->argc != 2) {
    printf("Usage: kill <pid>\n");
    return;
  }
  pid_t pid = atoi(cmd->argv[1]);
  sys_kill(pid);
}

void ps(cmd_t* cmd) {
  if (cmd->argc != 1) {
    printf("Usage: ps\n");
    return;
  }
  sys_ps();
}

void taskset(cmd_t* cmd) {
  if (cmd->argc < 3) {
    printf("Usage: taskset -p <mask> <pid> or taskset <mask> <programme> [args...] [&]\n");
    return;
  }
  if (strcmp(cmd->argv[1], "-p") == 0) {
    // taskset -p [mask] [pid]
    int pid = atoi(cmd->argv[3]);
    int mask = atoi(cmd->argv[2]);
    sys_taskset(pid, mask);
    return;
  }
  // taskset [mask] [programme] [args...] [&]
  bool run_in_bg = true;
  uint32_t mask = atoi(cmd->argv[1]);
  cmd->argc -= 2;  // remove "taskset" and mask
  cmd->argv += 2;  // now points to programme
  if (cmd->argc > 0 && strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
    run_in_bg = 1;
    cmd->argc--;  // remove "&"
  }
  if (cmd->argc == 0) {
    printf("Usage: taskset <mask> <programme> [args...]\n");
    return;
  }
  pid_t pid = sys_exec(cmd->argv[0], cmd->argc, cmd->argv);
  if (pid == -1) {
    printf("exec failed\n");
    return;
  }
  sys_taskset(pid, mask);
  if (!run_in_bg) sys_waitpid(pid);
}

int main(void) {
  sys_move_cursor(0, SHELL_BEGIN);
  for (int i = 0; i < CMD_POOL_SIZE; i++) cmd_queue[i] = i;
  sys_move_cursor(0, SHELL_BEGIN);
  printf("------------------- YoRHa -------------------\n");
  screen_clear();
  input_offset = strlen(prompt);
  while (1) {
    printf("%s", prompt);
    clear_buffer();
    int len = read_input(buffer);
    cmd_t* cmd = parse_cmd(buffer, len);
    if (cmd->argc == 0) {
      free_cmd(cmd);
      continue;
    }
    if (strcmp(cmd->argv[0], "ps") == 0) {
      ps(cmd);
    } else if (strcmp(cmd->argv[0], "exec") == 0) {
      exec(cmd);
    } else if (strcmp(cmd->argv[0], "kill") == 0) {
      kill(cmd);
    } else if (strcmp(cmd->argv[0], "clear") == 0) {
      screen_clear();
    } else if (strcmp(cmd->argv[0], "taskset") == 0) {
      taskset(cmd);
    } else {
      printf("Unknown command: %s\n", cmd->argv[0]);
    }
    free_cmd(cmd);
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
  }

  return 0;
}
