/* Implementation of the Schubfach algorithm:
 * https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f.
 * Copyright (c) 2025 - present, Victor Zverovich
 * Distributed under the MIT license (see LICENSE).
 */

#ifndef MULLE__DTOA_H
#define MULLE__DTOA_H

#ifdef __cplusplus
extern "C" {
#endif

#define MULLE__DTOA_BUFFER_SIZE 25

/* Writes the shortest correctly rounded decimal representation of `value` to
 * `buffer`. `buffer` should point to a buffer of size MULLE__DTOA_BUFFER_SIZE or larger.
 */
void mulle_dtoa(double value, char* buffer);

#ifdef __cplusplus
}
#endif

#endif  /* MULLE__DTOA_H */
