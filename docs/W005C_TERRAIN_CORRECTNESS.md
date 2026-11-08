# W005C terrain-correctness notes

## Initial investigation (before implementation)

Worldwright stores each ADT as a 129x129 outer-vertex grid and a 128x128
inner-vertex grid.  `TerrainDefinition::chunkVertexHeight()` expands those
unique values to the ADT's interleaved 145-value MCVT order: nine outer
vertices, eight inner vertices, repeated for eight rows, followed by the last
nine outer vertices.  Adjacent MCNKs therefore already share storage.

`TerrainHeightEditor` identifies vertices in a world-wide half-cell lattice.
Outer vertices have even/even coordinates, inner vertices have odd/odd
coordinates, and every enabled owner of an edited outer border vertex is
updated.  At a shared vertex the smallest row-major enabled tile is the
canonical read owner.  The GUI edits this toolkit-independent model and
`WorldExporter` passes each enabled tile's `TerrainDefinition` to `AdtWriter`.

For every MCNK, `AdtWriter` selects the minimum of its 145 absolute heights as
the MCNK base Z and writes `absolute - base` to MCVT.  The previous MCNR path
did not calculate normals; it wrote `(0, 0, 127)` for all 145 vertices.

The checked AzerothCore `map_extractor/adt.h` and `adt.cpp` require MVER 18,
follow MHDR/MCIN/MCNK offsets, and require an MCVT payload of exactly 580
bytes.  AzerothCore records but does not consume MCNR in this parser path.  The
W003.5 harness compiles that actual `adt.cpp` with a loose-file `FileLoader`;
its separate bounds preflight is necessary because the upstream parser uses
unchecked pointer arithmetic.

The local Blizzard samples and format references establish these conventions:

- each cell is four triangles connecting its four outer corners to its inner
  center; the winding is upward-facing;
- increasing editor row/column moves in negative WoW X/Y respectively (the
  axes are swapped in MCNK positions);
- MCNR contains 145 triples of signed bytes scaled by 127; the file components
  correspond to WoW horizontal X, horizontal Y, and up Z (often described as
  `X,Z,Y` by Y-up renderers);
- flat terrain is exactly `(0,0,127)`;
- the declared MCNR payload remains 435 bytes, followed by the existing 13
  bytes of external padding.

The implementation below uses these findings; no AzerothCore or client files
are modified.

## Normal generation and boundary policy

For every serialized vertex, the writer enumerates incident cells and uses the
four center-to-edge triangles in the ADT winding. Face cross-products are
accumulated (area weighting), then normalized. Non-finite/degenerate faces are
ignored and a vertex with no usable face deterministically points up. Values
are rounded to the nearest signed byte after multiplication by 127 and clamped
to `[-127,127]`; `-128` is never emitted.

`WorldExporter` provides the available 3x3 tile neighborhood. Height lookup
uses the editor's row-major canonical owner, so export order cannot affect a
shared result. A missing or disabled neighbor supplies no cell faces. This
does not invent terrain and gives a deterministic one-sided edge normal. The
single-ADT writer overload uses the same policy with only its center tile.

## Serialized verification

Before a world export is committed, its staged ADTs are reopened. The verifier
follows MHDR, MCIN, MCNK, MCVT, and MCNR references with bounds and payload
checks, reconstructs boundary height as `MCNK base Z + MCVT delta`, and compares
every adjacent border (including endpoints/corners). Height tolerance is
`0.0001` world units. MCNR values must match exactly after quantization. A
mismatch aborts the transaction and reports both files/coordinates, border,
vertex, reconstructed heights, difference, and encoded normals.

The independent CTest parser additionally reconstructs every outer and inner
height, checks finite values and normalized MCNR triples, compares the result
to `TerrainDefinition`, verifies duplicate MCNK vertices, and rejects invalid
MCIN references, truncated MCNK data, invalid MCVT offsets/sizes, and
out-of-bounds structures. It covers flat, analytic sloped, raised, lowered,
smoothed, two-ADT, four-corner, mixed untouched/edited, and repeated-export
terrain.

## AzerothCore compatibility procedure and result

The temporary W003.5 harness at `/tmp/worldwright-compat-test` compiles the
repository-external, unmodified AzerothCore
`src/tools/map_extractor/adt.cpp`. Its loose-file loader adds a bounds preflight
before invoking that actual parser, whose native pointer arithmetic otherwise
cannot safely reject every truncated reference. It was built with
AddressSanitizer and UndefinedBehaviorSanitizer and run against:

- nine fresh flat W005C ADTs;
- nine fresh sculpted W005C ADTs containing border/corner features and large
  positive/negative elevation changes;
- all nine available `/tmp/worldwright-w005b-test` ADTs.

All 27 files were accepted. Five malformed variants per run (MCNK signature,
MCIN reference, MCVT offset, MCVT payload size, and truncation) were rejected.
ASan and UBSan reported no findings.

Commands used:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

cmake -S /tmp/worldwright-compat-test \
  -B /tmp/worldwright-compat-test/build-w005c \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/worldwright-compat-test/build-w005c --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  /tmp/worldwright-compat-test/build-w005c/adt_compat_test INPUT_DIRECTORY \
  /tmp/worldwright-compat-test
```

Parser acceptance does not establish client rendering/lighting, playability,
complete map extraction, or EPF publication. No live client/server and no
publishing pipeline were exercised.

