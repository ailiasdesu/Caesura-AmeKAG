---
title: "Clean build exposes stale include paths to api/ headers"
date: 2026-06-12
category: build-errors
module: storage
problem_type: build_error
component: tooling
symptoms:
  - "fatal error C1083: 无法打开包括文件: 'ISaveProvider.h': No such file or directory"
  - "Linker errors for NullAnimationBackend methods in test project"
  - "Build passes after incremental change but fails on clean rebuild"
root_cause: missing_include
resolution_type: code_fix
severity: medium
tags: [cmake, include-path, clean-build, api-directory, test-project]
---

# Clean build exposes stale include paths to api/ headers

## Problem

When a header is in an `api/` subdirectory (`src/storage/api/ISaveProvider.h`) but the
corresponding `.cpp` file uses a bare include (`#include "ISaveProvider.h"`), the build
succeeds during incremental builds (cached object files) but fails on clean rebuild.

## Symptoms

- `fatal error C1083: 无法打开包括文件: "ISaveProvider.h"` — compiler cannot resolve
  the include because it looks relative to the `.cpp` file's directory, not the `api/` subdirectory
- Only surfaces during `cmake --build --clean-first` or fresh clone builds
- The main engine target may pass (if the file is only in the engine's source list)
  while the test target fails (if the test project also needs the file)

## What Didn't Work

- Checking only the engine target's build — the file compiled fine there, but the test
  project's separate CMakeLists.txt had a different include resolution

## Solution

Prefix the include with the `api/` subdirectory:

```cpp
// Before (broken on clean build):
#include "ISaveProvider.h"

// After (correct):
#include "api/ISaveProvider.h"
```

For test projects that include source files directly (not via a library target), also
ensure the source file is listed in the test's `CMakeLists.txt` `TEST_SOURCES`:

```cmake
# tests/CMakeLists.txt
set(TEST_SOURCES
    ...
    ${CMAKE_SOURCE_DIR}/src/live2d/NullAnimationBackend.cpp  # added for R2.1
    ...
)
```

## Why This Works

MSVC resolves `#include "file.h"` by first searching the directory of the including
file, then the include paths. When `ISaveProvider.cpp` is at `src/storage/` and the
header is at `src/storage/api/`, the compiler cannot find it without the `api/` prefix.
Incremental builds mask this because the old `.obj` file still exists.

## Prevention

- After any header move or rename, run `cmake --build . --clean-first` at least once
  to surface stale include paths
- When adding new `.cpp` files that are needed by the test project, add them to
  `tests/CMakeLists.txt` explicitly — do not rely on library-level linking
- grep for includes that reference headers in `api/` without the `api/` prefix:
  ```bash
  rg '#include\s+"[^/]*\.h"' src/ --iglob '*.cpp' | rg -v '/api/'
  ```
