# Compiler warning policy (FR-046)

Every PixelCSS target is compiled by `pxc_set_warnings()` (`cmake/CompilerWarnings.cmake`) with:

```
/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /sdl /guard:cf
/external:anglebrackets /external:W0        (Release adds /O2 /GL and /LTCG)
```

- **`/W4 /WX`**: warnings are errors, so the diagnostic bar is enforced, not advisory.
- **Third-party headers** (doctest, the Windows SDK and the STL included with angle brackets) are compiled at
  `/external:W0`, so their warnings never fail the build.
- **`UNICODE`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`** are defined for every target.

## Suppressed warnings

None. The list `PXC_WARNING_SUPPRESSIONS` in `cmake/CompilerWarnings.cmake` is empty. To add one, append
`/wdNNNN` there **and** add a row here:

| Warning | Scope | Justification | Added |
|---|---|---|---|
| — | — | — | — |

## Rejected toolchains

`cmake/MsvcGuard.cmake` stops configuration unless `CMAKE_CXX_COMPILER_ID` is `MSVC` **and** the front end is
`cl.exe` (`CMAKE_CXX_COMPILER_FRONTEND_VARIANT` = `MSVC`), and the target is 64-bit:

- **GCC / MinGW**: not the mandated toolchain (FR-042); different ABI and CRT.
- **clang-cl**: MSVC-compatible but not `cl.exe`; golden images and benchmarks are pinned to the MSVC reference
  toolchain (research R-01), and the constitution names MSVC explicitly.
- **x86 / ARM64**: deferred (spec Assumptions); x64 only.
