#include <debugs.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/string.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>

#include "os/smp.h"

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 50
#define SCREEN_LOC(x, y) ((y)*SCREEN_WIDTH + (x))

extern uint64_t argraw(int n);
extern int argint(int n, int *ip);
/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};

static spin_lock_t screen_lock = {
    .status = UNLOCKED,
    .cpuid = -1,
};

/* cursor position */
static void vt100_move_cursor(int x, int y) {
  // \033[y;xH
  printv("%c[%d;%dH", 27, y, x);
}

/* clear screen */
static void vt100_clear() {
  // \033[2J
  printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor() {
  // \033[?25l
  printv("%c[?25l", 27);
}

/* write a char */
static void screen_write_ch(char ch, tcb_t *t) {
  if (ch == '\n') {
    t->cursor_x = 0;
    t->cursor_y++;
  } else {
    new_screen[SCREEN_LOC(t->cursor_x, t->cursor_y)] = ch;
    t->cursor_x++;
  }
}

// this should only be called by cpu 0
void init_screen(void) {
  vt100_hidden_cursor();
  vt100_clear();
  screen_clear();
}

void screen_clear(void) {
  spin_lock_acquire(&screen_lock);
  int i, j;
  for (i = 0; i < SCREEN_HEIGHT; i++) {
    for (j = 0; j < SCREEN_WIDTH; j++) {
      new_screen[SCREEN_LOC(j, i)] = ' ';
    }
  }
  spin_lock_release(&screen_lock);
  mythread()->cursor_x = 0;
  mythread()->cursor_y = 0;
  screen_reflush();
}

void screen_clear_region(int x, int y, int w, int h) {
  spin_lock_acquire(&screen_lock);
  int i, j;
  for (i = y; i < y + h; i++) {
    for (j = x; j < x + w; j++) {
      new_screen[SCREEN_LOC(j, i)] = ' ';
    }
  }
  spin_lock_release(&screen_lock);
  mythread()->cursor_x = x;
  mythread()->cursor_y = y;
  screen_reflush();
}

void screen_move_cursor(int x, int y) { vt100_move_cursor(x, y); }

void screen_write(char *buff) {
  spin_lock_acquire(&screen_lock);
  int i = 0;
  int l = strlen(buff);
  tcb_t *t = mythread();
  for (i = 0; i < l; i++) {
    screen_write_ch(buff[i], t);
  }
  spin_lock_release(&screen_lock);
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void) {
  spin_lock_acquire(&screen_lock);
  int i, j;

  /* here to reflush screen buffer to serial port */
  for (i = 0; i < SCREEN_HEIGHT; i++) {
    for (j = 0; j < SCREEN_WIDTH; j++) {
      /* We only print the data of the modified location. */
      if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)]) {
        vt100_move_cursor(j + 1, i + 1);
        bios_putchar(new_screen[SCREEN_LOC(j, i)]);
        old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
      }
    }
  }

  /* recover cursor position */
  vt100_move_cursor(mythread()->cursor_x, mythread()->cursor_y);
  spin_lock_release(&screen_lock);
}

syscall_transfer_v_p(do_write, screen_write);
// transfer do_reflush to screen_reflush
syscall_transfer_v_v(do_reflush, screen_reflush);

void do_move_cursor_x(void) {
  int x;
  tcb_t *t = mythread();
  if (argint(0, &x) < 0) {
    t->trapframe->a0() = -1;
    return;
  }
  t->cursor_x = x;
  t->trapframe->a0() = 0;
}

void do_move_cursor_y(void) {
  int y;
  tcb_t *t = mythread();
  if (argint(0, &y) < 0) {
    t->trapframe->a0() = -1;
    return;
  }
  t->cursor_y = y;
  t->trapframe->a0() = 0;
}

void do_move_cursor(void) {
  int x, y;
  tcb_t *t = mythread();
  if (argint(0, &x) < 0 || argint(1, &y) < 0) {
    t->trapframe->a0() = -1;
    return;
  }
  t->cursor_x = x;
  t->cursor_y = y;
  t->trapframe->a0() = 0;
}

void do_clear_region(void) {
  int x, y, w, h;
  tcb_t *t = mythread();
  if (argint(0, &x) < 0 || argint(1, &y) < 0 || argint(2, &w) < 0 ||
      argint(3, &h) < 0) {
    t->trapframe->a0() = -1;
    return;
  }
  screen_clear_region(x, y, w, h);
  t->trapframe->a0() = 0;
}