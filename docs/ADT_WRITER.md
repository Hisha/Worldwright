# Worldwright ADT Writer (W003)

Status: implemented and structurally validated for flat terrain. Actual loading
by the World of Warcraft 3.3.5a client has **not** been tested and is not
claimed.

## Scope

W003 adds a Qt-, OpenGL-, StormLib-, and AzerothCore-independent C++17 terrain
model and writer. It creates one-file (pre-Cataclysm) ADTs for client build
12340, either individually or for every enabled tile in a `WorldDefinition`.
It does not create MPQs, WDLs, DBCs, water, objects, or publishing artifacts.
Future publication remains the mod-content-manager EPF pipeline.

## Research and provenance

The implementation was derived from public format documentation and independent
inspection of legally installed client data. No Noggit source was copied.

References consulted:

- [wowdev.wiki ADT v18](https://wowdev.wiki/ADT/v18), community format
  documentation (CC BY-SA site content at time of consultation).
- [AzerothCore map extractor ADT definitions](https://github.com/azerothcore/azerothcore-wotlk/blob/master/src/tools/map_extractor/adt.h),
  used only to confirm what the target server extractor checks; GPL-2.0-or-later
  code was not copied into Worldwright.
- [StormLib](https://github.com/ladislav-zezula/StormLib), used as an installed
  read-only extraction tool. It is not a Worldwright runtime dependency.

Two files were extracted read-only to `/tmp` and inspected. They are not stored
or redistributed by this repository:

| Role | Archive path | Bytes | SHA-256 |
|---|---|---:|---|
| Compact terrain | `expansion.MPQ` / `World\Maps\Expansion01\Expansion01_32_34.adt` | 320,752 | `18f66f316cc943e81c7b10dab093d486018dc9956292fc31623c9286381b46e1` |
| Typical/data-rich terrain | `lichking.MPQ` / `World\maps\Northrend\Northrend_37_25.adt` | 2,690,183 | `7e235756ed833f48b61a1bbda36b4c90cab75a4d2c2184f3fd4bc8d6687eed79` |

Both samples have `MVER` 18, a 64-byte `MHDR`, a 4096-byte `MCIN`, and
exactly 256 `MCNK`s. The compact sample was especially useful because its
chunks contain one texture layer, no objects, no alpha payload, no water
payload, flat upward normals, and the same 128-byte MCNK header shape used by
the writer. The typical sample confirmed non-empty `MMDX`, `MMID`, `MWMO`,
`MWID`, `MDDF`, `MODF`, `MH2O`, `MCCV`, `MCSH`, and `MCAL` handling conventions.

Their observed top-level sequences were:

- compact: fixed header/name/reference chunks, 256 `MCNK`s, then `MFBO`;
- typical: fixed header/name/reference chunks, `MH2O`, 256 `MCNK`s, then
  `MFBO`.

Thus optional-chunk position is resolved through `MHDR`, not inferred from one
universal order. W003 omits those optional chunks and uses the common fixed
prefix followed immediately by the 256 terrain chunks.

## Version and chunk rules

Every chunk has an 8-byte header: a reversed-on-disk four-character identifier
and a little-endian `uint32` payload length. There is no general four-byte
alignment rule: real top-level chunks can begin at unaligned offsets. MCNR is
the notable layout exception observed here: its declared payload is 435 bytes
(145 three-byte normals), followed by 13 padding bytes before the next
sub-chunk.

The generated top-level order is:

1. `MVER` (4-byte payload, value 18)
2. `MHDR` (64 bytes)
3. `MCIN` (4096 bytes, 256 entries of 16 bytes)
4. `MTEX` (one NUL-terminated base texture name)
5. empty `MMDX`, `MMID`, `MWMO`, `MWID`, `MDDF`, and `MODF`
6. 256 row-major `MCNK` chunks

Format roles and size rules established for the target version are:

| Chunk | Payload rule |
|---|---|
| `MVER` | Exactly 4 bytes; value 18. |
| `MHDR` | Exactly 64 bytes; flags and offsets described below. |
| `MCIN` | Exactly 4096 bytes: 256 × 16-byte records. |
| `MTEX` | Variable NUL-terminated texture-path string block. |
| `MMDX` / `MWMO` | Variable NUL-terminated M2/WMO path blocks. |
| `MMID` / `MWID` | 4-byte offsets into the corresponding path block. |
| `MDDF` | 36-byte M2 placement records; empty in W003. |
| `MODF` | 64-byte WMO placement records; empty in W003. |
| `MCNK` | 128-byte header followed by variable sub-chunks; 256 required. |
| `MCVT` | Exactly 145 floats / 580 bytes. |
| `MCNR` | Exactly 145 signed triples / 435 bytes in both inspected files. |
| `MCLY` | 16 bytes per texture layer. |
| `MCRF` | 4-byte parent-placement indices; counts come from the MCNK header. |
| `MCAL` | Variable alpha data for layers after the base layer; empty for W003's single layer. |
| `MCSH` | Optional 512-byte terrain shadow bitmask; omitted. |
| `MCLQ` | Legacy variable liquid data; empty marker only in W003. |
| `MFBO` | Optional 36-byte flying bounds. |
| `MTXF` | Optional 4-byte flags corresponding to `MTEX` entries. |

For Wrath water, `MH2O` begins with 256 12-byte per-chunk descriptors and may
contain variable instance/attribute data referenced by those descriptors. It is
not needed for dry W003 terrain. General `MCAL` sizes and compression depend on
layer flags and the WDT big-alpha setting; W003 has no additional layers and
does not implement either alpha representation.

The conservative base texture is
`Tileset\The Blasted Lands\BlastedLandsBlack.blp`, verified in the compact
client sample. The file references that client asset but does not redistribute
it. Whether a textureless/no-layer terrain chunk is accepted by every 3.3.5a
client path is not established, so W003 emits one base `MCLY` layer.

### Top-level chunks considered but not emitted

| Chunk | Meaning and W003 decision |
|---|---|
| `MFBO` | 36-byte flying-bound planes, selected by `MHDR.flags & 1`; optional and omitted. |
| `MH2O` | Wrath liquid table; optional and omitted because W003 has no water. |
| `MTXF` | Per-`MTEX` texture flags; present in some Wrath files, optional, absent in both inspected samples, and omitted. |

`MHDR` offsets point to chunk headers and are relative to the beginning of the
`MHDR` payload (file offset 20 in this layout). Zero means an optional chunk is
absent. `MCIN.offsMCNK` values are absolute file offsets. `MCIN.size` includes
the MCNK's 8-byte chunk header, as confirmed in both samples.

For the current deterministic layout, the first MCNK starts at byte 4292, each
MCNK occupies 1236 bytes including its chunk header, the last starts at byte
319472, and the file ends at byte 320708. These values are consequences checked
by tests, not offsets used to locate chunks while serializing: the writer tracks
actual buffer positions and patches `MHDR`, `MCIN`, and MCNK fields afterward.

## MCNK and sub-chunks

Each MCNK contains a 128-byte header followed by:

| Sub-chunk | Payload | Purpose |
|---|---:|---|
| `MCVT` | 580 | 145 little-endian float height deltas |
| `MCNR` | 435 | 145 signed byte triples; then 13 padding bytes outside the declared payload |
| `MCLY` | 16 | One texture-layer record referencing `MTEX[0]` |
| `MCRF` | 0 | No doodad or WMO references |
| `MCAL` | 0 | No alpha map is needed for a single base layer |
| `MCLQ` | 0 | Empty legacy-liquid marker; Wrath water data would use top-level `MH2O` |
| `MCSE` | 0 | No sound emitters |

`MCSH` (64×64-bit shadow mask) is absent and its offset/size are zero. `MCCV`
(145 BGRA vertex colors) is absent and its offset is zero. Both are optional.
The writer uses MCNK flag `0x8000`, matching the compact sample's
do-not-fix-alpha-map setting. Object counts, holes, area ID, and sound counts
are zero. A future area-authoring milestone must populate area IDs; zero is
structurally valid but has no authored zone semantics.

MCNK sub-chunk offsets are relative to the beginning of the MCNK chunk header,
not its 128-byte payload header. Worldwright computes each from its actual
write position.

## TerrainDefinition

`TerrainDefinition` stores absolute world elevations in the native staggered
WoW topology:

- a 129×129 outer grid (the 9×9 vertices of adjacent chunks share storage);
- a 128×128 inner grid (the interleaved 8×8 center vertices);
- 33,025 unique stored vertices;
- 37,120 values after expansion into 256 × 145 MCVT entries.

Within one MCVT, rows alternate 9 outer vertices and 8 inner vertices for eight
cell rows, followed by the final row of 9 outer vertices. This is not a
conventional square heightmap. Sharing outer vertices in the model prevents
intra-tile seams and leaves a suitable foundation for later sculpting.

The default elevation is zero. `setFlatHeight()` accepts finite positive,
negative, or zero `float` values and rejects NaN/infinity without modifying the
terrain. Query methods return absolute elevation. During serialization, each
chunk's minimum absolute elevation becomes its MCNK base Z and MCVT stores
`absoluteHeight - baseZ`. Thus a flat tile at 100 has base Z 100 and 145 zero
MCVT deltas per chunk.

Normals are encoded as signed bytes `(0, 0, 127)`, matching the compact client
sample. W005A can now serialize non-flat MCVT geometry, but normal recalculation
and confirmation of the non-flat component-axis permutation remain deferred.
Edited geometry therefore exports correctly while its lighting normals remain
flat, a documented compatibility limitation rather than a new ADT format.

## Coordinates and seams

File-name/WDT coordinates remain `(tileX, tileY)`, each in `[0,63]`. ADT world
coordinates run in the opposite direction from increasing tile indices. With:

```
tileSize  = 533.33333
chunkSize = tileSize / 16
center    = tileSize * 32
```

MCNK `(chunkX, chunkY)` stores:

```
position[0] = center - tileY * tileSize - chunkY * chunkSize
position[1] = center - tileX * tileSize - chunkX * chunkSize
position[2] = chunk base elevation
```

This axis swap is deliberate and was confirmed against the real
`Expansion01_32_34` sample. Within a chunk, increasing vertex row/column moves
in the negative corresponding world direction. The automated and independent
Stage B checks compare the final outer row/column with the next tile's first
row/column and also compare their world-space positions, not just heights.

## CLI

Single tile:

```bash
build/worldwright-adt-create \
  --output NewWorld_32_32.adt --tile 32 32 --height 100
```

WDT plus all enabled ADTs from a `WorldDefinition` rectangle:

```bash
mkdir -p generated
build/worldwright-adt-create \
  --world NewWorld --output-dir generated \
  --rectangle 30 30 32 32 --height 100
```

`--tile` is repeatable for sparse worlds. World mode creates the WDT even for
an empty definition and creates ADTs only for enabled tiles. Existing files are
rejected unless `--overwrite` is supplied. Coordinates, rectangles, names,
and elevations are validated; failures return nonzero.

Each individual file uses the W002 safe-write pattern: serialize fully, write a
temporary file in the destination directory, flush/close, then rename. POSIX
replacement is atomic. On Windows, replacing an existing destination requires
remove-then-rename and is not atomic. World mode preflights all existing paths,
but the group of WDT plus ADTs is not a single cross-file transaction; an I/O
failure partway through can leave a partial newly generated set.

## Validation performed

Stage A generated `/tmp/worldwright-stage-a-gwlUdS/NewWorld_32_32.adt` at
elevation 100. Its SHA-256 was
`f6d51e6c22cfe183339d05ba3d78c4d3dd8f6b69c2eb12be83d167d99837fb76`.
An independent raw-byte walker verified all chunk boundaries, offsets, sizes,
indices, positions, 37,120 heights, 37,120 normals, and exact end-of-file.

Stage B generated `NewWorld.wdt` and nine ADTs for `(30,30)` through `(32,32)`.
Independent checks verified the WDT's exact nine bits, all 2,304 MCNK world
positions, every horizontal and vertical outer-edge elevation, four-tile shared
corners, and byte-identical hashes from a second generation run.

CTest covers model behavior, raw layout, offsets, deterministic output,
single/empty/nine/sparse/boundary generation, filenames, invalid numeric input,
existing-file protection, explicit overwrite, temporary-file cleanup, and
seams. Blizzard sample files are not required by automated tests.

## Known limitations and uncertainties

- Client build 12340 loading has not been run. Structural validity and
  AzerothCore-extractor-compatible fields do not prove client acceptance.
- A base texture layer was chosen conservatively from a real minimal sample;
  whether a completely textureless terrain is accepted remains untested.
- Empty `MCLQ` and `MCSE` chunks mirror the compact sample. Whether they can be
  omitted entirely without affecting all client paths is not relied upon.
- No general non-flat normal calculation, textures/alpha painting, area IDs,
  holes, objects, shadows, vertex colors, liquids, flying bounds, or WDL exists.
- `MTXF` flag semantics were not needed by either inspected sample and remain
  outside W003.
- Multi-file output is preflighted but not transactionally rolled back.

The next recommended work is a narrowly scoped compatibility gate: package the
standalone WDT/ADT output through the existing EPF process in a disposable test
client and run the build-12340 client plus AzerothCore `map_extractor`. That is
validation work, not permission to begin W004 or implement a new publisher.
