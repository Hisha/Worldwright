# Worldwright

Worldwright is a standalone, open-source editor for creating World of Warcraft
3.3.5a world definitions and terrain. W004.5 provides a focused 2D
desktop editor for laying out a 64 x 64 world grid, assigning a flat elevation
to each enabled tile, and exporting loose WDT and ADT development assets.
W005A adds a reusable, headless terrain-height engine with raise, lower,
smoothing, cross-ADT brushes, and exact shared-edge consistency. Its operations
are available to core code; a graphical sculpting interface is future work.

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
5. Choose **File > Export World** and select an existing output directory.

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

The current GUI remains a 2D layout editor. It creates one active world at a
time and exposes only flat elevation assignment; it does not yet expose the
W005A brush engine as graphical sculpting. Worldwright does not provide 3D
terrain rendering, texture painting, water, object or NPC placement, DBC
editing, MPQ creation, EPF publishing, Portalkeeper integration, database
changes, or client-installation changes. Exported files are loose development
assets; future publishing is expected to use the mod-content-manager EPF
pipeline.

More format and architecture documentation is available in [docs](docs/README.md).
