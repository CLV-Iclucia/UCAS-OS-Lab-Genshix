#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/**
 * The ascii airplane is designed by Joan Stark
 * from: https://www.asciiart.eu/vehicles/airplanes
 */

static char blank[] = {
    "                                                                   "};
static char plane1[] = {"         _       "};
static char plane2[] = {"       -=\\`\\     "};
static char plane3[] = {"   |\\ ____\\_\\__  "};
static char plane4[] = {" -=\\c`\"\"\"\"\"\"\" \"`)"};
static char plane5[] = {"    `~~~~~/ /~~` "};
static char plane6[] = {"      -==/ /     "};
static char plane7[] = {"        '-'      "};

int main(int argc, char** argv) {
  int j = 10;
  int times = 1;
  if (argc == 2) {
    times = atoi(argv[1]);
  } else {
    sys_move_cursor(0, j + 0);
    printf("%s\n", "Usage: fly [times]");
    return 0;
  }
  if (times < 0) {
    sys_move_cursor(0, j + 0);
    printf("Error: times must be positive or zero\n");
    return 0;
  }
  while (times--) {
    for (int i = 0; i < 50; i++) {
      /* move */
      sys_move_cursor(i, j + 0);
      printf("%s", plane1);

      sys_move_cursor(i, j + 1);
      printf("%s", plane2);

      sys_move_cursor(i, j + 2);
      printf("%s", plane3);

      sys_move_cursor(i, j + 3);
      printf("%s", plane4);

      sys_move_cursor(i, j + 4);
      printf("%s", plane5);

      sys_move_cursor(i, j + 5);
      printf("%s", plane6);

      sys_move_cursor(i, j + 6);
      printf("%s", plane7);
    }

    sys_move_cursor(0, j + 0);
    printf("%s", blank);

    sys_move_cursor(0, j + 1);
    printf("%s", blank);

    sys_move_cursor(0, j + 2);
    printf("%s", blank);

    sys_move_cursor(0, j + 3);
    printf("%s", blank);

    sys_move_cursor(0, j + 4);
    printf("%s", blank);

    sys_move_cursor(0, j + 5);
    printf("%s", blank);

    sys_move_cursor(0, j + 6);
    printf("%s", blank);
  }
}
