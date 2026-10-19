### 0.2.1



* ``mulle_dtostr`` now produces correct results on platforms without a native 128-bit integer type (the `umul128` fallback counted the high half of `p0` twice)
* NaN sign handling in tests is now deterministic across compilers

## 0.2.0






feature: add locale-independent string to double conversion with bit-exact dtostr roundtrip

* new `mulle_strtod` API parses decimal strings into doubles with a configurable locale syntax (decimal point, grouping, exponent characters)
* bit-exact roundtrip: every string emitted by `mulle_dtostr` parses back to the identical bit pattern
* drop-in `mulle_strtod` and `mulle_strtod_len` wrappers for libc strtod
* parser returns scan status codes for overflow, underflow and no conversion, never touches errno
* single-digit significands now print without a stray decimal point (e.g. "1e+308" instead of "1.e+308")


### 0.1.2

Various small improvements
