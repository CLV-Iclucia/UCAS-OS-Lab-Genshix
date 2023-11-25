/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#define SHELL_BEGIN 20
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 20
#define BUFFER_SIZE 256

const char* prompt = "> root@Genshix: ";
char buffer[BUFFER_SIZE];
int cur = 0;

void clear_buffer() {
    memset(buffer, 0, BUFFER_SIZE);
    cur = 0;
}

int input_offset = 0;

static inline char getchar() {
    char c = sys_getchar();
    if (c == '\b' || c == 127) {
        if (cur > 0) buffer[cur--] = ' ';
        sys_move_cursor_x(input_offset);
        printf("%s", buffer);
    } else if (isprint(c)) {
        buffer[cur++] = c;
        buffer[cur] = '\0';
        sys_move_cursor_x(input_offset);
        printf("%s", buffer);
    }
    return c;
}

void screen_clear() {
    sys_clear_region(0, SHELL_BEGIN + 1, SCREEN_WIDTH, SCREEN_HEIGHT);
}

int read_input(char* buf) {
    clear_buffer();

    char c;
    cur = 0;
    do {
        c = getchar();
        if (isprint(c)) {
            printf("%c", c);
            buf[cur++] = c;
        } else if (c == '\b' || c == 127) {
            if (cur > 0) cur--;
        }
    } while (c != '\n');
    buf[cur] = '\0';
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

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    for (int i = 0; i < CMD_POOL_SIZE; i++)
        cmd_queue[i] = i;
    screen_clear();
    input_offset = strlen(prompt);
    printf("------------------- YoRHa -------------------\n");
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear    
        printf("%s", prompt);
        clear_buffer();
        int len = read_input(buffer);
        cmd_t* cmd = parse_cmd(buffer, len); 
        if (cmd->argc == 0) {
            free_cmd(cmd);
            continue;
        }
        if (strcmp(cmd->argv[0], "ps") == 0) {
            if (cmd->argc != 1) {
                printf("Usage: ps\n");
                continue;
            }
        } else if (strcmp(cmd->argv[0], "exec") == 0) {
            int run_in_bg = 0;
            cmd->argc--; // remove "exec"
            if (cmd->argc > 0 && strcmp(cmd->argv[cmd->argc], "&") == 0) {
                run_in_bg = 1;
                cmd->argc--; // remove "&"
            }
            if (cmd->argc == 0) {
                printf("Usage: exec <programme> [args...]\n");
                free_cmd(cmd);
                continue;
            }
            cmd->argv++; // now points to programme
            pid_t pid = sys_exec(cmd->argv[0], cmd->argc, cmd->argv);
            if (pid == -1) {
                printf("exec failed\n");
                free_cmd(cmd);
                continue;
            }
            if (!run_in_bg)
                sys_waitpid(pid);
        } else if (strcmp(cmd->argv[0], "kill") == 0) {
            if (cmd->argc != 2) {
                printf("Usage: kill <pid>\n");
                continue;
            }
            pid_t pid = atoi(cmd->argv[1]);
            sys_kill(pid);
        } else if (strcmp(cmd->argv[0], "clear") == 0) {
            screen_clear();
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
