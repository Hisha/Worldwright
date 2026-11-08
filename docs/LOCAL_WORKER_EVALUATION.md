# Local Worker Evaluation — W001

This evaluation is based on the worker's committed artifacts, not on its
conversation transcript. The implementation was inspected at repository HEAD
`59335b5` ("Establish Forgehand baseline"), built with the documented commands,
and compared with the W001 requirements and the wowdev 3.3.5a WDT layout.

## 1. What the worker accomplished

The worker produced a complete-looking vertical slice, not just notes:

- A CMake project with a library target, CLI target, install rules, C++17
  settings, warning flags, and a test executable.
- A proposed API shaped around `TileMap`, `hasTile`, tile counting, coordinate
  bounds, and a `--list` CLI.
- A test suite skeleton with ten named cases, including invalid input,
  out-of-bounds coordinates, and enumeration.
- Synthetic-fixture helpers and a fixture generator concept.
- A written implementation plan (`docs/WDT_IMPLEMENTATION_PLAN.md`) that
  captured the intended file layout and deliverables.

Useful ideas worth retaining:

- The requested public behaviour (`hasTile`, existing-tile count, coordinate
  list, bounds) is the right shape for a WDT inspector.
- The CLI output concept and `--list` switch match the task.
- CMake/CTest as the build and test mechanism is correct for Worldwright.
- The test-case list is a reasonable starting checklist; several cases were
  carried into the completed suite.

## 2. Where the worker struggled

### 2.1 The core format model was wrong

This is the most important finding. The worker did not implement the WDT
format described in its own research references:

| Worker assumption | Actual 3.3.5a format |
|---|---|
| `MVER` is a magic number `0x57445431` | `MVER` is a chunk; its payload is a `uint32` version, `18`. |
| The file starts with a packed `WdtHeader { MVER; MPHD; }` | The file is a sequence of chunks: 4-byte id, `uint32` size, payload. |
| `MPHD` is a header field, not a chunk | `MPHD` is a 32-byte chunk whose first `uint32` is flags. |
| `MAIN` is 4096 one-byte entries | `MAIN` is 4096 eight-byte entries (flags + async id), 32768 bytes. |
| Tile value `1` is read from one byte | Tile existence is bit `0x1` of the entry's first `uint32`. |
| Chunk ids appear as `MVER` on disk | Chunk ids are little-endian 32-bit values; bytes appear as `REVM`. |

Evidence: `src/worldwright/WdtParser.h:10,27-42`,
`src/worldwright/WdtParser.cpp:28-45,84-96`,
`src/worldwright/WdtTileMap.cpp:18-46`. A real WDT starts with bytes
`52 45 56 4d 04 00 00 00 12 00 00 00`, not with `WDT1`.

Because of this, even a fully compiling version would have rejected every real
WDT and accepted only its own synthetic format.

### 2.2 Compilation errors

The baseline did not build. A clean configure succeeded; `cmake --build` failed
with errors in both the CLI and tests. Examples:

- `main.cpp` and tests used `TileMap` without `worldwright::` qualification
  (`src/worldwright/main.cpp:39`, `tests/WDT_tests.cpp:126` and later).
- The test runner declared `printSummary()` as `void` but returned `1`/`0`
  (`tests/WDT_tests.cpp:46-60`).
- Tests called overloads that do not exist, e.g.
  `tileCount(data.data(), data.size())` while the API accepts a `TileMap*`
  (`tests/WDT_tests.cpp:90-101`).
- Fixture code treated a `WdtHeader` as an array (`header[0]`) and passed
  `std::string` where `const char*` was required
  (`src/worldwright/TestUtils.cpp:16-23`, `tests/WDT_tests.cpp:83-84`).
- `delete data.data()` on a vector's storage was both invalid C++ and
  undefined behaviour (`tests/WDT_tests.cpp:110`).

Some errors would have been caught immediately by running the build once. The
worker reported an implementation without a successful compile.

### 2.3 Missing implementations and declarations

- `WdtParser::getMainData()` is declared (`WdtParser.h:71`) and called by
  `TileMap` (`WdtTileMap.cpp:18`) but never defined. A static-library build does
  not catch this because linking the CLI fails first.
- The private `WdtParser()` constructor is declared (`WdtParser.h:74`) but not
  defined, while `create()` calls it (`WdtParser.cpp:103`).
- `WdtParser::parse()` is unused by the public path; `create(data,size)` calls
  it but the result is never checked consistently.
- `TileMap` has two constructors and duplicates the same parsing/counting logic
  (`WdtTileMap.cpp:7-34` and `104-147`), including the same wrong layout.

### 2.4 Incorrect class declarations and const-correctness

- `WdtMainEntry` mixes `__attribute__((aligned(4)))` and `__attribute__((packed))`
  on a member function, producing a compiler warning and no useful layout
  (`WdtParser.h:35-42`).
- `setTile(const std::vector<uint8_t>& data, ...)` attempts to modify a const
  object (`TestUtils.h:35`, `TestUtils.cpp:163-179`).
- `parseMainChunk(const void* data, size_t size)` returns an interior pointer
  with no lifetime contract and no chunk-header validation
  (`WdtParser.cpp:112-125`).
- The public API exposes raw `const uint8_t*` main data (`getMainData`) even
  though callers only need tile existence.

### 2.5 Unnecessary abstractions and inconsistent API design

- Three layers (`WdtParser`, `TileMap`, and free `load`/`hasTile`/`tileCount`
  wrappers) wrap a 4 KB bitmap. Ownership is manual and unclear: `load()`
  returns a raw `new TileMap` (`Worldwright.cpp:51`) that the caller must
  `delete`.
- `tileCount()` means "grid positions" in one place
  (`WdtTileMap.cpp:58`, always 4096) and is surrounded by `existingTileCount()`;
  this directly violates the W001 requirement that `tileCount()` report
  existing terrain tiles.
- `minX()/maxX()` always return the full grid (`WdtTileMap.cpp:80-94`), not the
  bounds of existing tiles.
- Naming mixes `WdtParser`, `TileMap`, `load`, `existingTileCount`, and
  `ENTRY_EXISTS`, with no documented coordinate convention beyond "BLOCK_X".

### 2.6 Incomplete or ineffective tests

The test file could not compile, and beyond that the assertions were not
testing the parser:

- Tests 5-9 assert that a **valid** WDT fails to load:
  `assertFalse("Load valid WDT", tile_map == nullptr)`
  (`tests/WDT_tests.cpp:186-187`, `206-207`, `226-227`, `246-247`,
  `266-267`). In other words, they encode the opposite of the requirement.
- Presence checks use `assertTrue("Tile exists", exists)` without identifying
  which tile failed, so a failure is not diagnosable.
- `assertEqual` compares decimal `std::to_string` output against a hexadecimal
  string literal (`tests/WDT_tests.cpp:83-84`), so the comparison is wrong even
  if it compiled.
- Every test increments `passCount` unconditionally, so a test with failed
  checks is still reported as passed (`tests/WDT_tests.cpp:112`, and the same
  pattern in all ten tests).
- No fixture writes a real chunked WDT; the fixtures use the worker's invented
  header/main layout.
- CTest ran zero tests: `enable_testing()` was called, but the executable was
  never registered with `add_test` (`CMakeLists.txt:64-75`).
- There was no repeated-load test, so stale-state behaviour was untested.

### 2.7 Build configuration and verification issues

- Warnings were added globally rather than per target (`CMakeLists.txt:17-22`).
- `-DDEBUG` was added globally for tests (`CMakeLists.txt:80-83`); it is not a
  standard CMake build-type mechanism.
- The `build/` directory and its generated binaries are tracked in Git
  (baseline commit), so every build dirties the repository.
- The worker did not run the documented build, did not run `ctest`, and did not
  execute the CLI. There is no evidence of a single successful end-to-end
  verification.

### 2.8 GPL provenance

No copied Noggit/AzerothCore text was found in the worker files. The code is
original but wrong. The GPL concern is therefore not copying; it is that the
implementation was not checked against the documented format. The completed
W001 implementation replaces it and was written from the wowdev specification.

## 3. Model capability versus agent configuration

Not every failure should be attributed to model capability. Observable coding
problems (wrong constants, missing definitions, const errors) are capability or
process failures. Several surrounding factors likely made them worse:

| Factor | Evidence | Likely contribution |
|---|---|---|
| Context compaction | The task states it occurred; the worker rewrote whole files repeatedly. | Loss of earlier decisions and duplication across files. |
| No compile feedback loop | The baseline does not compile, yet the plan reads as complete. | The agent did not treat "build passes" as a gate. |
| Ambiguous task input | The illustrative API used `load()`/`WdtReader`, while the worker plan switched to `TileMap`/`WdtParser`. | API churn and three competing layers. |
| Over-large single task | Ten files plus CMake plus tests in one assignment. | Unfinished definitions and shallow tests. |
| Specification retrieval failure | The format model contradicts the wowdev pages cited in the repository research. | Every downstream component inherited the wrong layout. |
| Unclear "done" criteria | No required build/test output was recorded. | Optimistic completion claim. |

The worker should not be judged on the format research alone; the human/agent
harness did not provide a short, canonical format excerpt or require a
compile-and-test transcript. Still, a coding agent should run the build before
declaring success.

## 4. Disposition of the worker's code

| Component | Decision | Reason |
|---|---|---|
| `WdtParser.*` | Replaced | Wrong chunk model; missing definitions; raw-pointer API. |
| `WdtTileMap.*` | Replaced | Duplicated parsing; `tileCount` semantics wrong; grid bounds wrong. |
| `Worldwright.*` free wrappers | Replaced | Manual ownership and redundant API. |
| `TestUtils.*` | Replaced | Fixtures do not represent WDT files; const/mutation bugs. |
| `tests/WDT_tests.cpp` | Replaced | Did not compile; several tests asserted the opposite result. |
| `CMakeLists.txt` | Reworked | Keep CMake/CTest direction; fix targets, warnings, test registration, install. |
| CLI concept | Retained and reimplemented | Output and `--list` were appropriate. |
| Test-case checklist | Retained and expanded | Good coverage intent; actual assertions rebuilt. |
| `docs/WDT_IMPLEMENTATION_PLAN.md` | Marked superseded | Retained as an audit record, not as guidance. |

## 5. Recommended future task size

The local worker is better suited to bounded, verifiable work than to
open-ended binary-format implementation.

### Appropriate for the local worker

- Single-file changes with a named function, clear input/output, and existing
  tests to extend.
- Mechanical work: rename a symbol, add an overload, fix warnings, add a
  diagnostic, update CMake for one target.
- Test additions against an already stable API, especially table-driven
  malformed-input cases.
- Documentation updates that summarize files already in the repository.
- Small fixture generators and CLI argument handling once the format constants
  are fixed by a stronger model.
- Code search and summarization with citations, when the output is reviewed.

Suggested cap: **1-3 files, under roughly 200-300 changed lines, one acceptance
command**.

### Better handled by a stronger coding model

- First implementation of any binary format reader/writer (WDT, ADT, WDL, BLP,
  DBC, MPQ), where byte order, chunk ids, bitfields, and version differences
  must be reconciled with documentation.
- Cross-cutting API or architecture decisions, ownership models, and public
  headers meant to last.
- Build-system migrations, dependency/licensing decisions, and cross-platform
  CI.
- Tasks requiring independent validation against real client files or external
  tools.
- Any task where the format spec is incomplete or contradictory.

### Recommended handoff pattern

1. Provide a one-page spec excerpt and a byte-level example.
2. State the exact public API and acceptance commands.
3. Require the worker to paste the configure/build/test output.
4. Split parsing, CLI, tests, and documentation into separate tasks with
   checkpoints.
5. Keep a strong-model review gate before any parser output is trusted.

## 6. Summary

The worker showed good project instincts: it understood CMake, CTest, a library
plus CLI layout, synthetic fixtures, and the user-facing questions a WDT tool
must answer. It failed at the two things that matter most for this task: it
modelled the binary format incorrectly, and it did not compile or test its own
result. The completed W001 implementation keeps the useful shape and replaces
the parser, fixtures, tests, and build wiring.
