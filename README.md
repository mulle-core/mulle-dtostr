# mulle-dtostr

#### 🧶 Double to string conversion

A C implementation of the Schubfach algorithm - fast and accurate conversion
of IEEE-754 `double` values to decimal strings. Basically a fork of
[vitaut/zimj](https://github.com/vitaut/zimj), ported to C then
recoded for the needs of [mulle-sprintf](//github.com/mulle-core/mulle-sprintf)


* It should be faster than anything else
* It tests for dtostr <-> strtod roundtrip value compatibility

## Usage

```c
#include <mulle-dtostr/mulle-dtostr.h>
#include <stdio.h>

int main(void) 
{
   char   buf[ MULLE__DTOSTR_BUFFER_SIZE];

   mulle_dtostr(6.62607015e-34, buf);
   puts( buf);
   return( 0);
}
```


![footer](https://www.mulle-kybernetik.com/pix/heartlessly-vibecoded.png)
