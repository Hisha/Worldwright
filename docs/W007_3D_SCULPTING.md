# W007 Direct 3D Terrain Sculpting

## Architecture and authoritative data

The 3D editor is another view over `WorldEditorModel`; it does not own terrain
heights. `Terrain3DWidget` converts an accepted pick to `TerrainBrush` and calls
`TerrainHeightEditor::raise`, `lower`, or `smooth`, exactly as Terrain Detail
does. Consequently radius, strength, linear falloff, smoothing snapshot
semantics, enabled-tile filtering, and shared-border writes are identical in
the two views. Switching views only changes the displayed widget. Export still
serializes the same `TerrainDefinition` objects through `WorldExporter` and
`AdtWriter`.

`TerrainMeshBuilder` remains the only render-geometry builder. Each
`TerrainMeshTileRange` identifies a tile's ranges in the existing vertex and
index arrays and its bounds. The metadata accelerates picking; it is not a
second terrain representation.

## Coordinate conventions

Worldwright retains the conventions established before W007:

| System | Horizontal axes | Vertical/elevation axis | Origin/direction |
|---|---|---|---|
| WDT/editor terrain | X, Y | separate height value | X increases with `tileX`/outer column; Y increases with `tileY`/outer row |
| ADT tile | `[tileX * TileSize, (tileX + 1) * TileSize]` and the equivalent Y interval | absolute WoW elevation | outer vertices use whole cells; inner vertices are half-cell offset |
| OpenGL mesh | X, Z | Y | render X = terrain X, render Z = terrain Y, render Y = elevation |
| Qt mouse | screen X, screen Y | none | top-left origin; X right and Y down |
| OpenGL NDC | X, Y | depth | mouse X maps to `2*x/width-1`; mouse Y maps to `1-2*y/height` |

`TerrainCamera` supplies an eye and target. `TerrainPicking::screenRay` derives
forward, screen-right, and screen-up vectors from them and applies the same
45-degree vertical field of view and aspect ratio as rendering. A pick result
maps render `(x,y,z)` back to terrain `(x,z)` plus elevation `y`. No axis is
negated and no new origin is introduced.

## Terrain picking

`TerrainPicking` is Qt-independent. It first rejects tile ranges with a ray/AABB
slab test, then runs a two-sided Moller-Trumbore intersection against the exact
triangle indices rendered by the viewport. It returns the nearest nonnegative
intersection, including the terrain coordinates, elevation, owning enabled
tile, distance, and triangle index. This handles mountains, negative valleys,
steep slopes, duplicated enabled-border triangles, and rays crossing several
surfaces. Empty meshes and misses return no result. Disabled or missing ADTs
have no mesh range and therefore cannot be picked.

`surfaceElevation` uses the same mesh and four triangle indices for the cell
containing a requested terrain point. It supports cursor placement without
scanning a complete ADT and is also covered by slope-interpolation tests.

## Brush cursor

The cursor is a separate dynamic VBO, so mouse motion never rebuilds terrain
geometry. Sixty-four samples form a magenta circular outline at the current
world-space radius. Every sample obtains elevation from the existing render
triangles and receives a render-only 0.15-unit lift to avoid z-fighting. Valid
adjacent samples become GL line segments; segments over missing terrain are
omitted, visibly breaking the ring at a noneditable boundary. The color is
distinct from the cyan wire overlay and orange ADT boundary overlay. Navigate
mode and invalid picks show no cursor.

## Interaction and stroke interpolation

The 3D toolbar exposes mutually exclusive **Navigate**, **Raise**, **Lower**,
and **Smooth** actions. The selected action remains checked. Radius and
strength controls use the same ranges, units, values, and validation as the 2D
controls; changing either view's controls synchronizes the other view.

- Navigate: left-drag orbits, middle-drag pans, wheel zooms.
- Raise/Lower/Smooth: left-drag sculpts, right-drag orbits, middle-drag pans,
  and wheel zooms.
- Fit and Reset preserve their W006 behavior. Cyan wireframe and orange ADT
  boundaries remain independent toggles.

An editable left press emits the initial sample. Both editors use
`TerrainStrokeSampler` with spacing equal to one quarter of the brush radius.
The sampler carries residual distance between input segments, so sampling is
deterministic for the same world-space polyline and does not depend on the
number of mouse-move events. Samples whose centers are not on enabled terrain
are rejected. Release, missing button state, focus loss, pointer leave, widget
hide, view change, and tool change reset the stroke and release mouse capture.

## Cross-ADT edits and refresh

Every accepted sample is passed once to `TerrainHeightEditor`. Its canonical
half-cell keys and deferred snapshot writes update all brush-overlapped enabled
ADTs, including two-tile edges and four-tile corners. Disabled ADTs are neither
created nor changed. The renderer then rebuilds the bounded selected 3 x 3
neighborhood from the authoritative model, regenerating geometry-derived
normals and uploading the refreshed buffers. Camera state, wireframe state,
and boundary state live outside the mesh and remain unchanged. Multiple
samples produced by one mouse event are applied before the single refresh.

The current bounded rebuild is deliberately compatible with W006 and avoids a
whole-world rebuild. Per-tile VBO updates can replace it later without changing
picking or editing APIs.

## Verification

Configure, build, and run all tests with:

```sh
cmake -S . -B build-w007 -DCMAKE_BUILD_TYPE=Debug -DWORLDWRIGHT_BUILD_GUI=ON
cmake --build build-w007 -j2
ctest --test-dir build-w007 --output-on-failure
```

`TerrainPickingTests.cpp` covers empty and missing terrain, flat, raised,
negative, sloped, screen-center camera mapping, misses, enabled ADT boundaries,
surface/cursor interpolation, multiple intersections, and nearest-hit
selection. Existing height-editor and stroke tests cover all three operations,
event-frequency-independent fast strokes, cross-ADT edges, shared corners, and
disabled tiles. GUI tests cover tool controls, checked selection, shared brush
values, view switching, authoritative model preservation, and CPU mesh refresh.
The W005C binary integrity suite supplies the W007 export scenario: a 3 x 3
world receives raised, lowered, smoothed, edge-crossing, and four-ADT-corner
features, is exported through `WorldExporter`, and has independent ADT height,
normal, seam, and deterministic-byte verification.

Qt's offscreen QPA plugin cannot create `QOpenGLWidget` in the accepted Linux
environment. The offscreen GUI test explicitly checks the unavailable message
and does not claim physical rendering validation. A desktop X11 or Wayland
session with an OpenGL 3.3 driver is required for the manual navigation,
cursor, lighting, and live-sculpt acceptance scenario.

## Known limitations

- Rendering and editing remain limited to the selected ADT's enabled 3 x 3
  neighborhood; W007 does not add streaming or LOD.
- Terrain editing and the bounded mesh refresh run synchronously on the GUI
  thread. Very large brushes or unusually dense input can be noticeable.
- There is no undo/redo, texture painting, water, object placement, or 3D
  gizmo support.
