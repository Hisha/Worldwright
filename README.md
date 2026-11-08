# Worldwright

Worldwright is a standalone, open-source editor for creating World of Warcraft
3.3.5a world definitions and terrain. W005B provides a 2D desktop editor with
two workspaces: a 64 x 64 World Layout and an elevation-colored Terrain Detail
canvas. Terrain Detail exposes the W005A raise, lower, smoothing, and cross-ADT
brush engine while editing the same geometry later written to ADT files.

The repository also provides the existing command-line tools:

- `worldwright-wdt-info` inspects a terrain WDT.
- `worldwright-wdt-create` creates a WDT from tile coordinates.
- `worldwright-adt-create` creates flat ADTs or a complete flat world set.

The graphical editor and command-line tools use the same `WorldDefinition`,
`TerrainDefinition`, `WdtWriter`, and `AdtWriter` implementations.

## Building

Worldwright requires a C++17 compiler and CMake 3.16 or newer. The graphical
application requires Qt 6.2 or newer with the Widgets component. On Ubuntu the
Qt development packages are:

```text
qt6-base-dev qt6-base-dev-tools
```

Worldwright does not install dependencies. After the dependencies are present:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the editor with:

```sh
./build/worldwright
```

If Qt development files are unavailable, CMake reports the packages above and
continues with the CLI tools and nonvisual tests. To request a deliberately
headless build and suppress Qt discovery, configure with:

```sh
cmake -S . -B build -DWORLDWRIGHT_BUILD_GUI=OFF
```

The GUI is never needed to run the core unit tests.

## Creating and exporting a world

1. Choose **File > New World** and enter a name such as `NewWorld`. Names must
   begin with an ASCII letter and contain only ASCII letters, digits, and
   underscores.
2. Use **Select** and drag across the map to select one tile or a rectangle.
3. Choose **Enable selected**. Enabled tiles are shown in teal; the current
   selection is highlighted in yellow.
4. Enter the flat height in the **Elevation** field in the Properties panel.
   The value is applied to every enabled tile in the selection.
5. With one enabled tile selected, choose **Edit Terrain** to open Terrain
   Detail. Choose **Return to World** to return to the layout without losing
   sculpted heights.
6. Choose **File > Export World** and select an existing output directory.

## Map tools and navigation

The editing toolbar provides four mutually exclusive tools:

- **Select**: left-click selects one tile and left-drag selects a rectangle.
  Selection alone never changes terrain. **Enable selected**, **Disable
  selected**, and the Properties panel continue to operate on the selection.
- **Paint Terrain**: left-click or left-drag enables tiles. Fast drags are
  joined across every sampled grid cell so they do not leave gaps. A newly
  enabled tile starts at elevation 0; painting an enabled tile preserves its
  elevation.
- **Erase Terrain**: left-click or left-drag disables tiles. Repeatedly erasing
  a disabled tile is harmless and unrelated tiles are not changed.
- **Pan**: left-drag moves the canvas without selecting or editing terrain.

Right-drag or middle-drag also pans in every tool. The mouse wheel zooms toward
the cursor and is limited to useful minimum and maximum scales. **Fit World**
on the toolbar or in the View menu centers the complete 64 x 64 world in the
available canvas. The fitted view continues adapting when the window is
resized; manual zoom or pan is preserved until **Fit World** is requested.
**Reset View** restores the centered 100% view.

The status bar continuously shows the active tool, hovered tile coordinates,
whether the hovered tile is enabled, the total enabled tile count, and the
current zoom. Hover information clears when the pointer leaves the canvas.
The Properties panel retains the selected coordinates, selection size,
enabled state, and elevation. At close zoom levels, coordinates are also
drawn inside individual cells.

## Terrain Detail sculpting

Select an enabled ADT in World Layout and choose **Edit Terrain** (also
available under **View > Edit Terrain**). The selected tile is outlined in
yellow as the active ADT. The **Active ADT** list changes focus to any other
enabled tile without rebuilding or flattening either tile. **Return to World**
switches back to the 64 x 64 layout; reentering Terrain Detail shows the same
edited geometry.

Terrain Detail draws the W005A staggered outer and inner vertex grids as a 2D
elevation image. Dark blue represents the low end of the visible scale, then
green and brown, with light gray at the high end. A legend reports the current
scale. Flat terrain uses a small default elevation range so its color remains
meaningful and the first edits remain visible. Enabled ADTs are colored,
unavailable terrain is checkered, and missing tiles are never enabled by a
brush.

The terrain toolbar provides four mutually exclusive tools:

- **Raise** adds the Strength value as an elevation delta at each spaced brush
  sample, reduced toward the edge by W005A's linear falloff.
- **Lower** subtracts the same elevation delta and falloff.
- **Smooth** blends vertices toward their mesh-neighbor average. Strength is a
  blend fraction from 0.01 to 1.00.
- **Pan** makes left-drag move the viewport without changing terrain.

**Radius** is measured in W005A world units; its default is four terrain cells,
and the control tooltip reports the cell size. Invalid, zero, negative, and
nonfinite values cannot enter the terrain engine. The white-and-black brush
outline uses the exact world-to-screen transform used for sculpting, scales
with zoom, crosses ADT outlines, and appears only when its center is on enabled
terrain.

Left-click or left-drag applies the selected sculpting tool. Drag paths are
sampled every quarter brush radius. The sampler carries unused distance between
mouse events, so fast movement does not leave large gaps and extra move events
do not multiply full-strength applications at the same location. The mouse
wheel zooms around the cursor.
Right- or middle-drag pans in every tool. **Fit Terrain** frames the active ADT
with one ADT of context in each available direction. The status bar reports
hovered world coordinates, edit availability, and zoom.

A brush near an enabled edge is passed once to the multi-ADT W005A engine. It
therefore edits enabled horizontal, vertical, and corner neighbors in one
operation and keeps their shared outer vertices identical. A missing neighbor
is safely ignored and never created. Export serializes these current
`TerrainDefinition` objects directly; it does not regenerate flat terrain.

For a world named `NewWorld` with tiles (30,30) through (32,32), export creates
`NewWorld.wdt` and nine files named `NewWorld_30_30.adt` through
`NewWorld_32_32.adt`. Existing files are replaced only after explicit
confirmation. Files are generated in a staging directory and committed as a
set so a generation error is not reported as a successful partial export.

The result can be checked with:

```sh
./build/worldwright-wdt-info /path/to/export/NewWorld.wdt --list
```

## Terrain-height engine

The Qt-independent `TerrainHeightEditor` edits the existing staggered outer and
inner vertex grids directly. Brushes use linear distance falloff in a common
world space, can span enabled ADTs, and do not create missing tiles. Smoothing
uses a source snapshot and the actual outer-to-inner mesh neighborhood.
Horizontal, vertical, and four-tile corner copies receive one canonical result
to prevent cracks and repeated-edit drift. See
[the W005A terrain documentation](docs/TERRAIN_HEIGHT_EDITOR.md) for coordinate
and seam policies.

## Current limitations

Terrain Detail is a 2D elevation visualization, not a 3D renderer. Tile images
are cached and only brush-overlapped tiles are invalidated, but very large
brushes spanning many enabled ADTs can still take noticeable time because the
correctness-first W005A operation and affected images run synchronously on the
UI thread. There is no undo/redo or project-file persistence. Worldwright does
not provide terrain texture painting, water, object or NPC placement, DBC
editing, MPQ creation, EPF publishing, Portalkeeper integration, database
changes, or client-installation changes. Exported files are loose development
assets; future publishing is expected to use the mod-content-manager EPF
pipeline.

More format and architecture documentation is available in [docs](docs/README.md).
