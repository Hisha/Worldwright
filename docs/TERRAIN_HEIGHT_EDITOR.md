# Terrain Height Editing Engine (W005A)

Status: implemented as a headless C++17 core component. W005A does not add a
graphical sculpting tool or terrain rendering.

## Data ownership

`TerrainDefinition` remains the only terrain-height representation. The engine
edits its 16,641 outer and 16,384 inner values directly; there is no second
heightmap to synchronize. `WorldEditorModel` continues to own one terrain
definition for each enabled tile. Disabling a tile erases that terrain, so
reenabling it creates flat height-zero terrain, matching the pre-W005A model
lifecycle. Flat-height assignment is unchanged.

## Editing coordinates

Editing uses a continuous terrain space aligned with WDT and ADT file-name
tile coordinates:

- X increases with `tileX` and with a vertex's local column.
- Y increases with `tileY` and with a vertex's local row.
- A tile occupies 533.33333 units on each axis.
- The outer grid has 129×129 vertices at `tileOrigin + (column,row) *
  tileSize/128`, including both tile boundaries.
- The inner grid has 128×128 vertices at `tileOrigin +
  (column+0.5,row+0.5) * tileSize/128`.
- Brush centers and radii use these same terrain-space units.

The inner points are therefore cell centers, not coincident with outer points.
This positive tile-grid coordinate space is intentionally distinct from ADT's
stored WoW world axes. `AdtWriter` retains the established axis swap and
decreasing-coordinate conversion described in `ADT_WRITER.md`; the editor does
not silently reverse either brush axis.

## Raise, lower, and falloff

Raise and lower share one deterministic linear influence:

```
influence = max(0, 1 - distance / radius)
result = sourceHeight +/- strength * influence
```

Radius must be finite and greater than zero. Raise/lower strength must be
finite and non-negative. Centers must be finite. Calculations use `double`,
are range-checked before conversion to `float`, and are committed only after
the whole operation validates, preventing partial edits and float overflow.
Vertices outside the radius are not written.

## Smoothing

Smoothing treats the staggered terrain as its real bipartite mesh. Each inner
vertex is directly connected to its four surrounding outer vertices. Each
outer vertex is connected to the available inner vertices at its four diagonal
half-cell offsets. At terrain or missing-tile boundaries, only existing
neighbors participate.

One pass lazily snapshots each source height it needs and defers every write
until all results validate. An affected vertex blends toward the arithmetic
mean of its available connected neighbors by `strength * influence`; smoothing
strength must be in `[0,1]`. These snapshot semantics make the result
independent of vertex and tile iteration order. Neighbor lookups cross into an
adjacent enabled tile only when the mesh connection requires it.

## Multiple tiles and seams

The engine tests enabled tile bounds against the world-space brush, edits all
overlapping enabled tiles, and never enables missing tiles. Duplicate outer
vertices on horizontal and vertical ADT edges use one integer half-cell world
key. A corner shared by four ADTs consequently has one key as well.

Every touched copy is calculated from the same canonical source and receives
the same resulting `float`. This canonical-source policy gives exact
horizontal, vertical, and four-way equality without repeated copy or averaging
drift.

For a preexisting mismatch at a touched seam, the canonical source is the
value owned by the smallest `(tileY, tileX)` tile (row-major, conventionally
northwest). This is a deliberate ownership rule, not an average. Untouched
seams are not rewritten by an unrelated brush. Repeated edits remain exact
because all copies start from and return to the same canonical value.

## Export and limitations

`AdtWriter` already derives each MCNK base elevation and MCVT delta from
`TerrainDefinition`, so no ADT format or writer change was needed beyond
sharing the existing tile-size constant. Tests reconstruct an edited absolute
height from serialized MCNK base Z plus its MCVT value.

At W005A completion the writer still emitted flat `(0,0,127)` MCNR values.
W005C supersedes that limitation with geometry-derived outer/inner normals,
cross-ADT boundary context, serialized seam verification, and actual
AzerothCore parser-harness coverage. See
[W005C terrain correctness](W005C_TERRAIN_CORRECTNESS.md).

The engine tests vertices only in brush-overlapped tiles and caches only source
heights needed by affected vertices (plus smoothing neighbors). It never copies
unrelated world terrain. Each overlapped tile's vertex arrays are still scanned
to find the circular brush footprint, so extremely large brushes scale with the
number of covered tiles and remain a known performance limit.
