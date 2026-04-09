## 0.1.0






* Public API functions `mulle_dtostr_decompose` and `mulle_dtostr` are annotated with `MULLE__DTOSTR_GLOBAL` so symbols are exported correctly for shared builds.
* Header now uses mulle-c11 build macros and drops the explicit extern "C" block — **BREAKING**: C++ consumers may need to ensure C linkage (wrap includes or rely on mulle-c11 macros).
