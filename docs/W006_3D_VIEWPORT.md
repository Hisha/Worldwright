# W006 Interactive 3D Terrain Viewport

## Architecture

`MainWindow` remains the owner of a single `WorldEditorModel` and switches a
`QStackedWidget` among World Layout, Terrain Detail, and 3D View. No terrain is
copied back into the model: `WorldDefinition` is authoritative for enabled
ADTs, while each enabled tile's `TerrainDefinition` owns its 129 x 129 outer
and 128 x 128 inner height grids. Terrain Detail edits those objects directly.

`TerrainMeshBuilder` and `TerrainCamera` are C++17, Qt-independent components
in `WorldwrightEditorCore`. `Terrain3DWidget` is confined to `WorldwrightGui`,
so CLI tools, terrain generation, and serialization have no Qt or OpenGL
dependency. Entering 3D View explicitly rebuilds render data. Model changes
made while already in the viewport through the Properties panel also refresh
it; hidden viewports do no rendering or buffer rebuild work.

## Mesh construction

Render X follows editor columns, render Z follows editor rows, and render Y is
elevation. For every terrain cell the builder emits the ADT topology:

1. inner center, top-left outer, bottom-left outer;
2. inner center, top-right outer, top-left outer;
3. inner center, bottom-right outer, top-right outer;
4. inner center, bottom-left outer, bottom-right outer.

This is the same fan and winding used by `AdtWriter` for MCNR calculation.
Face normals are accumulated using global half-cell vertex keys. Duplicate
outer vertices at adjacent tile edges therefore receive identical smoothed
normals as well as the identical heights already maintained by
`TerrainHeightEditor`. Missing or disabled ADTs emit no geometry. Negative
elevations are passed through unchanged.

ADT boundary lines use the tile perimeter's existing outer vertices with a
small render-only vertical offset. They never modify terrain.

## Graphics pipeline

The widget requests an OpenGL 3.3 core-profile, double-buffered context with a
24-bit depth buffer. Terrain is uploaded to a VAO, VBO, and 32-bit index
buffer. GLSL vertex and fragment shaders provide fixed ambient and directional
diffuse lighting with a neutral green terrain color and a subtle elevation
tint. Fill, wire overlay, and boundary lines share the shader; depth testing
is enabled. The wire overlay uses an unlit light-cyan color and negative line
polygon offset, keeping its one-pixel lines visible without z-fighting the
shaded fill. GPU resources are deleted while the widget's context is current.

Context, version, function-loader, and shader failures are reported inside the
viewport and to the diagnostic log instead of dereferencing invalid resources.

## Camera

The camera orbits a target using yaw, a pitch clamped between -89 and -5
degrees, and a positive clamped distance. Left-drag changes yaw and pitch;
middle-drag pans along camera screen-right and screen-up; wheel steps multiply
distance by 0.85. Fit derives target and distance from current mesh bounds.
Near and far planes scale with distance, supporting negative elevations,
steep terrain, and multi-ADT extents. Reset restores yaw -45 and pitch -35
without discarding the current fitted target or distance.

## Performance and synchronization

W006 builds the selected ADT and enabled neighbors within one tile in each
direction. This supports the complete 3 x 3 acceptance world while bounding
CPU mesh construction and GPU memory for large continents. A full 3 x 3 set
contains 297,225 unique-per-tile vertices and 589,824 triangles before shared
edge duplication. Buffers are static between refreshes and are not rebuilt per
frame. Future work can replace the focus policy with camera streaming or LOD
without changing the terrain model.

## Platform limitations

- A desktop OpenGL 3.3 core-profile driver and Qt 6 OpenGL/OpenGLWidgets are
  required for the viewport.
- Qt's `offscreen` QPA plugin does not support `QOpenGLWidget` in the accepted
  Ubuntu environment. Worldwright shows an explanatory placeholder under that
  plugin so widget tests remain headless. The offscreen test verifies UI and
  CPU synchronization only; it does not prove physical GPU rendering.
- Wire line width above one pixel is driver-dependent. Mesh visibility does
  not depend on wide lines.
- W006 has no streaming, LOD, textures, water, objects, or 3D sculpting.

## Verification

`TerrainMeshTests.cpp` covers flat, raised, and negative geometry; outer and
inner mapping; triangle count and winding; shared-edge positions, heights, and
normals; missing neighbors; empty worlds; deterministic generation; and camera
fit/orbit/pan/zoom/clipping mathematics. `TerrainDetailGuiTests.cpp` covers
view switching, terrain preservation, and CPU mesh synchronization, or the
explicit unsupported-platform placeholder when run with Qt offscreen.

The manual acceptance scenario still requires a desktop session and visual
inspection of an actual driver-backed viewport; the automated offscreen test
must not be interpreted as that evidence.
