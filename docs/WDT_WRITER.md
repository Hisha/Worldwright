# Worldwright WDT Writer and World Model (W002)

Status: implemented and tested. This document describes the world definition
model, the WoW 3.3.5a terrain WDT writer, and the `worldwright-wdt-create`
command-line tool added for Worldwright task W002.

W001 (the WDT reader) remains unchanged and continues to pass its full test
suite.

## Purpose and scope

W002 establishes that Worldwright can **create** a new continent definition:

- a format-independent `WorldDefinition` model with a fixed 64 x 64 terrain
  grid;
- a deterministic `WdtWriter` that serializes the model to a valid
  World of Warcraft 3.3.5a (build 12340) terrain `.wdt` file;
- a small creation CLI for development and validation;
- raw-byte format tests plus writer/reader round-trip tests.

It does **not** write ADT, WDL, WMO, MPQ, DBC, or GUI code. It does not modify
the WoW client, AzerothCore, mod-content-manager, or Portalkeeper.

## Architecture

### `WorldDefinition`

Header: `include/worldwright/WorldDefinition.h`
Implementation: `src/worldwright/WorldDefinition.cpp`

```cpp
namespace worldwright {

struct TileCoord { int x; int y; };
struct TileBounds { int minX; int minY; int maxX; int maxY; };

class WorldDefinition {
public:
    static constexpr int GridSize = 64;
    static constexpr int GridWidth = 64;
    static constexpr int GridHeight = 64;
    static constexpr std::size_t TileCapacity = 4096;

    static bool isValidTile(int x, int y) noexcept;

    bool setTile(int x, int y, bool enabled);
    bool hasTile(int x, int y) const noexcept;
    std::size_t tileCount() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

    std::vector<TileCoord> enabledTiles() const;
    std::optional<TileBounds> bounds() const noexcept;
};

}
```

Design properties:

- No Qt, OpenGL, AzerothCore, MPQ, or other external dependency.
- Value type: copyable, movable, no owning raw pointers, no class hierarchy.
- Default-constructed state is a valid **empty world**.
- An empty world reports `tileCount() == 0` and `bounds() == std::nullopt`.
  Using `std::optional` makes "no bounds" unambiguous rather than relying on
  sentinel coordinates.
- Invalid coordinates are rejected safely. Negative values and values outside
  `[0, 63]` return `false` from `setTile()` and `false` from `hasTile()`; they
  never index outside the array.
- Enabling an already enabled tile does not double-count. Disabling an already
  disabled tile is harmless.
- `enabledTiles()` enumerates in row-major order (y outer, x inner), matching
  the MAIN table order and `WdtReader::existingTiles()`.
- The model is deliberately small. Metadata, height data, texture layers, and
  other future world state can be added later without changing the writer's
  public contract.

### Coordinate convention

Coordinates are WDT grid coordinates, not world-space yards:

- `x` is the grid column, `0` to `63`;
- `y` is the grid row, `0` to `63`;
- `(0, 0)` is the first row and first column of the MAIN table;
- the corresponding ADT file name is `<map>_<x>_<y>.adt`.

This is the same convention used by `WdtReader` and documented in
`WDT_READER.md`.

### `WdtWriter`

Header: `include/worldwright/WdtWriter.h`
Implementation: `src/worldwright/WdtWriter.cpp`

```cpp
namespace worldwright {

enum class WdtWriteError {
    None, InvalidPath, OutputExists,
    OpenFailed, WriteFailed, ReplaceFailed,
};

struct WdtWriteOptions { bool overwrite = true; };

class WdtWriter {
public:
    static constexpr std::uint32_t kVersion = 18;
    static constexpr std::size_t kSerializedSize = 32828;

    static std::vector<std::uint8_t> serialize(const WorldDefinition& world);

    bool save(const WorldDefinition& world,
              const std::filesystem::path& outputPath,
              const WdtWriteOptions& options = {});

    WdtWriteError error() const noexcept;
    const std::string& errorMessage() const noexcept;
};

}
```

`serialize()` is a pure in-memory function and is the single source of truth for
the byte layout. `save()` calls it and writes the complete result. Tests can
therefore inspect bytes without touching the filesystem, and a future export
pipeline can reuse the same serializer.

The writer has no dependency on the reader. The reader is used only by tests to
verify round trips.

## Supported format

Target: **World of Warcraft 3.3.5a, build 12340**, ordinary terrain-based WDT.

Generated file structure, in order:

| Chunk | Payload size | Purpose |
|---|---:|---|
| `MVER` | 4 | Format version, value 18 |
| `MPHD` | 32 | Map header; flag word plus 28 reserved bytes |
| `MAIN` | 32768 | 64 x 64 table of 8-byte tile entries |

Total generated size: **32,828 bytes**.

No timestamps, random identifiers, file paths, or environment-dependent values
are written. The same `WorldDefinition` always produces byte-identical output.

### On-disk chunk encoding

Chunk identifiers are 32-bit little-endian values. The characters therefore
appear reversed on disk relative to their documented names:

| Logical name | On-disk bytes |
|---|---|
| `MVER` | `52 45 56 4D` (`REVM`) |
| `MPHD` | `44 48 50 4D` (`DHPM`) |
| `MAIN` | `4E 49 41 4D` (`NIAM`) |

Each chunk header is an 8-byte pair of little-endian `uint32` values: identifier
then payload size.

### Exact byte layout

| File offset | Size | Field |
|---:|---:|---|
| 0 | 4 | `REVM` chunk identifier |
| 4 | 4 | `MVER` payload size = 4 |
| 8 | 4 | Version = 18 (`12 00 00 00`) |
| 12 | 4 | `DHPM` chunk identifier |
| 16 | 4 | `MPHD` payload size = 32 |
| 20 | 4 | MPHD flags = 0 (`00 00 00 00`) |
| 24 | 28 | MPHD reserved bytes, all zero |
| 52 | 4 | `NIAM` chunk identifier |
| 56 | 4 | `MAIN` payload size = 32768 (`00 80 00 00`) |
| 60 | 32768 | 4096 MAIN entries |
| 32828 | — | End of file |

### MAIN entries

Each entry is 8 bytes:

| Entry offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Flags (`uint32`, little-endian) |
| 4 | 4 | Async/runtime field (`uint32`, little-endian), always 0 |

The only writer rule is:

```text
enabled tile  =>  entry flags = 0x00000001, async = 0
disabled tile =>  entry flags = 0x00000000, async = 0
```

Entries are stored row-major:

```text
entry index = y * 64 + x
entry file offset = 60 + (y * 64 + x) * 8
```

The async field is runtime-only. Real 3.3.5a terrain WDTs store zero there and
the client regenerates it, so the writer always emits zero.

### MPHD

The writer emits a 32-byte MPHD payload with:

- flag word `0x00000000`;
- the remaining 28 bytes zero.

For a new terrain map this is the conservative choice. The reader recognises
these optional flags from existing files but the writer does not set them:

| Bit | Name |
|---:|---|
| `0x0001` | global WMO |
| `0x0002` | ADTs contain vertex colours (`adt_has_mccv`) |
| `0x0004` | ADTs use big alpha |
| `0x0008` | ADT doodads sorted by size |

Those flags describe ADT content or a WMO-only map and are outside W002. They
are documented here so a later milestone can extend the model deliberately.

### Optional chunks

Stock Blizzard terrain WDTs in the local validation set include an additional
empty `MWMO` chunk after `MAIN` (8 bytes, payload size 0), making those files
32,836 bytes. `MWMO`/`MODF` describe a global WMO and are not needed for an
ordinary terrain map. Worldwright currently omits optional chunks, so generated
files end at 32,828 bytes. `WdtReader` accepts both forms.

## Command-line creation tool

Build target: `worldwright-wdt-create`.

```text
worldwright-wdt-create --output <file.wdt> [options]

Options:
  --output <file.wdt>        Output WDT path (required)
  --tile <x> <y>             Enable one terrain tile; repeatable
  --rectangle <minX> <minY> <maxX> <maxY>
                             Enable an inclusive rectangular region; repeatable
  --overwrite                Replace an existing output file
  --help, -h                 Show help
```

Examples:

```bash
# Empty world
worldwright-wdt-create --output NewWorld.wdt

# 3 x 3 island: nine enabled tiles
worldwright-wdt-create --output NewWorld.wdt --rectangle 30 30 32 32

# One tile
worldwright-wdt-create --output NewWorld.wdt --tile 32 32

# Several tiles
worldwright-wdt-create --output NewWorld.wdt \
    --tile 30 30 --tile 31 30 --tile 32 30

# Replace an existing file
worldwright-wdt-create --output NewWorld.wdt --rectangle 30 30 32 32 --overwrite
```

Successful output:

```text
Worldwright WDT Creator
Output: NewWorld.wdt
Version: 18
Grid: 64 x 64
Enabled tiles: 9
Tile bounds: (30,30) to (32,32)
Bytes written: 32828
```

Argument handling:

- coordinates must be decimal integers in `[0, 63]`;
- `--rectangle` rejects `minX > maxX` or `minY > maxY`;
- `--tile` and `--rectangle` may both be repeated and may be combined;
- duplicate selections are harmless;
- no `--tile`/`--rectangle` means a valid empty world;
- an existing output file is rejected unless `--overwrite` is supplied;
- errors go to stderr and return exit code 1;
- `--help` returns exit code 0.

Command-line parsing is dependency-free. It does not use Boost, getopt, or any
external argument library.

## Safe output behavior

`WdtWriter::save()` follows a complete-file-or-fail-cleanly policy:

1. Validate the destination path before writing anything:
   - reject an empty path or a path with no filename;
   - require the parent directory to exist and be a directory;
   - reject a destination that is itself a directory;
   - reject an existing destination when `overwrite == false`.
2. Serialize the entire WDT in memory first. A serialization failure cannot
   truncate an existing file because the destination has not been opened.
3. Create a temporary file in the **destination directory** (not in `/tmp` or
   another filesystem), named like
   `.NewWorld.wdt.worldwright-<counter>.tmp`.
4. Write, flush, and close the temporary file, checking every operation.
5. Rename the temporary file over the destination.
6. On any failure after the temporary file is created, remove it and leave the
   previous destination untouched.

Platform behavior:

- **POSIX/Linux:** `std::filesystem::rename()` replaces an existing destination
  atomically within the same directory. A reader sees either the old complete
  file or the new complete file, never a partially written file.
- **Windows:** `std::filesystem::rename()` does not replace an existing
  destination. When overwriting, the writer removes the destination first and
  then renames the temporary file. That replacement is **not atomic**; if the
  process or filesystem fails between those two operations, the destination can
  be absent. The old file is never replaced with partial content, but Windows
  callers should treat replacement as best-effort. A future milestone can use a
  platform replacement API if needed.

Other boundaries:

- The writer does not create missing parent directories.
- The writer does not coordinate concurrent writers to the same destination.
  Two concurrent saves to one path are not supported.
- The writer does not call `fsync`; durability after a power loss is left to the
  filesystem. An abnormal process termination can leave a temporary file
  behind, but it cannot leave a partially written destination.
- The temporary file name is not part of the output and does not affect
  determinism.
- Unrelated files are not read, modified, or deleted.

## Build instructions

Requirements:

- CMake 3.16 or newer;
- a C++17 compiler (verified with GCC 15.2 on Linux);
- no external libraries or new dependencies.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Targets:

| Target | Purpose |
|---|---|
| `WorldwrightWdtReader` | W001 reader library |
| `WorldwrightWdtWriter` | W002 world model and writer library |
| `worldwright-wdt-info` | W001 inspector CLI |
| `worldwright-wdt-create` | W002 creation CLI |
| `worldwright-wdt-tests` | W001 unit tests |
| `worldwright-wdt-writer-tests` | W002 model, writer, and round-trip tests |
| `worldwright-wdt-test-fixtures` | synthetic W001 CLI fixtures |

## Test coverage

CTest registers five tests:

| CTest name | What it covers |
|---|---|
| `wdt_reader_unit` | W001 parser API and malformed input (219 checks) |
| `wdt_writer_unit` | W002 model, raw serialization, safety, round trips (359 checks) |
| `wdt_fixtures` | Generates synthetic WDT files for the W001 CLI test |
| `wdt_cli` | W001 inspector output and exit codes |
| `wdt_create_cli` | W002 creation CLI, round trips, determinism, overwrite refusal |

The W002 unit suite has 23 named test groups:

**World model**

1. empty world;
2. enable/disable a tile;
3. idempotent enable/disable (no double-counting);
4. invalid coordinates;
5. boundary coordinates `(0,0)` and `(63,63)`;
6. clearing a world;
7. enumeration order and bounding rectangles.

**Serialization**

8. empty-world raw layout;
9. independent chunk walker: exactly three chunks, correct order and sizes;
10. single-tile raw MAIN entry;
11. boundary tiles map to the first and last MAIN entries;
12. nine-tile rectangle raw layout;
13. full 64 x 64 raw layout;
14. deterministic in-memory serialization.

**Round trip and safety**

15. empty world round trip;
16. single tile round trip;
17. nine tiles round trip;
18. full grid round trip;
19. every coordinate in a sparse world round trip;
20. repeated saves produce identical files;
21. invalid output paths;
22. existing-file overwrite protection;
23. failed saves leave no partial output and do not poison later saves.

Raw-byte tests do not use `WdtReader`. They independently assert:

- on-disk chunk identifiers (`REVM`, `DHPM`, `NIAM`);
- chunk ordering and payload sizes;
- MVER value 18;
- MPHD payload size, zero flags, and zero reserved bytes;
- MAIN payload size 32768;
- entry offsets computed as `60 + (y * 64 + x) * 8`;
- little-endian flags (`01 00 00 00` for enabled);
- zero async fields;
- exact counts of enabled/disabled entries.

The CLI test additionally creates files with the real executable, reads them back
with the real inspector, checks bounds and counts, compares SHA-256 hashes for
determinism, and verifies that an existing file is unchanged when overwrite is
refused.

## Real-world validation

The four extracted 3.3.5a WDT files used by W001 were still available at
`/tmp/worldwright-wdt-test/`. They are local test inputs only; no game assets are
stored in this repository.

`worldwright-wdt-info` and an independent Python chunk/MAIN scanner agreed
exactly:

| Map | Existing tiles | Bounds |
|---|---:|---|
| Azeroth | 687 | (24,20) to (44,61) |
| Kalimdor | 988 | (0,0) to (48,55) |
| Northrend | 1131 | (11,9) to (49,37) |
| Expansion01 | 800 | (12,6) to (60,44) |

Additional cross-check: for each real file, the first 32,828 bytes (MVER +
MPHD + MAIN, using that file's own MPHD flags) were reconstructed independently
with the writer's documented layout and compared byte-for-byte. All four matched
exactly. Each real file then has an 8-byte empty `MWMO` chunk at offset 32,828.

This validates the serialization layout against independently produced files.
It is **not** a claim that a generated WDT has been loaded by the actual WoW
client. Client acceptance remains untested and is deferred to a later milestone
with a legally obtained test client.

## EPF and mod-content-manager boundary

W002 generates standalone `.wdt` files only.

Worldwright does **not**:

- build MPQ archives;
- define a competing patch or package format;
- install client patches;
- modify an installed WoW client;
- bypass mod-content-manager or Portalkeeper.

The intended future flow remains:

```text
WorldDefinition
    -> WdtWriter::serialize()
    -> standalone WDT (and later ADT/WDL)
    -> Worldwright export directory
    -> mod-content-manager EPF pipeline
    -> Portalkeeper client delivery
```

`serialize()` returning a byte vector keeps the writer independent of file
packaging, so a future EPF exporter can consume the bytes without Worldwright
learning MPQ internals. No changes to mod-content-manager or Portalkeeper were
made for W002.

## Licensing and provenance

The implementation was written from the wowdev WDT format documentation and the
Phase 0 research in this repository. It does not copy code from Noggit,
AzerothCore, mod-content-manager, or Portalkeeper. It has no GPL dependency and
introduces no third-party library.

Reference: <https://wowdev.wiki/WDT> (revision `oldid=37101`, retrieved
2026-11-07), plus the independently observed layout of the four local 3.3.5a
files described above.

## Known limitations

- Terrain WDT only. WMO-only WDTs are not written.
- WDT version 18 only (3.3.5a build 12340).
- Only `MVER`, `MPHD`, and `MAIN` are written. `MAID`, `MWMO`, and `MODF` are
  not written.
- MPHD flags are fixed to zero. There is no model API yet for `adt_has_mccv`,
  `adt_has_big_alpha`, sorted doodads, or global WMO.
- No ADT, WDL, DBC, BLP, M2, WMO, or MPQ writing.
- No world-space or coordinate-system conversion; coordinates are table
  coordinates only.
- No automatic parent-directory creation.
- No concurrent-writer coordination.
- Windows overwrite replacement is not atomic (see above).
- Generated files have not been tested in the WoW client.
- No GUI. The CLI is a development and validation tool, not the eventual editor.

## Future integration with the Worldwright GUI

The intended GUI integration is deliberately straightforward:

- `WorldDefinition` becomes (or is owned by) the editor's world document model.
- Tile selection, rectangle tools, and import/export operate on the same
  `setTile` / `enabledTiles` / `bounds` API used by the CLI.
- The GUI can call `WdtWriter::serialize()` for in-memory validation and
  `WdtWriter::save()` for project output.
- A future command/undo layer can wrap `setTile` operations without changing
  the model or writer.
- Map metadata and ADT/WDL content can be added to the model in later
  milestones without rewriting the WDT writer's tile serialization.
- Export remains a separate step that produces files for the mod-content-manager
  EPF pipeline; the GUI never writes into a WoW client directory.

W003 has not been started. Candidate next work is ADT writing for the tiles
declared by a `WorldDefinition`, followed by client-format validation.
