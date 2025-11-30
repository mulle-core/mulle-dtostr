# mulle-dtoa

A C implementation of the Schubfach algorithm - fast and accurate conversion
of IEEE-754 `double` values to decimal strings.

Usage:

```c
#include "mulle-dtoa.h"
#include <stdio.h>

int main(void) 
{
   char   buf[ MULLE__DTOA_BUFFER_SIZE];

   mulle_dtoa(6.62607015e-34, buf);
   puts( buf);
   return( 0);
}
```

Average formatting time from [dtoa-benchmark](https://github.com/fmtlib/dtoa-benchmark), smaller is better:

<img width="787" height="353" alt="image"
     src="https://github.com/user-attachments/assets/68c36484-2a1c-478c-89e4-8055880594cf" />

The binary size is ~16kiB:

```
% gcc -c -Os -DNDEBUG -std=c99 mulle-dtoa.c
% du -h mulle-dtoa.o
 16K	mulle-dtoa.o
```

