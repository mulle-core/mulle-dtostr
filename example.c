#include "mulle-dtoa.h"
#include <stdio.h>

int main(void) {
  char buf[MULLE__DTOA_BUFFER_SIZE];
  mulle_dtoa(6.62607015e-34, buf);
  puts(buf);
  return 0;
}
