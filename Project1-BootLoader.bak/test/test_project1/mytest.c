#include <kernel.h>

int s[4] = {1, 2, 4, 3};
int main() {
  if (s[0] != 1 || s[1] != 2 || s[2] != 4 || s[3] != 3) {
      bios_putstr("fail!\n\r");
  }
  return 0;
}
