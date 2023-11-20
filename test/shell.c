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
#include <ctype.h>

#define SHELL_BEGIN 20
#define BUFFER_SIZE 256

const char* prompt = "> root@UCAS_OS: ";
char buffer[BUFFER_SIZE];
int cur = 0;

void clear_buffer() {
    memset(buffer, 0, BUFFER_SIZE);
    cur = 0;
}

char* read_input(int* len) {
    clear_buffer();
    printf("%s", prompt);

    char c;
    do {
        c = sys_getchar();
        if (isprint(c)) {
            printf("%c", c);
            buffer[cur++] = c;
        } else if (c == '\b' || c == 127) {
            if (cur > 0) cur--;
        }
    } while (c != '\n');
    buffer[cur] = '\0';
    *len = cur;
    return buffer;
}
int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- YoRHa -------------------\n");
    printf("             Glory for mankind!              \n");
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear    
        printf("%s", prompt);
        int len = 0;
        char* cmd = read_input(&len);
        
        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}
