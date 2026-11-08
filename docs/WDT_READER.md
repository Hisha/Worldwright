# Worldwright WDT Reader (W001)

Status: implemented and tested. This document describes the read-only WDT
component completed for Worldwright task W001.

## Purpose and scope

The WDT reader loads a World of Warcraft 3.3.5a terrain `.wdt` file and answers
one question: **which 64 x 64 terrain tiles exist for this map?**

It is a small, dependency-free C++17 library with a command-line inspector. It
is intentionally limited to reading:

- the root WDT file;
- the `MVER`, `MPHD`, and `MAIN` chunks;
- terrain tile existence flags.

It does **not** read or write ADT, WDL, WMO, MPQ, or DBC data, and it does not
provide world-space geometry. Those are separate Worldwright tasks.

## Supported client version

Target: **World of Warcraft 3.3.5a, build 12340** (Wrath of the Lich King).

For this client the WDT format version (`MVER`) is **18**. A file whose `MVER`
value is anything else is rejected as unsupported. The formats of later clients
(Cataclysm and beyond) added new flags and chunks and are out of scope.

## WDT format overview

A WDT is a chunked binary file. Every chunk starts with an 8-byte header:

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 4 | Chunk identifier |
| 4 | 4 | Payload size in bytes (little-endian `uint32`) |

The payload follows immediately.

### Chunk identifiers and byte order

WoW stores a four-character identifier as a little-endian 32-bit value. On disk
the characters therefore appear reversed relative to the name used in format
documentation. For example, the `MVER` chunk begins with the bytes `R E V M`.
This reader decodes every integer explicitly as little-endian; it does not rely
on host byte order or packed structs.

### `MVER`

Four-byte payload containing the format version. For 3.3.5a it must be `18`.

### `MPHD`

32-byte map header. The first `uint32` is the flag word; the remaining 28 bytes
are not needed by this reader. Recognised flags:

| Bit | Name | Meaning |
|---:|---|---|
| `0x0001` | global WMO | The map has a global WMO instead of terrain. |
| `0x0002` | `adt_has_mccv` | ADTs are expected to contain vertex colours. |
| `0x0004` | `adt_has_big_alpha` | ADTs use the big-alpha layout. |
| `0x0008` | doodads sorted | ADT doodad references are sorted by size category. |

Unknown flag bits are retained in `headerFlags()` and otherwise ignored. The
reader does not validate ADT contents against these flags; that belongs to a
later ADT validator.

### `MAIN`

The terrain tile table:

- 64 x 64 = 4096 entries;
- each entry is 8 bytes: a little-endian `uint32` flags field followed by a
  little-endian `uint32` async/runtime field;
- total payload size is therefore 32768 bytes.

The only existence rule for a 3.3.5a terrain WDT is:

```
tile exists  <=>  (entry.flags & 0x0001) != 0
```

The second `uint32` is runtime-only and is ignored when reading files. Other
flag bits are ignored by this reader. In particular, the Cataclysm "all water"
bit is not interpreted for a 3.3.5a file.

Entries are stored row-major: `index = y * 64 + x`, with `y` as the outer loop.

### Optional and unknown chunks

Chunks that are not `MVER`, `MPHD`, or `MAIN` are skipped using their declared
size. Zero-sized unknown chunks are allowed. A chunk whose declared payload
extends past the end of the file is rejected as truncated.

### Required chunks

A terrain WDT must contain exactly one `MVER`, one `MPHD`, and one `MAIN` chunk.
Duplicate required chunks are rejected. A WMO-only WDT (global WMO flag set,
no `MAIN`) is reported as unsupported rather than silently treated as an empty
terrain map.

## Tile coordinate conventions

Tile coordinates are the coordinates used by WDT and by ADT file names:

- `x` is the grid column, `0` to `63`;
- `y` is the grid row, `0` to `63`;
- `(0, 0)` is the first row and first column of the `MAIN` table;
- the corresponding ADT file is `<map>_<x>_<y>.adt`.

The reader does **not** convert tile coordinates to world coordinates or yards.
The eventual map/project layer will own that conversion. In particular, do not
assume that `(0, 0)` is a particular compass corner; it is a table coordinate.

Example: a 3 x 3 island in columns 30-32 and rows 30-32 has bounds
`(30,30)` to `(32,32)` and nine existing tiles.

## Public API

Header: `include/worldwright/WdtReader.h`

```cpp
#include <worldwright/WdtReader.h>

worldwright::WdtReader reader;
if (!reader.load("World/Maps/TestWorld/TestWorld.wdt")) {
    std::cerr << reader.errorMessage() << '\n';
    return 1;
}

if (reader.hasTile(30, 30)) {
    // The ADT for column 30, row 30 exists.
}

const std::size_t existing = reader.tileCount();       // number of tiles
const auto tiles = reader.existingTiles();             // row-major (x, y) list
```

Main methods:

| Method | Behaviour |
|---|---|
| `bool load(path)` | Loads a file. Clears all previous state first. |
| `bool loadFromMemory(data, size)` | Parses a buffer without retaining it. |
| `bool isLoaded()` | Whether the most recent load succeeded. |
| `bool hasTile(x, y)` | True only for an in-range tile with the existence bit set. |
| `std::size_t tileCount()` | Number of **existing terrain tiles**, never 4096 unless all tiles exist. |
| `existingTiles()` | Existing tiles in row-major order. |
| `tileBounds(...)` | Inclusive bounding box; false when there are no tiles. |
| `version()` | `MVER` value, or 0 when nothing is loaded. |
| `headerFlags()` | Full `MPHD` flag word, or 0 when nothing is loaded. |
| `hasGlobalWmo()`, `hasMccv()`, `hasBigAlpha()` | Named `MPHD` flag checks. |
| `error()`, `errorMessage()` | Failure category and human-readable diagnostic. |

`WdtReader` is copyable and movable. It keeps no global state. After a
successful load, const access is safe to share between threads as long as no
thread calls `load()` on the same object concurrently.

**Stale-data guarantee:** `load()` and `loadFromMemory()` clear the previous
result before parsing. If a later load fails, `isLoaded()` is false,
`tileCount()` is zero, `existingTiles()` is empty, and `version()` and
`headerFlags()` are zero. A previous successful file is never left visible.

## Error handling

Failures are reported by the return value and by `error()`:

| `WdtError` | Meaning |
|---|---|
| `None` | No error. |
| `FileOpenFailed` | The path could not be opened. |
| `FileReadFailed` | The file could not be sized or read completely. |
| `FileTooLarge` | File exceeds the 64 MiB safety limit. |
| `TruncatedData` | Empty file, incomplete chunk header, or chunk payload past end of file. |
| `MissingRequiredChunk` | `MVER`, `MPHD`, or terrain `MAIN` is absent. |
| `DuplicateChunk` | More than one `MVER`, `MPHD`, or `MAIN`. |
| `InvalidChunkSize` | A required chunk has the wrong payload size. |
| `UnsupportedVersion` | `MVER` is not 18. |
| `UnsupportedWmoOnly` | Global-WMO WDT without a terrain `MAIN` grid. |

## Command-line inspector

Build target: `worldwright-wdt-info`.

```text
worldwright-wdt-info <file.wdt>
worldwright-wdt-info <file.wdt> --list
worldwright-wdt-info --help
```

Default output:

```text
Worldwright WDT Inspector
File: TestWorld.wdt
Version: 18
Grid: 64 x 64
Existing tiles: 9
Tile bounds: (30,30) to (32,32)
```

`--list` adds the existing coordinates:

```text
Existing tiles:
  (30,30)
  (31,30)
  ...
```

Exit codes:

- `0`: file parsed successfully, or `--help`;
- nonzero: usage error, unknown option, missing file, or invalid WDT.

## Build instructions

Requirements:

- CMake 3.16 or newer;
- a C++17 compiler (tested with GCC 15.2 on Linux; no platform-specific code);
- no external libraries.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The project enables `-Wall -Wextra -Wpedantic` for GCC and Clang, and `/W4` for
MSVC. The verified build is warning-free.

Run the inspector:

```bash
./build/worldwright-wdt-info path/to/TestWorld.wdt --list
```

Install (optional):

```bash
cmake --install build --prefix /some/prefix
```

This installs the static library, the public header directory, and the CLI.

## Test coverage

Tests are registered with CTest:

| CTest name | What it covers |
|---|---|
| `wdt_reader_unit` | Parser API and malformed-input cases |
| `wdt_fixtures` | Generates synthetic WDT files for CLI tests |
| `wdt_cli` | Runs the real CLI and checks output and exit codes |

The unit suite performs 219 checks across 19 test groups, including:

1. valid empty terrain grid;
2. one existing terrain tile;
3. nine existing terrain tiles;
4. correct x/y coordinate lookup and row-major ordering;
5. out-of-range and negative coordinates;
6. missing `MVER`, `MPHD`, and `MAIN`;
7. truncated chunk headers and payloads;
8. invalid `MVER`, `MPHD`, and `MAIN` sizes;
9. unsupported `MVER` versions;
10. malformed/duplicate `MAIN`;
11. duplicate required chunks;
12. unknown/optional chunks before and after `MAIN`;
13. `MPHD` flag recognition;
14. `MAIN` flag semantics (only bit 0 means "exists");
15. full 4096-tile grid and bounds;
16. repeated loading, and clearing stale state after a failed load;
17. path loading, including names with spaces;
18. trailing bytes shorter than a chunk header;
19. a byte-level check that generated fixtures use the documented on-disk
    layout (including the reversed chunk identifiers).

Fixtures are generated at test time. No Blizzard assets are stored in the
repository.

### Synthetic versus real-client validation

The automated suite validates parser behaviour against **synthetic** fixtures
built from the documented 3.3.5a layout, plus a byte-level layout assertion.

As an additional manual check, the inspector was run read-only against a WDT
created by a local Noggit workspace (not a stock Blizzard file):

```text
File: .../Noggit_Workspace/world/maps/testing/testing.wdt
Size: 32,828 bytes
SHA-256: 2861a898de98abedf634cff3736de4ca57fa42c71592ca8f1a4d2aafdd7ad1a5
Result: version 18, 4 existing tiles, bounds (26,29) to (27,30)
```

An independent Python scan of the same bytes found the same four tiles. This
shows compatibility with one independently produced file. It is **not** a
claim that the reader has been validated against stock WoW 3.3.5a client
archives; that test requires a legally obtained client copy and remains open.

## Known limitations

- The reader itself is read-only. W002 adds a separate WDT writer; see
  `WDT_WRITER.md`.
- Root terrain WDT only; WMO-only WDTs are rejected.
- One version only: `MVER` 18 (3.3.5a).
- Exact chunk sizes are required for `MVER` (4), `MPHD` (32), and `MAIN`
  (32768), matching the documented 3.3.5a layout.
- Non-zero `MAIN` async fields and flag bits other than `0x0001` are ignored.
- No `MWMO`/`MODF` parsing and no global-WMO data exposed.
- Trailing bytes that do not form a complete 8-byte chunk header are rejected.
- The whole file is read into memory, with a 64 MiB safety limit.
- No world-space conversion, MPQ reading, or ADT validation.
- No automated test against stock Blizzard client WDT files.

## Future integration with Worldwright

The reader is the first reusable format component and is meant to stay small:

- The future project/map loader can call `WdtReader` to discover which tiles a
  map declares, then load or create only those ADTs.
- A future WDT writer should reuse the constants and coordinate conventions
  here and add round-trip tests (`write -> read -> identical tile set`). W002
  implements this in `WDT_WRITER.md`.
- The content pipeline's planned `terrain[]` validation can use this reader to
  check that the WDT tile table matches the ADTs listed in an EPF manifest.
- The CLI is suitable for diagnostics and CI; it does not write anything.

## Provenance and licensing

The implementation was written from the wowdev WDT format documentation and
the Phase 0 research in this repository. It does not copy code from Noggit,
AzerothCore, mod-content-manager, or Portalkeeper, and it has no GPL
dependencies. This follows the clean implementation direction recommended in
`LICENSING_REVIEW.md` (decision D1-b).

Reference: <https://wowdev.wiki/WDT> (revision `oldid=37101`, retrieved
2026-11-07).
